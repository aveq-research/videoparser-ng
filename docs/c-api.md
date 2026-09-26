# C API

libvideoparser has a plain C interface next to its C++ interface, declared in `VideoParser/videoparser_c.h`. It is meant for callers in other languages (for example Rust, Python or Go), which cannot use the C++ classes directly. It gives the same values as the CLI and the C++ API, reads from a file or from caller-supplied bytes, and gives access to the decoded pictures.

The C++ interface (`VideoParser.h`) stays as it is. The C API is a thin layer over it, compiled into the same library, in the static and in the shared build.

## Goals

- Same output as `video-parser`: the fields of each frame and of the sequence and summary records are the same values the CLI writes. `test/c-api/videoparser-c-test.c` prints the CLI's JSON through the C API only, and `test/test-c-api.py` checks that the output is byte-identical.
- Stable ABI: opaque handles, plain C structs with fixed-width types, no C++ types or exceptions across the boundary, and an explicit version.
- Custom input: a read callback and an optional seek callback, so that the caller can feed MPEG-TS, segments or elementary streams from memory or the network. FFmpeg's network protocols stay disabled.
- Decoded pictures: the planes of the decoded frame, without a copy, for pixel metrics and full-reference metrics in the caller's process.

Not included yet: decoded audio and a comparison API with FFmpeg filters. See "Later extensions".

## Conventions

- All functions start with `vp_`, all types with `vp_`, all macros with `VP_`.
- Functions that can fail return a `vp_status`. `VP_OK` (0) is success. `VP_END` (1) is not an error: `vp_next_frame()` returns it at the end of the stream or after the frame limit. All other values are errors.
- After an error, `vp_last_error()` returns a message for the last failed call on the calling thread. The message stays valid until the next failed call on the same thread. `vp_status_string()` gives a short, static description of a status code.
- No C++ exception leaves the library. Allocation failures return `VP_ERROR_OUT_OF_MEMORY`, all other unexpected exceptions `VP_ERROR_INTERNAL`.
- Every struct that crosses the interface starts with `uint32_t struct_size`, which the caller sets to `sizeof` the struct as the caller's header declares it. The library fills only the fields that fit, so a caller compiled against an older header keeps working with a newer library that appended fields. Fields are only ever appended. A `struct_size` smaller than the first version of the struct gives `VP_ERROR_INVALID_ARGUMENT`.
- A parser handle must not be used from several threads at the same time. Different handles may be used from different threads.
- Strings are UTF-8 and null-terminated. Paths are passed to FFmpeg as they are.

## Versioning

- `VP_API_VERSION_MAJOR` and `VP_API_VERSION_MINOR` in the header, and `vp_api_version()` at run time, which returns `(major << 16) | minor`. The major version changes on incompatible changes (removed or changed functions, changed struct layouts). The minor version changes when functions, struct fields or status codes are added. A caller compiled against version `M.m` works with a library of the same major version and a minor version of at least `m`.
- `vp_version()` returns the library version, for example `"0.8.0"`, which is the same as the CLI's `--version`.
- `vp_build_flags()` returns `VP_BUILD_LEGACY` if the library was built in legacy mode (`VP_MV_POC_NORMALIZATION=1`). P.1204.3 needs the legacy build, so callers can check this at run time. The flag comes from the FFmpeg fork (`videoparser_legacy_mode()` in libavutil), so it is also correct for an FFmpeg rebuilt in place with `VP_EXTRA_CFLAGS`.

## Memory

- The library allocates the parser (`vp_open_file()`, `vp_open_io()`); `vp_close()` frees it.
- The caller owns all structs it passes in (`vp_options`, `vp_io`, `vp_sequence_info`, `vp_frame_info`, `vp_summary`, `vp_picture`). The library copies what it needs from `vp_options` and `vp_io` during the open call; the strings in `vp_options` only need to be valid during that call.
- The plane pointers in `vp_picture` point into the decoder's frame. They stay valid until the next call of `vp_next_frame()` or `vp_close()` on the same parser. To keep a picture longer, copy it.
- `vp_picture.pix_fmt`, `vp_version()`, `vp_status_string()` return static strings.

## Functions

Library information:

- `uint32_t vp_api_version(void)`
- `const char *vp_version(void)`
- `uint32_t vp_build_flags(void)`
- `const char *vp_status_string(vp_status status)`
- `const char *vp_last_error(void)`

