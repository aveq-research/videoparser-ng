/**
 * @file videoparser-c-test.c
 * @brief Test program for the C API: writes the same NDJSON as video-parser
 *
 * Uses only videoparser_c.h. Compiled as C11.
 *
 * Usage: videoparser-c-test [options] <file>
 *
 *   -n <frames>        parse only the first n frames (as the CLI)
 *   --io               read the file through the custom input callbacks
 *   --io-no-seek       as --io, without a seek callback
 *   --read-size <n>    return at most n bytes per read callback
 *   --format <name>    FFmpeg demuxer name
 *   --stream <index>   index of the video stream
 *   --no-scan          do not read the packets before decoding
 *   --raw <file>       write the decoded pictures as raw video to a file
 *   --log-callback     route the log lines through the log callback
 *   --all-frames       also return frames without statistics
 *
 * @copyright Copyright (c) 2026, AVEQ GmbH. Copyright (c) 2026,
 * videoparser-ng contributors.
 */

#define _POSIX_C_SOURCE 200809L

#include "videoparser_c.h"
#include "json_double.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

/* JSON object with keys sorted as nlohmann::json sorts them */

#define MAX_FIELDS 40

typedef struct json_field {
  const char *key;
  char value[96];
} json_field;

typedef struct json_object {
  json_field fields[MAX_FIELDS];
  int count;
} json_object;

static json_field *add_field(json_object *obj, const char *key) {
  if (obj->count >= MAX_FIELDS) {
    fprintf(stderr, "Error: too many JSON fields\n");
    exit(EXIT_FAILURE);
  }
  json_field *field = &obj->fields[obj->count++];
  field->key = key;
  return field;
}

static void add_double(json_object *obj, const char *key, double value) {
  json_format_double(add_field(obj, key)->value, value);
}

static void add_int(json_object *obj, const char *key, int64_t value) {
  json_field *field = add_field(obj, key);
  snprintf(field->value, sizeof(field->value), "%lld", (long long)value);
}

static void add_uint(json_object *obj, const char *key, uint64_t value) {
  json_field *field = add_field(obj, key);
  snprintf(field->value, sizeof(field->value), "%llu",
           (unsigned long long)value);
}

static void add_bool(json_object *obj, const char *key, int value) {
  strcpy(add_field(obj, key)->value, value ? "true" : "false");
}

static void add_string(json_object *obj, const char *key, const char *value) {
  json_field *field = add_field(obj, key);
  char *out = field->value;
  char *end = field->value + sizeof(field->value) - 8;
  *out++ = '"';
  for (const unsigned char *c = (const unsigned char *)value; *c && out < end;
       c++) {
    switch (*c) {
    case '"':
      *out++ = '\\';
      *out++ = '"';
      break;
    case '\\':
      *out++ = '\\';
      *out++ = '\\';
      break;
    case '\b':
      *out++ = '\\';
      *out++ = 'b';
      break;
    case '\f':
      *out++ = '\\';
      *out++ = 'f';
      break;
    case '\n':
      *out++ = '\\';
      *out++ = 'n';
      break;
    case '\r':
      *out++ = '\\';
      *out++ = 'r';
      break;
    case '\t':
      *out++ = '\\';
      *out++ = 't';
      break;
    default:
      if (*c < 0x20) {
        out += sprintf(out, "\\u%04x", *c);
      } else {
        *out++ = (char)*c;
      }
    }
  }
  *out++ = '"';
  *out = '\0';
}

static int compare_fields(const void *a, const void *b) {
  return strcmp(((const json_field *)a)->key, ((const json_field *)b)->key);
}

static void print_object(json_object *obj) {
  qsort(obj->fields, (size_t)obj->count, sizeof(json_field), compare_fields);
  putchar('{');
  for (int i = 0; i < obj->count; i++) {
    printf("%s\"%s\":%s", i ? "," : "", obj->fields[i].key,
           obj->fields[i].value);
  }
  printf("}\n");
}

static void print_sequence_info(const vp_sequence_info *info) {
  json_object obj = {0};
  add_string(&obj, "type", "sequence_info");
  add_double(&obj, "video_duration", info->video_duration);
  add_string(&obj, "video_codec", info->video_codec);
  add_double(&obj, "video_bitrate", info->video_bitrate);
  add_double(&obj, "video_framerate", info->video_framerate);
  add_int(&obj, "video_width", info->video_width);
  add_int(&obj, "video_height", info->video_height);
  add_int(&obj, "video_codec_profile", info->video_codec_profile);
  add_int(&obj, "video_codec_level", info->video_codec_level);
  add_int(&obj, "video_bit_depth", info->video_bit_depth);
  add_string(&obj, "video_pix_fmt", info->video_pix_fmt);
  add_uint(&obj, "video_frame_count", info->video_frame_count);
  print_object(&obj);
}

