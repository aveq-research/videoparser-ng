/**
 * @file VideoParser.h
 * @author Werner Robitza
 * @copyright Copyright (c) 2023-2025, AVEQ GmbH. Copyright (c) 2023-2025,
 * videoparser-ng contributors.
 */

#ifndef VIDEOPARSER_H
#define VIDEOPARSER_H

#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iomanip> // for std::fixed and std::setprecision
#include <iostream>
#include <optional>
#include <stdexcept>
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
#define VIDEOPARSER_VERSION_MINOR 8
#define VIDEOPARSER_VERSION_PATCH 0

namespace videoparser {
/**
 * @brief Error thrown by VideoParser, with a category for the C API
 */
class Error : public std::runtime_error {
public:
  enum class Code {
    Open,          /**< The input or its stream information cannot be read */
    NoVideoStream, /**< No (selected) video stream */
    Unsupported,   /**< No decoder or unknown pixel format */
    Decode,        /**< The decoder failed */
    Io,            /**< Reading or seeking the input failed */
    OutOfMemory,   /**< An allocation failed */
    Internal,      /**< Unexpected FFmpeg error */
  };

  /**
   * @param code Category of the error
   * @param message Error message
   * @param av_error FFmpeg error code, or 0 if there is none
   */
  Error(Code code, const std::string &message, int av_error = 0)
      : std::runtime_error(message), code(code), av_error(av_error) {}

  Code code;
  int av_error;
};

/**
 * @brief Custom input through callbacks, wrapped in an FFmpeg AVIOContext
 *
 * The callbacks have the semantics of avio_alloc_context(): read returns the
 * number of bytes read or a negative AVERROR (AVERROR_EOF at the end), and
 * seek supports SEEK_SET, SEEK_CUR, SEEK_END and AVSEEK_SIZE.
 */
struct CustomInput {
  int (*read)(void *opaque, uint8_t *buf, int size) = nullptr;
  /** Seek callback, or nullptr for non-seekable input */
  int64_t (*seek)(void *opaque, int64_t offset, int whence) = nullptr;
  void *opaque = nullptr;  /**< Passed to the callbacks */
  int buffer_size = 32768; /**< Size of the I/O buffer in bytes */
};

/**
 * @brief Options for opening an input
 */
struct OpenOptions {
  /** Index of the video stream, or -1 for the first video stream */
  int stream_index = -1;
  /** Name of the FFmpeg demuxer, or nullptr to detect the format */
  const char *input_format = nullptr;
  /** Read all video packets before decoding to estimate the bitrate and frame
   * count if the container lacks them (only for seekable input) */
  bool scan = true;
  /** Also return frames without statistics (for example of codecs that the
   * FFmpeg fork does not patch, such as FFV1), with FrameInfo::has_statistics
   * false and the statistics at their defaults */
  bool frames_without_statistics = false;
};

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
  double video_duration = 0.0;    /**< Duration of the file in seconds */
  char video_codec[8] = {};       /**< Codec used for video stream */
  double video_bitrate = 0.0;     /**< Bitrate of the video stream in kbps */
  double video_framerate = 0.0;   /**< Framerate of the video stream */
  int video_width = 0;            /**< Width of the video stream in pixels */
  int video_height = 0;           /**< Height of the video stream in pixels */
  int video_codec_profile = 0;    /**< Profile of the video codec */
  int video_codec_level = 0;      /**< Level of the video codec */
  int video_bit_depth = 0;        /**< Bit depth of the video stream */
  char video_pix_fmt[32] = {};    /**< Pixel format of the video stream */
  uint32_t video_frame_count = 0; /**< Number of frames in the video stream */
};

/**
 * @brief Counts over the frames parsed so far.
 */
