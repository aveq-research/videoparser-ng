/**
 * @file videoparser-seek-test.c
 * @brief Test program for seeking, the key frame scan and decode-only mode
 *
 * Uses only videoparser_c.h. Compiled as C11.
 *
 * 1. Decodes the file from the start and keeps the index, timestamp, key
 *    frame flag and MD5 of the picture of every frame.
 * 2. Decodes it again in decode-only mode and compares.
 * 3. Runs the key frame scan and compares its key frames (timestamp and
 *    index) with the key frames of the first pass.
 * 4. Seeks by timestamp to the key frames, in random order, and also to
 *    timestamps of later frames in the same GOP, then decodes up to and
 *    including the next key frame and compares index, timestamp, type, size
 *    and MD5 of each frame. A seek to the time of a key frame without a packet
 *    timestamp (see the scan) must go to the key frame before it.
 * 5. Does the same with byte positions from the scan.
 * 6. Repeats 4 and 5 on a parser in decode-only mode.
 * 7. Checks the error paths: vp_options_init() of an older, smaller
 *    vp_options, bad arguments, custom input without a seek callback, and a
 *    seek callback that fails and then works again.
 *
 * Prints one JSON line with the counts and exits with 1 on any mismatch.
 *
 * Usage: videoparser-seek-test [options] <file>
 *
 *   --seed <n>         seed of the random order (default 1)
 *   --seeks <n>        largest number of key frames to seek to (default 12)
 *   --max-frames <n>   largest number of frames to decode after a seek
 *                      (default 400)
 *   --all-frames       also return frames without statistics
 *   --io               read the file through the custom input callbacks
 *   --verbose          print each mismatch to stderr
 *
 * @copyright Copyright (c) 2026, videoparser-ng contributors.
 */

#define _POSIX_C_SOURCE 200809L

#include "md5.h"
#include "videoparser_c.h"

#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

typedef struct frame_record {
  int32_t frame_idx;
  int64_t pts_raw;
  int32_t is_idr;
  int32_t is_keyframe;
  int32_t frame_type;
  int32_t size;
  int32_t decode_error;
  /* A key frame whose packet has a timestamp, which a seek by timestamp can
   * go to (see "Seeking" in videoparser_c.h) */
  int seekable;
  uint8_t md5[16];
} frame_record;

typedef struct frame_list {
  frame_record *frames;
  size_t count;
  size_t capacity;
} frame_list;

typedef struct test_options {
  const char *filename;
  int all_frames;
  int use_io;
  int verbose;
  int max_seeks;
  int max_frames;
  unsigned seed;
} test_options;

typedef struct counts {
  int mismatches;
  int frames;
  int keyframes;
  int decode_only_frames;
  int scan_keyframes;
  int scan_unknown_idx;
  int pts_seeks;
  int pts_frames;
  int decode_only_seeks;
  int pts_passed_over;
  int byte_unreachable;
  int concealed;
  int pts_unknown;
  int api_checks;
  int byte_seeks;
  int byte_frames;
  const char *pts_seek_note;
  const char *byte_seek_note;
} counts;

static const test_options *opts;
static counts result;
/* Whether the input has no timestamps of its own (raw bitstreams) */
static int no_timestamps;
/* Whether the seeks run on a parser in decode-only mode */
static int seek_decode_only;

static void mismatch(const char *format, ...) {
  result.mismatches++;
  if (opts->verbose || result.mismatches <= 10) {
    va_list args;
    va_start(args, format);
    fprintf(stderr, "mismatch: %s", seek_decode_only ? "decode-only " : "");
    vfprintf(stderr, format, args);
    fprintf(stderr, "\n");
    va_end(args);
  }
}

static void die(const char *what, vp_status status) {
  fprintf(stderr, "Error: %s: %s (%s)\n", what, vp_last_error(),
          vp_status_string(status));
  exit(2);
}

/* Custom input from a file */

static int32_t file_read(void *opaque, uint8_t *buf, int32_t size) {
  FILE *file = opaque;
  size_t n = fread(buf, 1, (size_t)size, file);
  if (n == 0 && ferror(file)) {
    return -1;
  }
  return (int32_t)n;
}

