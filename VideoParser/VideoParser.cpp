/**
 * @file VideoParser.cpp
 * @author Werner Robitza
 * @copyright Copyright (c) 2023, AVEQ GmbH. Copyright (c) 2023, videoparser-ng
 * contributors.
 */

#include "VideoParser.h"

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
  for (uint i = 0; i < format_context->nb_streams; i++) {
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

  sequence_info.video_duration = format_context->duration / AV_TIME_BASE;
  strncpy(sequence_info.video_codec, codec->name,
          sizeof(sequence_info.video_codec) - 1);
  sequence_info.video_codec[sizeof(sequence_info.video_codec) - 1] = '\0';

  // fix: we replace libaom-av1 with "av1"
  if (strcmp(codec->name, "libaom-av1") == 0) {
    strncpy(sequence_info.video_codec, "av1",
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

  // Open codec
  codec_context = avcodec_alloc_context3(codec);
  if (!codec_context) {
    throw std::runtime_error("Error allocating codec context");
  }

  if (avcodec_parameters_to_context(codec_context, codec_parameters) < 0) {
    throw std::runtime_error("Error setting codec parameters");
  }

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

    // convert via packet size sum (in bytes) to kbit/s
    sequence_info.video_bitrate =
        packet_size_sum * 8 / 1000 / sequence_info.video_duration;
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

  // count general size statistics
  packet_size_sum += current_packet->size;

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
  frame_info.size = current_packet->size;
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

/**
 * @brief Parse a single frame and set the frame_info struct
 *
 * @param frame_info The frame_info struct to be set
 * @return true If a frame was parsed and the frame_info struct was set
 * @return false If no frame was parsed (stop parsing)
 */
bool VideoParser::parse_frame(FrameInfo &frame_info) {
  while (av_read_frame(format_context, current_packet) == 0) {
    if (current_packet->stream_index == video_stream_idx) {
      if (avcodec_send_packet(codec_context, current_packet) == 0) {
        while (avcodec_receive_frame(codec_context, frame) == 0) {
          try {
            set_frame_info(frame_info);
            // only unref and return true if we successfully set frame info
            av_packet_unref(current_packet);
            return true;
          } catch (const std::exception &e) {
            if (verbose) {
              std::cerr << "Warning: Could not set frame info for frame index "
                        << frame_idx << ": " << e.what() << std::endl;
            }
            // continue to next frame if we couldn't set frame info
            continue;
          }
        }
      }
    }
    av_packet_unref(current_packet);
  }

  // Free the packet, no more frames
  av_packet_free(&current_packet);
  return false;
}

/**
 * @brief Close the input and free memory
 */
void VideoParser::close() {
  av_packet_free(&current_packet);
  av_frame_free(&frame);
}
} // namespace videoparser
