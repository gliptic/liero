#include "video_recorder.h"

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libavutil/channel_layout.h>
#include <libavutil/opt.h>
#include <libavutil/imgutils.h>
#include <string.h>
#include <assert.h>

#define STREAM_PIX_FMT    AV_PIX_FMT_YUV420P
#define SOURCE_PIX_FMT    AV_PIX_FMT_BGRA

#define SCALE_FLAGS SWS_BICUBIC

static int write_encoded_frame(AVFormatContext *fmt_ctx, AVCodecContext *c,
                               AVStream *st, AVFrame *frame, AVPacket *pkt)
{
	int ret;

	ret = avcodec_send_frame(c, frame);
	if (ret < 0) {
		fprintf(stderr, "Error sending a frame to the encoder: %s\n",
				av_err2str(ret));
		return ret;
	}

	while (ret >= 0) {
		ret = avcodec_receive_packet(c, pkt);
		if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
			break;
		else if (ret < 0) {
			fprintf(stderr, "Error encoding a frame: %s\n", av_err2str(ret));
			return ret;
		}

		av_packet_rescale_ts(pkt, c->time_base, st->time_base);
		pkt->stream_index = st->index;

		ret = av_interleaved_write_frame(fmt_ctx, pkt);
		if (ret < 0) {
			fprintf(stderr, "Error while writing output packet: %s\n", av_err2str(ret));
			return ret;
		}
	}

	return 0;
}

static AVFrame *alloc_frame(enum AVPixelFormat pix_fmt, int width, int height)
{
	AVFrame *frame;
	int ret;

	frame = av_frame_alloc();
	if (!frame)
		return NULL;

	frame->format = pix_fmt;
	frame->width  = width;
	frame->height = height;

	ret = av_frame_get_buffer(frame, 0);
	if (ret < 0) {
		fprintf(stderr, "Could not allocate frame data.\n");
		av_frame_free(&frame);
		return NULL;
	}

	return frame;
}