static int64_t file_seek(void *opaque, int64_t offset, int32_t whence) {
  FILE *file = opaque;
  if (whence == VP_SEEK_SIZE) {
    struct stat st;
    if (fstat(fileno(file), &st) != 0) {
      return -1;
    }
    return (int64_t)st.st_size;
  }
  if (fseeko(file, (off_t)offset, whence) != 0) {
    return -1;
  }
  clearerr(file);
  return (int64_t)ftello(file);
}

/* Seek callback that fails while `seek_fails` is set */
static int seek_fails;

static int64_t file_seek_or_fail(void *opaque, int64_t offset, int32_t whence) {
  if (seek_fails) {
    return -1;
  }
  return file_seek(opaque, offset, whence);
}

typedef struct opened {
  vp_parser *parser;
  vp_keyframe_scan *scan;
  FILE *file;
} opened;

static void open_input(opened *out, int decode_only, int scan) {
  vp_options options;
  vp_options_init(&options);
  options.frames_without_statistics = opts->all_frames;
  options.decode_only = decode_only;
  memset(out, 0, sizeof(*out));
  vp_status status;
  if (opts->use_io) {
    out->file = fopen(opts->filename, "rb");
    if (!out->file) {
      fprintf(stderr, "Error: cannot open %s\n", opts->filename);
      exit(2);
    }
    vp_io io = {.struct_size = sizeof(io),
                .read = file_read,
                .seek = file_seek,
                .opaque = out->file};
    status = scan ? vp_keyframe_scan_open_io(&io, &options, &out->scan)
                  : vp_open_io(&io, &options, &out->parser);
  } else {
    status =
        scan ? vp_keyframe_scan_open_file(opts->filename, &options, &out->scan)
             : vp_open_file(opts->filename, &options, &out->parser);
  }
  if (status != VP_OK) {
    die("open", status);
  }
}

static void close_input(opened *in) {
  vp_close(in->parser);
  vp_keyframe_scan_close(in->scan);
  if (in->file) {
    fclose(in->file);
  }
}

static void picture_md5(vp_parser *parser, uint8_t digest[16]) {
  vp_picture picture = {.struct_size = sizeof(picture)};
  vp_status status = vp_get_picture(parser, &picture);
  if (status != VP_OK) {
    die("vp_get_picture", status);
  }
  md5_context ctx;
  md5_init(&ctx);
  for (int p = 0; p < picture.nb_planes; p++) {
    const uint8_t *row = picture.data[p];
    for (int y = 0; y < picture.plane_height[p]; y++) {
      md5_update(&ctx, row, (size_t)picture.row_bytes[p]);
      row += picture.linesize[p];
    }
  }
  md5_final(&ctx, digest);
}

static void add_frame(frame_list *list, const frame_record *record) {
  if (list->count == list->capacity) {
    list->capacity = list->capacity ? list->capacity * 2 : 256;
    list->frames = realloc(list->frames, list->capacity * sizeof(frame_record));
    if (!list->frames) {
      fprintf(stderr, "Error: out of memory\n");
      exit(2);
    }
  }
  list->frames[list->count++] = *record;
}

/* Decode the whole input and keep each frame's record */
static void decode_all(vp_parser *parser, frame_list *list) {
  vp_frame_info frame = {.struct_size = sizeof(frame)};
  vp_status status;
  while ((status = vp_next_frame(parser, &frame)) == VP_OK) {
    frame_record record = {.frame_idx = frame.frame_idx,
                           .pts_raw = frame.pts_raw,
                           .is_idr = frame.is_idr,
                           .is_keyframe = frame.is_keyframe,
                           .frame_type = frame.frame_type,
                           .size = frame.size,
                           .decode_error = frame.decode_error};
    picture_md5(parser, record.md5);
    add_frame(list, &record);
  }
  if (status != VP_END) {
    die("vp_next_frame", status);
  }
}

/* Whether the size of a frame is that of the first pass. In decode-only mode,
 * the size of an H.264 field pair is that of its first field (see
 * "Decode-only mode"), so it is not compared there. */
static int same_size(const vp_frame_info *frame, const frame_record *expected) {
  return seek_decode_only || frame->size == expected->size;
}

/* Index of the first key frame after `idx` in the first pass, or the frame
 * count */
static size_t next_keyframe_after(const frame_list *full, size_t idx) {
  for (size_t i = idx + 1; i < full->count; i++) {
    if (full->frames[i].is_keyframe) {
      return i;
    }
  }
  return full->count;
}

/* Decode after a seek to the key frame with index `key` and compare with the
 * first pass, up to and including the next key frame. Returns the number of
 * frames compared. */
