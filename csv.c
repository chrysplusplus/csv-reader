#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DA_INIT_CAPACITY 256

#define DA_DECLARE(T) struct { T *items; size_t count, capacity; }

#define da_destroy(da)  \
  do {                  \
    free((da)->items);  \
    (da)->items = NULL; \
    (da)->capacity = 0; \
    (da)->count = 0;    \
  } while (0)

#define da_reserve(da, exp_capacity)                                             \
  do {                                                                           \
    if ((exp_capacity) > (da)->capacity) {                                       \
      if ((da)->capacity == 0)                                                   \
        (da)->capacity = DA_INIT_CAPACITY;                                       \
      while ((exp_capacity) > (da)->capacity)                                    \
        (da)->capacity *= 2;                                                     \
      (da)->items = realloc((da)->items, (da)->capacity * sizeof(*(da)->items)); \
      assert("Out of memory" && (da)->items != NULL);                            \
    }                                                                            \
  } while (0)

#define da_append_n(da, elem, n)                    \
  do {                                              \
    da_reserve((da), (da)->count + (n));            \
    memcpy((da)->items + (da)->count, (elem), (n)); \
    (da)->count += (n);                             \
  } while (0)

#define da_append(da, elem)            \
  do {                                 \
    da_reserve((da), (da)->count + 1); \
    (da)->items[(da)->count] = elem;   \
    ++(da)->count;                     \
  } while (0)                          \

#define da_free(da)                          \
  do {                                       \
    void *ptr_to_free = (void *)(da)->items; \
    (da)->items = NULL;                      \
    (da)->count = 0;                         \
    (da)->capacity = 0;                      \
    free(ptr_to_free);                       \
  } while (0)

#define print_strings_at_offsets_in_contiguous_buffer(offsets, buf, fstring, sep, stream) \
  for (size_t i = 0; i < (offsets)->count; ++i) {                                         \
    fprintf((stream), (fstring), (buf) + (offsets)->items[i]);                            \
    if (i + 1 != (offsets)->count)                                                        \
      fputs((sep), (stream));                                                             \
  }

#define ASSERT_UNREACHABLE assert("Unreachable" && 0)

static const char *SHORT_FLAG_FILTER = "f";
static const char *LONG_FLAG_FILTER  = "filter";
static const char *SHORT_FLAG_LIST   = "l";
static const char *LONG_FLAG_LIST    = "list";
static const char *LONG_NO_HEADER    = "no-header";
static const char *SHORT_NO_HEADER   = "H";
static const char *SHORT_FLAG_HELP   = "h";
static const char *LONG_FLAG_HELP    = "help";

static const char *USAGE = "%s [OPTIONS] file\n\n"
  "Options:\n"
  "    -f, --filter columns : Filter output to specified columns\n"
  "    -l, --list           : List columns in file\n"
  "    -H, --no-header      : Don't print header in output\n"
  "    -h, --help           : Print this message\n";

typedef enum {
  OPT_UNKNOWN, OPT_FILTER, OPT_LIST, OPT_NO_HEADER, OPT_HELP
} ProgramOpt;

typedef enum {
  MODE_FILTER = 0, MODE_LIST
} ProgramMode;

typedef enum {
  FLAG_NORMAL    = 0,
  FLAG_NO_HEADER = 1 << 0
} OutputFlags;

#define PROG_FLAGS_FILTERS_INIT_CAPACITY 256

typedef struct CSV {
  DA_DECLARE(char)    columns_storage;
  DA_DECLARE(char)    data_storage;
  DA_DECLARE(size_t)  columns_offsets;
  DA_DECLARE(size_t)  points_offsets;
} CSV;

typedef struct Program {
  DA_DECLARE(char)    filters;
  DA_DECLARE(size_t)  valid_filter_column_offsets;
  FILE               *file;
  const char         *filepath;
  CSV                *csv;
  const char         *program_name;
  OutputFlags         output_flags;
  ProgramMode         mode;
} Program;

struct IndexDA {
  size_t *items;
  size_t count, capacity;
};

