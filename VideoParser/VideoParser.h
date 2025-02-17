/**
 * @file VideoParser.h
 * @author Werner Robitza
 * @copyright Copyright (c) 2023, AVEQ GmbH. Copyright (c) 2023, videoparser-ng
 * contributors.
 */

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
#include "include/shared.h"
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/frame.h>
#include <libavutil/motion_vector.h>
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

/**
 * @brief General information about the video sequence.
 */
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
  uint32_t video_frame_count;   /**< Number of frames in the video stream */
};

enum FrameType {
  UNKNOWN,
  I,
  P,
  B,
};

/**
 * @brief Specific frame information.
 */
struct FrameInfo {
  int32_t frame_idx = 0; /**< Frame number, zero-based */
  double dts;            /**< Decoding timestamp in seconds */
  double pts;            /**< Presentation timestamp in seconds */
  int size;              /**< Frame size in bytes */
  FrameType frame_type;  /**< Frame type (0 = unknown, 1 = I, 2 = P, 3 = B) */
  bool is_idr;           /**< Whether the frame is an IDR frame */

  // from SharedFrameInfo
  uint32_t qp_min;  /**< Minimum QP value encountered in this frame */
  uint32_t qp_max;  /**< Maximum QP value encountered in this frame */
  uint32_t qp_init; /**< QP Value the frame is starting with (to be found in the
                       slice- or frame-header) */
  double qp_avg;    /**< Average QP of the whole frame */
  double qp_stdev;  /**< Standard deviation of Av_QP */
  double qp_bb_avg; /**< Average QP without the black border */
  double qp_bb_stdev; /**< Standard deviation of the average QP */

  // motion estimation
  double motion_avg;        /**< Average of Av_Motion */
  double motion_stdev;      /**< Standard Deviation of Av_Motion */
  double motion_x_avg;      /**< Average of abs(MotX) */
  double motion_y_avg;      /**< Average of abs(MotY) */
  double motion_x_stdev;    /**< Standard deviation of Av_MotionX */
  double motion_y_stdev;    /**< Standard deviation of Av_MotionY */
  double motion_diff_avg;   /**< Difference of the motion with its prediction */
  double motion_diff_stdev; /**< Standard deviation of Av_MotionDif */
  int current_poc;
  int poc_diff;
  uint32_t motion_bit_count; /**< The number of bits used for coding motion */
  uint32_t coefs_bit_count /**< The number of bits used for coding coeffs */;
  int mb_mv_count;    /**< Number of macroblocks with MVs */
  int mv_coded_count; /**< Number of coded MVs */

  // Adding these to make debugging easier (so that they can be printed in the
  // JSON)
  // double mv_length;       /**< Motion Vector (MV) length, overall */
  // double mv_sum_sqr;      /**< Sum of squared MV lengths */
  // double mv_x_length;     /**< MV length in the X direction */
  // double mv_y_length;     /**< MV length in the Y direction */
  // double mv_x_sum_sqr;    /**< Sum of squared MV lengths in the X direction
  // */ double mv_y_sum_sqr;    /**< Sum of squared MV lengths in the Y
  // direction */ double mv_length_diff;  /** < Difference in MV length */
  // double mv_diff_sum_sqr; /**< Sum of squared MV differences */
};

void set_verbose(bool verbose);

/**
 * @brief A Video Parser implementation.
 *
 * This class is used to parse video files and extract information about the
 * video sequence and individual frames. Call get_sequence_info() to get the
 * general information about the video sequence, either before or after parsing
 * the frames. Call parse_frame() to parse the next frame and get its
 * information, in a loop. After parsing all frames, call close() to close the
 * file and free all resources.
 */
class VideoParser {
public:
  VideoParser(const std::string &filename);
  SequenceInfo get_sequence_info();
  bool parse_frame(FrameInfo &frame_info);
  void close();

private:
  int video_stream_idx = -1;
  AVFormatContext *format_context = nullptr;
  AVCodecContext *codec_context = nullptr;
  AVPacket *current_packet = nullptr;
  AVFrame *frame = nullptr;
  uint32_t frame_idx = 0;
  SequenceInfo sequence_info;
  double first_pts = 0;
  double last_pts = 0;
  uint64_t packet_size_sum = 0; // accumulated packet size sum, if not
                                // available from format context
  std::function<void()> close_input;

  void print_shared_frame_info(SharedFrameInfo &shared_frame_info);
  void set_frame_info(FrameInfo &frame_info);
  void set_frame_info_h264(FrameInfo &frame_info);
  void set_frame_info_h265(FrameInfo &frame_info);
  void set_frame_info_vp9(FrameInfo &frame_info);
  void set_frame_info_av1(FrameInfo &frame_info);
};
} // namespace videoparser

#endif // VIDEOPARSER_H
