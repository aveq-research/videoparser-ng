#ifndef VIDEOPARSER_H
#define VIDEOPARSER_H

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iomanip> // for std::fixed and std::setprecision
#include <iostream>
#include <optional>
#include <string>
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <unistd.h>
}

#define VIDEOPARSER_VERSION_MAJOR 0
#define VIDEOPARSER_VERSION_MINOR 0
#define VIDEOPARSER_VERSION_PATCH 1

namespace videoparser {
class ScopeExit {
  std::function<void()> fn;

public:
  ScopeExit(std::function<void()> fn) : fn(fn) {}
  ~ScopeExit() { fn(); }
};

void set_verbose(bool verbose);
void parse_file(const std::string filename);
} // namespace videoparser

#endif // VIDEOPARSER_H