struct Summary {
  uint32_t frame_count = 0; /**< Number of frames returned by parse_frame() */
  /** Number of frames with decode errors, plus packets and frames the decoder
   * rejected as invalid */
  uint32_t decode_errors = 0;
  /** Number of video packets that the demuxer marked as corrupt (for example,
   * after MPEG-TS continuity counter errors) */
  uint32_t corrupt_packets = 0;
  /** Number of frames whose timestamp is more than 5 seconds later or more than
   * 1 second earlier than the end of the previous frame */
  uint32_t discontinuities = 0;
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
  double dts = 0.0;      /**< Decoding timestamp in seconds */
  double pts = 0.0;      /**< Presentation timestamp in seconds */
  int size = 0;          /**< Frame size in bytes */
  /** Frame type (0 = unknown, 1 = I, 2 = P, 3 = B) */
  FrameType frame_type = UNKNOWN;
  bool is_idr = false; /**< Whether the frame is an IDR frame */
  /** Whether the decoder reported errors for this frame (for example,
   * concealed macroblocks or missing references) */
  bool decode_error = false;
  /** Whether the timestamp of this frame jumps against the end of the previous
   * frame (a discontinuity; see Summary::discontinuities) */
  bool discontinuity = false;
  /** Whether the decoder attached statistics to this frame; false only with
   * OpenOptions::frames_without_statistics */
  bool has_statistics = true;

  // from SharedFrameInfo
  uint32_t qp_min = 0;  /**< Minimum QP value encountered in this frame */
  uint32_t qp_max = 0;  /**< Maximum QP value encountered in this frame */
  uint32_t qp_init = 0; /**< QP Value the frame is starting with (to be found in
                       the slice- or frame-header) */
  double qp_avg = 0.0;  /**< Average QP of the whole frame */
  double qp_stdev = 0.0;    /**< Standard deviation of Av_QP */
  double qp_bb_avg = 0.0;   /**< Average QP without the black border */
  double qp_bb_stdev = 0.0; /**< Standard deviation of the average QP */

  // motion estimation
  double motion_avg = 0.0;     /**< Average of Av_Motion */
  double motion_stdev = 0.0;   /**< Standard Deviation of Av_Motion */
  double motion_x_avg = 0.0;   /**< Average of abs(MotX) */
  double motion_y_avg = 0.0;   /**< Average of abs(MotY) */
  double motion_x_stdev = 0.0; /**< Standard deviation of Av_MotionX */
  double motion_y_stdev = 0.0; /**< Standard deviation of Av_MotionY */
  /** Difference of the motion with its prediction */
  double motion_diff_avg = 0.0;
  double motion_diff_stdev = 0.0; /**< Standard deviation of Av_MotionDif */
  int current_poc = 0; /**< Picture Order Count of the current frame */
  int poc_diff = 0;    /**< Difference to the previous frame's POC */
  /** The number of bits used for coding motion */
  uint32_t motion_bit_count = 0;
  /** The number of bits used for coding coeffs */
  uint32_t coefs_bit_count = 0;
  int mb_mv_count = 0;    /**< Number of macroblocks with MVs */
  int mv_coded_count = 0; /**< Number of coded MVs */

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

/**
 * @brief Set verbose mode for the parser
 *
 * When verbose mode is enabled, additional debug information will be printed
 * to stderr during parsing.
 *
 * @param verbose True to enable verbose mode, false to disable (default)
 */
void set_verbose(bool verbose);

/**
 * @brief Log callback
 *
 * @param user_data Pointer passed to set_log_callback()
 * @param level FFmpeg log level of the message (AV_LOG_*)
 * @param line One line with a trailing newline
 */
using LogCallback = void (*)(void *user_data, int level, const char *line);

/**
 * @brief Pass the parser's warnings and verbose output to a callback instead
 * of writing them to stderr
 *
 * Process-wide. With a callback, messages above av_log_get_level() are
 * dropped. Without one (the default), all messages go to stderr. FFmpeg's own
 * messages are not affected; use av_log_set_callback() for them.
 *
 * @param callback The callback, or nullptr to write to stderr again
 * @param user_data Passed to the callback
 */
void set_log_callback(LogCallback callback, void *user_data);

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
  /**
   * @brief Construct a new Video Parser object
   *
   * Opens the specified video file and initializes the parser. This constructor
   * will throw exceptions if the file cannot be opened or if no video stream is
   * found.
   *
   * @param filename C-style string path to the video file to parse
   * @throws std::runtime_error If the file cannot be opened or no video stream
   * is found
   */
  VideoParser(const char *filename);

  /**
   * @brief Open a file with options
   *
   * @param filename Path to the video file
   * @param options Options for opening the file
   * @throws Error If the file cannot be opened or no video stream is found
   */
  VideoParser(const char *filename, const OpenOptions &options);