ProgramOpt extract_prog_opt(char *arg) {
  assert(*arg == '-');
  ++arg;
  if (*arg == '-') {
    ++arg;
    if (strcmp(arg, LONG_FLAG_FILTER) == 0)
      return OPT_FILTER;
    if (strcmp(arg, LONG_FLAG_LIST) == 0)
      return OPT_LIST;
    if (strcmp(arg, LONG_NO_HEADER) == 0)
      return OPT_NO_HEADER;
    if (strcmp(arg, LONG_FLAG_HELP) == 0)
      return OPT_HELP;
    return OPT_UNKNOWN;
  }

  if (strcmp(arg, SHORT_FLAG_FILTER) == 0)
    return OPT_FILTER;
  if (strcmp(arg, SHORT_FLAG_LIST) == 0)
    return OPT_LIST;
  if (strcmp(arg, SHORT_NO_HEADER) == 0)
    return OPT_NO_HEADER;
  if (strcmp(arg, SHORT_FLAG_HELP) == 0)
    return OPT_HELP;
  return OPT_UNKNOWN;
}

char *program_expect_filter(Program *, int argc, char **argv) {
  if (argc == 0 || *argv[0] == '-') {
    fputs("Error: Expected argument for --filter\n", stderr);
    return NULL;
  }
  else {
    return *argv;
  }
}

void program_filters_split(Program *program) {
  char *buf = program->filters.items;
  for (size_t i = 0; i < program->filters.count; ++i)
    if (buf[i] == ',')
      buf[i] = '\0';
}

int program_flags_parse(Program *program, int argc, char **argv) {
  program->program_name = *(argv++);
  --argc;

  while (argc > 0) {
    char *cur_arg = *argv;
    if (cur_arg[0] == '-') {
      switch (extract_prog_opt(cur_arg)) {
        case OPT_UNKNOWN:
          fputs("Error: Unknown flag\n", stderr);
          return 1;
        case OPT_FILTER:
          {
            --argc;
            ++argv;
            char *filter = program_expect_filter(program, argc, argv);
            if (filter == NULL)
              return 1;

            da_append_n(&program->filters, filter, strlen(filter) + 1);
            break;
          }
        case OPT_LIST:
          program->mode = MODE_LIST;
          break;
        case OPT_NO_HEADER:
          program->output_flags |= FLAG_NO_HEADER;
          break;
        case OPT_HELP:
          fprintf(stdout, USAGE, program->program_name);
          exit(EXIT_FAILURE);
          break;
        default:
          ASSERT_UNREACHABLE;
          break;
      }
    }
    else if (program->filepath != NULL) {
      fputs("Error: Too many filepaths provided\n", stderr);
      return 1;
    }
    else {
      program->filepath = cur_arg;
    }

    --argc;
    ++argv;
  }

  program_filters_split(program);
  return 0;
}

int program_flags_open_filepath(Program *program) {
  program->file = program->filepath != NULL ? fopen(program->filepath, "r") : stdin;
  return program->file == NULL;
}

int program_close_filepath(Program *program) {
  FILE *file_to_close = program->file;
  program->file = NULL;
  return file_to_close != stdin ? fclose(file_to_close) : 0;
}

void csv_free(CSV *csv) {
  da_free(&csv->columns_storage);
  da_free(&csv->data_storage);
  da_free(&csv->columns_offsets);
  da_free(&csv->points_offsets);
}

void program_free(Program *program) {
  da_free(&program->filters);
  da_free(&program->valid_filter_column_offsets);
  program->file != NULL ? program_close_filepath(program) : 0;
  program->csv != NULL ? csv_free(program->csv) : 0;
}