static int check_after_seek(vp_parser *parser, const frame_list *full,
                            size_t key, const char *kind, int64_t target) {
  size_t last = next_keyframe_after(full, key);
  if (last >= full->count) {
    last = full->count - 1;
  }
  if (last - key + 1 > (size_t)opts->max_frames) {
    last = key + (size_t)opts->max_frames - 1;
  }
  vp_frame_info frame = {.struct_size = sizeof(frame)};
  int compared = 0;
  /* The decoder conceals damaged frames with the frames before them, which
   * differ after a seek (none, or concealed ones), so pictures from the
   * first frame with a decode error up to the next key frame may differ */
  int damaged = 0;
  for (size_t i = key; i <= last; i++) {
    vp_status status = vp_next_frame(parser, &frame);
    if (status != VP_OK) {
      mismatch("%s seek to %lld (frame %zu): frame %zu missing (%s: %s)", kind,
               (long long)target, key, i, vp_status_string(status),
               vp_last_error());
      return compared;
    }
    const frame_record *expected = &full->frames[i];
    if (expected->is_keyframe && i != key) {
      damaged = 0;
    }
    if (expected->decode_error) {
      damaged = 1;
    }
    uint8_t md5[16];
    picture_md5(parser, md5);
    int same_picture = memcmp(md5, expected->md5, 16) == 0;
    if (!same_picture && damaged) {
      result.concealed++;
      same_picture = 1;
    }
    /* After a byte seek, the demuxer's generated timestamps (raw bitstreams)
     * start again, and in MPEG-PS a frame without a timestamp of its own has
     * none until the next one with a timestamp */
    int same_pts = frame.pts_raw == expected->pts_raw;
    if (!same_pts && strcmp(kind, "byte") == 0 &&
        (no_timestamps || frame.pts_raw == VP_NOPTS)) {
      result.pts_unknown++;
      same_pts = 1;
    }
    if (frame.frame_idx != expected->frame_idx || !same_pts ||
        frame.is_idr != expected->is_idr ||
        frame.is_keyframe != expected->is_keyframe ||
        frame.frame_type != expected->frame_type ||
        !same_size(&frame, expected) || !same_picture) {
      mismatch("%s seek to %lld (frame %zu): frame %zu differs: idx %d/%d "
               "pts %lld/%lld idr %d/%d key %d/%d size %d/%d md5 %s",
               kind, (long long)target, key, i, frame.frame_idx,
               expected->frame_idx, (long long)frame.pts_raw,
               (long long)expected->pts_raw, frame.is_idr, expected->is_idr,
               frame.is_keyframe, expected->is_keyframe, frame.size,
               expected->size, same_picture ? "same" : "differs");
      return compared;
    }
    compared++;
  }
  return compared;
}

/* Index of the first key frame after `idx` that a seek by timestamp can go
 * to, or the frame count */
static size_t next_seekable_after(const frame_list *full, size_t idx) {
  for (size_t i = idx + 1; i < full->count; i++) {
    if (full->frames[i].is_keyframe && full->frames[i].seekable) {
      return i;
    }
  }
  return full->count;
}

static void shuffle(size_t *values, size_t count) {
  for (size_t i = count; i > 1; i--) {
    size_t j = (size_t)rand() % i;
    size_t t = values[i - 1];
    values[i - 1] = values[j];
    values[j] = t;
  }
}

/* Whether the timestamps of the key frames increase with their index, which
 * a seek by timestamp needs. Key frames without a timestamp are left out. */
static int keyframe_pts_increase(const frame_list *full) {
  int64_t last = VP_NOPTS;
  for (size_t i = 0; i < full->count; i++) {
    const frame_record *f = &full->frames[i];
    if (!f->is_keyframe || !f->seekable) {
      continue;
    }
    if (last != VP_NOPTS && f->pts_raw <= last) {
      return 0;
    }
    last = f->pts_raw;
  }
  return 1;
}

/* Whether the timestamp of frame `i` is not earlier than that of the key
 * frame `key` and earlier than that of the next key frame that a seek can go
 * to, so that it selects this key frame */
static int frame_selects_key(const frame_list *full, size_t key, size_t i) {
  size_t next = next_seekable_after(full, key);
  int64_t pts = full->frames[i].pts_raw;
  if (pts == VP_NOPTS || pts < full->frames[key].pts_raw) {
    return 0;
  }
  return next >= full->count || pts < full->frames[next].pts_raw;
}

