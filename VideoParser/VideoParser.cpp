/**
 * @file VideoParser.cpp
 * @author Werner Robitza
 * @copyright Copyright (c) 2023-2025, AVEQ GmbH. Copyright (c) 2023-2025,
 * videoparser-ng contributors.
 */

#include "VideoParser.h"

#include <cstring>

namespace videoparser {
static bool verbose = false;

void set_verbose(bool verbose) {
  videoparser::verbose = verbose;
  if (verbose) {
    av_log_set_level(AV_LOG_DEBUG);
  } else {
    av_log_set_level(AV_LOG_QUIET);
  }
}

VideoParser::VideoParser(const char *filename) {
  // Initialize FFmpeg networking
  avformat_network_init();

  // Open the video file
  if (avformat_open_input(&format_context, filename, nullptr, nullptr) != 0) {
    throw std::runtime_error("Error opening the file");
  }

  // ScopeExit for closing the input and freeing memory
  close_input = [this]() {
    // Close the video file
    avformat_close_input(&format_context);

    // Free up memory
    avformat_free_context(format_context);
  };

  // Retrieve stream information
  if (avformat_find_stream_info(format_context, nullptr) < 0) {
    throw std::runtime_error("Error finding the stream information");
  }

  // Find the first video stream
  for (unsigned int i = 0; i < format_context->nb_streams; i++) {
    if (format_context->streams[i]->codecpar->codec_type ==
        AVMEDIA_TYPE_VIDEO) {
      video_stream_idx = i;
      break;
    }
  }

  // Warn if there was more than one video stream
  if (video_stream_idx > 0) {
    std::cerr << "Warning, more than one video stream found, will only "
                 "consider the first"
              << std::endl;
  }

  // Add video codec information to struct
  if (video_stream_idx < 0) {
    throw std::runtime_error("Error finding a video stream");
  }

  AVCodecParameters *codec_parameters =
      format_context->streams[video_stream_idx]->codecpar;

  const AVCodec *codec = avcodec_find_decoder(codec_parameters->codec_id);
  if (!codec) {
    throw std::runtime_error("Error finding the video codec");
  }

  sequence_info.video_duration =
      format_context->duration / static_cast<double>(AV_TIME_BASE);
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
  if (sequence_info.video_bitrate == 0 ||
      sequence_info.video_frame_count == 0) {
    scan_video_packets();
  }

  // Open codec
  codec_context = avcodec_alloc_context3(codec);
  if (!codec_context) {
    throw std::runtime_error("Error allocating codec context");
  }

  if (avcodec_parameters_to_context(codec_context, codec_parameters) < 0) {
    throw std::runtime_error("Error setting codec parameters");
  }

  // Preserve packet metadata on the decoded frame. This is needed for codecs
  // with frame reordering, where the packet being read is not necessarily the
  // packet corresponding to the frame returned by the decoder.
  codec_context->flags |= AV_CODEC_FLAG_COPY_OPAQUE;

  const char *pix_fmt_name = av_get_pix_fmt_name(codec_context->pix_fmt);
  strncpy(sequence_info.video_pix_fmt, pix_fmt_name ? pix_fmt_name : "",
          sizeof(sequence_info.video_pix_fmt) - 1);
  sequence_info.video_pix_fmt[sizeof(sequence_info.video_pix_fmt) - 1] = '\0';
  sequence_info.video_bit_depth =
      av_pix_fmt_desc_get(codec_context->pix_fmt)->comp[0].depth;

  AVDictionary *opts = nullptr;
  // TODO: this is how we can get the motion vectors from ffmpeg, but only for
  // H.264
  // // https://ffmpeg.org/doxygen/trunk/extract_mvs_8c-example.html
  // av_dict_set(&opts, "flags2", "+export_mvs", 0);

  if (avcodec_open2(codec_context, codec, &opts) < 0) {
    throw std::runtime_error("Error opening codec");
  }

  av_dict_free(&opts);

  // Allocate packet and frame
  current_packet = av_packet_alloc();
  if (!current_packet) {
    throw std::runtime_error("Error allocating packet");
  }

  frame = av_frame_alloc();
  if (!frame) {
    throw std::runtime_error("Error allocating frame");
  }
}

namespace {
/**
 * @brief Time span covered by a sequence of packet timestamps.
 */
struct TimestampSpan {
  int64_t min_ts = AV_NOPTS_VALUE;
  int64_t max_end_ts = AV_NOPTS_VALUE;
  int64_t packets_without_ts = 0; /**< Packets after the last timestamp */

