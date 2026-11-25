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
#include <deque>
#include <exception>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iomanip> // for std::fixed and std::setprecision
#include <iostream>
#include <limits>
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
#define VIDEOPARSER_VERSION_MINOR 17
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
    /** A seek target has no key frame, or a call is not allowed in this mode */
    InvalidArgument,
    /** A call is not allowed in the current state (after a failed seek) */
    InvalidState,
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
  /** Decode the pictures only: skip the work of the library on the
   * statistics (see VideoParser::parse_frame()). The FFmpeg fork still
   * computes them while decoding. The pictures, the frame indices and the
   * frame types and timestamps are the same as without this option; the
   * statistics are 0. */
  bool decode_only = false;
  /** Only read packets, for next_keyframe(); no decoder is opened, and
   * parse_frame() returns no frames */
  bool demux_only = false;
};

/**
 * @brief A key frame found by VideoParser::next_keyframe()
 */
struct Keyframe {
  /** Byte position of the packet in the input, for seek_byte(); -1 if the
   * demuxer does not report it */
  int64_t pos = -1;
  /** Presentation timestamp in the stream time base, or AV_NOPTS_VALUE */
  int64_t pts = AV_NOPTS_VALUE;
  /** Decoding timestamp in the stream time base, or AV_NOPTS_VALUE */
  int64_t dts = AV_NOPTS_VALUE;
  /** Index the frame gets when all frames are parsed from the start, or -1
   * if it cannot be determined without decoding */
  int64_t frame_idx = -1;
  /** Number of video packets before this one, in decoding order */
  int64_t packet_idx = 0;
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
  /** Whether the first decoded frame is interlaced */
  bool video_interlaced = false;
  /** Field order of the first decoded frame: "progressive", "tt" (top field
   * first), "bb" (bottom field first), or "unknown" if no frame could be
   * decoded when opening the input */
  char video_field_order[16] = "unknown";
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
  /** Number of interlaced frames (see FrameInfo::interlaced) */
  uint32_t interlaced_frames = 0;
  /** Sum of FrameInfo::repeated_fields over all frames */
  uint32_t repeated_fields = 0;
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

  // block counts and QP of coded blocks
  /** Area of all blocks, in units of 4x4 luma samples; 0 if the codec does
   * not report block counts */
  int blocks_total = 0;
  /** Area of skipped blocks (no residual and no coded motion vector
   * difference), in 4x4 units */
  int blocks_skipped = 0;
  /** Area of intra blocks and blocks with coded residual, in 4x4 units */
  int blocks_coded = 0;
  /** Minimum QP of coded blocks; 0 without coded blocks */
  uint32_t qp_coded_min = 0;
  /** Maximum QP of coded blocks; 0 without coded blocks */
  uint32_t qp_coded_max = 0;
  /** Average QP of coded blocks; NaN without coded blocks */
  double qp_coded_avg = std::numeric_limits<double>::quiet_NaN();
  /** Standard deviation of the QP of coded blocks; NaN without coded blocks */
  double qp_coded_stdev = std::numeric_limits<double>::quiet_NaN();
  /** Whether this is a non-I frame in which all blocks are skipped */
  bool is_all_skip = false;

  // interlacing, from the decoded frame
  /** Whether the frame is interlaced (its two fields show different times) */
  bool interlaced = false;
  /** Whether the top field is displayed first */
  bool top_field_first = false;
  /** Number of fields to display again after the two fields of the frame
   * (FFmpeg's repeat_pict): 1 for a repeated first field (soft telecine), 2
   * for frame doubling, 4 for frame tripling, otherwise 0 */
  int repeated_fields = 0;

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
   * Opens the specified video file and initializes the parser. Decodes the
   * first frame for SequenceInfo::video_interlaced and
   * SequenceInfo::video_field_order; parse_frame() returns it. This
   * constructor will throw exceptions if the file cannot be opened or if no
   * video stream is found.
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
   * @brief Continue at the key frame at or before a presentation timestamp
   *
   * The next parse_frame() returns the last key frame (in file order) whose
   * packet has a timestamp at or before `pts`, and then the frames after it
   * in presentation order. The decoder and all state that depends on
   * previous frames are reset. Frames that come before the key frame in
   * presentation order (leading pictures of an open GOP) are dropped.
   * Needs a seekable input (a file, or custom input with a seek callback).
   *
   * @param pts Timestamp in the stream time base (get_time_base())
   * @param next_frame_idx Frame index of the key frame
   * @throws Error Unsupported for inputs without timestamps (raw
   * bitstreams) or that are not seekable, with the parser unchanged;
   * InvalidArgument if there is no key frame at or before `pts`; Io if
   * seeking fails. After any error other than Unsupported, parse_frame()
   * throws InvalidState until a seek succeeds.
   */
  void seek_pts(int64_t pts, uint32_t next_frame_idx);