int csv_parse(CSV *csv, Program *program) {
  enum {
    COLUMNS, UNQUOTED_ROW, QUOTED_ROW, EXPECT_SEPARATOR
  } state = COLUMNS;

  assert(program->file != NULL && "Stream is closed");
  FILE *f = program->file;

  char last_char = '\0';
  size_t byte_count = 0, point_start = 0, target_row_count = 0, row_count = 0, line_count = 1;
  for (int byte = fgetc(f); byte != EOF; byte = fgetc(f)) {
    char ch = (char)byte;
    switch (state) {
      case COLUMNS:
        {
          if (ch == ',') {
            da_append(&csv->columns_storage, '\0');
            da_append(&csv->columns_offsets, point_start);
            point_start = ++byte_count;
            last_char = '\0';
          }
          else if (ch == '\n') {
            da_append(&csv->columns_storage, '\0');
            da_append(&csv->columns_offsets, point_start);
            target_row_count = csv->columns_offsets.count;
            state = UNQUOTED_ROW;
            last_char = '\0';
            point_start = 0;
            byte_count = 0;
            ++line_count;
          }
          else {
            da_append(&csv->columns_storage, ch);
            last_char = ch;
            ++byte_count;
          }
          break;
        }
      case UNQUOTED_ROW:
        {
          if (ch == ',') {
            da_append(&csv->data_storage, '\0');
            da_append(&csv->points_offsets, point_start);
            last_char = '\0';
            point_start = ++byte_count;
            ++row_count;
          }
          else if (ch == '\n') {
            da_append(&csv->data_storage, '\0');
            da_append(&csv->points_offsets, point_start);
            last_char = '\0';
            point_start = ++byte_count;
            ++row_count;
            ++line_count;

            if (row_count != target_row_count) {
              fprintf(stderr, "Error on line %ld: Not enough values, expected %ld, got %ld\n", line_count, target_row_count, row_count);
              return 1;
            }

            row_count = 0;
          }
          else if (ch == '"') {
            state = QUOTED_ROW;
            if (last_char != '\0') {
              fprintf(stderr, "Error on line %ld: Quote in the middle of a value\n", line_count);
              return 1;
            }
          }
          else {
            da_append(&csv->data_storage, ch);
            last_char = ch;
            ++byte_count;
          }
          break;
        }
      case QUOTED_ROW:
        {
          if (ch == '"') {
            state = EXPECT_SEPARATOR;
          }
          else if (ch == '\n') {
            fprintf(stderr, "Error on line %ld: Expected end quote, got end of line\n", line_count);
            return 1;
          }
          else {
            da_append(&csv->data_storage, ch);
            last_char = ch;
            ++byte_count;
          }
          break;
        }
      case EXPECT_SEPARATOR:
        {
          if (ch == ',') {
            da_append(&csv->data_storage, '\0');
            da_append(&csv->points_offsets, point_start);
            state = UNQUOTED_ROW;
            last_char = '\0';
            point_start = ++byte_count;
            ++row_count;
          }
          else if (ch == '\n') {
            da_append(&csv->data_storage, '\0');
            da_append(&csv->points_offsets, point_start);
            last_char = '\0';
            point_start = ++byte_count;
            ++row_count;
            ++line_count;

            if (row_count != target_row_count) {
              fprintf(stderr, "Error on line %ld: Not enough values, expected %ld, got %ld\n", line_count, target_row_count, row_count);
              return 1;
            }

            row_count = 0;
          }
          else {
            fprintf(stderr, "Error on line %ld: Expected separator, got %c\n", line_count, ch);
            return 1;
          }
          break;
        }
      default:
        ASSERT_UNREACHABLE;
        break;
    }
  }

  program->csv = csv;
  return 0;
}

size_t count_contiguous_strings(const char *buf, size_t buf_size) {
  size_t count = 0;
  for (size_t i = 0; i < buf_size; ++i)
    if (buf[i] == '\0')
      ++count;
  return count;
}

void prog_flags_validate_filters(Program *program) {
  CSV *csv = program->csv;
  char *filter = program->filters.items;
  for (size_t n = count_contiguous_strings(filter, program->filters.count); n > 0; --n) {
    size_t last_valid_size = program->valid_filter_column_offsets.count;

    for (size_t i = 0; i < csv->columns_offsets.count; ++i) {
      size_t column_offset = csv->columns_offsets.items[i];
      const char *column = csv->columns_storage.items + column_offset;
      if (strcmp(filter, column) == 0) {
        da_append(&program->valid_filter_column_offsets, column_offset);
        break;
      }
    }

    if (last_valid_size == program->valid_filter_column_offsets.count)
      fprintf(stderr, "Warning: Ignoring unknown filter '%s'\n", filter);

    while (*filter != '\0')
      ++filter;
    ++filter; // at end-of-loop this does go past the buffer, but it never gets dereferenced
  }
}

void print_csv_header(const CSV *csv) {
  const typeof (csv->columns_offsets) *columns_offsets = &csv->columns_offsets;
  const typeof (csv->columns_storage) *columns = &csv->columns_storage;

  const char *ptr = columns->items;
  for (size_t i = 0; i < columns_offsets->count; ++i) {
    fputs(ptr, stdout);
    if (i + 1 != columns_offsets->count)
      fputc('\t', stdout);

    while (*ptr != '\0')
      ++ptr;
    ++ptr;
  }

  fputc('\n', stdout);
}