static void test_pts_seeks(const frame_list *full, size_t *keys,
                           size_t key_count) {
  opened in;
  open_input(&in, seek_decode_only, 0);
  vp_parser *parser = in.parser;
  /* Seeking after the end of the stream must work */
  frame_list ignored = {0};
  decode_all(parser, &ignored);
  free(ignored.frames);

  /* Raw bitstreams have no timestamps; the demuxer may still generate some
   * by counting frames (MPEG-1/2 elementary streams) */
  vp_status probe =
      vp_seek(parser, full->count ? full->frames[0].pts_raw : 0, 0);
  if (probe == VP_ERROR_UNSUPPORTED) {
    no_timestamps = 1;
  }
  if (full->count == 0 || full->frames[0].pts_raw == VP_NOPTS ||
      no_timestamps) {
    if (!no_timestamps) {
      mismatch("seek by timestamp without timestamps: %s instead of "
               "unsupported",
               vp_status_string(probe));
    }
    result.pts_seek_note = "no timestamps: unsupported (as expected)";
    close_input(&in);
    return;
  }
  if (!keyframe_pts_increase(full)) {
    result.pts_seek_note =
        "skipped: key frame timestamps do not increase (ambiguous)";
    close_input(&in);
    return;
  }

  size_t seeks = 0;
  for (size_t k = 0; k < key_count && seeks < (size_t)opts->max_seeks; k++) {
    size_t key = keys[k];
    if (!full->frames[key].seekable) {
      /* A seek to the time of a key frame without a packet timestamp goes to
       * the key frame before it that has one */
      size_t before = key;
      while (before > 0 && !(full->frames[before - 1].is_keyframe &&
                             full->frames[before - 1].seekable)) {
        before--;
      }
      int64_t target = full->frames[key].pts_raw;
      if (before == 0 || target == VP_NOPTS) {
        continue;
      }
      before--;
      seeks++;
      vp_status status =
          vp_seek(parser, target, full->frames[before].frame_idx);
      if (status != VP_OK) {
        mismatch("seek to timestamp %lld failed: %s (%s)", (long long)target,
                 vp_last_error(), vp_status_string(status));
        continue;
      }
      result.pts_seeks++;
      result.pts_passed_over++;
      result.pts_frames +=
          check_after_seek(parser, full, before, "pts", target);
      continue;
    }
    seeks++;
    size_t s = seeks - 1;
    /* Alternately the timestamp of the key frame and of a later frame of its
     * GOP */
    int64_t target = full->frames[key].pts_raw;
    if (s % 2 == 1) {
      size_t next = next_seekable_after(full, key);
      if (next > key + 1) {
        size_t i = key + 1 + (size_t)rand() % (next - key - 1);
        if (frame_selects_key(full, key, i)) {
          target = full->frames[i].pts_raw;
        }
      }
    }
    vp_status status = vp_seek(parser, target, full->frames[key].frame_idx);
    if (status != VP_OK) {
      mismatch("seek to timestamp %lld failed: %s (%s)", (long long)target,
               vp_last_error(), vp_status_string(status));
      continue;
    }
    result.pts_seeks++;
    result.pts_frames += check_after_seek(parser, full, key, "pts", target);
  }

  /* Before the first key frame there is nothing to seek to */
  {
    int64_t first = INT64_MAX;
    for (size_t k = 0; k < key_count; k++) {
      if (full->frames[keys[k]].seekable &&
          full->frames[keys[k]].pts_raw < first) {
        first = full->frames[keys[k]].pts_raw;
      }
    }
    vp_status status = vp_seek(parser, first - 1000000, 0);
    if (status != VP_ERROR_INVALID_ARGUMENT) {
      mismatch("seek before the first key frame: %s instead of invalid "
               "argument",
               vp_status_string(status));
    }
    vp_frame_info frame = {.struct_size = sizeof(frame)};
    status = vp_next_frame(parser, &frame);
    if (status != VP_ERROR_INVALID_STATE) {
      mismatch("vp_next_frame after a failed seek: %s instead of invalid "
               "state",
               vp_status_string(status));
    }
  }
  close_input(&in);
}

typedef struct scan_entry {
  vp_keyframe keyframe;
  size_t full_index; /* index of the key frame in the first pass */
} scan_entry;

