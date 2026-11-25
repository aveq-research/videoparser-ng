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
 *    timestamps of later frames in the same GOP, then decodes at least up to
 *    the next key frame and compares index, timestamp and MD5 of each frame.
 * 5. Does the same with byte positions from the scan.
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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

typedef struct frame_record {
  int32_t frame_idx;
  int64_t pts_raw;
  int32_t is_idr;
  int32_t frame_type;
  int32_t decode_error;
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
  int pts_unsupported;
  int concealed;
  int pts_unknown;
  int byte_seeks;
  int byte_frames;
  const char *pts_seek_note;
  const char *byte_seek_note;
} counts;

static const test_options *opts;
static counts result;
/* Whether the input has no timestamps of its own (raw bitstreams) */
static int no_timestamps;

static void mismatch(const char *format, ...) {
  result.mismatches++;
  if (opts->verbose || result.mismatches <= 10) {
    va_list args;
    va_start(args, format);
    fprintf(stderr, "mismatch: ");
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
                           .frame_type = frame.frame_type,
                           .decode_error = frame.decode_error};
    picture_md5(parser, record.md5);
    add_frame(list, &record);
  }
  if (status != VP_END) {
    die("vp_next_frame", status);
  }
}

/* Index of the first key frame after `idx` in the first pass, or the frame
 * count */
static size_t next_keyframe_after(const frame_list *full, size_t idx) {
  for (size_t i = idx + 1; i < full->count; i++) {
    if (full->frames[i].is_idr) {
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
    if (expected->is_idr && i != key) {
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
        frame.frame_type != expected->frame_type || !same_picture) {
      mismatch("%s seek to %lld (frame %zu): frame %zu differs: idx %d/%d "
               "pts %lld/%lld idr %d/%d md5 %s",
               kind, (long long)target, key, i, frame.frame_idx,
               expected->frame_idx, (long long)frame.pts_raw,
               (long long)expected->pts_raw, frame.is_idr, expected->is_idr,
               same_picture ? "same" : "differs");
      return compared;
    }
    compared++;
  }
  return compared;
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
    if (!f->is_idr || f->pts_raw == VP_NOPTS) {
      continue;
    }
    if (last != VP_NOPTS && f->pts_raw <= last) {
      return 0;
    }
    last = f->pts_raw;
  }
  return 1;
}

/* Whether all frame timestamps of the GOP after the key frame `key` are later
 * than its own and earlier than that of the next key frame, so that they
 * select this key frame */
static int frame_selects_key(const frame_list *full, size_t key, size_t i) {
  size_t next = next_keyframe_after(full, key);
  int64_t pts = full->frames[i].pts_raw;
  if (pts == VP_NOPTS || pts < full->frames[key].pts_raw) {
    return 0;
  }
  return next >= full->count || pts < full->frames[next].pts_raw;
}

static void test_pts_seeks(const frame_list *full, size_t *keys,
                           size_t key_count) {
  opened in;
  open_input(&in, 0, 0);
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

  /* Key frames without a timestamp cannot be targets, and may make a seek
   * ambiguous, which the parser reports as unsupported */
  int keys_without_pts = 0;
  for (size_t k = 0; k < key_count; k++) {
    keys_without_pts += full->frames[keys[k]].pts_raw == VP_NOPTS;
  }
  size_t seeks = 0;
  for (size_t k = 0; k < key_count && seeks < (size_t)opts->max_seeks; k++) {
    size_t key = keys[k];
    if (full->frames[key].pts_raw == VP_NOPTS) {
      continue;
    }
    seeks++;
    size_t s = seeks - 1;
    /* Alternately the timestamp of the key frame and of a later frame of its
     * GOP */
    int64_t target = full->frames[key].pts_raw;
    if (s % 2 == 1) {
      size_t next = next_keyframe_after(full, key);
      if (next > key + 1) {
        size_t i = key + 1 + (size_t)rand() % (next - key - 1);
        if (frame_selects_key(full, key, i)) {
          target = full->frames[i].pts_raw;
        }
      }
    }
    vp_status status = vp_seek(parser, target, full->frames[key].frame_idx);
    if (status == VP_ERROR_UNSUPPORTED && keys_without_pts > 0) {
      result.pts_unsupported++;
      continue;
    }
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
      if (full->frames[keys[k]].pts_raw != VP_NOPTS &&
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
  open_input(&in, 0, 0);
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
    if (full.frames[i].is_idr) {
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
        a->is_idr != b->is_idr || a->frame_type != b->frame_type ||
        memcmp(a->md5, b->md5, 16) != 0) {
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

  /* 4. Seeks by timestamp, 5. by byte position */
  if (key_count > 0) {
    shuffle(keys, key_count);
    test_pts_seeks(&full, keys, key_count);
    test_byte_seeks(&full, scan,
                    scan_count < key_count ? scan_count : key_count);
  }

  printf("{\"file\":\"%s\",\"frames\":%d,\"keyframes\":%d,"
         "\"decode_only_frames\":%d,\"scan_keyframes\":%d,"
         "\"scan_unknown_idx\":%d,\"pts_seeks\":%d,\"pts_frames\":%d,"
         "\"pts_unsupported\":%d,\"byte_seeks\":%d,\"byte_frames\":%d",
         options.filename, result.frames, result.keyframes,
         result.decode_only_frames, result.scan_keyframes,
         result.scan_unknown_idx, result.pts_seeks, result.pts_frames,
         result.pts_unsupported, result.byte_seeks, result.byte_frames);
  print_json_string("pts_seek_note", result.pts_seek_note);
  print_json_string("byte_seek_note", result.byte_seek_note);
  printf(",\"concealed\":%d,\"pts_unknown\":%d,\"mismatches\":%d}\n",
         result.concealed, result.pts_unknown, result.mismatches);

  free(scan);
  free(keys);
  free(full.frames);
  return result.mismatches ? 1 : 0;
}
