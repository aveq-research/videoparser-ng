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
  // TODO: duration may only be available at the end of the file
  // double duration = 0.0;        /**< Duration of the file in seconds */
  // double start_time = 0.0;      /**< Start time of the file in seconds */
  std::string video_codec;      /**< Codec used for video stream */
  double video_bitrate = 0.0;   /**< Bitrate of the video stream in kbps */
  double video_framerate = 0.0; /**< Framerate of the video stream */
  int video_width = 0;          /**< Width of the video stream in pixels */
  int video_height = 0;         /**< Height of the video stream in pixels */
  int video_codec_profile = 0;  /**< Profile of the video codec */
  int video_codec_level = 0;    /**< Level of the video codec */
  std::string video_pix_fmt;    /**< Pixel format of the video stream */
};

struct FrameInfo {
  int32_t frame_idx = 0; /**< Frame number */
};

void set_verbose(bool verbose);
void parse_file(const std::string filename, SequenceInfo &sequence_info,
                std::vector<videoparser::FrameInfo> &frame_infos);
} // namespace videoparser

#endif // VIDEOPARSER_H
