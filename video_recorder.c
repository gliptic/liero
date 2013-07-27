#include "video_recorder.h"

#include "libavutil/mathematics.h"
#include "libavformat/avformat.h"
#include "libswscale/swscale.h"

#define STREAM_PIX_FMT    PIX_FMT_YUV420P
#define SOURCE_PIX_FMT    PIX_FMT_BGRA

static int sws_flags = SWS_BICUBIC;

#include "libavutil/audioconvert.h"
#include "libavutil/avutil.h"

static AVFrame *alloc_picture(enum PixelFormat pix_fmt, int width, int height)
{
	AVFrame *picture;
	uint8_t *picture_buf;
	int size;

	picture = avcodec_alloc_frame();
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
	self->fmt = av_guess_format("mp4", NULL, NULL);
	self->oc = avformat_alloc_context();
	self->oc->oformat = self->fmt;

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

		/* Put sample parameters. */
		c->bit_rate = 400000;
		/* Resolution must be a multiple of two. */
		c->width    = width;
		c->height   = height;
		/* timebase: This is the fundamental unit of time (in seconds) in terms
		 * of which frame timestamps are represented. For fixed-fps content,
		 * timebase should be 1/framerate and timestamp increments should be
		 * identical to 1. */
		c->time_base = framerate;
		c->gop_size      = 12; /* emit one intra frame every twelve frames at most */
		c->pix_fmt       = STREAM_PIX_FMT;

#if 0 // for x264
		c->bit_rate = 500*1000;
		c->bit_rate_tolerance = 0;
		c->rc_max_rate = 0;
		c->rc_buffer_size = 0;
		c->gop_size = 40;
		c->max_b_frames = 3;
		c->b_frame_strategy = 1;
		c->coder_type = 1;
		c->me_cmp = 1;
		c->me_range = 16;
		c->qmin = 10;
		c->qmax = 51;
		c->scenechange_threshold = 40;
		c->flags |= CODEC_FLAG_LOOP_FILTER;
		c->me_method = ME_HEX;
		c->me_subpel_quality = 5;
		c->i_quant_factor = 0.71f;
		c->qcompress = 0.6f;
		c->max_qdiff = 4;
		//c->directpred = 1;
		//c->flags2 |= CODEC_FLAG2_FAST;
#else // Main
		c->bit_rate = 500*1000;
		c->bit_rate_tolerance = 0;
		c->coder_type = 1;  // coder = 1
		c->flags |= CODEC_FLAG_LOOP_FILTER;   // flags=+loop
		c->me_cmp |= 1;  // cmp=+chroma, where CHROMA = 1
		//c->partitions|=X264_PART_I8X8+X264_PART_I4X4+X264_PART_P8X8+X264_PART_B8X8; // partitions=+parti8x8+parti4x4+partp8x8+partb8x8
		c->me_method = ME_HEX;    // me_method=hex
		c->me_subpel_quality = 7;   // subq=7
		c->me_range = 16;   // me_range=16
		c->gop_size = 250;  // g=250
		c->keyint_min = 25; // keyint_min=25
		c->scenechange_threshold = 40;  // sc_threshold=40
		c->i_quant_factor = 0.71f; // i_qfactor=0.71
		c->b_frame_strategy = 1;  // b_strategy=1
		c->qcompress = 0.6f; // qcomp=0.6
		c->qmin = 10;   // qmin=10
		c->qmax = 51;   // qmax=51
		c->max_qdiff = 4;   // qdiff=4
		c->max_b_frames = 3;    // bf=3
		c->refs = 3;    // refs=3
		//c->directpred = 1;  // directpred=1
		c->trellis = 1; // trellis=1
		//c->flags2|=CODEC_FLAG2_BPYRAMID+CODEC_FLAG2_MIXED_REFS+CODEC_FLAG2_WPRED+CODEC_FLAG2_8X8DCT+CODEC_FLAG2_FASTPSKIP;  // flags2=+bpyramid+mixed_refs+wpred+dct8x8+fastpskip
		//c->weighted_p_pred = 2; // wpredp=2
#endif
		if (c->codec_id == AV_CODEC_ID_MPEG2VIDEO) {
			/* just for testing, we also add B frames */
			c->max_b_frames = 2;
		}
		if (c->codec_id == AV_CODEC_ID_MPEG1VIDEO) {
			/* Needed to avoid using macroblocks in which some coeffs overflow.
			 * This does not happen with normal video, it just happens here as
			 * the motion of the chroma plane does not match the luma plane. */
			c->mb_decision = 2;
		}
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

		c = self->audio_st->codec;

		/* put sample parameters */
		c->sample_fmt  = AV_SAMPLE_FMT_S16;
		c->bit_rate    = 64000;
		c->sample_rate = 44100;
		c->channels    = 1;
		c->channel_layout = AV_CH_LAYOUT_MONO;
		//c->profile = FF_PROFILE_AAC_MAIN;
		//c->frame_size = 1024;
		//c->time_base.num = 1;
		//c->time_base.num = c->sample_rate;

		// some formats want stream headers to be separate
		if (self->oc->oformat->flags & AVFMT_GLOBALHEADER)
			c->flags |= CODEC_FLAG_GLOBAL_HEADER;


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

		/* If the output format is not YUV420P, then a temporary YUV420P
		 * picture is needed too. It is then converted to the required
		 * output format. */

		self->tmp_picture = alloc_picture(SOURCE_PIX_FMT, c->width, c->height);
		if (!self->tmp_picture) {
			fprintf(stderr, "Could not allocate temporary picture\n");
			return 1;
		}
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
	int i, ret;

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
	AVFrame *frame = avcodec_alloc_frame();
	int got_packet;
	int r;

	av_init_packet(&pkt);
	c = self->audio_st->codec;

	// get_audio_frame(samples, audio_input_frame_size, c->channels);

	frame->nb_samples = audio_input_frame_size;
	frame->channel_layout = c->channel_layout;

	r = avcodec_fill_audio_frame(frame, c->channels, c->sample_fmt,
								(uint8_t *)samples,
								audio_input_frame_size *
								av_get_bytes_per_sample(c->sample_fmt) *
								c->channels, 1);

	r = avcodec_encode_audio2(c, &pkt, frame, &got_packet);
	if (got_packet)
	{
		pkt.stream_index = self->audio_st->index;

		/* Write the compressed frame to the media file. */
		if (av_interleaved_write_frame(self->oc, &pkt) != 0) {
			fprintf(stderr, "Error while writing audio frame\n");
			return 1;
		}
	}

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
		self->img_convert_ctx = sws_getContext(c->width, c->height,
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

	sws_scale(self->img_convert_ctx, pic->data, pic->linesize,
				0, c->height, self->picture->data, self->picture->linesize);

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

	++self->frame_count;

	return 0;
}
