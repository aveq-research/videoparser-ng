#include "VideoParser.h"
#include "json.hpp"

using json = nlohmann::json;

void print_usage(const char *program_name) {
  std::cerr << "Usage: " << program_name << " [options] <filename>"
            << std::endl;
  std::cerr << std::endl;
  std::cerr << "Options:" << std::endl;
  std::cerr << "  -v, --verbose     Show verbose output" << std::endl;
  std::cerr << "  -h, --help        Show this help message" << std::endl;
  std::cerr << std::endl;
  std::cerr << "Copyright 2023 AVEQ GmbH" << std::endl;
}

void print_sequence_info(const videoparser::SequenceInfo &info) {
  std::cerr << "Video duration: " << info.video_duration << " s" << std::endl;
  std::cerr << "Video codec: " << info.video_codec << std::endl;
  std::cerr << "Video bitrate: " << info.video_bitrate << " kbps" << std::endl;
  std::cerr << "Video framerate: " << info.video_framerate << " fps"
            << std::endl;
  std::cerr << "Video width: " << info.video_width << " px" << std::endl;
  std::cerr << "Video height: " << info.video_height << " px" << std::endl;
  std::cerr << "Video codec profile: " << info.video_codec_profile << std::endl;
  std::cerr << "Video codec level: " << info.video_codec_level << std::endl;
  std::cerr << "Video pixel format: " << info.video_pix_fmt << std::endl;
}

void print_sequence_info_json(const videoparser::SequenceInfo &info) {
  json j;
  j["video_duration"] = info.video_duration;
  j["video_codec"] = info.video_codec;
  j["video_bitrate"] = info.video_bitrate;
  j["video_framerate"] = info.video_framerate;
  j["video_width"] = info.video_width;
  j["video_height"] = info.video_height;
  j["video_codec_profile"] = info.video_codec_profile;
  j["video_codec_level"] = info.video_codec_level;
  j["video_pix_fmt"] = info.video_pix_fmt;
  std::cout << j.dump() << std::endl;
}

void print_frame_info(const videoparser::FrameInfo &info) {
  std::cerr << "Frame index: " << info.frame_idx << std::endl;
}

void print_frame_info_json(const videoparser::FrameInfo &info) {
  json j;
  j["frame_idx"] = info.frame_idx;
  std::cout << j.dump() << std::endl;
}

int main(int argc, char *argv[]) {
  if (argc < 2) {
    print_usage(argv[0]);
    return EXIT_FAILURE;
  }

  // parse options and positional arguments
  bool verbose = false;
  std::string filename;
  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];
    if (arg == "-v" || arg == "--verbose") {
      verbose = true;
      videoparser::set_verbose(true);
    } else if (arg == "-h" || arg == "--help") {
      print_usage(argv[0]);
      return EXIT_SUCCESS;
    } else {
      filename = argv[i];
      // std::cerr << "Error: Unknown option '" << arg << "'" << std::endl;
      // return EXIT_FAILURE;
    }
  }

  // check if file exists
  if (!std::filesystem::exists(filename)) {
    std::cerr << "Error: File '" << filename << "' does not exist" << std::endl;
    return EXIT_FAILURE;
  }

  try {
    videoparser::VideoParser parser(filename);
    videoparser::SequenceInfo sequence_info;

    videoparser::FrameInfo frame_info;

    if (verbose)
      std::cerr << "Parsing frames ..." << std::endl;

    while (parser.parse_frame(frame_info)) {
      if (verbose)
        print_frame_info(frame_info);
      print_frame_info_json(frame_info);
    }

    sequence_info = parser.get_sequence_info();
    if (verbose) {
      std::cerr << "Sequence info:" << std::endl;
      print_sequence_info(sequence_info);
    }
    print_sequence_info_json(sequence_info);

    parser.close();
  } catch (const std::exception &e) {
    std::cerr << "Error: " << e.what() << std::endl;
    return EXIT_FAILURE;
  }
}
