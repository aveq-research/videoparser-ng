/**
 * @file main.cpp
 * @author Werner Robitza
 * @copyright Copyright (c) 2023, AVEQ GmbH. Copyright (c) 2023, videoparser-ng
 * contributors.
 */

#include "VideoParser.h"
#include "json.hpp"
#include "termcolor.hpp"

using json = nlohmann::json;

void print_usage(const char *program_name) {
  std::cerr << "Usage: " << program_name << " [options] <filename>"
            << std::endl;
  std::cerr << std::endl;
  std::cerr << "Parsing Options:" << std::endl;
  std::cerr << "  -n, --num-frames      Parse only the first n frames"
            << std::endl;
  std::cerr << std::endl;
  std::cerr << "General Options:" << std::endl;
  std::cerr << "  -v, --verbose         Show verbose output" << std::endl;
  std::cerr << "  -h, --help            Show this help message" << std::endl;
  std::cerr << std::endl;
  std::cerr << "Copyright 2023 AVEQ GmbH" << std::endl;
}

void print_sequence_info(const videoparser::SequenceInfo &info) {
  std::cerr << termcolor::yellow
            << "=================== SEQUENCE INFO ==================="
            << termcolor::reset << std::endl;
  std::cerr << "Video duration      = " << info.video_duration << " s"
            << std::endl;
  std::cerr << "Video codec         = " << info.video_codec << std::endl;
  std::cerr << "Video bitrate       = " << info.video_bitrate << " kbps"
            << std::endl;
  std::cerr << "Video framerate     = " << info.video_framerate << " fps"
            << std::endl;
  std::cerr << "Video width         = " << info.video_width << " px"
            << std::endl;
  std::cerr << "Video height        = " << info.video_height << " px"
            << std::endl;
  std::cerr << "Video codec profile = " << info.video_codec_profile
            << std::endl;
  std::cerr << "Video codec level   = " << info.video_codec_level << std::endl;
  std::cerr << "Video bit depth     = " << info.video_bit_depth << std::endl;
  std::cerr << "Video pixel format  = " << info.video_pix_fmt << std::endl;
  std::cerr << "Video frame count   = " << info.video_frame_count << std::endl;
}

void print_sequence_info_json(const videoparser::SequenceInfo &info) {
  json j;
  j["type"] = "sequence_info";
  j["video_duration"] = info.video_duration;
  j["video_codec"] = info.video_codec;
  j["video_bitrate"] = info.video_bitrate;
  j["video_framerate"] = info.video_framerate;
  j["video_width"] = info.video_width;
  j["video_height"] = info.video_height;
  j["video_codec_profile"] = info.video_codec_profile;
  j["video_codec_level"] = info.video_codec_level;
  j["video_bit_depth"] = info.video_bit_depth;
  j["video_pix_fmt"] = info.video_pix_fmt;
  j["video_frame_count"] = info.video_frame_count;
  std::cout << j.dump() << std::endl;
}

// Note: This is not everything, see the JSON below
void print_general_frame_info(const videoparser::FrameInfo &frame_info) {
  std::cerr << termcolor::yellow
            << "=================== FRAME INFO ==================="
            << termcolor::reset << std::endl;
  std::cerr << "Frame index = " << frame_info.frame_idx << std::endl;
  std::cerr << "DTS         = " << frame_info.dts << " s" << std::endl;
  std::cerr << "PTS         = " << frame_info.pts << " s" << std::endl;
  std::cerr << "Size        = " << frame_info.size << " bytes" << std::endl;
  std::cerr << "Frame type  = " << frame_info.frame_type << std::endl;
  std::cerr << "Is IDR      = " << frame_info.is_idr << std::endl;
}

void print_frame_info_json(const videoparser::FrameInfo &frame_info) {
  json j;
  j["type"] = "frame_info";
  j["frame_idx"] = frame_info.frame_idx;
  j["dts"] = frame_info.dts;
  j["pts"] = frame_info.pts;
  j["size"] = frame_info.size;
  j["frame_type"] = frame_info.frame_type;
  j["is_idr"] = frame_info.is_idr;

  // QP values
  j["qp_min"] = frame_info.qp_min;
  j["qp_max"] = frame_info.qp_max;
  j["qp_init"] = frame_info.qp_init;
  j["qp_avg"] = frame_info.qp_avg;
  j["qp_stdev"] = frame_info.qp_stdev;
  j["qp_bb_avg"] = frame_info.qp_bb_avg;
  j["qp_bb_stdev"] = frame_info.qp_bb_stdev;

  // Motion estimation
  j["motion_avg"] = frame_info.motion_avg;
  j["motion_stdev"] = frame_info.motion_stdev;
  j["motion_x_avg"] = frame_info.motion_x_avg;
  j["motion_y_avg"] = frame_info.motion_y_avg;
  j["motion_x_stdev"] = frame_info.motion_x_stdev;
  j["motion_y_stdev"] = frame_info.motion_y_stdev;
  j["motion_diff_avg"] = frame_info.motion_diff_avg;
  j["motion_diff_stdev"] = frame_info.motion_diff_stdev;
  j["current_poc"] = frame_info.current_poc;
  j["poc_diff"] = frame_info.poc_diff;
  j["motion_bit_count"] = frame_info.motion_bit_count;
  j["coefs_bit_count"] = frame_info.coefs_bit_count;
  j["mb_mv_count"] = frame_info.mb_mv_count;
  j["mv_coded_count"] = frame_info.mv_coded_count;

  // Temporary MV values
  // j["mv_length"] = frame_info.mv_length;
  // j["mv_sum_sqr"] = frame_info.mv_sum_sqr;
  // j["mv_x_length"] = frame_info.mv_x_length;
  // j["mv_y_length"] = frame_info.mv_y_length;
  // j["mv_x_sum_sqr"] = frame_info.mv_x_sum_sqr;
  // j["mv_y_sum_sqr"] = frame_info.mv_y_sum_sqr;
  // j["mv_length_diff"] = frame_info.mv_length_diff;
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
  int num_frames = -1;
  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];
    if (arg == "-v" || arg == "--verbose") {
      verbose = true;
      videoparser::set_verbose(true);
    } else if (arg == "-h" || arg == "--help") {
      print_usage(argv[0]);
      return EXIT_SUCCESS;
    } else if (arg == "-n" || arg == "--num-frames") {
      if (i + 1 < argc) {
        try {
          num_frames = std::stoi(argv[i + 1]);
          ++i;
        } catch (const std::exception &e) {
          std::cerr << "Error: Invalid argument for option '" << arg << "'"
                    << std::endl;
          return EXIT_FAILURE;
        }
      } else {
        std::cerr << "Error: Missing argument for option '" << arg << "'"
                  << std::endl;
        return EXIT_FAILURE;
      }
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

    sequence_info = parser.get_sequence_info();
    if (verbose)
      print_sequence_info(sequence_info);
    print_sequence_info_json(sequence_info);

    if (verbose)
      std::cerr << "Parsing frames ..." << std::endl;

    // track actual frames processed
    int frames_processed = 0;

    while (parser.parse_frame(frame_info)) {
      // only check num_frames against actual processed frames
      if (num_frames >= 0 && frames_processed >= num_frames) {
        break;
      }

      if (verbose)
        print_general_frame_info(frame_info);
      print_frame_info_json(frame_info);

      frames_processed++;
    }

    parser.close();
  } catch (const std::exception &e) {
    std::cerr << "Error: " << e.what() << std::endl;
    return EXIT_FAILURE;
  }
}