  /**
   * @brief Open custom input through callbacks
   *
   * Without a seek callback, the packet scan is skipped (see
   * OpenOptions::scan), and formats that need seeking (for example MP4 with
   * the index at the end) cannot be opened.
   *
   * @param input Callbacks and their opaque pointer
   * @param options Options for opening the input
   * @throws Error If the input cannot be opened or no video stream is found
   */
  VideoParser(const CustomInput &input, const OpenOptions &options);

  /**
   * @brief Destroy the Video Parser object and free all resources not yet
   * freed by close()
   */
  ~VideoParser();

  VideoParser(const VideoParser &) = delete;
  VideoParser &operator=(const VideoParser &) = delete;

  /**
   * @brief Get information about the video sequence
   *
   * This method can be called either before or after parsing frames.
   *
   * @return SequenceInfo Struct containing general information about the video
   * sequence
   */
  SequenceInfo get_sequence_info();

  /**
   * @brief Parse the next frame in the video
   *
   * This method should be called in a loop to parse all frames in the video.
   * It will fill the provided frame_info struct with information about the
   * parsed frame.
   *
   * @param frame_info Reference to a FrameInfo struct to be filled with frame
   * information
   * @return true If a frame was successfully parsed
   * @return false If no more frames are available or an error occurred
   */
  bool parse_frame(FrameInfo &frame_info);

  /**
   * @brief Get counts of decode errors and discontinuities
   *
   * Call this after parsing the frames; it covers the frames parsed so far.
   *
   * @return Summary Struct with the counts
   */
  Summary get_summary() const;

  /**
   * @brief Get the decoded frame of the last successful parse_frame() call
   *
   * The frame belongs to the parser and is valid until the next call of
   * parse_frame() or close().
   *
   * @return const AVFrame* The frame, or nullptr before the first frame
   */
  const AVFrame *get_frame() const;

  /**
   * @brief Get the index of the parsed video stream in the container
   */
  int get_stream_index() const;

  /**
   * @brief Get the time base of the parsed video stream
   */
  AVRational get_time_base() const;

  /**
   * @brief Close the video file and free resources
   *
   * This method should be called after parsing is complete to properly close
   * the video file and free all allocated resources. Calling it more than once
   * is safe. The destructor also calls it.
   */
  void close();

private:
  int video_stream_idx = -1;
  AVFormatContext *format_context = nullptr;
  AVCodecContext *codec_context = nullptr;
  AVPacket *current_packet = nullptr;
  AVFrame *frame = nullptr;
  bool decoder_draining = false;
  bool decoder_finished = false;
  uint32_t frame_idx = 0;
  SequenceInfo sequence_info;
  double first_pts = 0;
  double last_pts = 0;
  /** Index and time of the last frame with a timestamp */
  struct TimestampAnchor {
    int64_t frame_idx = -1;
    double time = 0.0;
  };
  TimestampAnchor last_valid_pts; // for frames without a pts
  TimestampAnchor last_valid_dts; // for frames without a dts
  uint64_t packet_size_sum = 0;   // accumulated packet size sum, if not
                                  // available from format context
  bool bitrate_from_scan = false; // bitrate estimated by scan_video_packets()
  double next_pts = std::nan(""); // end of the previous frame, in seconds
  Summary summary;
  bool network_initialized = false; // avformat_network_init() was called
  std::string filename;             // empty for custom input
  CustomInput custom_input;
  AVIOContext *io_context = nullptr; // for custom input
  OpenOptions options;
  std::string input_format_name; // copy of options.input_format

  void open();
  void open_input();
  void scan_video_packets();
  void print_shared_frame_info(SharedFrameInfo &shared_frame_info);
  void set_frame_info(FrameInfo &frame_info);
  void set_frame_info_h264(FrameInfo &frame_info);
  void set_frame_info_h265(FrameInfo &frame_info);
  void set_frame_info_vp9(FrameInfo &frame_info);
  void set_frame_info_av1(FrameInfo &frame_info);
  void set_frame_info_mpeg2(FrameInfo &frame_info);
};
} // namespace videoparser

#endif // VIDEOPARSER_H