Logging (process-wide, since FFmpeg's logging is global):

- `void vp_set_log_level(int32_t level)`: FFmpeg's log level (`VP_LOG_QUIET`, `VP_LOG_ERROR`, `VP_LOG_WARNING`, `VP_LOG_INFO`, `VP_LOG_DEBUG`; the values are FFmpeg's). The default is FFmpeg's default, `VP_LOG_INFO`, as for the CLI.
- `void vp_set_log_callback(vp_log_callback callback, void *user_data)`: receive the log lines of FFmpeg and of the parser instead of having them written to stderr. `NULL` restores the default. The callback gets the level and one formatted line with a trailing newline: FFmpeg's lines have the component prefix (for example `[h264 @ 0x...] error while decoding MB 0 21`), the parser's warnings come as `VP_LOG_WARNING` (for example `Warning, more than one video stream found, will only consider the first`). Lines above the log level are dropped. The callback may be called from any thread that runs a parser. Without a callback, the parser's warnings go to stderr regardless of the log level, as in the CLI.

Opening:

- `void vp_options_init(vp_options *options)`: set `struct_size` and the defaults.
- `vp_status vp_open_file(const char *path, const vp_options *options, vp_parser **out)`: open a local file. `options` may be `NULL` for the defaults.
- `vp_status vp_open_io(const vp_io *io, const vp_options *options, vp_parser **out)`: open custom input.

Both read the stream information, find the video stream and open the decoder. If the container signals no bitrate or frame count (MPEG-TS, MPEG-PS, raw bitstreams), they read all video packets once to estimate them and then go back to the start, as the CLI does. This needs a seekable input.

Parsing:

- `vp_status vp_get_sequence_info(vp_parser *parser, vp_sequence_info *info)`: sequence information. The CLI calls it before the first frame; the values are the same. Called after the last frame, it fills in the duration, bitrate and frame count if they were unknown at the start, as the C++ `get_sequence_info()` does.
- `vp_status vp_next_frame(vp_parser *parser, vp_frame_info *frame)`: decode the next frame and fill its statistics. Returns `VP_OK` with a frame, `VP_END` at the end of the stream or after `max_frames` frames, and `VP_ERROR_NO_FRAMES` if the stream ended without any frame (for example a codec without statistics, or an undecodable stream), unless `max_frames` is 0. Frames come in presentation order. After an error, only `vp_get_sequence_info()`, `vp_get_summary()` and `vp_close()` are allowed.
- `vp_status vp_get_picture(vp_parser *parser, vp_picture *picture)`: the decoded picture of the frame that the last `vp_next_frame()` returned. `VP_ERROR_INVALID_STATE` if that call did not return a frame.
- `vp_status vp_get_summary(vp_parser *parser, vp_summary *summary)`: counts over the frames returned so far (frames, decode errors, corrupt packets, discontinuities). The CLI writes this after the last frame.
- `void vp_close(vp_parser *parser)`: close the input and free the parser. `NULL` is allowed.

## Options

`vp_options` (defaults from `vp_options_init()`):

- `max_frames` (default -1): return at most this many frames, then `VP_END`. Same as the CLI's `-n`; the summary covers only the returned frames. -1 means all frames.
- `stream_index` (default -1): index of the video stream in the container. -1 selects the first video stream, as the CLI does. For MPEG-TS with several programs, the caller finds the stream index of the wanted program or PID (for example with its own TS parser or `ffprobe`). An index that is not a video stream gives `VP_ERROR_NO_VIDEO_STREAM`.
- `input_format` (default `NULL`): name of the FFmpeg demuxer (for example `"mpegts"`), or `NULL` to detect the format. Useful for custom input without a seek callback, where probing only sees the first bytes.
- `scan` (default `VP_SCAN_AUTO`): `VP_SCAN_AUTO` reads all video packets before decoding if the container lacks the bitrate or frame count and the input is seekable. `VP_SCAN_OFF` never does; the duration, bitrate and frame count then come from the parsed frames at the end (as for non-seekable input). Live input should use `VP_SCAN_OFF`.
- `io_buffer_size` (default 0 = 32768): size of the buffer for custom input, in bytes.
- `frames_without_statistics` (default 0): with 1, `vp_next_frame()` also returns frames that carry no statistics, with `has_statistics` 0 and all statistics 0. This is for codecs that the FFmpeg fork does not patch (for example FFV1 references for VMAF), whose frames are otherwise skipped, so that the stream ends with `VP_ERROR_NO_FRAMES`. Frame type, size, timestamps, flags and the picture are set as usual. For the patched codecs, the output is the same as without the option on all test clips.

Legacy or normal motion vector statistics are not an option: the mode is compiled into FFmpeg, so there is one library per mode. `vp_build_flags()` tells which one is loaded. Threads are not an option either: the decoder always uses one thread, because the patched statistics are only correct with one decoder thread.

## Custom input

`vp_io`:

- `read(void *opaque, uint8_t *buf, int32_t size)`: copy up to `size` bytes into `buf` and return the number of bytes, 0 at the end of the input, or a negative value on an error. The call may block until data is available.
- `seek(void *opaque, int64_t offset, int32_t whence)` (optional): `whence` is `SEEK_SET`, `SEEK_CUR` or `SEEK_END` (0, 1, 2); return the new position, or a negative value on an error. With `whence == VP_SEEK_SIZE`, return the size of the input without moving, or a negative value if it is unknown. `NULL` for non-seekable input.
- `opaque`: passed to both callbacks.