static void test_byte_seeks(const frame_list *full, const scan_entry *scan,
                            size_t scan_count) {
  opened in;
  open_input(&in, seek_decode_only, 0);
  vp_parser *parser = in.parser;
  if (scan_count == 0) {
    close_input(&in);
    return;
  }

  size_t *order = malloc(scan_count * sizeof(size_t));
  for (size_t i = 0; i < scan_count; i++) {
    order[i] = i;
  }
  shuffle(order, scan_count);
  size_t seeks = scan_count < (size_t)opts->max_seeks ? scan_count
                                                      : (size_t)opts->max_seeks;
  for (size_t s = 0; s < seeks; s++) {
    const scan_entry *entry = &scan[order[s]];
    if (entry->full_index >= full->count) {
      continue;
    }
    /* In MPEG-PS, a key frame that follows another in the same PES packet
     * has no position of its own */
    if (entry->keyframe.pos < 0) {
      result.byte_unreachable++;
      continue;
    }
    int64_t frame_idx = entry->keyframe.frame_idx >= 0
                            ? entry->keyframe.frame_idx
                            : full->frames[entry->full_index].frame_idx;
    vp_status status = vp_seek_byte(parser, entry->keyframe.pos, frame_idx);
    if (status == VP_ERROR_UNSUPPORTED) {
      if (s == 0) {
        result.byte_seek_note = "unsupported for this container (as expected)";
      } else {
        mismatch("byte seek unsupported after it worked");
      }
      break;
    }
    if (status != VP_OK) {
      mismatch("seek to byte %lld failed: %s (%s)",
               (long long)entry->keyframe.pos, vp_last_error(),
               vp_status_string(status));
      continue;
    }
    result.byte_seeks++;
    result.byte_frames += check_after_seek(parser, full, entry->full_index,
                                           "byte", entry->keyframe.pos);
  }
  free(order);
  close_input(&in);
}

static void api_check(int ok, const char *format, ...) {
  if (ok) {
    result.api_checks++;
    return;
  }
  result.mismatches++;
  va_list args;
  va_start(args, format);
  fprintf(stderr, "mismatch: api: ");
  vfprintf(stderr, format, args);
  fprintf(stderr, "\n");
  va_end(args);
}

/* vp_options_init() of a program built with API version 1.2, whose
 * vp_options ends with frames_without_statistics: the function (not the
 * macro) must not write after it */
static void test_options_size(void) {
  size_t old_size =
      offsetof(vp_options, frames_without_statistics) + sizeof(int32_t);
  union {
    vp_options options;
    unsigned char bytes[sizeof(vp_options) + 16];
  } buf;
  memset(&buf, 0xAA, sizeof(buf));
  (vp_options_init)(&buf.options);
  int untouched = 1;
  for (size_t i = old_size; i < sizeof(buf.bytes); i++) {
    untouched &= buf.bytes[i] == 0xAA;
  }
  api_check(untouched && buf.options.struct_size == old_size &&
                buf.options.max_frames == -1,
            "vp_options_init() of an older vp_options: struct_size %u, "
            "wrote after it: %s",
            buf.options.struct_size, untouched ? "no" : "yes");

  memset(&buf, 0xAA, sizeof(buf));
  vp_options_init_size(&buf.options, old_size);
  untouched = 1;
  for (size_t i = old_size; i < sizeof(buf.bytes); i++) {
    untouched &= buf.bytes[i] == 0xAA;
  }
  api_check(untouched && buf.options.struct_size == old_size,
            "vp_options_init_size() of an older vp_options");

  /* Opening with it leaves the newer options at their defaults */
  vp_parser *parser = NULL;
  vp_status status = vp_open_file(opts->filename, &buf.options, &parser);
  vp_frame_info frame = {.struct_size = sizeof(frame)};
  if (status == VP_OK) {
    status = vp_next_frame(parser, &frame);
  }
  api_check(status == VP_OK && frame.has_statistics,
            "open with an older vp_options: %s", vp_status_string(status));
  vp_close(parser);

  memset(&buf, 0xAA, sizeof(buf));
  vp_options_init(&buf.options);
  api_check(buf.options.struct_size == sizeof(vp_options) &&
                buf.options.decode_only == 0 &&
                buf.bytes[sizeof(vp_options)] == 0xAA,
            "vp_options_init() macro");
}

typedef vp_status (*seek_function)(vp_parser *, int64_t, int64_t);

