#pragma once

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswresample/swresample.h>
#include <libswscale/swscale.h>

/* C header — keep `typedef struct` so video_recorder.c parses. */
// NOLINTBEGIN(modernize-use-using)
typedef struct VideoRecorder {
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
// NOLINTEND(modernize-use-using)

int VidrecInit(video_recorder* self, char const* filename, int width, int height,
               AVRational framerate);
int VidrecWriteAudioFrame(video_recorder* self, int16_t* samples, int audio_input_frame_size);
int VidrecWriteVideoFrame(video_recorder* self, AVFrame* pic);
int VidrecFinalize(video_recorder* self);