The library wraps the callbacks in an FFmpeg `AVIOContext`. With a seek callback, custom input gives exactly the same output as the same bytes in a file (checked by `test/test-c-api.py`). Without one, the parser cannot go back after the scan, so the scan is skipped: the frame records are the same, but the sequence information before the first frame has no bitrate or frame count for MPEG-TS/PS and raw bitstreams, and MP4 files with the index at the end cannot be opened (the error message then says that the input is not seekable). A failed read or seek ends the stream: the frames decoded so far are returned, then `vp_next_frame()` returns `VP_ERROR_IO`. Failures while opening give `VP_ERROR_IO` too.

## Structs

`vp_sequence_info`, `vp_frame_info` and `vp_summary` have the same fields, names and meaning as the CLI's `sequence_info`, `frame_info` and `summary` records (see `README.md` and `METRICS.md`), with fixed-width types and flags as `int32_t` (0 or 1). Additional fields:

- `vp_sequence_info.stream_index`: index of the parsed stream in the container.
- `vp_sequence_info.time_base_num`, `time_base_den`: time base of the stream.
- `vp_frame_info.has_statistics`: 1 if the frame has statistics; 0 only with `frames_without_statistics`.
- `vp_frame_info.pts_raw`, `dts_raw`: the timestamps in the stream's time base, or `VP_NOPTS` (`INT64_MIN`) if the frame has none; `pts` and `dts` in seconds are then estimated from the previous timestamp and the frame rate, as in the CLI (and NaN without a frame rate, which the CLI writes as `null`).

`vp_picture`:

- `width`, `height`: size of the picture in pixels.
- `pix_fmt`: FFmpeg's name of the pixel format, for example `"yuv420p"`, `"yuv420p10le"` or `"gray"`. The name is stable across FFmpeg versions, unlike the enum value.
- `bit_depth`: bits per sample of the first component.
- `nb_planes`: number of planes (up to 4).
- `data[4]`, `linesize[4]`: plane pointers and the distance between rows in bytes.
- `row_bytes[4]`, `plane_height[4]`: bytes of picture data per row (without padding) and rows per plane, so that a plane `p` covers `row_bytes[p]` bytes in each of `plane_height[p]` rows.
- `pts`, `pts_raw`: timestamp of the picture, as in `vp_frame_info`.
- `interlaced`, `top_field_first`: flags of the decoded frame.
- `sample_aspect_num`, `sample_aspect_den`: sample aspect ratio, 0/1 if unknown.
- `color_range`, `color_space`, `color_primaries`, `color_transfer`: FFmpeg's names, for example `"tv"` and `"bt709"`, or `"unknown"`.

## Status codes

- `VP_OK` (0), `VP_END` (1): see above.
- `VP_ERROR_INVALID_ARGUMENT`: a null pointer, a too small `struct_size`, or an invalid option.
- `VP_ERROR_INVALID_STATE`: the call is not allowed now, for example `vp_get_picture()` without a current frame, or `vp_next_frame()` after an error.
- `VP_ERROR_OPEN`: the input could not be opened or its format and streams could not be read (for example a missing file or an unknown format).
- `VP_ERROR_NO_VIDEO_STREAM`: the input has no video stream, or `stream_index` is not a video stream.
- `VP_ERROR_UNSUPPORTED`: no decoder for the codec, or the pixel format cannot be determined (for example a PMT that declares the wrong codec).
- `VP_ERROR_DECODE`: the decoder failed in a way that ends the stream. Damaged data that the decoder conceals is not an error; it shows up in `decode_error` and the summary.
- `VP_ERROR_IO`: the read or seek callback failed, or seeking back after the scan failed.
- `VP_ERROR_NO_FRAMES`: the stream ended without any frame.
- `VP_ERROR_OUT_OF_MEMORY`, `VP_ERROR_INTERNAL`.

## Example

```c
#include <videoparser_c.h>

vp_options options;
vp_options_init(&options);
vp_parser *parser = NULL;
if (vp_open_file("input.ts", &options, &parser) != VP_OK) {
  fprintf(stderr, "%s\n", vp_last_error());
  return 1;
}
vp_sequence_info info = {.struct_size = sizeof(info)};
vp_get_sequence_info(parser, &info);
vp_frame_info frame = {.struct_size = sizeof(frame)};
vp_status status;
while ((status = vp_next_frame(parser, &frame)) == VP_OK) {
  printf("%d %f\n", frame.frame_idx, frame.qp_avg);
}
if (status != VP_END)
  fprintf(stderr, "%s\n", vp_last_error());
vp_close(parser);
```