/* Read a frame, expect `expected` from the seek, and check that the parser
 * goes on with the next frame */
static void check_unchanged(vp_parser *parser, seek_function seek,
                            int64_t target, int64_t frame_idx,
                            vp_status expected, const char *what) {
  vp_frame_info before = {.struct_size = sizeof(before)};
  vp_frame_info after = {.struct_size = sizeof(after)};
  vp_status first = vp_next_frame(parser, &before);
  vp_status status = seek(parser, target, frame_idx);
  vp_status next = vp_next_frame(parser, &after);
  int goes_on = first != VP_OK || next == VP_END ||
                (next == VP_OK && after.frame_idx == before.frame_idx + 1);
  api_check(status == expected && goes_on, "%s: %s, then %s", what,
            vp_status_string(status), vp_status_string(next));
}

static void test_seek_errors(const frame_list *full) {
  if (full->count == 0) {
    return;
  }
  /* The first key frame */
  const frame_record *key = &full->frames[0];
  for (size_t i = 0; i < full->count; i++) {
    if (full->frames[i].is_keyframe) {
      key = &full->frames[i];
      break;
    }
  }

  /* Bad arguments leave the parser unchanged */
  opened in;
  open_input(&in, 0, 0);
  check_unchanged(in.parser, vp_seek, key->pts_raw, -5,
                  no_timestamps ? VP_ERROR_UNSUPPORTED
                                : VP_ERROR_INVALID_ARGUMENT,
                  "vp_seek() with frame_idx -5");
  /* Byte seeks are unsupported for MP4 and Matroska (an index out of range
   * tells which it is, without changing the parser) */
  vp_status expected =
      vp_seek_byte(in.parser, 0, (int64_t)INT32_MAX + 1) == VP_ERROR_UNSUPPORTED
          ? VP_ERROR_UNSUPPORTED
          : VP_ERROR_INVALID_ARGUMENT;
  check_unchanged(in.parser, vp_seek_byte, -1, 0, expected,
                  "vp_seek_byte() to -1");
  api_check(vp_seek(NULL, 0, 0) == VP_ERROR_INVALID_ARGUMENT &&
                vp_seek_byte(NULL, 0, 0) == VP_ERROR_INVALID_ARGUMENT,
            "seek on NULL");
  close_input(&in);

  /* Custom input without a seek callback cannot seek; the parser goes on */
  FILE *file = fopen(opts->filename, "rb");
  if (!file) {
    fprintf(stderr, "Error: cannot open %s\n", opts->filename);
    exit(2);
  }
  vp_options options;
  vp_options_init(&options);
  options.frames_without_statistics = opts->all_frames;
  vp_io io = {.struct_size = sizeof(io), .read = file_read, .opaque = file};
  vp_parser *parser = NULL;
  if (vp_open_io(&io, &options, &parser) == VP_OK) {
    check_unchanged(parser, vp_seek, key->pts_raw, key->frame_idx,
                    VP_ERROR_UNSUPPORTED, "vp_seek() without seek callback");
    check_unchanged(parser, vp_seek_byte, 0, key->frame_idx,
                    VP_ERROR_UNSUPPORTED,
                    "vp_seek_byte() without seek callback");
  }
  vp_close(parser);
  fclose(file);

  /* A seek callback that fails: the seek fails or works (if the data is
   * still buffered), and after a failure only a seek continues; once the
   * callback works again, a seek works */
  file = fopen(opts->filename, "rb");
  if (!file) {
    fprintf(stderr, "Error: cannot open %s\n", opts->filename);
    exit(2);
  }
  io.seek = file_seek_or_fail;
  io.opaque = file;
  parser = NULL;
  if (vp_open_io(&io, &options, &parser) != VP_OK) {
    die("open", VP_ERROR_OPEN);
  }
  vp_frame_info frame = {.struct_size = sizeof(frame)};
  for (size_t i = 0; i < full->count / 2; i++) {
    if (vp_next_frame(parser, &frame) != VP_OK) {
      break;
    }
  }
  int by_pts = !no_timestamps && key->pts_raw != VP_NOPTS;
  seek_fails = 1;
  vp_status status = by_pts ? vp_seek(parser, key->pts_raw, key->frame_idx)
                            : vp_seek_byte(parser, 0, key->frame_idx);
  seek_fails = 0;
  if (status != VP_OK) {
    vp_status next = vp_next_frame(parser, &frame);
    api_check(next == VP_ERROR_INVALID_STATE,
              "vp_next_frame() after a failed seek (%s): %s",
              vp_status_string(status), vp_status_string(next));
    status = by_pts ? vp_seek(parser, key->pts_raw, key->frame_idx)
                    : vp_seek_byte(parser, 0, key->frame_idx);
    api_check(status == VP_OK, "seek after the seek callback works again: %s",
              vp_status_string(status));
  }
  if (status == VP_OK) {
    vp_status next = vp_next_frame(parser, &frame);
    api_check(next == VP_OK && frame.frame_idx == key->frame_idx &&
                  frame.size == key->size &&
                  (frame.pts_raw == key->pts_raw || !by_pts),
              "first frame after the seek: %s, idx %d/%d size %d/%d pts "
              "%lld/%lld",
              vp_status_string(next), frame.frame_idx, key->frame_idx,
              frame.size, key->size, (long long)frame.pts_raw,
              (long long)key->pts_raw);
  }
  vp_close(parser);
  fclose(file);
}

