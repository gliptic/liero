#include "video_recorder.h"

#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libavutil/avutil.h"
#include "libavutil/error.h"
#include "libavutil/opt.h"
#include "assert.h"

#define STREAM_PIX_FMT    PIX_FMT_YUV420P
#define SOURCE_PIX_FMT    PIX_FMT_BGRA

static int sws_flags = SWS_BICUBIC;

static AVFrame *alloc_picture(enum PixelFormat pix_fmt, int width, int height)
{
	AVFrame *picture;
	uint8_t *picture_buf;
	int size;

	picture = av_frame_alloc();
	if (!picture)
		return NULL;
	size        = avpicture_get_size(pix_fmt, width, height);
	picture_buf = av_malloc(size);
	if (!picture_buf) {
		av_free(picture);
		return NULL;
	}
	avpicture_fill((AVPicture *)picture, picture_buf,
				   pix_fmt, width, height);
	return picture;
}

int vidrec_init(video_recorder* self, char const* filename, int width, int height, AVRational framerate)
{
	memset(self, 0, sizeof(*self));

	self->fmt = av_guess_format("mp4", NULL, "video/h264");
	self->oc = avformat_alloc_context();
	self->oc->oformat = self->fmt;
	self->pts = 0;

	{
		AVCodecContext *c;
		AVCodec *codec;

		/* find the video encoder */
		codec = avcodec_find_encoder(self->fmt->video_codec);
		if (!codec) {
			fprintf(stderr, "codec not found\n");
			return 1;
		}

		self->video_st = avformat_new_stream(self->oc, codec);
		if (!self->video_st) {
			fprintf(stderr, "Could not alloc stream\n");
			return 1;
		}

		c = self->video_st->codec;

		self->video_st->time_base = framerate;

		/* Resolution must be a multiple of two. */
		c->width    = width;
		c->height   = height;
		/* timebase: This is the fundamental unit of time (in seconds) in terms
		 * of which frame timestamps are represented. For fixed-fps content,
		 * timebase should be 1/framerate and timestamp increments should be
		 * identical to 1. */
		c->time_base = framerate;
		/* This is configured according to what Youtube wants, see:
		 * https://support.google.com/youtube/answer/1722171?hl=en */
		c->pix_fmt = STREAM_PIX_FMT;
		c->profile = FF_PROFILE_H264_HIGH;
		c->max_b_frames = 2;
		c->gop_size = 30;
		c->bit_rate = 7500000;

		/* Some formats want stream headers to be separate. */
		if (self->oc->oformat->flags & AVFMT_GLOBALHEADER)
			c->flags |= CODEC_FLAG_GLOBAL_HEADER;

	}

	{
		AVCodecContext *c;
		AVCodec *codec;

		/* find the audio encoder */
		codec = avcodec_find_encoder(self->fmt->audio_codec);
		if (!codec) {
			fprintf(stderr, "codec not found\n");
			return 1;
		}

		self->audio_st = avformat_new_stream(self->oc, codec);
		if (!self->audio_st) {
			fprintf(stderr, "Could not alloc stream\n");
			return 1;
		}

		self->audio_st->time_base = framerate;
		c = self->audio_st->codec;
		c->time_base = framerate;
		/* put sample parameters */
		c->sample_fmt  = AV_SAMPLE_FMT_FLTP;
		c->bit_rate    = 128000;
		/* FIXME: youtube wants 48 khz, but using that causes sync problems
		 * that I haven't been able to solve so far */
		c->sample_rate = 44100;
		c->channels    = 1;
		c->profile = FF_PROFILE_AAC_LOW;
		c->channel_layout = AV_CH_LAYOUT_MONO;
		c->strict_std_compliance = -2;

		// some formats want stream headers to be separate
		if (self->oc->oformat->flags & AVFMT_GLOBALHEADER)
			c->flags |= CODEC_FLAG_GLOBAL_HEADER;

		/* liero samples are 44.1 khz S16, while we need 48 khz FLTP */
		self->swr = swr_alloc();
		av_opt_set_int(self->swr, "in_channel_layout",  c->channel_layout, 0);
		av_opt_set_int(self->swr, "out_channel_layout", c->channel_layout,  0);
		av_opt_set_int(self->swr, "in_sample_rate",     44100, 0);
		av_opt_set_int(self->swr, "out_sample_rate",    c->sample_rate, 0);
		av_opt_set_sample_fmt(self->swr, "in_sample_fmt",  AV_SAMPLE_FMT_S16, 0);
		av_opt_set_sample_fmt(self->swr, "out_sample_fmt", c->sample_fmt,  0);
		swr_init(self->swr);
	}

	{
		AVCodecContext *c = self->video_st->codec;
		/**/
		/* open the codec */
		if (avcodec_open2(c, NULL, NULL) < 0) {
			fprintf(stderr, "could not open codec\n");
			return 1;
		}

		self->video_outbuf = NULL;
		if (!(self->oc->oformat->flags & AVFMT_RAWPICTURE)) {
			/* Allocate output buffer. */
			/* XXX: API change will be done. */
			/* Buffers passed into lav* can be allocated any way you prefer,
			 * as long as they're aligned enough for the architecture, and
			 * they're freed appropriately (such as using av_free for buffers
			 * allocated with av_malloc). */
			self->video_outbuf_size = 200000;
			self->video_outbuf      = av_malloc(self->video_outbuf_size);
		}

		/* Allocate the encoded raw picture. */
		self->picture = alloc_picture(c->pix_fmt, c->width, c->height);
		if (!self->picture) {
			fprintf(stderr, "Could not allocate picture\n");
			return 1;
		}
		self->picture->format = c->pix_fmt;
		self->picture->width = c->width;
		self->picture->height = c->height;

		/* If the output format is not YUV420P, then a temporary YUV420P
		 * picture is needed too. It is then converted to the required
		 * output format. */

		self->tmp_picture = alloc_picture(SOURCE_PIX_FMT, 640, 400);
		if (!self->tmp_picture) {
			fprintf(stderr, "Could not allocate temporary picture\n");
			return 1;
		}
		self->tmp_picture->format = SOURCE_PIX_FMT;
		self->tmp_picture->width = 640;
		self->tmp_picture->height = 400;
    }

	{
		AVCodecContext *c = self->audio_st->codec;
		if (avcodec_open2(c, NULL, NULL) < 0) {
			fprintf(stderr, "could not open codec\n");
			return 1;
		}
	}

	/* open the output file, if needed */
	if (!(self->fmt->flags & AVFMT_NOFILE)) {
		if (avio_open(&self->oc->pb, filename, AVIO_FLAG_WRITE) < 0) {
			fprintf(stderr, "Could not open '%s'\n", filename);
			return 1;
		}
	}

	/* Write the stream header, if any. */
	avformat_write_header(self->oc, NULL);

	return 0;
}