## Building and linking

The header is installed with the SDK to `include/VideoParser/videoparser_c.h`, and a pkg-config file to `lib/pkgconfig/videoparser.pc`:

- Shared build (`util/build-cmake.sh --shared [--legacy]`): link with `-lvideoparser`; the library finds the FFmpeg libraries next to it.
- Static build: `pkg-config --static --libs videoparser` adds FFmpeg, libaom and the C++ runtime, since the library is C++ inside.

The shared library exports the C functions with default visibility. The C++ API stays exported too.

## Tests

- `test/c-api/videoparser-c-test.c` is a C11 program that uses only `videoparser_c.h`. It writes the same NDJSON as the CLI (including nlohmann::json's number formatting), with options to read through the custom input callbacks (`--io`, `--io-no-seek`), to write the decoded pictures as raw video (`--raw <file>`), to limit the frames (`-n`), and to return frames without statistics (`--all-frames`).
- `test/test-c-api.py` runs the CLI and the test program on a set of clips and compares their output byte by byte: by path, through custom input with seek (also with reads of at most 1000 bytes), with a frame limit, through custom input without seek (frame and summary records only), and with `frames_without_statistics` (same output where the CLI succeeds; for FFV1, the number of frames is listed). With `--raw`, it also compares the decoded pictures, with `frames_without_statistics` so that FFV1 is included, with the raw video of FFmpeg's `ffmpeg` program. On damaged MPEG-2 streams, the concealed pictures of the `ffmpeg` program change from run to run (also with a stock FFmpeg 7.1), so a mismatch there is expected; the test program's pictures are the same in every run.

Run it with the build directories to test, for example:

```bash
uv run test/test-c-api.py --build build --build build/shared-legacy \
  --clips test/ --clips /path/to/more/clips
```

The test program can be built with AddressSanitizer to check for memory errors and leaks on damaged input:

```bash
cmake -S . -B build/asan -DSKIP_FFMPEG_BUILD=ON \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_C_FLAGS=-fsanitize=address -DCMAKE_CXX_FLAGS=-fsanitize=address \
  -DCMAKE_EXE_LINKER_FLAGS=-fsanitize=address
cmake --build build/asan
```

## Later extensions

These fit into the design without incompatible changes (new functions, new option fields, and new flags):

- Decoded audio: open the audio streams too (an option), and `vp_next_audio()` or a combined `vp_next_event()` that returns either a video frame or a block of float samples with channel layout and sample rate.
- Comparison: an API that takes two parsers or two inputs, runs FFmpeg's `libvmaf`, `psnr` and `ssim` filters and returns per-frame scores.
- Callbacks per stream, as in the toolkit PRD, can be built on top of the pull interface by the caller; the pull interface is simpler to bind and to stop.

## Binding from Rust

The Surfmeter Media Toolkit would use the C API through two crates in the `surfmeter-media-core` workspace, as planned in its PRD:

- `videoparser-sys`: raw declarations. Generate them once with `bindgen` from `videoparser_c.h` and commit the result, rather than running bindgen in `build.rs`: the header is small and stable, and this avoids a libclang dependency in every build. A test in the crate compares the committed bindings with a fresh bindgen run in CI. `build.rs` finds the library with `pkg-config` (or `VIDEOPARSER_DIR`), links it dynamically, and sets the runpath (`$ORIGIN` on Linux) for the bundled layout. The crate re-exports `VP_API_VERSION_*` for the check below.
- `videoparser`: the safe wrapper. `Parser::open(path, Options)` and `Parser::open_reader(impl Read + Seek + Send, Options)` (and one for `Read` only) that box the reader and pass it as `opaque`, with `extern "C"` trampolines that catch panics (`std::panic::catch_unwind`) and map `io::Error` to a negative return. `Parser` implements `Iterator<Item = Result<Frame, Error>>` or has `next_frame()`; `Parser::picture(&self) -> Option<Picture<'_>>` borrows the parser, so the borrow checker enforces that the picture is only used until the next frame. `Error` carries the `vp_status` and the `vp_last_error()` message. `Parser` is `Send` but not `Sync`, matching the threading rule. On first use, the crate checks `vp_api_version()` against the major and minimum minor version it was built for, and the toolkit checks `vp_build_flags()` for `VP_BUILD_LEGACY`.
- The log callback goes to `tracing` or `log`, set once at startup.
- Frames feed the `p1204` crate directly (the fields of `vp_frame_info` that `p1204_frame_info` needs have the same names), so that P.1204.3 runs on the same decoding pass.
- The crate lives in `surfmeter-media-core/crates/videoparser-sys` and `crates/videoparser`. Because videoparser-ng is LGPL and linked dynamically, the crates only declare and call the functions and contain no videoparser-ng code.
