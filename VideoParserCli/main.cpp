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

int main(int argc, char *argv[]) {
  if (argc < 2) {
    print_usage(argv[0]);
    return EXIT_FAILURE;
  }

  // parse options and positional arguments
  std::string filename;
  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];
    if (arg == "-v" || arg == "--verbose") {
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

  videoparser::SequenceInfo sequence_info;
  std::vector<videoparser::FrameInfo> frame_infos;

  try {
    videoparser::parse_file(filename, sequence_info, frame_infos);
  } catch (const std::exception &e) {
    std::cerr << "Error: " << e.what() << std::endl;
    return EXIT_FAILURE;
  }

  // print out frame infos
  json j;
  for (const auto &frame_info : frame_infos) {
    j.push_back({{"frame_idx", frame_info.frame_idx}});
  }
  std::cout << j.dump(2) << std::endl;

  print_sequence_info(sequence_info);
}
