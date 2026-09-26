/**
 * @file videoparser_c.h
 * @author Werner Robitza
 * @copyright Copyright (c) 2026, AVEQ GmbH. Copyright (c) 2026,
 * videoparser-ng contributors.
 *
 * @brief C API of libvideoparser
 *
 * Plain C interface to the video parser, for callers that cannot use the C++
 * API (for example, bindings for other languages). It gives the same values
 * as the video-parser CLI. See docs/c-api.md for the design.
 *
 * Conventions:
 *
 * - Functions returning vp_status return VP_OK (0) on success. VP_END (1) is
 *   not an error. On other values, vp_last_error() returns a message for the
 *   last failed call on the calling thread.
 * - Every struct starts with struct_size, which the caller sets to sizeof the
 *   struct. The library fills only the fields that fit. Fields are only ever
 *   appended.
 * - No C++ exceptions cross this interface.
 * - A parser must not be used from several threads at the same time.
 *   Different parsers may be used from different threads.
 */

#ifndef VIDEOPARSER_C_H
#define VIDEOPARSER_C_H

#include <stddef.h>
#include <stdint.h>

#if defined(_WIN32)
#if defined(VIDEOPARSER_BUILDING_SHARED)
#define VP_EXPORT __declspec(dllexport)
#elif defined(VIDEOPARSER_USING_SHARED)
#define VP_EXPORT __declspec(dllimport)
#else
#define VP_EXPORT
#endif
#else
#define VP_EXPORT __attribute__((visibility("default")))
#endif

