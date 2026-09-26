/**
 * @file videoparser_c.cpp
 * @author Werner Robitza
 * @copyright Copyright (c) 2026, AVEQ GmbH. Copyright (c) 2026,
 * videoparser-ng contributors.
 *
 * @brief C API of libvideoparser, implemented on top of the C++ API
 */

#include "videoparser_c.h"
#include "VideoParser.h"

#include <algorithm>
#include <atomic>
#include <cerrno>
#include <cstdarg>
#include <cstring>
#include <memory>
#include <new>
#include <string>

extern "C" {
#include <libavutil/imgutils.h>
}

// Size of the first version of a struct: the end of its last field. Fields
// added later do not change it, so callers built against version 1 keep
// working.
#define VP_SIZE_V1(type, last_field)                                           \
  (offsetof(type, last_field) + sizeof(((type *)nullptr)->last_field))

namespace {

constexpr size_t kOptionsSizeV1 = VP_SIZE_V1(vp_options, io_buffer_size);
constexpr size_t kIoSizeV1 = VP_SIZE_V1(vp_io, opaque);
constexpr size_t kSequenceInfoSizeV1 =
    VP_SIZE_V1(vp_sequence_info, time_base_den);
constexpr size_t kFrameInfoSizeV1 = VP_SIZE_V1(vp_frame_info, mv_coded_count);
constexpr size_t kSummarySizeV1 = VP_SIZE_V1(vp_summary, discontinuities);
constexpr size_t kPictureSizeV1 = VP_SIZE_V1(vp_picture, color_transfer);

thread_local std::string last_error;

vp_status fail(vp_status status, const std::string &message) {
  last_error = message;
  return status;
}

/** Check that a struct from the caller is at least as large as version 1 */
template <typename T> bool valid_struct(const T *value, size_t size_v1) {
  return value && value->struct_size >= size_v1;
}

/** Copy a struct to the caller, as far as the caller's struct_size allows */
template <typename T> void copy_out(T *out, const T &in) {
  uint32_t size = out->struct_size;
  std::memcpy(out, &in, std::min<size_t>(size, sizeof(T)));
  out->struct_size = size;
}

/** Copy a struct from the caller, keeping the defaults of missing fields */
template <typename T> void copy_in(T *out, const T *in) {
  std::memcpy(out, in, std::min<size_t>(in->struct_size, sizeof(T)));
  out->struct_size = sizeof(T);
}

std::string av_error_string(int av_error) {
  char buf[AV_ERROR_MAX_STRING_SIZE] = {};
  av_strerror(av_error, buf, sizeof(buf));
  return buf;
}

vp_status status_of(videoparser::Error::Code code) {
  using Code = videoparser::Error::Code;
  switch (code) {
  case Code::Open:
    return VP_ERROR_OPEN;
  case Code::NoVideoStream:
    return VP_ERROR_NO_VIDEO_STREAM;
  case Code::Unsupported:
    return VP_ERROR_UNSUPPORTED;
  case Code::Decode:
    return VP_ERROR_DECODE;
  case Code::Io:
    return VP_ERROR_IO;
  case Code::OutOfMemory:
    return VP_ERROR_OUT_OF_MEMORY;
  case Code::Internal:
    return VP_ERROR_INTERNAL;
  }
  return VP_ERROR_INTERNAL;
}

/** Status and message of the exception being handled */
vp_status fail_with_current_exception() {
  try {
    throw;
  } catch (const videoparser::Error &e) {
    std::string message = e.what();
    if (e.av_error != 0) {
      message += ": " + av_error_string(e.av_error);
    }
    return fail(status_of(e.code), message);
  } catch (const std::bad_alloc &) {
    return fail(VP_ERROR_OUT_OF_MEMORY, "Out of memory");
  } catch (const std::exception &e) {
    return fail(VP_ERROR_INTERNAL, e.what());
  } catch (...) {
    return fail(VP_ERROR_INTERNAL, "Unknown error");
  }
}

/** Custom input callbacks of the caller */
struct IoState {
  vp_read_callback read = nullptr;
  vp_seek_callback seek = nullptr;
  void *opaque = nullptr;
  /** A callback failed */
  bool failed = false;
};

int read_trampoline(void *opaque, uint8_t *buf, int size) {
  auto *io = static_cast<IoState *>(opaque);
  int32_t result = io->read(io->opaque, buf, size);
  if (result > 0) {
    return std::min(result, static_cast<int32_t>(size));
  }
  if (result == 0) {
    return AVERROR_EOF;
  }
  io->failed = true;
  return AVERROR(EIO);
}

int64_t seek_trampoline(void *opaque, int64_t offset, int whence) {
  auto *io = static_cast<IoState *>(opaque);
  whence &= ~AVSEEK_FORCE;
  if (whence == AVSEEK_SIZE) {
    int64_t size = io->seek(io->opaque, 0, VP_SEEK_SIZE);
    return size >= 0 ? size : AVERROR(ENOSYS);
  }
  int64_t result = io->seek(io->opaque, offset, whence);
  if (result < 0) {
    io->failed = true;
    return AVERROR(EIO);
  }
  return result;
}

std::atomic<vp_log_callback> log_callback{nullptr};
std::atomic<void *> log_user_data{nullptr};

void log_trampoline(void *avcl, int level, const char *fmt, va_list vl) {
  vp_log_callback callback = log_callback.load();
  if (!callback || level > av_log_get_level()) {
    return;
  }
  // Whether the next line starts a new message and needs the component
  // prefix, as in av_log_default_callback()
  thread_local int print_prefix = 1;
  char line[1024];
  av_log_format_line2(avcl, level, fmt, vl, line, sizeof(line), &print_prefix);
  callback(log_user_data.load(), level, line);
}

void parser_log_trampoline(void *user_data, int level, const char *line) {
  vp_log_callback callback = log_callback.load();
  if (callback) {
    callback(user_data, level, line);
  }
}

void copy_string(char *out, size_t size, const char *in) {
  std::strncpy(out, in, size - 1);
  out[size - 1] = '\0';
}

const char *name_or_unknown(const char *name) {
  return name ? name : "unknown";
}

} // namespace