  void add(int64_t ts, int64_t duration) {
    if (ts == AV_NOPTS_VALUE) {
      packets_without_ts++;
      return;
    }
    packets_without_ts = 0;
    if (min_ts == AV_NOPTS_VALUE || ts < min_ts)
      min_ts = ts;
    if (max_end_ts == AV_NOPTS_VALUE || ts + duration > max_end_ts)
      max_end_ts = ts + duration;
  }

  /**
   * @brief Length of the span, extrapolated for trailing packets without a
   * timestamp. Returns 0 if no timestamp was found.
   */
  int64_t length(int64_t frame_period) const {
    if (min_ts == AV_NOPTS_VALUE || max_end_ts <= min_ts)
      return 0;
    return max_end_ts + packets_without_ts * frame_period - min_ts;
  }
};
} // namespace

/**
 * @brief Estimate bitrate and frame count by reading all video packets without
 * decoding them, then seek back to the start of the file.
 */
void VideoParser::scan_video_packets() {
  AVPacket *packet = av_packet_alloc();
  if (!packet) {
    throw std::runtime_error("Error allocating packet");
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

  while (av_read_frame(format_context, packet) == 0) {
    if (packet->stream_index == video_stream_idx) {
      size_sum += packet->size;
      packet_count++;
      int64_t packet_duration =
          packet->duration > 0 ? packet->duration : frame_period;
      spans[0].add(packet->dts, packet_duration);
      spans[1].add(packet->pts, packet_duration);
    }
    av_packet_unref(packet);
  }
  av_packet_free(&packet);

  // Duration of the video stream, or of the whole file as fallback. DTS is
  // preferred, as PTS starts later with B-frames and is not set for every
  // packet in MPEG-PS.
  double duration = sequence_info.video_duration;
  for (const TimestampSpan &span : spans) {
    int64_t length = span.length(frame_period);
    if (length > 0) {
      duration = length * av_q2d(stream->time_base);
      break;
    }
  }

  if (sequence_info.video_bitrate == 0 && duration > 0) {
    sequence_info.video_bitrate = size_sum * 8 / 1000.0 / duration;
    bitrate_from_scan = true;
  }
  if (sequence_info.video_frame_count == 0) {
    sequence_info.video_frame_count = packet_count;
  }

  if (av_seek_frame(format_context, -1, 0, AVSEEK_FLAG_BYTE) < 0) {
    throw std::runtime_error("Error seeking back to the start of the file");
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
      std::cerr << "Warning: video duration not set initially, setting to "
                << last_pts - first_pts << std::endl;
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
  double pts = (frame->pts != AV_NOPTS_VALUE ? frame->pts
                                             : frame->best_effort_timestamp) *
               av_q2d(format_context->streams[video_stream_idx]->time_base);
  double dts =
      (frame->pkt_dts != AV_NOPTS_VALUE ? frame->pkt_dts
                                        : frame->best_effort_timestamp) *
      av_q2d(format_context->streams[video_stream_idx]->time_base);
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
  } else {
    std::cerr << "Warning: unsupported codec "
              << avcodec_get_name(codec_context->codec_id)
              << ", no extra information will be available." << std::endl;
  }

  frame_idx++;
}

void VideoParser::print_shared_frame_info(SharedFrameInfo &shared_frame_info) {
  std::cerr << "================ SHARED FRAME INFO ================"
            << std::endl;
  std::cerr << "frame_idx      = " << shared_frame_info.frame_idx << std::endl;
  std::cerr << "qp_sum         = " << shared_frame_info.qp_sum << std::endl;
  std::cerr << "qp_sum_sqr     = " << shared_frame_info.qp_sum_sqr << std::endl;
  std::cerr << "qp_cnt         = " << shared_frame_info.qp_cnt << std::endl;
  std::cerr << "qp_sum_bb      = " << shared_frame_info.qp_sum_bb << std::endl;
  std::cerr << "qp_sum_sqr_bb  = " << shared_frame_info.qp_sum_sqr_bb
            << std::endl;
  std::cerr << "qp_cnt_bb      = " << shared_frame_info.qp_cnt_bb << std::endl;
  std::cerr << "----------------------------------------------------"
            << std::endl;
  std::cerr << "qp_min         = " << shared_frame_info.qp_min << std::endl;
  std::cerr << "qp_max         = " << shared_frame_info.qp_max << std::endl;
  std::cerr << "qp_init        = " << shared_frame_info.qp_init << std::endl;
  std::cerr << "qp_avg         = " << shared_frame_info.qp_avg << std::endl;
  std::cerr << "qp_stdev       = " << shared_frame_info.qp_stdev << std::endl;
  std::cerr << "qp_bb_avg      = " << shared_frame_info.qp_bb_avg << std::endl;
  std::cerr << "qp_bb_stdev    = " << shared_frame_info.qp_bb_stdev
            << std::endl;
  std::cerr << "----------------------------------------------------"
            << std::endl;
  std::cerr << "motion_avg        = " << shared_frame_info.motion_avg
            << std::endl;
  std::cerr << "motion_stdev      = " << shared_frame_info.motion_stdev
            << std::endl;
  std::cerr << "motion_x_avg      = " << shared_frame_info.motion_x_avg
            << std::endl;
  std::cerr << "motion_y_avg      = " << shared_frame_info.motion_y_avg
            << std::endl;
  std::cerr << "motion_x_stdev    = " << shared_frame_info.motion_x_stdev
            << std::endl;
  std::cerr << "motion_y_stdev    = " << shared_frame_info.motion_y_stdev
            << std::endl;
  std::cerr << "motion_diff_avg   = " << shared_frame_info.motion_diff_avg
            << std::endl;
  std::cerr << "motion_diff_stdev = " << shared_frame_info.motion_diff_stdev
            << std::endl;
  std::cerr << "current_poc       = " << shared_frame_info.current_poc
            << std::endl;
  std::cerr << "poc_diff          = " << shared_frame_info.poc_diff
            << std::endl;
  std::cerr << "motion_bit_count  = " << shared_frame_info.motion_bit_count
            << std::endl;
  std::cerr << "coefs_bit_count   = " << shared_frame_info.coefs_bit_count
            << std::endl;
  std::cerr << "mb_mv_count       = " << shared_frame_info.mb_mv_count
            << std::endl;
  std::cerr << "mv_coded_count    = " << shared_frame_info.mv_coded_count
            << std::endl;

  // adding these to make debugging easier
  // std::cerr << "mv_length         = " << shared_frame_info.mv_length <<
  // std::endl; std::cerr << "mv_sum_sqr        = " <<
  // shared_frame_info.mv_sum_sqr << std::endl; std::cerr << "mv_x_length = " <<
  // shared_frame_info.mv_x_length << std::endl; std::cerr << "mv_y_length = "
  // << shared_frame_info.mv_y_length << std::endl; std::cerr << "mv_x_sum_sqr
  // = " << shared_frame_info.mv_x_sum_sqr << std::endl; std::cerr <<
  // "mv_y_sum_sqr      = " << shared_frame_info.mv_y_sum_sqr << std::endl;
  // std::cerr << "mv_length_diff    = " << shared_frame_info.mv_length_diff <<
  // std::endl; std::cerr << "mv_diff_sum_sqr   = " <<
  // shared_frame_info.mv_diff_sum_sqr << std::endl;
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
  if (decoder_finished) {
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
          std::cerr << "Warning: Could not set frame info for frame index "
                    << frame_idx << ": " << e.what() << std::endl;
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

    if (receive_result != AVERROR(EAGAIN)) {
      throw std::runtime_error("Error receiving a decoded frame");
    }

    if (decoder_draining) {
      throw std::runtime_error("Decoder requested input after end of stream");
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
        throw std::runtime_error("Error allocating packet metadata");
      }
      memcpy(current_packet->opaque_ref->data, &current_packet->size,
             sizeof(current_packet->size));

      int send_result = avcodec_send_packet(codec_context, current_packet);
      av_packet_unref(current_packet);
      if (send_result < 0) {
        throw std::runtime_error("Error sending a packet for decoding");
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
      throw std::runtime_error("Error flushing the video decoder");
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
}
} // namespace videoparser