int VidrecInit(video_recorder* self, char const* filename, int width, int height, AVRational framerate)
{
	memset(self, 0, sizeof(*self));

	/* Allocate the output media context */
	avformat_alloc_output_context2(&self->oc, NULL, "mp4", filename);
	if (!self->oc) {
		fprintf(stderr, "Could not allocate output context\n");
		return 1;
	}
	self->fmt = self->oc->oformat;

	/* Add video stream */
	{
		const AVCodec *codec;
		AVCodecContext *c;

		codec = avcodec_find_encoder(self->fmt->video_codec);
		if (!codec) {
			fprintf(stderr, "Video codec not found\n");
			return 1;
		}

		self->video_st = avformat_new_stream(self->oc, NULL);
		if (!self->video_st) {
			fprintf(stderr, "Could not allocate video stream\n");
			return 1;
		}
		self->video_st->id = self->oc->nb_streams - 1;

		c = avcodec_alloc_context3(codec);
		if (!c) {
			fprintf(stderr, "Could not alloc video encoding context\n");
			return 1;
		}
		self->video_enc = c;

		c->codec_id = self->fmt->video_codec;
		c->width    = width;
		c->height   = height;
		self->video_st->time_base = framerate;
		c->time_base = framerate;
		c->pix_fmt = STREAM_PIX_FMT;
		c->profile = AV_PROFILE_H264_HIGH;
		c->max_b_frames = 2;
		c->gop_size = 30;
		c->bit_rate = 7500000;

		if (self->oc->oformat->flags & AVFMT_GLOBALHEADER)
			c->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

		/* Open the codec */
		if (avcodec_open2(c, codec, NULL) < 0) {
			fprintf(stderr, "Could not open video codec\n");
			return 1;
		}

		/* Allocate the encoded raw picture */
		self->picture = alloc_frame(c->pix_fmt, c->width, c->height);
		if (!self->picture) {
			fprintf(stderr, "Could not allocate picture\n");
			return 1;
		}

		/* Allocate temporary picture for source format conversion */
		self->tmp_picture = alloc_frame(SOURCE_PIX_FMT, width, height);
		if (!self->tmp_picture) {
			fprintf(stderr, "Could not allocate temporary picture\n");
			return 1;
		}

		/* Copy the stream parameters to the muxer */
		if (avcodec_parameters_from_context(self->video_st->codecpar, c) < 0) {
			fprintf(stderr, "Could not copy video stream parameters\n");
			return 1;
		}
	}

	/* Add audio stream */
	{
		const AVCodec *codec;
		AVCodecContext *c;

		codec = avcodec_find_encoder(self->fmt->audio_codec);
		if (!codec) {
			fprintf(stderr, "Audio codec not found\n");
			return 1;
		}

		self->audio_st = avformat_new_stream(self->oc, NULL);
		if (!self->audio_st) {
			fprintf(stderr, "Could not allocate audio stream\n");
			return 1;
		}
		self->audio_st->id = self->oc->nb_streams - 1;

		c = avcodec_alloc_context3(codec);
		if (!c) {
			fprintf(stderr, "Could not alloc audio encoding context\n");
			return 1;
		}
		self->audio_enc = c;

		c->sample_fmt  = AV_SAMPLE_FMT_FLTP;
		c->bit_rate    = 128000;
		c->sample_rate = 44100;
		av_channel_layout_copy(&c->ch_layout, &(AVChannelLayout)AV_CHANNEL_LAYOUT_MONO);
		c->profile = AV_PROFILE_AAC_LOW;
		c->strict_std_compliance = -2;
		self->audio_st->time_base = (AVRational){ 1, c->sample_rate };
		c->time_base = (AVRational){ 1, c->sample_rate };

		if (self->oc->oformat->flags & AVFMT_GLOBALHEADER)
			c->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

		/* Open the codec */
		if (avcodec_open2(c, codec, NULL) < 0) {
			fprintf(stderr, "Could not open audio codec\n");
			return 1;
		}

		/* Copy the stream parameters to the muxer */
		if (avcodec_parameters_from_context(self->audio_st->codecpar, c) < 0) {
			fprintf(stderr, "Could not copy audio stream parameters\n");
			return 1;
		}

		/* Set up resampler: liero samples are 44.1 kHz S16 mono, encoder wants FLTP */
		self->swr = swr_alloc();
		if (!self->swr) {
			fprintf(stderr, "Could not allocate resampler context\n");
			return 1;
		}
		av_opt_set_chlayout  (self->swr, "in_chlayout",       &c->ch_layout,      0);
		av_opt_set_int       (self->swr, "in_sample_rate",     44100,              0);
		av_opt_set_sample_fmt(self->swr, "in_sample_fmt",      AV_SAMPLE_FMT_S16, 0);
		av_opt_set_chlayout  (self->swr, "out_chlayout",      &c->ch_layout,      0);
		av_opt_set_int       (self->swr, "out_sample_rate",    c->sample_rate,    0);
		av_opt_set_sample_fmt(self->swr, "out_sample_fmt",     c->sample_fmt,     0);
		if (swr_init(self->swr) < 0) {
			fprintf(stderr, "Failed to initialize the resampling context\n");
			return 1;
		}
	}

	/* Allocate packet */
	self->tmp_pkt = av_packet_alloc();
	if (!self->tmp_pkt) {
		fprintf(stderr, "Could not allocate AVPacket\n");
		return 1;
	}

	/* Open the output file */
	if (!(self->fmt->flags & AVFMT_NOFILE)) {
		if (avio_open(&self->oc->pb, filename, AVIO_FLAG_WRITE) < 0) {
			fprintf(stderr, "Could not open '%s'\n", filename);
			return 1;
		}
	}

	/* Write the stream header */
	if (avformat_write_header(self->oc, NULL) < 0) {
		fprintf(stderr, "Error occurred when opening output file\n");
		return 1;
	}

	return 0;
}