int vidrec_finalize(video_recorder* self)
{
	unsigned int i;
	int ret;

	while (1)
	{
		AVPacket pkt = {0};
		int got_packet = 0;
		AVCodecContext *c = self->audio_st->codec;

		av_init_packet(&pkt);

		ret = avcodec_encode_audio2(c, &pkt, NULL, &got_packet);
		if (got_packet)
		{
			pkt.stream_index = self->audio_st->index;

			/* Write the compressed frame to the media file. */
			if (av_interleaved_write_frame(self->oc, &pkt) != 0) {
				fprintf(stderr, "Error while writing audio frame\n");
				return 1;
			}
		}
		else
		{
			break;
		}
	}

	while (1)
	{
		AVPacket pkt = {0};
		int out_size, got_packet = 0;
		AVCodecContext *c = self->video_st->codec;

		av_init_packet(&pkt);

		out_size = avcodec_encode_video2(c, &pkt, NULL, &got_packet);

		/* If size is zero, it means the image was buffered. */
		if (got_packet) {
			/* Write the compressed frame to the media file. */
			ret = av_interleaved_write_frame(self->oc, &pkt);
		} else {
			break;
		}
	}

	/* Write the trailer, if any. The trailer must be written before you
	 * close the CodecContexts open when you wrote the header; otherwise
	 * av_write_trailer() may try to use memory that was freed on
	 * av_codec_close(). */
	av_write_trailer(self->oc);

	{
		avcodec_close(self->video_st->codec);
		av_free(self->picture->data[0]);
		av_free(self->picture);
		if (self->tmp_picture) {
			av_free(self->tmp_picture->data[0]);
			av_free(self->tmp_picture);
		}
		av_free(self->video_outbuf);
	}

	{
		avcodec_close(self->audio_st->codec);
	}

	/* Free the streams. */
	for (i = 0; i < self->oc->nb_streams; i++) {
		av_freep(&self->oc->streams[i]->codec);
		av_freep(&self->oc->streams[i]);
	}

	if (!(self->fmt->flags & AVFMT_NOFILE))
		/* Close the output file. */
		avio_close(self->oc->pb);

	/* free the stream */
	av_free(self->oc);

	return 0;
}

