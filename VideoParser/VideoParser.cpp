/**
 * @file VideoParser.cpp
 * @author Werner Robitza
 * @copyright Copyright (c) 2023-2025, AVEQ GmbH. Copyright (c) 2023-2025,
 * videoparser-ng contributors.
 */

#include "VideoParser.h"

#include <atomic>
#include <cmath>
#include <cstring>
#include <sstream>

namespace videoparser {
static bool verbose = false;

static std::atomic<LogCallback> log_callback{nullptr};
static std::atomic<void *> log_user_data{nullptr};

void set_log_callback(LogCallback callback, void *user_data) {
  log_user_data.store(user_data);
  log_callback.store(callback);
}

/**
 * @brief Write a message of one or more lines to the log callback, line by
 * line, or to stderr if there is no callback
 *
 * @param level FFmpeg log level (AV_LOG_*); messages above av_log_get_level()
 * do not go to the callback
 * @param message Message without a trailing newline
 */
static void log(int level, const std::string &message) {
  LogCallback callback = log_callback.load();
  if (!callback) {
    std::cerr << message << std::endl;
    return;
  }
  if (level > av_log_get_level()) {
    return;
  }
  std::istringstream lines(message);
  std::string line;
  while (std::getline(lines, line)) {
    line += '\n';
    callback(log_user_data.load(), level, line.c_str());
  }
}

// Largest deviation of a timestamp from the end of the previous frame, forward
// and backward, in seconds. A larger deviation is a discontinuity.
constexpr double MAX_TIMESTAMP_GAP = 5.0;
constexpr double MAX_TIMESTAMP_STEP_BACK = 1.0;

void set_verbose(bool verbose) {
  videoparser::verbose = verbose;
  if (verbose) {
    av_log_set_level(AV_LOG_DEBUG);
  } else {
    av_log_set_level(AV_LOG_QUIET);
  }
}

VideoParser::VideoParser(const char *filename)
    : VideoParser(filename, OpenOptions()) {}

VideoParser::VideoParser(const char *filename, const OpenOptions &options)
    : filename(filename), options(options) {
  // The destructor does not run if the constructor throws, so free what was
  // opened so far
  try {
    open();
  } catch (...) {
    close();
    throw;
  }
}

VideoParser::VideoParser(const CustomInput &input, const OpenOptions &options)
    : custom_input(input), options(options) {
  if (!input.read) {
    throw Error(Error::Code::Open, "No read callback given");
  }
  try {
    open();
  } catch (...) {
    close();
    throw;
  }
}

VideoParser::~VideoParser() { close(); }

/**
 * @brief Open the file and the decoder, and fill the sequence info
 */
void VideoParser::open() {
  // The strings only need to be valid during the constructor
  if (options.input_format) {
    input_format_name = options.input_format;
    options.input_format = nullptr;
  }

  // Initialize FFmpeg networking
  avformat_network_init();
  network_initialized = true;

  open_input();

  if (options.stream_index >= 0) {
    if (static_cast<unsigned int>(options.stream_index) >=
            format_context->nb_streams ||
        format_context->streams[options.stream_index]->codecpar->codec_type !=
            AVMEDIA_TYPE_VIDEO) {
      throw Error(Error::Code::NoVideoStream,
                  "Stream " + std::to_string(options.stream_index) +
                      " is not a video stream");
    }
    video_stream_idx = options.stream_index;
  } else {
    // Find the first video stream
    for (unsigned int i = 0; i < format_context->nb_streams; i++) {
      if (format_context->streams[i]->codecpar->codec_type ==
          AVMEDIA_TYPE_VIDEO) {
        video_stream_idx = i;
        break;
      }
    }
  }

  // Warn if there was more than one video stream
  if (video_stream_idx > 0 && options.stream_index < 0) {
    log(AV_LOG_WARNING, "Warning, more than one video stream found, will only "
                        "consider the first");
  }

  // Add video codec information to struct
  if (video_stream_idx < 0) {
    throw Error(Error::Code::NoVideoStream, "Error finding a video stream");
  }

  AVCodecParameters *codec_parameters =
      format_context->streams[video_stream_idx]->codecpar;

  const AVCodec *codec = avcodec_find_decoder(codec_parameters->codec_id);
  if (!codec) {
    throw Error(Error::Code::Unsupported, "Error finding the video codec");
  }

  // Raw bitstreams have no duration; it is then estimated below
  if (format_context->duration != AV_NOPTS_VALUE) {
    sequence_info.video_duration =
        format_context->duration / static_cast<double>(AV_TIME_BASE);
  }
  strncpy(sequence_info.video_codec, codec->name,
          sizeof(sequence_info.video_codec) - 1);
  sequence_info.video_codec[sizeof(sequence_info.video_codec) - 1] = '\0';

  // fix: we replace libaom-av1 with "av1", and mpeg{1,2}video with
  // "mpeg{1,2}" (the full name does not fit into the field)
  const char *codec_name_override = nullptr;
  if (strcmp(codec->name, "libaom-av1") == 0) {
    codec_name_override = "av1";
  } else if (strcmp(codec->name, "mpeg2video") == 0) {
    codec_name_override = "mpeg2";
  } else if (strcmp(codec->name, "mpeg1video") == 0) {
    codec_name_override = "mpeg1";
  }
  if (codec_name_override) {
    strncpy(sequence_info.video_codec, codec_name_override,
            sizeof(sequence_info.video_codec) - 1);
    sequence_info.video_codec[sizeof(sequence_info.video_codec) - 1] = '\0';
  }

  // Note: the below may be zero if not indicated in the file
  sequence_info.video_bitrate = codec_parameters->bit_rate / 1000;
  sequence_info.video_framerate =
      av_q2d(format_context->streams[video_stream_idx]->avg_frame_rate);
  sequence_info.video_width = codec_parameters->width;
  sequence_info.video_height = codec_parameters->height;
  sequence_info.video_codec_profile = codec_parameters->profile;
  sequence_info.video_codec_level = codec_parameters->level;
  // Note: the below may be zero if not indicated in the file
  sequence_info.video_frame_count =
      format_context->streams[video_stream_idx]->nb_frames;

  // Containers like MPEG-TS/PS or raw bitstreams signal neither bitrate nor
  // frame count, so estimate them from the video packets
  // The scan needs to go back to the start afterwards
  bool seekable = !filename.empty() || custom_input.seek;
  if (options.scan && seekable &&
      (sequence_info.video_bitrate == 0 ||
       sequence_info.video_frame_count == 0)) {
    scan_video_packets();
    // The scan may have reopened the file
    codec_parameters = format_context->streams[video_stream_idx]->codecpar;
  }

  // Open codec
  codec_context = avcodec_alloc_context3(codec);
  if (!codec_context) {
    throw Error(Error::Code::OutOfMemory, "Error allocating codec context");
  }

  int params_result =
      avcodec_parameters_to_context(codec_context, codec_parameters);
  if (params_result < 0) {
    throw Error(Error::Code::Internal, "Error setting codec parameters",
                params_result);
  }

  // The statistics of the patched decoders are only correct with one thread
  codec_context->thread_count = 1;

  // Preserve packet metadata on the decoded frame. This is needed for codecs
  // with frame reordering, where the packet being read is not necessarily the
  // packet corresponding to the frame returned by the decoder.
  codec_context->flags |= AV_CODEC_FLAG_COPY_OPAQUE;

  const char *pix_fmt_name = av_get_pix_fmt_name(codec_context->pix_fmt);
  strncpy(sequence_info.video_pix_fmt, pix_fmt_name ? pix_fmt_name : "",
          sizeof(sequence_info.video_pix_fmt) - 1);
  sequence_info.video_pix_fmt[sizeof(sequence_info.video_pix_fmt) - 1] = '\0';
  // Unknown if the stream information could not be read, for example if the
  // container declares the wrong codec
  const AVPixFmtDescriptor *pix_fmt_desc =
      av_pix_fmt_desc_get(codec_context->pix_fmt);
  if (!pix_fmt_desc) {
    throw Error(
        Error::Code::Unsupported,
        "Cannot determine the video format (unknown pixel format); the stream "
        "may be damaged or declare the wrong codec");
  }
  sequence_info.video_bit_depth = pix_fmt_desc->comp[0].depth;

  AVDictionary *opts = nullptr;
  // TODO: this is how we can get the motion vectors from ffmpeg, but only for
  // H.264
  // // https://ffmpeg.org/doxygen/trunk/extract_mvs_8c-example.html
  // av_dict_set(&opts, "flags2", "+export_mvs", 0);

  int open_result = avcodec_open2(codec_context, codec, &opts);
  av_dict_free(&opts);
  if (open_result < 0) {
    throw Error(Error::Code::Unsupported, "Error opening codec", open_result);
  }

  // Allocate packet and frame
  current_packet = av_packet_alloc();
  if (!current_packet) {
    throw Error(Error::Code::OutOfMemory, "Error allocating packet");
  }

  frame = av_frame_alloc();
  if (!frame) {
    throw Error(Error::Code::OutOfMemory, "Error allocating frame");
  }
}

/**
 * @brief Open the file or custom input and read its stream information. For
 * custom input that was opened before, start again at the beginning.
 */
void VideoParser::open_input() {
  const AVInputFormat *input_format = nullptr;
  if (!input_format_name.empty()) {
    input_format = av_find_input_format(input_format_name.c_str());
    if (!input_format) {
      throw Error(Error::Code::Open,
                  "Unknown input format: " + input_format_name);
    }
  }

  if (filename.empty()) {
    if (!io_context) {
      int buffer_size =
          custom_input.buffer_size > 0 ? custom_input.buffer_size : 32768;
      auto *buffer = static_cast<unsigned char *>(av_malloc(buffer_size));
      if (!buffer) {
        throw Error(Error::Code::OutOfMemory, "Error allocating I/O buffer");
      }
      io_context =
          avio_alloc_context(buffer, buffer_size, 0, custom_input.opaque,
                             custom_input.read, nullptr, custom_input.seek);
      if (!io_context) {
        av_free(buffer);
        throw Error(Error::Code::OutOfMemory, "Error allocating I/O context");
      }
    } else {
      int64_t seek_result = avio_seek(io_context, 0, SEEK_SET);
      if (seek_result < 0) {
        throw Error(Error::Code::Io,
                    "Error seeking back to the start of the input",
                    static_cast<int>(seek_result));
      }
    }
    format_context = avformat_alloc_context();
    if (!format_context) {
      throw Error(Error::Code::OutOfMemory, "Error allocating format context");
    }
    format_context->pb = io_context;
  }

  // On failure, this frees the format context, but not the custom I/O context
  int open_result = avformat_open_input(&format_context, filename.c_str(),
                                        input_format, nullptr);
  if (open_result != 0) {
    throw Error(Error::Code::Open, "Error opening the file", open_result);
  }

  int info_result = avformat_find_stream_info(format_context, nullptr);
  if (info_result < 0) {
    throw Error(Error::Code::Open, "Error finding the stream information",
                info_result);
  }
}

namespace {
/**
 * @brief Time covered by a sequence of packet timestamps.
 *
 * A timestamp that deviates from the end of the previous packet by more than
 * the thresholds is a discontinuity. It starts a new segment, and the gap
 * between the segments does not count.
 */
struct TimestampSpan {
  int64_t max_gap = 0;                 /**< Largest forward deviation */
  int64_t max_step_back = 0;           /**< Largest backward deviation */
  int64_t min_ts = AV_NOPTS_VALUE;     /**< Start of the current segment */
  int64_t max_end_ts = AV_NOPTS_VALUE; /**< End of the current segment */
  /** End of the last packet with a timestamp */
  int64_t next_ts = AV_NOPTS_VALUE;
  int64_t packets_without_ts = 0; /**< Packets after the last timestamp */
  int64_t closed_length = 0;      /**< Length of the finished segments */
  uint32_t discontinuities = 0;