struct vp_parser {
  std::unique_ptr<videoparser::VideoParser> parser;
  /** Owned by the parser, since FFmpeg holds a pointer to it */
  std::unique_ptr<IoState> io;
  int64_t max_frames = -1;
  int64_t frames_returned = 0;
  /** The last vp_next_frame() returned a frame */
  bool has_picture = false;
  /** vp_next_frame() returned VP_END */
  bool ended = false;
  /** Error of vp_next_frame(), after which it cannot be called again */
  vp_status failed = VP_OK;
  double last_pts = 0.0;
  int64_t last_pts_raw = VP_NOPTS;
};

extern "C" {

uint32_t vp_api_version(void) { return VP_API_VERSION; }

const char *vp_version(void) {
#define VP_STRINGIFY_(x) #x
#define VP_STRINGIFY(x) VP_STRINGIFY_(x)
  return VP_STRINGIFY(VIDEOPARSER_VERSION_MAJOR) "." VP_STRINGIFY(
      VIDEOPARSER_VERSION_MINOR) "." VP_STRINGIFY(VIDEOPARSER_VERSION_PATCH);
#undef VP_STRINGIFY
#undef VP_STRINGIFY_
}

uint32_t vp_build_flags(void) {
  uint32_t flags = 0;
  if (videoparser_legacy_mode()) {
    flags |= VP_BUILD_LEGACY;
  }
  return flags;
}

const char *vp_status_string(vp_status status) {
  switch (status) {
  case VP_OK:
    return "success";
  case VP_END:
    return "end of stream";
  case VP_ERROR_INVALID_ARGUMENT:
    return "invalid argument";
  case VP_ERROR_INVALID_STATE:
    return "invalid state";
  case VP_ERROR_OPEN:
    return "cannot open input";
  case VP_ERROR_NO_VIDEO_STREAM:
    return "no video stream";
  case VP_ERROR_UNSUPPORTED:
    return "unsupported codec or format";
  case VP_ERROR_DECODE:
    return "decoding failed";
  case VP_ERROR_IO:
    return "input/output error";
  case VP_ERROR_NO_FRAMES:
    return "no frames";
  case VP_ERROR_OUT_OF_MEMORY:
    return "out of memory";
  case VP_ERROR_INTERNAL:
    return "internal error";
  default:
    return "unknown status";
  }
}

const char *vp_last_error(void) { return last_error.c_str(); }

void vp_set_log_level(int32_t level) { av_log_set_level(level); }

void vp_set_log_callback(vp_log_callback callback, void *user_data) {
  log_user_data.store(user_data);
  log_callback.store(callback);
  av_log_set_callback(callback ? log_trampoline : av_log_default_callback);
  videoparser::set_log_callback(callback ? parser_log_trampoline : nullptr,
                                user_data);
}

void vp_options_init(vp_options *options) {
  if (!options) {
    return;
  }
  std::memset(options, 0, sizeof(*options));
  options->struct_size = sizeof(*options);
  options->max_frames = -1;
  options->stream_index = -1;
  options->scan = VP_SCAN_AUTO;
  options->input_format = nullptr;
  options->io_buffer_size = 0;
}

/**
 * @brief Check the options and convert them to the C++ options
 */
static vp_status convert_options(const vp_options *in, vp_options &options,
                                 videoparser::OpenOptions &open_options) {
  vp_options_init(&options);
  if (in) {
    if (!valid_struct(in, kOptionsSizeV1)) {
      return fail(VP_ERROR_INVALID_ARGUMENT,
                  "vp_options: struct_size is too small");
    }
    copy_in(&options, in);
  }
  if (options.max_frames < -1) {
    return fail(VP_ERROR_INVALID_ARGUMENT, "max_frames must be -1 or >= 0");
  }
  if (options.stream_index < -1) {
    return fail(VP_ERROR_INVALID_ARGUMENT, "stream_index must be -1 or >= 0");
  }
  if (options.scan != VP_SCAN_AUTO && options.scan != VP_SCAN_OFF) {
    return fail(VP_ERROR_INVALID_ARGUMENT,
                "scan must be VP_SCAN_AUTO or VP_SCAN_OFF");
  }
  if (options.io_buffer_size < 0) {
    return fail(VP_ERROR_INVALID_ARGUMENT, "io_buffer_size must be >= 0");
  }
  open_options.stream_index = options.stream_index;
  open_options.input_format = options.input_format;
  open_options.scan = options.scan == VP_SCAN_AUTO;
  return VP_OK;
}

vp_status vp_open_file(const char *path, const vp_options *options,
                       vp_parser **out) {
  if (!path || !out) {
    return fail(VP_ERROR_INVALID_ARGUMENT, "path and out must not be NULL");
  }
  *out = nullptr;
  vp_options opts;
  videoparser::OpenOptions open_options;
  vp_status status = convert_options(options, opts, open_options);
  if (status != VP_OK) {
    return status;
  }
  try {
    auto handle = std::make_unique<vp_parser>();
    handle->max_frames = opts.max_frames;
    handle->parser =
        std::make_unique<videoparser::VideoParser>(path, open_options);
    *out = handle.release();
    return VP_OK;
  } catch (...) {
    return fail_with_current_exception();
  }
}

vp_status vp_open_io(const vp_io *io, const vp_options *options,
                     vp_parser **out) {
  if (!io || !out) {
    return fail(VP_ERROR_INVALID_ARGUMENT, "io and out must not be NULL");
  }
  *out = nullptr;
  if (!valid_struct(io, kIoSizeV1)) {
    return fail(VP_ERROR_INVALID_ARGUMENT, "vp_io: struct_size is too small");
  }
  if (!io->read) {
    return fail(VP_ERROR_INVALID_ARGUMENT, "vp_io: read must not be NULL");
  }
  vp_options opts;
  videoparser::OpenOptions open_options;
  vp_status status = convert_options(options, opts, open_options);
  if (status != VP_OK) {
    return status;
  }
  try {
    auto handle = std::make_unique<vp_parser>();
    handle->max_frames = opts.max_frames;
    handle->io = std::make_unique<IoState>();
    handle->io->read = io->read;
    handle->io->seek = io->seek;
    handle->io->opaque = io->opaque;

    videoparser::CustomInput input;
    input.read = read_trampoline;
    input.seek = io->seek ? seek_trampoline : nullptr;
    input.opaque = handle->io.get();
    if (opts.io_buffer_size > 0) {
      input.buffer_size = opts.io_buffer_size;
    }
    try {
      handle->parser =
          std::make_unique<videoparser::VideoParser>(input, open_options);
    } catch (...) {
      status = fail_with_current_exception();
      if (handle->io->failed) {
        return fail(VP_ERROR_IO,
                    "The input callback failed: " + std::string(last_error));
      }
      if (!io->seek) {
        last_error += " (the input is not seekable, and the format may need "
                      "seeking, for example MP4 with the index at the end)";
      }
      return status;
    }
    *out = handle.release();
    return VP_OK;
  } catch (...) {
    return fail_with_current_exception();
  }
}

vp_status vp_get_sequence_info(vp_parser *parser, vp_sequence_info *info) {
  if (!parser || !valid_struct(info, kSequenceInfoSizeV1)) {
    return fail(VP_ERROR_INVALID_ARGUMENT,
                "parser and info must not be NULL, and info->struct_size must "
                "be set");
  }
  try {
    videoparser::SequenceInfo in = parser->parser->get_sequence_info();
    vp_sequence_info result;
    std::memset(&result, 0, sizeof(result));
    result.video_duration = in.video_duration;
    copy_string(result.video_codec, sizeof(result.video_codec), in.video_codec);
    result.video_bitrate = in.video_bitrate;
    result.video_framerate = in.video_framerate;
    result.video_width = in.video_width;
    result.video_height = in.video_height;
    result.video_codec_profile = in.video_codec_profile;
    result.video_codec_level = in.video_codec_level;
    result.video_bit_depth = in.video_bit_depth;
    copy_string(result.video_pix_fmt, sizeof(result.video_pix_fmt),
                in.video_pix_fmt);
    result.video_frame_count = in.video_frame_count;
    result.stream_index = parser->parser->get_stream_index();
    AVRational time_base = parser->parser->get_time_base();
    result.time_base_num = time_base.num;
    result.time_base_den = time_base.den;
    copy_out(info, result);
    return VP_OK;
  } catch (...) {
    return fail_with_current_exception();
  }
}

vp_status vp_next_frame(vp_parser *parser, vp_frame_info *frame) {
  if (!parser || !valid_struct(frame, kFrameInfoSizeV1)) {
    return fail(VP_ERROR_INVALID_ARGUMENT,
                "parser and frame must not be NULL, and frame->struct_size "
                "must be set");
  }
  parser->has_picture = false;
  if (parser->failed != VP_OK) {
    return fail(VP_ERROR_INVALID_STATE,
                "vp_next_frame() failed before with: " +
                    std::string(vp_status_string(parser->failed)));
  }
  if (parser->ended) {
    return VP_END;
  }
  if (parser->max_frames >= 0 &&
      parser->frames_returned >= parser->max_frames) {
    parser->ended = true;
    return VP_END;
  }

  videoparser::FrameInfo in;
  bool parsed = false;
  try {
    parsed = parser->parser->parse_frame(in);
  } catch (...) {
    parser->failed = fail_with_current_exception();
    return parser->failed;
  }

  if (!parsed) {
    parser->ended = true;
    if (parser->io && parser->io->failed) {
      parser->failed = fail(
          VP_ERROR_IO, "The input callback failed; the stream ended early");
      return parser->failed;
    }
    if (parser->frames_returned == 0) {
      parser->failed =
          fail(VP_ERROR_NO_FRAMES,
               "No frames could be parsed from the video stream (unsupported "
               "codec or undecodable stream)");
      return parser->failed;
    }
    return VP_END;
  }

  vp_frame_info result;
  std::memset(&result, 0, sizeof(result));
  result.frame_idx = in.frame_idx;
  result.dts = in.dts;
  result.pts = in.pts;
  result.pts_raw = VP_NOPTS;
  result.dts_raw = VP_NOPTS;
  // The same timestamps as in VideoParser::set_frame_info()
  if (const AVFrame *decoded = parser->parser->get_frame()) {
    int64_t pts = decoded->pts != AV_NOPTS_VALUE
                      ? decoded->pts
                      : decoded->best_effort_timestamp;
    int64_t dts = decoded->pkt_dts != AV_NOPTS_VALUE
                      ? decoded->pkt_dts
                      : decoded->best_effort_timestamp;
    result.pts_raw = pts != AV_NOPTS_VALUE ? pts : VP_NOPTS;
    result.dts_raw = dts != AV_NOPTS_VALUE ? dts : VP_NOPTS;
  }
  result.size = in.size;
  result.frame_type = static_cast<int32_t>(in.frame_type);
  result.is_idr = in.is_idr ? 1 : 0;
  result.decode_error = in.decode_error ? 1 : 0;
  result.discontinuity = in.discontinuity ? 1 : 0;
  result.qp_min = in.qp_min;
  result.qp_max = in.qp_max;
  result.qp_init = in.qp_init;
  result.qp_avg = in.qp_avg;
  result.qp_stdev = in.qp_stdev;
  result.qp_bb_avg = in.qp_bb_avg;
  result.qp_bb_stdev = in.qp_bb_stdev;
  result.motion_avg = in.motion_avg;
  result.motion_stdev = in.motion_stdev;
  result.motion_x_avg = in.motion_x_avg;
  result.motion_y_avg = in.motion_y_avg;
  result.motion_x_stdev = in.motion_x_stdev;
  result.motion_y_stdev = in.motion_y_stdev;
  result.motion_diff_avg = in.motion_diff_avg;
  result.motion_diff_stdev = in.motion_diff_stdev;
  result.current_poc = in.current_poc;
  result.poc_diff = in.poc_diff;
  result.motion_bit_count = in.motion_bit_count;
  result.coefs_bit_count = in.coefs_bit_count;
  result.mb_mv_count = in.mb_mv_count;
  result.mv_coded_count = in.mv_coded_count;
  copy_out(frame, result);

  parser->frames_returned++;
  parser->has_picture = parser->parser->get_frame() != nullptr;
  parser->last_pts = result.pts;
  parser->last_pts_raw = result.pts_raw;
  return VP_OK;
}

vp_status vp_get_picture(vp_parser *parser, vp_picture *picture) {
  if (!parser || !valid_struct(picture, kPictureSizeV1)) {
    return fail(VP_ERROR_INVALID_ARGUMENT,
                "parser and picture must not be NULL, and "
                "picture->struct_size must be set");
  }
  const AVFrame *decoded =
      parser->has_picture ? parser->parser->get_frame() : nullptr;
  if (!decoded) {
    return fail(VP_ERROR_INVALID_STATE,
                "No current frame; call vp_next_frame() first");
  }
  auto format = static_cast<AVPixelFormat>(decoded->format);
  const AVPixFmtDescriptor *desc = av_pix_fmt_desc_get(format);
  if (!desc || (desc->flags & AV_PIX_FMT_FLAG_HWACCEL)) {
    return fail(VP_ERROR_UNSUPPORTED, "Unknown pixel format of the picture");
  }

  vp_picture result;
  std::memset(&result, 0, sizeof(result));
  result.width = decoded->width;
  result.height = decoded->height;
  result.pix_fmt = desc->name;
  result.bit_depth = desc->comp[0].depth;
  int nb_planes = av_pix_fmt_count_planes(format);
  result.nb_planes = std::max(0, std::min(nb_planes, 4));

  int row_bytes[4] = {};
  if (av_image_fill_linesizes(row_bytes, format, decoded->width) < 0) {
    return fail(VP_ERROR_UNSUPPORTED, "Cannot compute the plane sizes");
  }
  for (int i = 0; i < result.nb_planes; i++) {
    // Chroma planes are subsampled, as in av_image_fill_plane_sizes()
    int shift = (i == 1 || i == 2) ? desc->log2_chroma_h : 0;
    result.data[i] = decoded->data[i];
    result.linesize[i] = decoded->linesize[i];
    result.row_bytes[i] = row_bytes[i];
    result.plane_height[i] = (decoded->height + (1 << shift) - 1) >> shift;
  }

  result.pts = parser->last_pts;
  result.pts_raw = parser->last_pts_raw;
  result.interlaced = (decoded->flags & AV_FRAME_FLAG_INTERLACED) ? 1 : 0;
  result.top_field_first =
      (decoded->flags & AV_FRAME_FLAG_TOP_FIELD_FIRST) ? 1 : 0;
  result.sample_aspect_num = decoded->sample_aspect_ratio.num;
  result.sample_aspect_den =
      decoded->sample_aspect_ratio.num ? decoded->sample_aspect_ratio.den : 1;
  result.color_range =
      name_or_unknown(av_color_range_name(decoded->color_range));
  result.color_space =
      name_or_unknown(av_color_space_name(decoded->colorspace));
  result.color_primaries =
      name_or_unknown(av_color_primaries_name(decoded->color_primaries));
  result.color_transfer =
      name_or_unknown(av_color_transfer_name(decoded->color_trc));
  copy_out(picture, result);
  return VP_OK;
}

vp_status vp_get_summary(vp_parser *parser, vp_summary *summary) {
  if (!parser || !valid_struct(summary, kSummarySizeV1)) {
    return fail(VP_ERROR_INVALID_ARGUMENT,
                "parser and summary must not be NULL, and "
                "summary->struct_size must be set");
  }
  videoparser::Summary in = parser->parser->get_summary();
  vp_summary result;
  std::memset(&result, 0, sizeof(result));
  result.frame_count = in.frame_count;
  result.decode_errors = in.decode_errors;
  result.corrupt_packets = in.corrupt_packets;
  result.discontinuities = in.discontinuities;
  copy_out(summary, result);
  return VP_OK;
}

void vp_close(vp_parser *parser) {
  if (!parser) {
    return;
  }
  try {
    parser->parser.reset();
  } catch (...) {
    // close() does not throw; ignore anything unexpected
  }
  delete parser;
}

} // extern "C"