  /**
   * @brief Continue at the first key frame whose packet starts at or after a
   * byte position
   *
   * As seek_pts(), for inputs with unreliable timestamps. Use the positions
   * of next_keyframe(). Only for MPEG-TS, MPEG-PS and raw bitstreams.
   *
   * @param pos Byte position in the input
   * @param next_frame_idx Frame index of the key frame
   * @throws Error Unsupported for other containers or inputs that are not
   * seekable, with the parser unchanged; InvalidArgument for a negative
   * `pos` (the parser is unchanged) or if there is no key frame at or after
   * `pos`; Io if seeking fails. After other errors, parse_frame() throws
   * InvalidState until a seek succeeds.
   */
  void seek_byte(int64_t pos, uint32_t next_frame_idx);

  /**
   * @brief Whether seek_byte() is supported for the input
   */
  bool can_seek_byte() const;

  /**
   * @brief Whether seek_pts() is supported for the input (not for raw
   * bitstreams, which have no timestamps)
   */
  bool can_seek_pts() const;

  /**
   * @brief Whether the input is seekable: a file, or custom input with a
   * seek callback. seek_pts() and seek_byte() need it.
   */
  bool is_seekable() const;

  /**
   * @brief Read packets up to the next key frame, without decoding
   *
   * Only with OpenOptions::demux_only.
   *
   * @param keyframe Receives the key frame
   * @return false at the end of the input
   */
  bool next_keyframe(Keyframe &keyframe);

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
  // The decoded frame in `frame` is not returned by parse_frame() yet
  bool frame_pending = false;
  // Error while decoding the first frame in open(), thrown by parse_frame()
  std::exception_ptr pending_error;
  // H.264 only: parser for the picture structure of each packet, and the size
  // of the last unpaired field (shared with its decoded frame)
  AVCodecParserContext *field_parser = nullptr;
  AVCodecContext *field_parser_context = nullptr;
  AVBufferRef *first_field_size = nullptr;
  int first_field_structure = AV_PICTURE_STRUCTURE_UNKNOWN;
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
  // Legacy mode: rounded Matroska default packet duration, and the truncated
  // duration of the legacy parser that replaces it (equal if not used)
  int64_t legacy_rounded_duration = 0;
  int64_t legacy_truncated_duration = 0;
  Summary summary;
  bool network_initialized = false; // avformat_network_init() was called
  std::string filename;             // empty for custom input
  CustomInput custom_input;
  AVIOContext *io_context = nullptr; // for custom input
  OpenOptions options;
  std::string input_format_name; // copy of options.input_format

  /** A frame seen by next_keyframe(), in decoding order */
  struct ScanFrame {
    Keyframe keyframe;
    bool key = false;
    int pict_type = AV_PICTURE_TYPE_NONE;
    /** Picture order count from the parser (H.264, HEVC), or INT_MIN */
    int poc = 0;
    bool has_poc = false;
  };
  /** A key frame of next_keyframe() that waits for the frames after it */
  struct PendingKeyframe {
    ScanFrame frame;
    int64_t leading = 0;      // frames after it, shown before it
    int frames_after = 0;     // frames after it seen so far
    bool anchor_seen = false; // MPEG-1/2: an I or P frame after it
    bool done = false;
  };

  // Packets read by seek_pts() or seek_byte(), sent to the decoder first
  std::deque<AVPacket *> queued_packets;
  // The first packet sent after a seek is the key frame; frames before it
  // in presentation order are dropped
  bool seek_key_next = false;
  AVBufferRef *seek_key_ref = nullptr;
  int dropped_after_seek = 0;
  bool seeked = false;
  // A seek failed after it reset the parser; parse_frame() throws until a
  // seek succeeds
  bool seek_failed = false;

  // next_keyframe(): parser for the picture structure, type and order
  AVCodecParserContext *scan_parser = nullptr;
  AVCodecContext *scan_parser_context = nullptr;
  bool scan_first_field = false;
  int scan_first_field_structure = AV_PICTURE_STRUCTURE_UNKNOWN;
  int64_t scan_packets = 0;
  int64_t scan_frames = 0;
  bool scan_unreliable = false;
  int64_t scan_last_pos = 0;       // last known packet position
  bool scan_last_pos_used = false; // a key frame comes after it
  bool scan_finished = false;
  std::deque<PendingKeyframe> scan_pending;

  void open();
  void open_input();
  void open_decoder();
  void open_scan_parser();
  void init_field_parser();
  void reset_for_seek();
  void finish_seek(uint32_t next_frame_idx);
  void seek_to_start();
  void seek_input_byte(int64_t pos, const char *message);
  void trim_mpeg12_packet(AVPacket *packet) const;
  bool read_video_packet(AVPacket *packet);
  bool find_keyframe_by_pts(int64_t pts);
  bool next_packet(AVPacket *packet);
  void scan_add_frame(const ScanFrame &scan_frame);
  bool scan_is_leading(const PendingKeyframe &pending,
                       const ScanFrame &frame) const;
  void scan_video_packets();
  void decode_first_frame();
  bool receive_frame();
  void add_second_field_size(const AVPacket *packet);
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