#ifdef __cplusplus
extern "C" {
#endif

/** Major version of the C API; changes on incompatible changes */
#define VP_API_VERSION_MAJOR 1
/** Minor version of the C API; changes when functions or fields are added */
#define VP_API_VERSION_MINOR 0
/** Version of the C API as returned by vp_api_version() */
#define VP_API_VERSION ((VP_API_VERSION_MAJOR << 16) | VP_API_VERSION_MINOR)

/* Status codes */
typedef int32_t vp_status;
#define VP_OK 0  /**< Success */
#define VP_END 1 /**< No more frames (not an error) */
#define VP_ERROR_INVALID_ARGUMENT                                              \
  2 /**< Null pointer, bad struct_size or bad option */
#define VP_ERROR_INVALID_STATE 3 /**< Call not allowed in the current state */
#define VP_ERROR_OPEN 4 /**< Input or its stream information cannot be read */
#define VP_ERROR_NO_VIDEO_STREAM                                               \
  5 /**< No video stream, or stream_index is not one */
#define VP_ERROR_UNSUPPORTED 6 /**< No decoder or unknown pixel format */
#define VP_ERROR_DECODE 7      /**< The decoder failed */
#define VP_ERROR_IO 8          /**< Read or seek callback or seeking failed */
#define VP_ERROR_NO_FRAMES 9   /**< The stream ended without any frame */
#define VP_ERROR_OUT_OF_MEMORY 10 /**< Allocation failed */
#define VP_ERROR_INTERNAL 11      /**< Unexpected internal error */

/* Build flags (vp_build_flags) */
/** Built in legacy mode (VP_MV_POC_NORMALIZATION=1), as P.1204.3 needs */
#define VP_BUILD_LEGACY (1u << 0)

/* Log levels (the values of FFmpeg's AV_LOG_*) */
#define VP_LOG_QUIET (-8)
#define VP_LOG_PANIC 0
#define VP_LOG_FATAL 8
#define VP_LOG_ERROR 16
#define VP_LOG_WARNING 24
#define VP_LOG_INFO 32
#define VP_LOG_VERBOSE 40
#define VP_LOG_DEBUG 48

/* Frame types */
#define VP_FRAME_TYPE_UNKNOWN 0
#define VP_FRAME_TYPE_I 1
#define VP_FRAME_TYPE_P 2
#define VP_FRAME_TYPE_B 3

/* Scan modes (vp_options.scan) */
/** Read all video packets before decoding if the container lacks the
 * bitrate or frame count and the input is seekable */
#define VP_SCAN_AUTO 0
/** Never read the packets in advance */
#define VP_SCAN_OFF 1

/** whence value of the seek callback: return the size of the input */
#define VP_SEEK_SIZE 0x10000

/** Timestamp value for "no timestamp" */
#define VP_NOPTS INT64_MIN

/** Opaque parser handle */
typedef struct vp_parser vp_parser;

/**
 * @brief Options for opening an input
 *
 * Initialize with vp_options_init().
 */
typedef struct vp_options {
  uint32_t struct_size; /**< sizeof(vp_options) */
  /** Return at most this many frames (as the CLI's -n); -1 for all */
  int64_t max_frames;
  /** Index of the video stream in the container; -1 for the first one */
  int32_t stream_index;
  /** VP_SCAN_AUTO or VP_SCAN_OFF */
  int32_t scan;
  /** Name of the FFmpeg demuxer (for example "mpegts"); NULL to detect it */
  const char *input_format;
  /** Size of the buffer for custom input in bytes; 0 for 32768 */
  int32_t io_buffer_size;
  /** 1 to also return frames without statistics (for example FFV1
   * references for full-reference metrics), with has_statistics 0 and the
   * statistics 0; 0 (default) to skip them, as the CLI does */
  int32_t frames_without_statistics;
} vp_options;

/**
 * @brief Read callback for custom input
 *
 * @param opaque vp_io.opaque
 * @param buf Buffer to fill
 * @param size Size of the buffer in bytes
 * @return Number of bytes read (> 0), 0 at the end of the input, or a
 * negative value on an error
 */
typedef int32_t (*vp_read_callback)(void *opaque, uint8_t *buf, int32_t size);

/**
 * @brief Seek callback for custom input
 *
 * @param opaque vp_io.opaque
 * @param offset Offset in bytes
 * @param whence SEEK_SET (0), SEEK_CUR (1), SEEK_END (2), or VP_SEEK_SIZE to
 * return the size of the input without moving
 * @return The new position (or the size for VP_SEEK_SIZE), or a negative
 * value on an error or if the size is unknown
 */
typedef int64_t (*vp_seek_callback)(void *opaque, int64_t offset,
                                    int32_t whence);

/**
 * @brief Custom input
 */
typedef struct vp_io {
  uint32_t struct_size;  /**< sizeof(vp_io) */
  vp_read_callback read; /**< Read callback (required) */
  vp_seek_callback seek; /**< Seek callback, or NULL for non-seekable input */
  void *opaque;          /**< Passed to the callbacks */
} vp_io;

/**
 * @brief Information about the video sequence (the CLI's sequence_info)
 */
typedef struct vp_sequence_info {
  uint32_t struct_size;  /**< sizeof(vp_sequence_info) */
  double video_duration; /**< Duration in seconds */
  char video_codec[16]; /**< "h264", "hevc", "vp9", "av1", "mpeg2" or "mpeg1" */
  double video_bitrate; /**< Bitrate in kbit/s */
  double video_framerate;      /**< Frame rate in frames per second */
  int32_t video_width;         /**< Width in pixels */
  int32_t video_height;        /**< Height in pixels */
  int32_t video_codec_profile; /**< FFmpeg's profile value */
  int32_t video_codec_level;   /**< FFmpeg's level value */
  int32_t video_bit_depth;     /**< Bit depth */
  char video_pix_fmt[32];      /**< FFmpeg's pixel format name */
  uint32_t video_frame_count;  /**< Number of frames */
  int32_t stream_index;        /**< Index of the stream in the container */
  int32_t time_base_num;       /**< Time base of the stream (numerator) */
  int32_t time_base_den;       /**< Time base of the stream (denominator) */
} vp_sequence_info;

/**
 * @brief Statistics of one frame (the CLI's frame_info)
 *
 * See METRICS.md for the definitions.
 */
typedef struct vp_frame_info {
  uint32_t struct_size; /**< sizeof(vp_frame_info) */
  int32_t frame_idx;    /**< Frame number in presentation order, zero-based */
  double dts;           /**< Decoding timestamp in seconds; NaN if unknown */
  double pts; /**< Presentation timestamp in seconds; NaN if unknown */
  /** Presentation timestamp in the stream's time base, or VP_NOPTS (pts is
   * then estimated) */
  int64_t pts_raw;
  /** Decoding timestamp in the stream's time base, or VP_NOPTS */
  int64_t dts_raw;
  int32_t size;         /**< Frame size in bytes */
  int32_t frame_type;   /**< VP_FRAME_TYPE_* */
  int32_t is_idr;       /**< 1 for IDR or key frames */
  int32_t decode_error; /**< 1 if the decoder reported errors for this frame */
  int32_t
      discontinuity; /**< 1 if the timestamp jumps against the previous frame */

  uint32_t qp_min;  /**< Minimum QP */
  uint32_t qp_max;  /**< Maximum QP */
  uint32_t qp_init; /**< QP from the slice or frame header */
  double qp_avg;    /**< Average QP */
  double qp_stdev;  /**< Standard deviation of the QP */
  double qp_bb_avg; /**< Average QP without the black border */
  double
      qp_bb_stdev; /**< Standard deviation of the QP without the black border */

  double motion_avg;      /**< Average motion vector length */
  double motion_stdev;    /**< Standard deviation of the motion vector length */
  double motion_x_avg;    /**< Average of abs(motion x) */
  double motion_y_avg;    /**< Average of abs(motion y) */
  double motion_x_stdev;  /**< Standard deviation of motion x */
  double motion_y_stdev;  /**< Standard deviation of motion y */
  double motion_diff_avg; /**< Average difference of motion to its prediction */
  double motion_diff_stdev;  /**< Standard deviation of the motion difference */
  int32_t current_poc;       /**< Picture order count (H.264, HEVC) */
  int32_t poc_diff;          /**< Difference to the previous POC */
  uint32_t motion_bit_count; /**< Bits used for coding motion */
  uint32_t coefs_bit_count;  /**< Bits used for coding coefficients */
  int32_t mb_mv_count;       /**< Number of blocks with motion vectors */
  int32_t mv_coded_count;    /**< Number of coded motion vectors */
  /** 1 if the decoder attached statistics to the frame; 0 only with
   * vp_options.frames_without_statistics */
  int32_t has_statistics;
} vp_frame_info;

/**
 * @brief Counts over the frames returned so far (the CLI's summary)
 */
typedef struct vp_summary {
  uint32_t struct_size;   /**< sizeof(vp_summary) */
  uint32_t frame_count;   /**< Frames returned by vp_next_frame() */
  uint32_t decode_errors; /**< Frames with decode errors, plus rejected packets
                             and frames */
  uint32_t corrupt_packets; /**< Video packets the demuxer marked as corrupt */
  uint32_t discontinuities; /**< Frames with a timestamp jump */
} vp_summary;

/**
 * @brief Decoded picture
 *
 * The pointers are valid until the next vp_next_frame() or vp_close() on the
 * same parser.
 */
typedef struct vp_picture {
  uint32_t struct_size; /**< sizeof(vp_picture) */
  int32_t width;        /**< Width in pixels */
  int32_t height;       /**< Height in pixels */
  /** FFmpeg's pixel format name, for example "yuv420p" (static string) */
  const char *pix_fmt;
  int32_t bit_depth;       /**< Bits per sample of the first component */
  int32_t nb_planes;       /**< Number of planes */
  const uint8_t *data[4];  /**< Plane pointers; NULL for unused planes */
  int32_t linesize[4];     /**< Distance between rows in bytes */
  int32_t row_bytes[4];    /**< Bytes of picture data per row */
  int32_t plane_height[4]; /**< Rows per plane */
  double pts; /**< Presentation timestamp in seconds, as in vp_frame_info */
  int64_t pts_raw; /**< Presentation timestamp in the time base, or VP_NOPTS */
  int32_t interlaced;      /**< 1 if the frame is interlaced */
  int32_t top_field_first; /**< 1 if the top field comes first */
  int32_t
      sample_aspect_num; /**< Sample aspect ratio (numerator), 0 if unknown */
  int32_t sample_aspect_den;   /**< Sample aspect ratio (denominator) */
  const char *color_range;     /**< FFmpeg's name, for example "tv" */
  const char *color_space;     /**< FFmpeg's name, for example "bt709" */
  const char *color_primaries; /**< FFmpeg's name, for example "bt709" */
  const char *color_transfer;  /**< FFmpeg's name, for example "bt709" */
} vp_picture;

/**
 * @brief Log callback
 *
 * @param user_data Pointer passed to vp_set_log_callback()
 * @param level VP_LOG_* level of the message
 * @param line Formatted line, with the component prefix and a trailing
 * newline
 */
typedef void (*vp_log_callback)(void *user_data, int32_t level,
                                const char *line);

/** Version of the C API: (major << 16) | minor */
VP_EXPORT uint32_t vp_api_version(void);

/** Version of the library, for example "0.8.0" (static string) */
VP_EXPORT const char *vp_version(void);

/** Build flags (VP_BUILD_*) */
VP_EXPORT uint32_t vp_build_flags(void);

/** Short description of a status code (static string) */
VP_EXPORT const char *vp_status_string(vp_status status);

/**
 * @brief Message of the last failed call on the calling thread
 *
 * Empty if no call has failed on this thread. Valid until the next failed call
 * on the same thread.
 */
VP_EXPORT const char *vp_last_error(void);

/** Set FFmpeg's log level (process-wide) */
VP_EXPORT void vp_set_log_level(int32_t level);

/**
 * @brief Receive the log lines of FFmpeg and of the parser instead of
 * printing them to stderr
 *
 * Process-wide. Messages above the log level are not passed on. The callback
 * may be called from any thread that uses a parser. NULL restores the
 * default output to stderr.
 */
VP_EXPORT void vp_set_log_callback(vp_log_callback callback, void *user_data);

/** Set struct_size and the default options */
VP_EXPORT void vp_options_init(vp_options *options);

/**
 * @brief Open a local file
 *
 * @param path Path to the file
 * @param options Options, or NULL for the defaults
 * @param out Receives the parser; free it with vp_close()
 */
VP_EXPORT vp_status vp_open_file(const char *path, const vp_options *options,
                                 vp_parser **out);

/**
 * @brief Open custom input
 *
 * The callbacks are called during this call and vp_next_frame(), on the
 * calling thread, until vp_close().
 *
 * @param io Callbacks and their opaque pointer (copied)
 * @param options Options, or NULL for the defaults
 * @param out Receives the parser; free it with vp_close()
 */
VP_EXPORT vp_status vp_open_io(const vp_io *io, const vp_options *options,
                               vp_parser **out);

/**
 * @brief Get the sequence information
 *
 * After the last frame, fills in the duration, bitrate and frame count if
 * they were unknown when the input was opened.
 */
VP_EXPORT vp_status vp_get_sequence_info(vp_parser *parser,
                                         vp_sequence_info *info);

/**
 * @brief Decode the next frame and get its statistics
 *
 * @return VP_OK with a frame, VP_END at the end or after max_frames frames,
 * VP_ERROR_NO_FRAMES if the stream ended without any frame, or another error
 */
VP_EXPORT vp_status vp_next_frame(vp_parser *parser, vp_frame_info *frame);

/**
 * @brief Get the decoded picture of the frame the last vp_next_frame()
 * returned
 *
 * @return VP_ERROR_INVALID_STATE if the last vp_next_frame() returned no frame
 */
VP_EXPORT vp_status vp_get_picture(vp_parser *parser, vp_picture *picture);

/** Get the counts over the frames returned so far */
VP_EXPORT vp_status vp_get_summary(vp_parser *parser, vp_summary *summary);

/** Close the input and free the parser (NULL is allowed) */
VP_EXPORT void vp_close(vp_parser *parser);

#ifdef __cplusplus
}
#endif

#endif /* VIDEOPARSER_C_H */