  void add(int64_t ts, int64_t duration, int64_t frame_period) {
    if (ts == AV_NOPTS_VALUE) {
      packets_without_ts++;
      return;
    }
    if (next_ts != AV_NOPTS_VALUE) {
      int64_t expected = next_ts + packets_without_ts * frame_period;
      if (ts > expected + max_gap || ts < expected - max_step_back) {
        closed_length += segment_length(frame_period);
        min_ts = AV_NOPTS_VALUE;
        max_end_ts = AV_NOPTS_VALUE;
        discontinuities++;
      }
    }
    packets_without_ts = 0;
    if (min_ts == AV_NOPTS_VALUE || ts < min_ts)
      min_ts = ts;
    if (max_end_ts == AV_NOPTS_VALUE || ts + duration > max_end_ts)
      max_end_ts = ts + duration;
    next_ts = ts + duration;
  }

  /**
   * @brief Length of the current segment, extrapolated for trailing packets
   * without a timestamp. Returns 0 if no timestamp was found.
   */
  int64_t segment_length(int64_t frame_period) const {
    if (min_ts == AV_NOPTS_VALUE || max_end_ts <= min_ts)
      return 0;
    return max_end_ts + packets_without_ts * frame_period - min_ts;
  }

  /**
   * @brief Length of all segments. Returns 0 if no timestamp was found.
   */
  int64_t length(int64_t frame_period) const {
    return closed_length + segment_length(frame_period);
  }
};
} // namespace

/**
 * @brief Estimate bitrate and frame count by reading all video packets without
 * decoding them, then go back to the start of the file.
 */
void VideoParser::scan_video_packets() {
  AVPacket *packet = av_packet_alloc();
  if (!packet) {
    throw Error(Error::Code::OutOfMemory, "Error allocating packet");
  }

  AVStream *stream = format_context->streams[video_stream_idx];
  uint64_t size_sum = 0;
  uint32_t packet_count = 0;
  // Frame period in stream time base, for packets without a duration
  int64_t frame_period = 0;
  if (stream->avg_frame_rate.num > 0 && stream->avg_frame_rate.den > 0) {
    frame_period =
        av_rescale_q(1, av_inv_q(stream->avg_frame_rate), stream->time_base);
  }
  // Time span of the video packets, by DTS (index 0) and PTS (index 1)
  TimestampSpan spans[2];
  for (TimestampSpan &span : spans) {
    span.max_gap =
        av_rescale_q(static_cast<int64_t>(MAX_TIMESTAMP_GAP * AV_TIME_BASE),
                     AV_TIME_BASE_Q, stream->time_base);
    span.max_step_back = av_rescale_q(
        static_cast<int64_t>(MAX_TIMESTAMP_STEP_BACK * AV_TIME_BASE),
        AV_TIME_BASE_Q, stream->time_base);
  }

  while (av_read_frame(format_context, packet) == 0) {
    if (packet->stream_index == video_stream_idx) {
      size_sum += packet->size;
      packet_count++;
      int64_t packet_duration =
          packet->duration > 0 ? packet->duration : frame_period;
      spans[0].add(packet->dts, packet_duration, frame_period);
      spans[1].add(packet->pts, packet_duration, frame_period);
    }
    av_packet_unref(packet);
  }
  av_packet_free(&packet);

  // Duration of the video stream, or of the whole file as fallback. DTS is
  // preferred, as PTS starts later with B-frames and is not set for every
  // packet in MPEG-PS.
  double duration = sequence_info.video_duration;
  bool duration_from_timestamps = false;
  uint32_t discontinuities = 0;
  for (const TimestampSpan &span : spans) {
    int64_t length = span.length(frame_period);
    if (length > 0) {
      duration = length * av_q2d(stream->time_base);
      duration_from_timestamps = true;
      discontinuities = span.discontinuities;
      break;
    }
  }
  // Without timestamps or a duration (raw bitstreams), use the frame rate
  if (!duration_from_timestamps && duration <= 0 && frame_period > 0) {
    duration = packet_count * frame_period * av_q2d(stream->time_base);
  }
  // The container duration includes the gaps of discontinuities, so replace
  // it with the time covered by the packets
  if ((sequence_info.video_duration <= 0 || discontinuities > 0) &&
      duration > 0) {
    sequence_info.video_duration = duration;
  }

  if (sequence_info.video_bitrate == 0 && duration > 0) {
    sequence_info.video_bitrate = size_sum * 8 / 1000.0 / duration;
    bitrate_from_scan = true;
  }
  if (sequence_info.video_frame_count == 0) {
    sequence_info.video_frame_count = packet_count;
  }

  // Go back to the start. A byte seek works for formats without reliable
  // timestamps (MPEG-TS/PS, raw bitstreams). Other demuxers, such as Matroska
  // and MP4, keep their end-of-file state after a byte seek or do not support
  // it, so reopen the file for them.
  const AVInputFormat *input_format = format_context->iformat;
  bool byte_seek =
      (input_format->flags & (AVFMT_TS_DISCONT | AVFMT_NOTIMESTAMPS)) &&
      !(input_format->flags & AVFMT_NO_BYTE_SEEK);
  if (byte_seek) {
    int seek_result = av_seek_frame(format_context, -1, 0, AVSEEK_FLAG_BYTE);
    if (seek_result < 0) {
      throw Error(Error::Code::Io,
                  "Error seeking back to the start of the file", seek_result);
    }
  } else {
    avformat_close_input(&format_context);
    open_input();
  }
}

/**
 * @brief Get the sequence info. Call this after the frames are parsed, if the
 * video duration is not set yet.
 *
 * @return SequenceInfo The sequence info struct.
 */
SequenceInfo VideoParser::get_sequence_info() {
  // update the sequence info based on the accumulated video duration and packet
  // size sum, if frames were read at all
  if (frame_idx > 0) {
    if (sequence_info.video_duration == 0) {
      std::ostringstream message;
      message << "Warning: video duration not set initially, setting to "
              << last_pts - first_pts;
      log(AV_LOG_WARNING, message.str());
      sequence_info.video_duration = last_pts - first_pts;
    }

    if (sequence_info.video_frame_count == 0) {
      // no warning needed, default behavior for some containers
      // std::cerr << "Warning: video frame count not set initially, setting to
      // "
      //           << frame_idx << std::endl;
      sequence_info.video_frame_count = frame_idx;
    }

    // convert via packet size sum (in bytes) to kbit/s, unless already
    // estimated from all packets of the file
    if (!bitrate_from_scan) {
      sequence_info.video_bitrate =
          packet_size_sum * 8 / 1000 / sequence_info.video_duration;
    }
  }

  return sequence_info;
}

/**
 * @brief Set the frame info struct from current ffmpeg frame and packet
 *
 * @param frame_info
 */
void VideoParser::set_frame_info(FrameInfo &frame_info) {
  if (frame == nullptr) {
    throw std::runtime_error(
        "Error setting frame info, did you call parse_frame() before?");
  }

  // get the SharedFrameInfo, sometimes it's empty, so we skip this iteration
  SharedFrameInfo *shared_frame_info =
      videoparser_get_final_shared_frame_info(frame);
  if (!shared_frame_info) {
    throw std::runtime_error("No shared frame info found");
  }

  // collect frame timing information
  double time_base =
      av_q2d(format_context->streams[video_stream_idx]->time_base);
  int64_t pts_ts =
      frame->pts != AV_NOPTS_VALUE ? frame->pts : frame->best_effort_timestamp;
  int64_t dts_ts = frame->pkt_dts != AV_NOPTS_VALUE
                       ? frame->pkt_dts
                       : frame->best_effort_timestamp;
  // Frames without a timestamp (all frames of raw bitstreams, or single
  // frames at the end of a stream) get the last known timestamp plus the
  // frame distance, or the frame index divided by the frame rate if there is
  // none. Without a frame rate, NaN (null in JSON).
  double framerate = sequence_info.video_framerate;
  auto estimate = [&](const TimestampAnchor &anchor) {
    if (!(framerate > 0)) {
      return std::nan("");
    }
    if (anchor.frame_idx < 0) {
      return frame_idx / framerate;
    }
    return anchor.time + (frame_idx - anchor.frame_idx) / framerate;
  };
  double pts =
      pts_ts != AV_NOPTS_VALUE ? pts_ts * time_base : estimate(last_valid_pts);
  double dts =
      dts_ts != AV_NOPTS_VALUE ? dts_ts * time_base : estimate(last_valid_dts);
  if (pts_ts != AV_NOPTS_VALUE) {
    last_valid_pts = {static_cast<int64_t>(frame_idx), pts};
  }
  if (dts_ts != AV_NOPTS_VALUE) {
    last_valid_dts = {static_cast<int64_t>(frame_idx), dts};
  }
  // Compare the timestamp with the end of the previous frame
  bool discontinuity = false;
  if (std::isfinite(pts) && std::isfinite(next_pts)) {
    double deviation = pts - next_pts;
    discontinuity =
        deviation > MAX_TIMESTAMP_GAP || deviation < -MAX_TIMESTAMP_STEP_BACK;
  }
  double frame_duration = 0.0;
  if (frame->duration > 0) {
    frame_duration = frame->duration * time_base;
  } else if (framerate > 0) {
    frame_duration = 1.0 / framerate;
  }
  next_pts = pts + frame_duration;

  // set first and last pts to calculate video duration at the end
  if (frame_idx == 0) {
    first_pts = pts;
  }
  last_pts = pts;

  int packet_size = current_packet ? current_packet->size : 0;
  if (frame->opaque_ref && frame->opaque_ref->size == sizeof(packet_size)) {
    memcpy(&packet_size, frame->opaque_ref->data, sizeof(packet_size));
  }

  // count general size statistics
  packet_size_sum += packet_size;

  // set the frame type
  FrameType frame_type = UNKNOWN;
  if (frame->pict_type == AV_PICTURE_TYPE_I) {
    frame_type = I;
  } else if (frame->pict_type == AV_PICTURE_TYPE_P) {
    frame_type = P;
  } else if (frame->pict_type == AV_PICTURE_TYPE_B) {
    frame_type = B;
  }

  // --------------------------------------------------------------------------------------------------------
  // Set the frame_info values here

  // things we can get from ffmpeg's API directly
  frame_info.frame_idx = frame_idx;
  frame_info.pts = pts;
  frame_info.dts = dts;
  frame_info.size = packet_size;
  frame_info.frame_type = frame_type;
  frame_info.is_idr = frame->flags & AV_FRAME_FLAG_KEY;
  frame_info.decode_error =
      frame->decode_error_flags != 0 || (frame->flags & AV_FRAME_FLAG_CORRUPT);
  frame_info.discontinuity = discontinuity;

  if (verbose)
    print_shared_frame_info(*shared_frame_info);
  frame_info.qp_min = shared_frame_info->qp_min;
  frame_info.qp_max = shared_frame_info->qp_max;
  frame_info.qp_init = shared_frame_info->qp_init;
  frame_info.qp_avg = shared_frame_info->qp_avg;
  frame_info.qp_stdev = shared_frame_info->qp_stdev;
  frame_info.qp_bb_avg = shared_frame_info->qp_bb_avg;
  frame_info.qp_bb_stdev = shared_frame_info->qp_bb_stdev;

  // motion estimation
  frame_info.motion_avg = shared_frame_info->motion_avg;
  frame_info.motion_stdev = shared_frame_info->motion_stdev;
  frame_info.motion_x_avg = shared_frame_info->motion_x_avg;
  frame_info.motion_y_avg = shared_frame_info->motion_y_avg;
  frame_info.motion_x_stdev = shared_frame_info->motion_x_stdev;
  frame_info.motion_y_stdev = shared_frame_info->motion_y_stdev;
  frame_info.motion_diff_avg = shared_frame_info->motion_diff_avg;
  frame_info.motion_diff_stdev = shared_frame_info->motion_diff_stdev;
  frame_info.current_poc = shared_frame_info->current_poc;
  frame_info.poc_diff = shared_frame_info->poc_diff;
  frame_info.mb_mv_count = shared_frame_info->mb_mv_count;
  frame_info.motion_bit_count = shared_frame_info->motion_bit_count;
  frame_info.coefs_bit_count = shared_frame_info->coefs_bit_count;
  frame_info.mv_coded_count = shared_frame_info->mv_coded_count;

  // Adding these to make debugging easier
  // frame_info.mv_length = shared_frame_info->mv_length;
  // frame_info.mv_sum_sqr = shared_frame_info->mv_sum_sqr;
  // frame_info.mv_x_length = shared_frame_info->mv_x_length;
  // frame_info.mv_y_length = shared_frame_info->mv_y_length;
  // frame_info.mv_x_sum_sqr = shared_frame_info->mv_x_sum_sqr;
  // frame_info.mv_y_sum_sqr = shared_frame_info->mv_y_sum_sqr;

  // codec-specific handling
  if (codec_context->codec_id == AV_CODEC_ID_H264) {
    set_frame_info_h264(frame_info);
  } else if (codec_context->codec_id == AV_CODEC_ID_H265) {
    set_frame_info_h265(frame_info);
  } else if (codec_context->codec_id == AV_CODEC_ID_VP9) {
    set_frame_info_vp9(frame_info);
  } else if (codec_context->codec_id == AV_CODEC_ID_AV1) {
    set_frame_info_av1(frame_info);
  } else if (codec_context->codec_id == AV_CODEC_ID_MPEG2VIDEO ||
             codec_context->codec_id == AV_CODEC_ID_MPEG1VIDEO) {
    set_frame_info_mpeg2(frame_info);
  } else if (shared_frame_info) {
    log(AV_LOG_WARNING, std::string("Warning: unsupported codec ") +
                            avcodec_get_name(codec_context->codec_id) +
                            ", no extra information will be available.");
  }

  frame_idx++;
  summary.frame_count++;
  if (frame_info.decode_error)
    summary.decode_errors++;
  if (frame_info.discontinuity)
    summary.discontinuities++;
}

Summary VideoParser::get_summary() const { return summary; }

const AVFrame *VideoParser::get_frame() const {
  return frame && frame->buf[0] ? frame : nullptr;
}

int VideoParser::get_stream_index() const { return video_stream_idx; }

AVRational VideoParser::get_time_base() const {
  if (!format_context || video_stream_idx < 0) {
    return AVRational{0, 1};
  }
  return format_context->streams[video_stream_idx]->time_base;
}

void VideoParser::print_shared_frame_info(SharedFrameInfo &shared_frame_info) {
  std::ostringstream out;
  out << "================ SHARED FRAME INFO ================" << std::endl;
  out << "frame_idx      = " << shared_frame_info.frame_idx << std::endl;
  out << "qp_sum         = " << shared_frame_info.qp_sum << std::endl;
  out << "qp_sum_sqr     = " << shared_frame_info.qp_sum_sqr << std::endl;
  out << "qp_cnt         = " << shared_frame_info.qp_cnt << std::endl;
  out << "qp_sum_bb      = " << shared_frame_info.qp_sum_bb << std::endl;
  out << "qp_sum_sqr_bb  = " << shared_frame_info.qp_sum_sqr_bb << std::endl;
  out << "qp_cnt_bb      = " << shared_frame_info.qp_cnt_bb << std::endl;
  out << "----------------------------------------------------" << std::endl;
  out << "qp_min         = " << shared_frame_info.qp_min << std::endl;
  out << "qp_max         = " << shared_frame_info.qp_max << std::endl;
  out << "qp_init        = " << shared_frame_info.qp_init << std::endl;
  out << "qp_avg         = " << shared_frame_info.qp_avg << std::endl;
  out << "qp_stdev       = " << shared_frame_info.qp_stdev << std::endl;
  out << "qp_bb_avg      = " << shared_frame_info.qp_bb_avg << std::endl;
  out << "qp_bb_stdev    = " << shared_frame_info.qp_bb_stdev << std::endl;
  out << "----------------------------------------------------" << std::endl;
  out << "motion_avg        = " << shared_frame_info.motion_avg << std::endl;
  out << "motion_stdev      = " << shared_frame_info.motion_stdev << std::endl;
  out << "motion_x_avg      = " << shared_frame_info.motion_x_avg << std::endl;
  out << "motion_y_avg      = " << shared_frame_info.motion_y_avg << std::endl;
  out << "motion_x_stdev    = " << shared_frame_info.motion_x_stdev
      << std::endl;
  out << "motion_y_stdev    = " << shared_frame_info.motion_y_stdev
      << std::endl;
  out << "motion_diff_avg   = " << shared_frame_info.motion_diff_avg
      << std::endl;
  out << "motion_diff_stdev = " << shared_frame_info.motion_diff_stdev
      << std::endl;
  out << "current_poc       = " << shared_frame_info.current_poc << std::endl;
  out << "poc_diff          = " << shared_frame_info.poc_diff << std::endl;
  out << "motion_bit_count  = " << shared_frame_info.motion_bit_count
      << std::endl;
  out << "coefs_bit_count   = " << shared_frame_info.coefs_bit_count
      << std::endl;
  out << "mb_mv_count       = " << shared_frame_info.mb_mv_count << std::endl;
  out << "mv_coded_count    = " << shared_frame_info.mv_coded_count
      << std::endl;

  // adding these to make debugging easier
  // out << "mv_length         = " << shared_frame_info.mv_length <<
  // std::endl; out << "mv_sum_sqr        = " <<
  // shared_frame_info.mv_sum_sqr << std::endl; out << "mv_x_length = " <<
  // shared_frame_info.mv_x_length << std::endl; out << "mv_y_length = "
  // << shared_frame_info.mv_y_length << std::endl; out << "mv_x_sum_sqr
  // = " << shared_frame_info.mv_x_sum_sqr << std::endl; out <<
  // "mv_y_sum_sqr      = " << shared_frame_info.mv_y_sum_sqr << std::endl;
  // out << "mv_length_diff    = " << shared_frame_info.mv_length_diff <<
  // std::endl; out << "mv_diff_sum_sqr   = " <<
  // shared_frame_info.mv_diff_sum_sqr << std::endl;
  log(AV_LOG_DEBUG, out.str().substr(0, out.str().size() - 1));
}

void VideoParser::set_frame_info_h264(FrameInfo &frame_info) {}
void VideoParser::set_frame_info_h265(FrameInfo &frame_info) {}
void VideoParser::set_frame_info_vp9(FrameInfo &frame_info) {}
void VideoParser::set_frame_info_av1(FrameInfo &frame_info) {}
void VideoParser::set_frame_info_mpeg2(FrameInfo &frame_info) {}

/**
 * @brief Parse a single frame and set the frame_info struct
 *
 * @param frame_info The frame_info struct to be set
 * @return true If a frame was parsed and the frame_info struct was set
 * @return false If no frame was parsed (stop parsing)
 */
bool VideoParser::parse_frame(FrameInfo &frame_info) {
  // No more frames after the end of the stream or after close()
  if (decoder_finished || !codec_context) {
    return false;
  }

  while (true) {
    int receive_result = avcodec_receive_frame(codec_context, frame);
    if (receive_result == 0) {
      try {
        set_frame_info(frame_info);
        return true;
      } catch (const std::exception &e) {
        if (verbose) {
          log(AV_LOG_VERBOSE,
              "Warning: Could not set frame info for frame index " +
                  std::to_string(frame_idx) + ": " + e.what());
        }
        // Continue to the next decoded frame if this one has no parser data.
        continue;
      }
    }

    if (receive_result == AVERROR_EOF) {
      decoder_finished = true;
      av_packet_free(&current_packet);
      return false;
    }

    // Skip frames the decoder rejects as invalid, as the ffmpeg program does
    if (receive_result == AVERROR_INVALIDDATA) {
      summary.decode_errors++;
      continue;
    }

    if (receive_result != AVERROR(EAGAIN)) {
      throw Error(Error::Code::Decode, "Error receiving a decoded frame",
                  receive_result);
    }

    if (decoder_draining) {
      throw Error(Error::Code::Decode,
                  "Decoder requested input after end of stream");
    }

    bool packet_sent = false;
    while (av_read_frame(format_context, current_packet) == 0) {
      if (current_packet->stream_index != video_stream_idx) {
        av_packet_unref(current_packet);
        continue;
      }

      current_packet->opaque_ref =
          av_buffer_alloc(sizeof(current_packet->size));
      if (!current_packet->opaque_ref) {
        av_packet_unref(current_packet);
        throw Error(Error::Code::OutOfMemory,
                    "Error allocating packet metadata");
      }
      memcpy(current_packet->opaque_ref->data, &current_packet->size,
             sizeof(current_packet->size));

      if (current_packet->flags & AV_PKT_FLAG_CORRUPT) {
        summary.corrupt_packets++;
      }

      int send_result = avcodec_send_packet(codec_context, current_packet);
      av_packet_unref(current_packet);
      // Skip packets the decoder rejects as invalid
      if (send_result == AVERROR_INVALIDDATA) {
        summary.decode_errors++;
        continue;
      }
      if (send_result < 0) {
        throw Error(Error::Code::Decode, "Error sending a packet for decoding",
                    send_result);
      }

      packet_sent = true;
      break;
    }

    if (packet_sent) {
      continue;
    }

    // A null packet signals EOF to codecs with delayed output (notably H.264
    // and HEVC with frame reordering). Keep receiving until AVERROR_EOF.
    int send_result = avcodec_send_packet(codec_context, nullptr);
    if (send_result < 0 && send_result != AVERROR_EOF) {
      throw Error(Error::Code::Decode, "Error flushing the video decoder",
                  send_result);
    }
    decoder_draining = true;
  }
}

/**
 * @brief Close the input and free memory
 */
void VideoParser::close() {
  av_packet_free(&current_packet);
  av_frame_free(&frame);
  // Also closes the decoder, including its libaom state
  avcodec_free_context(&codec_context);
  // Also frees the format context and sets it to nullptr
  avformat_close_input(&format_context);
  // Custom I/O is not freed by avformat_close_input(). FFmpeg may have
  // replaced the buffer, so free the current one.
  if (io_context) {
    av_freep(&io_context->buffer);
    avio_context_free(&io_context);
  }
  if (network_initialized) {
    avformat_network_deinit();
    network_initialized = false;
  }
}
} // namespace videoparser