int vidrec_write_audio_frame(video_recorder* self, int16_t* samples, int audio_input_frame_size)
{
	AVCodecContext *c;
	AVPacket pkt = { 0 }; // data and size must be 0;
	AVFrame *frame = av_frame_alloc();
	// FIXME calculate this properly, this is just a magic number that happens
	// to be big enough
	uint8_t *converted_samples = av_malloc(8192);
	int got_packet;
	int r;

	av_init_packet(&pkt);
	c = self->audio_st->codec;

	r = swr_convert(self->swr, &converted_samples, audio_input_frame_size,
	                (const uint8_t **) &samples, audio_input_frame_size);
	assert(r >= 0);

	frame->nb_samples = r;
	frame->channel_layout = c->channel_layout;
	frame->pts = self->pts;

	r = avcodec_fill_audio_frame(frame, c->channels, c->sample_fmt,
								 converted_samples,
								 r *
								 av_get_bytes_per_sample(c->sample_fmt) *
								 c->channels, 1);
	assert(r >= 0);

	r = avcodec_encode_audio2(c, &pkt, frame, &got_packet);
	assert(r >= 0);
	if (got_packet)
	{
		pkt.stream_index = self->audio_st->index;

		/* Write the compressed frame to the media file. */
		if (av_interleaved_write_frame(self->oc, &pkt) != 0) {
			fprintf(stderr, "Error while writing audio frame\n");
			return 1;
		}
	}

	self->pts += frame->nb_samples;

	av_free(converted_samples);
	av_frame_free(&frame);

	return 0;
}

int vidrec_write_video_frame(video_recorder* self, AVFrame* pic)
{
	int out_size, ret;
	AVCodecContext *c;

	c = self->video_st->codec;

	/* as we only generate a YUV420P picture, we must convert it
		* to the codec pixel format if needed */
	if (self->img_convert_ctx == NULL) {
		self->img_convert_ctx = sws_getContext(640, 400,
											SOURCE_PIX_FMT,
											c->width, c->height,
											c->pix_fmt,
											sws_flags, NULL, NULL, NULL);
		if (self->img_convert_ctx == NULL) {
			fprintf(stderr,
					"Cannot initialize the conversion context\n");
			return 1;
		}
	}

	sws_scale(self->img_convert_ctx, (const uint8_t * const*) pic->data,
	          pic->linesize, 0, c->height, self->picture->data,
	          self->picture->linesize);

	{
		/* encode the image */
		AVPacket pkt = {0};
		int got_packet = 0;

		av_init_packet(&pkt);

		self->picture->pts = self->frame_count;
		out_size = avcodec_encode_video2(c, &pkt, self->picture, &got_packet);

		/* If size is zero, it means the image was buffered. */
		if (got_packet) {
			/* Write the compressed frame to the media file. */
			ret = av_interleaved_write_frame(self->oc, &pkt);
		} else {
			ret = 0;
		}
	}

	if (ret != 0) {
		fprintf(stderr, "Error while writing video frame\n");
		return 1;
	}

	// I have no idea why this needs to be 256. I would think it should be 1
	self->frame_count+=256;

	return 0;
}