static void usage(void) {
  fprintf(stderr, "Usage: videoparser-seek-test [--seed <n>] [--seeks <n>] "
                  "[--max-frames <n>] [--all-frames] [--io] [--verbose] "
                  "<file>\n");
}

static void print_json_string(const char *key, const char *value) {
  printf(",\"%s\":", key);
  if (!value) {
    printf("null");
    return;
  }
  putchar('"');
  for (const char *c = value; *c; c++) {
    if (*c == '"' || *c == '\\') {
      putchar('\\');
    }
    putchar(*c);
  }
  putchar('"');
}

int main(int argc, char *argv[]) {
  test_options options = {.max_seeks = 12, .max_frames = 400, .seed = 1};
  for (int i = 1; i < argc; i++) {
    const char *arg = argv[i];
    int has_value = i + 1 < argc;
    if (strcmp(arg, "--seed") == 0 && has_value) {
      options.seed = (unsigned)strtoul(argv[++i], NULL, 10);
    } else if (strcmp(arg, "--seeks") == 0 && has_value) {
      options.max_seeks = (int)strtol(argv[++i], NULL, 10);
    } else if (strcmp(arg, "--max-frames") == 0 && has_value) {
      options.max_frames = (int)strtol(argv[++i], NULL, 10);
    } else if (strcmp(arg, "--all-frames") == 0) {
      options.all_frames = 1;
    } else if (strcmp(arg, "--io") == 0) {
      options.use_io = 1;
    } else if (strcmp(arg, "--verbose") == 0) {
      options.verbose = 1;
    } else if (arg[0] == '-' && arg[1] != '\0') {
      usage();
      return 2;
    } else {
      options.filename = arg;
    }
  }
  if (!options.filename || options.max_seeks < 1 || options.max_frames < 1) {
    usage();
    return 2;
  }
  opts = &options;
  srand(options.seed);
  vp_set_log_level(VP_LOG_QUIET);

  /* 1. Pass from the start */
  frame_list full = {0};
  opened in;
  open_input(&in, 0, 0);
  decode_all(in.parser, &full);
  close_input(&in);
  result.frames = (int)full.count;

  size_t *keys = malloc((full.count + 1) * sizeof(size_t));
  size_t key_count = 0;
  for (size_t i = 0; i < full.count; i++) {
    if (full.frames[i].is_keyframe) {
      keys[key_count++] = i;
    }
  }
  result.keyframes = (int)key_count;

  /* 2. Decode-only */
  frame_list decode_only = {0};
  open_input(&in, 1, 0);
  decode_all(in.parser, &decode_only);
  close_input(&in);
  result.decode_only_frames = (int)decode_only.count;
  if (decode_only.count != full.count) {
    mismatch("decode-only: %zu frames instead of %zu", decode_only.count,
             full.count);
  }
  for (size_t i = 0; i < decode_only.count && i < full.count; i++) {
    const frame_record *a = &full.frames[i], *b = &decode_only.frames[i];
    if (a->frame_idx != b->frame_idx || a->pts_raw != b->pts_raw ||
        a->is_idr != b->is_idr || a->is_keyframe != b->is_keyframe ||
        a->frame_type != b->frame_type || memcmp(a->md5, b->md5, 16) != 0) {
      mismatch("decode-only: frame %zu differs", i);
      break;
    }
  }
  free(decode_only.frames);

  /* 3. Key frame scan; its key frames are matched with those of the first
   * pass by their order */
  scan_entry *scan = NULL;
  size_t scan_count = 0, scan_capacity = 0;
  open_input(&in, 0, 1);
  vp_keyframe keyframe = {.struct_size = sizeof(keyframe)};
  vp_status status;
  while ((status = vp_keyframe_scan_next(in.scan, &keyframe)) == VP_OK) {
    if (scan_count == scan_capacity) {
      scan_capacity = scan_capacity ? scan_capacity * 2 : 64;
      scan = realloc(scan, scan_capacity * sizeof(scan_entry));
    }
    scan[scan_count].keyframe = keyframe;
    scan[scan_count].full_index =
        scan_count < key_count ? keys[scan_count] : full.count;
    scan_count++;
  }
  if (status != VP_END) {
    die("vp_keyframe_scan_next", status);
  }
  close_input(&in);
  result.scan_keyframes = (int)scan_count;
  if (scan_count != key_count) {
    mismatch("scan: %zu key frames instead of %zu", scan_count, key_count);
  }
  for (size_t i = 0; i < scan_count && i < key_count; i++) {
    const vp_keyframe *k = &scan[i].keyframe;
    const frame_record *f = &full.frames[keys[i]];
    if (k->frame_idx < 0) {
      result.scan_unknown_idx++;
    } else if (k->frame_idx != f->frame_idx) {
      mismatch("scan: key frame %zu has index %lld instead of %d", i,
               (long long)k->frame_idx, f->frame_idx);
    }
    if (k->pts_raw != VP_NOPTS && f->pts_raw != VP_NOPTS &&
        k->pts_raw != f->pts_raw) {
      mismatch("scan: key frame %zu has timestamp %lld instead of %lld", i,
               (long long)k->pts_raw, (long long)f->pts_raw);
    }
  }
  /* A seek by timestamp can go to the key frames with a packet timestamp,
   * which the scan returns */
  for (size_t i = 0; i < key_count; i++) {
    frame_record *f = &full.frames[keys[i]];
    f->seekable = scan_count == key_count ? scan[i].keyframe.pts_raw != VP_NOPTS
                                          : f->pts_raw != VP_NOPTS;
  }

  /* 4. Seeks by timestamp, 5. by byte position */
  if (key_count > 0) {
    size_t byte_keys = scan_count < key_count ? scan_count : key_count;
    shuffle(keys, key_count);
    test_pts_seeks(&full, keys, key_count);
    test_byte_seeks(&full, scan, byte_keys);
    /* The same on a parser in decode-only mode */
    int seeks_before = result.pts_seeks + result.byte_seeks;
    seek_decode_only = 1;
    shuffle(keys, key_count);
    test_pts_seeks(&full, keys, key_count);
    test_byte_seeks(&full, scan, byte_keys);
    seek_decode_only = 0;
    result.decode_only_seeks =
        result.pts_seeks + result.byte_seeks - seeks_before;
  }

  /* 7. Error paths */
  test_options_size();
  test_seek_errors(&full);

  printf("{\"file\":\"%s\",\"frames\":%d,\"keyframes\":%d,"
         "\"decode_only_frames\":%d,\"scan_keyframes\":%d,"
         "\"scan_unknown_idx\":%d,\"pts_seeks\":%d,\"pts_frames\":%d,"
         "\"decode_only_seeks\":%d,\"pts_passed_over\":%d,\"byte_unreachable\":"
         "%d,\"byte_seeks\":%d,"
         "\"byte_frames\":%d",
         options.filename, result.frames, result.keyframes,
         result.decode_only_frames, result.scan_keyframes,
         result.scan_unknown_idx, result.pts_seeks, result.pts_frames,
         result.decode_only_seeks, result.pts_passed_over,
         result.byte_unreachable, result.byte_seeks, result.byte_frames);
  print_json_string("pts_seek_note", result.pts_seek_note);
  print_json_string("byte_seek_note", result.byte_seek_note);
  printf(",\"concealed\":%d,\"pts_unknown\":%d,\"api_checks\":%d,"
         "\"mismatches\":%d}\n",
         result.concealed, result.pts_unknown, result.api_checks,
         result.mismatches);

  free(scan);
  free(keys);
  free(full.frames);
  return result.mismatches ? 1 : 0;
}