static void print_frame_info(const vp_frame_info *frame) {
  json_object obj = {0};
  add_string(&obj, "type", "frame_info");
  add_int(&obj, "frame_idx", frame->frame_idx);
  add_double(&obj, "dts", frame->dts);
  add_double(&obj, "pts", frame->pts);
  add_int(&obj, "size", frame->size);
  add_int(&obj, "frame_type", frame->frame_type);
  add_bool(&obj, "is_idr", frame->is_idr);
  add_bool(&obj, "decode_error", frame->decode_error);
  add_bool(&obj, "discontinuity", frame->discontinuity);
  add_uint(&obj, "qp_min", frame->qp_min);
  add_uint(&obj, "qp_max", frame->qp_max);
  add_uint(&obj, "qp_init", frame->qp_init);
  add_double(&obj, "qp_avg", frame->qp_avg);
  add_double(&obj, "qp_stdev", frame->qp_stdev);
  add_double(&obj, "qp_bb_avg", frame->qp_bb_avg);
  add_double(&obj, "qp_bb_stdev", frame->qp_bb_stdev);
  add_double(&obj, "motion_avg", frame->motion_avg);
  add_double(&obj, "motion_stdev", frame->motion_stdev);
  add_double(&obj, "motion_x_avg", frame->motion_x_avg);
  add_double(&obj, "motion_y_avg", frame->motion_y_avg);
  add_double(&obj, "motion_x_stdev", frame->motion_x_stdev);
  add_double(&obj, "motion_y_stdev", frame->motion_y_stdev);
  add_double(&obj, "motion_diff_avg", frame->motion_diff_avg);
  add_double(&obj, "motion_diff_stdev", frame->motion_diff_stdev);
  add_int(&obj, "current_poc", frame->current_poc);
  add_int(&obj, "poc_diff", frame->poc_diff);
  add_uint(&obj, "motion_bit_count", frame->motion_bit_count);
  add_uint(&obj, "coefs_bit_count", frame->coefs_bit_count);
  add_int(&obj, "mb_mv_count", frame->mb_mv_count);
  add_int(&obj, "mv_coded_count", frame->mv_coded_count);
  print_object(&obj);
}

static void print_summary(const vp_summary *summary) {
  json_object obj = {0};
  add_string(&obj, "type", "summary");
  add_uint(&obj, "frame_count", summary->frame_count);
  add_uint(&obj, "decode_errors", summary->decode_errors);
  add_uint(&obj, "corrupt_packets", summary->corrupt_packets);
  add_uint(&obj, "discontinuities", summary->discontinuities);
  print_object(&obj);
}

/* Custom input from a file */

typedef struct file_input {
  FILE *file;
  int32_t read_size; /* at most this many bytes per read; 0 for no limit */
} file_input;

static int32_t file_read(void *opaque, uint8_t *buf, int32_t size) {
  file_input *input = opaque;
  if (input->read_size > 0 && size > input->read_size) {
    size = input->read_size;
  }
  size_t n = fread(buf, 1, (size_t)size, input->file);
  if (n == 0 && ferror(input->file)) {
    return -1;
  }
  return (int32_t)n;
}

static int64_t file_seek(void *opaque, int64_t offset, int32_t whence) {
  file_input *input = opaque;
  if (whence == VP_SEEK_SIZE) {
    struct stat st;
    if (fstat(fileno(input->file), &st) != 0) {
      return -1;
    }
    return (int64_t)st.st_size;
  }
  if (fseeko(input->file, (off_t)offset, whence) != 0) {
    return -1;
  }
  clearerr(input->file);
  return (int64_t)ftello(input->file);
}

/* Write the picture data without padding */
static int write_picture(vp_parser *parser, FILE *out) {
  vp_picture picture = {.struct_size = sizeof(picture)};
  vp_status status = vp_get_picture(parser, &picture);
  if (status != VP_OK) {
    fprintf(stderr, "Error: %s\n", vp_last_error());
    return -1;
  }
  for (int p = 0; p < picture.nb_planes; p++) {
    const uint8_t *row = picture.data[p];
    for (int y = 0; y < picture.plane_height[p]; y++) {
      if (fwrite(row, 1, (size_t)picture.row_bytes[p], out) !=
          (size_t)picture.row_bytes[p]) {
        fprintf(stderr, "Error: cannot write the picture\n");
        return -1;
      }
      row += picture.linesize[p];
    }
  }
  return 0;
}

static void log_to_stderr(void *user_data, int32_t level, const char *line) {
  (void)user_data;
  fprintf(stderr, "[log %d] %s", (int)level, line);
}

static void usage(void) {
  fprintf(stderr,
          "Usage: videoparser-c-test [-n <frames>] [--io | --io-no-seek] "
          "[--read-size <n>] [--format <name>] [--stream <index>] "
          "[--no-scan] [--raw <file>] [--log-callback] [--all-frames] "
          "<file>\n");
}