void print_csv_body(const CSV *csv) {
  const typeof (csv->columns_offsets) *columns_offsets = &csv->columns_offsets;
  const typeof (csv->data_storage) *data = &csv->data_storage;
  const typeof (csv->points_offsets) *points = &csv->points_offsets;

  const char *ptr = data->items;
  size_t record_count = points->count / columns_offsets->count;
  for (size_t record = 0; record < record_count; ++record) {
    for (size_t i = 0; i < columns_offsets->count; ++i) {
      fputs(ptr, stdout);
      if (i + 1 != columns_offsets->count) {
        fputc('\t', stdout);
      }

      while (*ptr != '\0')
        ++ptr;
      ++ptr;
    }

    fputc('\n', stdout);
  }
}

void populate_filter_indices(struct IndexDA *indices, const Program *program) {
  const CSV *csv = program->csv;
  const typeof (program->valid_filter_column_offsets) *filters = &program->valid_filter_column_offsets;
  const typeof (csv->columns_offsets) *offsets = &csv->columns_offsets;

  for (const size_t *off = filters->items; off < filters->items + filters->count; ++off)
    for (size_t i = 0; i < offsets->count; ++i)
      if (*off == offsets->items[i])
        da_append(indices, i);
}

void print_filtered_csv_header(const CSV *csv, const size_t *indices, size_t count) {
  const typeof (csv->columns_offsets) *offsets = &csv->columns_offsets;
  const typeof (csv->columns_storage) *columns = &csv->columns_storage;

  const size_t *end = indices + count;
  const char *last_str = NULL;
  for (const size_t *it = indices; it != end; ++it) {
    const char *str = columns->items + offsets->items[*it];
    if (last_str != NULL)
      fputc('\t', stdout);

    fputs(str, stdout);
    last_str = str;
  }

  fputc('\n', stdout);
}

void print_filtered_csv_body(const CSV *csv, const size_t *indices, size_t count) {
  const typeof (csv->columns_offsets) *columns = &csv->columns_offsets;
  const typeof (csv->data_storage) *data = &csv->data_storage;
  const typeof (csv->points_offsets) *points = &csv->points_offsets;

  size_t record_count = points->count / columns->count;
  const size_t *end = indices + count;
  for (size_t record = 0; record < record_count; ++record) {
    const char *last_str = NULL;
    for (const size_t *it = indices; it != end; ++it) {
      const char *str = data->items + points->items[record * columns->count + *it];
      if (last_str != NULL)
        fputc('\t', stdout);

      fputs(str, stdout);
      last_str = str;
    }
    fputc('\n', stdout);
  }
}

void print_csv_data(const Program *program) {
  struct IndexDA indices = {0};
  populate_filter_indices(&indices, program);

  if (indices.count != 0 && !(program->output_flags & FLAG_NO_HEADER)) {
    print_filtered_csv_header(program->csv, indices.items, indices.count);
    print_filtered_csv_body(program->csv, indices.items, indices.count);
  }
  else if (indices.count != 0) {
    print_filtered_csv_body(program->csv, indices.items, indices.count);
  }
  else if (!(program->output_flags & FLAG_NO_HEADER)) {
    print_csv_header(program->csv);
    print_csv_body(program->csv);
  }
  else {
    print_csv_body(program->csv);
  }
}

void print_csv_columns(const CSV *csv) {
  const char *buf_columns = csv->columns_storage.items;
  print_strings_at_offsets_in_contiguous_buffer(&csv->columns_offsets, buf_columns, "%s", "\n", stdout);
  fputc('\n', stdout);
}

void run_program_mode(const Program *program) {
  assert(program->csv != NULL);
  const CSV *csv = program->csv;
  switch (program->mode) {
    case MODE_LIST:
      print_csv_columns(csv);
      break;
    case MODE_FILTER:
      print_csv_data(program);
      break;
    default:
      ASSERT_UNREACHABLE;
      break;
  }
}

int main(int argc, char **argv) {
  Program program = {0};
  if (program_flags_parse(&program, argc, argv)) {
    fputs("Error parsing program\n", stderr);
    return EXIT_FAILURE;
  }

  if (program_flags_open_filepath(&program)) {
    fprintf(stderr, "Error opening file: %s\n", program.filepath);
    return EXIT_FAILURE;
  }

  CSV csv = {0};
  if (csv_parse(&csv, &program)) {
    fputs("Error parsing csv\n", stderr);
    return EXIT_FAILURE;
  }

  prog_flags_validate_filters(&program);
  run_program_mode(&program);

  if (program_close_filepath(&program)) {
    fputs("There was an error closing the file\n", stderr);
    return EXIT_FAILURE;
  }

  program_free(&program);
}
