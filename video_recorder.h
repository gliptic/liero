#ifndef VIDEOTEST_H
#define VIDEOTEST_H

#include "tl/platform.h"

#define inline TL_INLINE

#include "libavutil/mathematics.h"
#include "libavformat/avformat.h"
#include "libswscale/swscale.h"

typedef struct video_recorder {
	AVFrame *picture, *tmp_picture;
	uint8_t *video_outbuf;
	int frame_count, video_outbuf_size;
	AVOutputFormat *fmt;
	AVFormatContext *oc;
	AVStream *audio_st, *video_st;
	struct SwsContext *img_convert_ctx;
} video_recorder;

int  vidrec_init(video_recorder* self, char const* filename, int width, int height, AVRational framerate);
int  vidrec_write_audio_frame(video_recorder* self, int16_t* samples, int audio_input_frame_size);
int  vidrec_write_video_frame(video_recorder* self, AVFrame* pic);
int  vidrec_finalize(video_recorder* self);

#endif // VIDEOTEST_H
