#ifndef VIDEOTEST_H
#define VIDEOTEST_H

#include "tl/platform.h"

#include "libavformat/avformat.h"
#include "libswscale/swscale.h"
#include "libswresample/swresample.h"
#define inline TL_INLINE

typedef struct video_recorder {
	AVFrame *picture, *tmp_picture;
	uint8_t *video_outbuf;
	int frame_count, video_outbuf_size;
	AVOutputFormat *fmt;
	AVFormatContext *oc;
	AVStream *audio_st, *video_st;
    SwrContext *swr;
	struct SwsContext *img_convert_ctx;
    int64_t pts;
} video_recorder;

int  vidrec_init(video_recorder* self, char const* filename, int width, int height, AVRational framerate);
int  vidrec_write_audio_frame(video_recorder* self, int16_t* samples, int audio_input_frame_size);
int  vidrec_write_video_frame(video_recorder* self, AVFrame* pic);
int  vidrec_finalize(video_recorder* self);

#endif // VIDEOTEST_H
