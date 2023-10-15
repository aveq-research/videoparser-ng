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

void parse_file(const std::string filename) {
  avformat_network_init();

  AVFormatContext *format_context = nullptr;

  // Open video file
  if (avformat_open_input(&format_context, filename.c_str(), nullptr,
                          nullptr) != 0) {
    throw std::runtime_error("Error opening the file");
  }

  ScopeExit close_input([&format_context]() {
    // Close the video file
    avformat_close_input(&format_context);

    // Free up memory
    avformat_free_context(format_context);
  });

  // Retrieve stream information
  if (avformat_find_stream_info(format_context, nullptr) < 0) {
    throw std::runtime_error("Error finding the stream information");
  }

  // Find the first video stream
  int video_stream_idx = -1;
  for (uint i = 0; i < format_context->nb_streams; i++) {
    if (format_context->streams[i]->codecpar->codec_type ==
        AVMEDIA_TYPE_VIDEO) {
      video_stream_idx = i;
      break;
    }
  }

  // warn if there was more than one video stream
  if (video_stream_idx > 0) {
    std::cerr << "Warning, more than one video stream found, will only "
                 "consider first"
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

  std::cerr << "Video codec: " << codec->name << std::endl;

  // Open codec, and iterate through every frame
  AVCodecContext *codec_context = avcodec_alloc_context3(codec);
  if (!codec_context) {
    throw std::runtime_error("Error allocating codec context");
  }

  if (avcodec_parameters_to_context(codec_context, codec_parameters) < 0) {
    throw std::runtime_error("Error setting codec parameters");
  }

  if (avcodec_open2(codec_context, codec, nullptr) < 0) {
    throw std::runtime_error("Error opening codec");
  }

  AVPacket *packet = av_packet_alloc();
  if (!packet) {
    throw std::runtime_error("Error allocating packet");
  }

  AVFrame *frame = av_frame_alloc();
  if (!frame) {
    throw std::runtime_error("Error allocating frame");
  }

  uint32_t frame_count = 0;
  while (av_read_frame(format_context, packet) == 0) {
    if (packet->stream_index == video_stream_idx) {
      if (avcodec_send_packet(codec_context, packet) == 0) {
        while (avcodec_receive_frame(codec_context, frame) == 0) {
          std::cerr << "Frame " << frame_count++ << std::endl;
          // TODO parse out the data
        }
      }
    }
    av_packet_unref(packet);
  }
  av_packet_free(&packet);
  av_frame_free(&frame);
}
} // namespace videoparser
