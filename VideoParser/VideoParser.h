#ifndef VIDEOPARSER_H
#define VIDEOPARSER_H

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iomanip> // for std::fixed and std::setprecision
#include <iostream>
#include <optional>
#include <string>
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/frame.h>
#include <libavutil/pixdesc.h>
#include <unistd.h>
}

#define VIDEOPARSER_VERSION_MAJOR 0
#define VIDEOPARSER_VERSION_MINOR 0
#define VIDEOPARSER_VERSION_PATCH 1

namespace videoparser {
class ScopeExit {
  std::function<void()> fn;

public:
  ScopeExit(std::function<void()> fn) : fn(fn) {}
  ~ScopeExit() { fn(); }
};

struct SequenceInfo {
  double video_duration = 0.0;  /**< Duration of the file in seconds */
  std::string video_codec;      /**< Codec used for video stream */
  double video_bitrate = 0.0;   /**< Bitrate of the video stream in kbps */
  double video_framerate = 0.0; /**< Framerate of the video stream */
  int video_width = 0;          /**< Width of the video stream in pixels */
  int video_height = 0;         /**< Height of the video stream in pixels */
  int video_codec_profile = 0;  /**< Profile of the video codec */
  int video_codec_level = 0;    /**< Level of the video codec */
  int video_bit_depth = 0;      /**< Bit depth of the video stream */
  std::string video_pix_fmt;    /**< Pixel format of the video stream */
};

enum FrameType {
  UNKNOWN,
  I,
  P,
  B,
};

struct FrameInfo {
  int32_t frame_idx = 0; /**< Frame number */
  double dts;            /**< Decoding timestamp in seconds */
  double pts;            /**< Presentation timestamp in seconds */
  int size;              /**< Frame size in bytes */
  FrameType frame_type;  /**< Frame type */
  bool is_idr;           /**< Is IDR frame */
};

void set_verbose(bool verbose);

class VideoParser {
public:
  VideoParser(const std::string &filename);
  SequenceInfo get_sequence_info();
  bool parse_frame(FrameInfo &frame_info);
  void close();

private:
  AVFormatContext *format_context = nullptr;
  int video_stream_idx = -1;
  SequenceInfo sequence_info;
  AVCodecContext *codec_context = nullptr;
  AVPacket *current_packet = nullptr;
  AVFrame *frame = nullptr;
  uint32_t frame_idx = 0;
  std::function<void()> close_input;
  double first_pts = 0;
  double last_pts = 0;
  uint64_t packet_size_sum = 0; // accumulated packet size sum, if not available
                                // from format context

  void set_frame_info(FrameInfo &frame_info);
};
} // namespace videoparser

#endif // VIDEOPARSER_H
