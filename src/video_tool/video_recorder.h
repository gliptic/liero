#pragma once

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswresample/swresample.h>
#include <libswscale/swscale.h>

typedef struct video_recorder {
  AVFrame *picture, *tmp_picture;
  int frame_count;
  const AVOutputFormat* fmt;
  AVFormatContext* oc;
  AVStream *audio_st, *video_st;
  AVCodecContext *video_enc, *audio_enc;
  AVPacket* tmp_pkt;
  SwrContext* swr;
  struct SwsContext* img_convert_ctx;
  int64_t audio_pts;
} video_recorder;

int vidrec_init(video_recorder* self, char const* filename, int width, int height,
                AVRational framerate);
int vidrec_write_audio_frame(video_recorder* self, int16_t* samples, int audio_input_frame_size);
int vidrec_write_video_frame(video_recorder* self, AVFrame* pic);
int vidrec_finalize(video_recorder* self);