int VidrecFinalize(video_recorder* self)
{
	/* Flush video encoder */
	write_encoded_frame(self->oc, self->video_enc, self->video_st, NULL, self->tmp_pkt);

	/* Flush audio encoder */
	write_encoded_frame(self->oc, self->audio_enc, self->audio_st, NULL, self->tmp_pkt);

	/* Write the trailer */
	av_write_trailer(self->oc);

	/* Clean up video */
	avcodec_free_context(&self->video_enc);
	av_frame_free(&self->picture);
	av_frame_free(&self->tmp_picture);

	/* Clean up audio */
	avcodec_free_context(&self->audio_enc);

	/* Clean up shared resources */
	av_packet_free(&self->tmp_pkt);
	sws_freeContext(self->img_convert_ctx);
	swr_free(&self->swr);

	if (!(self->fmt->flags & AVFMT_NOFILE))
		avio_closep(&self->oc->pb);

	avformat_free_context(self->oc);

	return 0;
}

int VidrecWriteAudioFrame(video_recorder* self, int16_t* samples, int audio_input_frame_size)
{
	AVCodecContext *c = self->audio_enc;
	AVFrame *frame;
	int ret;

	/* Allocate a frame for the converted audio */
	frame = av_frame_alloc();
	if (!frame) {
		fprintf(stderr, "Could not allocate audio frame\n");
		return 1;
	}

	frame->format = c->sample_fmt;
	av_channel_layout_copy(&frame->ch_layout, &c->ch_layout);
	frame->sample_rate = c->sample_rate;
	frame->nb_samples = audio_input_frame_size;

	ret = av_frame_get_buffer(frame, 0);
	if (ret < 0) {
		fprintf(stderr, "Could not allocate audio frame buffer\n");
		av_frame_free(&frame);
		return 1;
	}

	/* Convert from S16 to the encoder's sample format */
	ret = swr_convert(self->swr, frame->data, audio_input_frame_size,
	                  (const uint8_t **) &samples, audio_input_frame_size);
	if (ret < 0) {
		fprintf(stderr, "Error while converting audio\n");
		av_frame_free(&frame);
		return 1;
	}
	frame->nb_samples = ret;

	frame->pts = self->audio_pts;
	self->audio_pts += frame->nb_samples;

	/* Encode and write */
	ret = write_encoded_frame(self->oc, c, self->audio_st, frame, self->tmp_pkt);

	av_frame_free(&frame);
	return ret < 0 ? 1 : 0;
}

int VidrecWriteVideoFrame(video_recorder* self, AVFrame* pic)
{
	int ret;
	AVCodecContext *c = self->video_enc;

	/* Set up the conversion context if needed */
	if (self->img_convert_ctx == NULL) {
		self->img_convert_ctx = sws_getContext(pic->width, pic->height,
		                                       SOURCE_PIX_FMT,
		                                       c->width, c->height,
		                                       c->pix_fmt,
		                                       SCALE_FLAGS, NULL, NULL, NULL);
		if (self->img_convert_ctx == NULL) {
			fprintf(stderr, "Cannot initialize the conversion context\n");
			return 1;
		}
	}

	/* Make sure we can write to the frame */
	ret = av_frame_make_writable(self->picture);
	if (ret < 0) {
		fprintf(stderr, "Could not make video frame writable\n");
		return 1;
	}

	sws_scale(self->img_convert_ctx, (const uint8_t * const*) pic->data,
	          pic->linesize, 0, pic->height, self->picture->data,
	          self->picture->linesize);

	self->picture->pts = self->frame_count++;

	/* Encode and write */
	ret = write_encoded_frame(self->oc, c, self->video_st, self->picture, self->tmp_pkt);
	return ret < 0 ? 1 : 0;
}