int main(int argc, char *argv[]) {
  const char *filename = NULL;
  const char *raw_path = NULL;
  int use_io = 0;
  int io_seek = 1;
  int log_callback = 0;
  int32_t read_size = 0;
  vp_options options;
  vp_options_init(&options);

  for (int i = 1; i < argc; i++) {
    const char *arg = argv[i];
    int has_value = i + 1 < argc;
    if (strcmp(arg, "-n") == 0 && has_value) {
      long long n = strtoll(argv[++i], NULL, 10);
      options.max_frames = n < 0 ? -1 : n;
    } else if (strcmp(arg, "--io") == 0) {
      use_io = 1;
    } else if (strcmp(arg, "--io-no-seek") == 0) {
      use_io = 1;
      io_seek = 0;
    } else if (strcmp(arg, "--read-size") == 0 && has_value) {
      read_size = (int32_t)strtol(argv[++i], NULL, 10);
    } else if (strcmp(arg, "--format") == 0 && has_value) {
      options.input_format = argv[++i];
    } else if (strcmp(arg, "--stream") == 0 && has_value) {
      options.stream_index = (int32_t)strtol(argv[++i], NULL, 10);
    } else if (strcmp(arg, "--no-scan") == 0) {
      options.scan = VP_SCAN_OFF;
    } else if (strcmp(arg, "--raw") == 0 && has_value) {
      raw_path = argv[++i];
    } else if (strcmp(arg, "--all-frames") == 0) {
      options.frames_without_statistics = 1;
    } else if (strcmp(arg, "--log-callback") == 0) {
      log_callback = 1;
    } else if (arg[0] == '-' && arg[1] != '\0') {
      usage();
      return EXIT_FAILURE;
    } else {
      filename = arg;
    }
  }
  if (!filename) {
    usage();
    return EXIT_FAILURE;
  }

  uint32_t api_version = vp_api_version();
  if ((int)(api_version >> 16) != VP_API_VERSION_MAJOR ||
      (int)(api_version & 0xffff) < VP_API_VERSION_MINOR) {
    fprintf(stderr, "Error: C API version mismatch\n");
    return EXIT_FAILURE;
  }
  if (log_callback) {
    vp_set_log_callback(log_to_stderr, NULL);
  }

  FILE *raw = NULL;
  if (raw_path) {
    raw = fopen(raw_path, "wb");
    if (!raw) {
      fprintf(stderr, "Error: cannot open %s: %s\n", raw_path,
              strerror(errno));
      return EXIT_FAILURE;
    }
  }

  file_input input = {0};
  vp_parser *parser = NULL;
  vp_status status;
  if (use_io) {
    input.file = fopen(filename, "rb");
    if (!input.file) {
      fprintf(stderr, "Error: File '%s' does not exist\n", filename);
      if (raw)
        fclose(raw);
      return EXIT_FAILURE;
    }
    input.read_size = read_size;
    vp_io io = {.struct_size = sizeof(io),
                .read = file_read,
                .seek = io_seek ? file_seek : NULL,
                .opaque = &input};
    status = vp_open_io(&io, &options, &parser);
  } else {
    status = vp_open_file(filename, &options, &parser);
  }

  int exit_code = EXIT_SUCCESS;
  if (status != VP_OK) {
    fprintf(stderr, "Error: %s (%s)\n", vp_last_error(),
            vp_status_string(status));
    exit_code = EXIT_FAILURE;
    goto end;
  }

  vp_sequence_info info = {.struct_size = sizeof(info)};
  status = vp_get_sequence_info(parser, &info);
  if (status != VP_OK) {
    fprintf(stderr, "Error: %s\n", vp_last_error());
    exit_code = EXIT_FAILURE;
    goto end;
  }
  print_sequence_info(&info);

  vp_frame_info frame = {.struct_size = sizeof(frame)};
  while ((status = vp_next_frame(parser, &frame)) == VP_OK) {
    print_frame_info(&frame);
    if (raw && write_picture(parser, raw) != 0) {
      exit_code = EXIT_FAILURE;
      goto end;
    }
  }
  if (status != VP_END) {
    fprintf(stderr, "Error: %s (%s)\n", vp_last_error(),
            vp_status_string(status));
    exit_code = EXIT_FAILURE;
    goto end;
  }

  vp_summary summary = {.struct_size = sizeof(summary)};
  status = vp_get_summary(parser, &summary);
  if (status != VP_OK) {
    fprintf(stderr, "Error: %s\n", vp_last_error());
    exit_code = EXIT_FAILURE;
    goto end;
  }
  print_summary(&summary);

end:
  vp_close(parser);
  if (input.file)
    fclose(input.file);
  if (raw && fclose(raw) != 0) {
    fprintf(stderr, "Error: cannot write %s\n", raw_path);
    exit_code = EXIT_FAILURE;
  }
  fflush(stdout);
  return exit_code;
}
