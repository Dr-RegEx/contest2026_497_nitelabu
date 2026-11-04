/****************************************************************************
 * apps/examples/s31fsbench/s31fsbench_main.c
 * SPDX-License-Identifier: Apache-2.0
 ****************************************************************************/

#include <nuttx/config.h>

#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#define RECORDS 200
#define BLOCKS 200
#define READS 1000
#define WRITES 10

struct fs_diag_s;

#ifdef CONFIG_ESP32S31_FLASH_DIAGNOSTICS
#  define DIAG_SAMPLES 6
#  define DIAG_TEXT 8192

struct fs_diag_s
{
  char *text;
  int64_t sample_us[DIAG_SAMPLES];
  int64_t phase_us[DIAG_SAMPLES - 1];
  unsigned int count;
};
#endif

struct fs_record_s
{
  int32_t number;
  char str[16];
};

_Static_assert(sizeof(struct fs_record_s) == 20, "20-byte test record");

static void record_make(struct fs_record_s *record, unsigned int index,
                        int replaced)
{
  memset(record, 0, sizeof(*record));
  record->number = replaced ? -1 : (int32_t)index;
  if (replaced)
    {
      strcpy(record->str, "XXXXXXXXXX-0000");
    }
  else
    {
      snprintf(record->str, sizeof(record->str), "ABCDEFGHIJ-%4u",
               index % RECORDS);
    }
}

static unsigned int random_index(unsigned int iteration)
{
  srand(iteration);
  return (unsigned int)rand() % RECORDS;
}

static int64_t now_us(void)
{
  struct timespec ts;

  if (clock_gettime(CLOCK_MONOTONIC, &ts) < 0)
    {
      return -1;
    }

  return (int64_t)ts.tv_sec * 1000000 + ts.tv_nsec / 1000;
}

#ifdef CONFIG_ESP32S31_FLASH_DIAGNOSTICS
static int diag_sample(struct fs_diag_s *diag)
{
  int64_t begin = now_us();
  char *text;
  ssize_t length;
  int fd;

  if (begin < 0 || diag->count >= DIAG_SAMPLES)
    {
      return -1;
    }

  text = diag->text + diag->count * DIAG_TEXT;
  fd = open("/dev/s31stat", O_RDONLY);
  if (fd < 0)
    {
      return -1;
    }

  /* One read per open: s31stat formats a fresh report on every read.
   * Save all six texts and parse/print only after the workload. This costs
   * 48 KiB of diagnostic-only heap but avoids parsing/UART between phases.
   */

  length = read(fd, text, DIAG_TEXT - 1);
  close(fd);
  diag->sample_us[diag->count] = now_us() - begin;
  if (length <= 0 || length >= DIAG_TEXT - 1 ||
      diag->sample_us[diag->count] < 0)
    {
      return -1;
    }

  text[length] = '\0';
  diag->count++;
  return 0;
}

static int diag_report(const struct fs_diag_s *diag)
{
  static const char *names[] =
    {
      "setup_200_flush_sync", "random_read_1000",
      "random_write_10_before_final_sync", "final_flush_sync", "full_verify"
    };
  uint64_t previous_end = 0;

  if (diag->count != DIAG_SAMPLES)
    {
      return -1;
    }

  puts("FSBENCH diagnostic hw_gap excludes formatter intervals; includes "
       "read-return/open-close/clock-call overhead and scheduling; "
       "mono_work excludes status reads; sampling can repay tick debt");
  for (unsigned int i = 0; i < DIAG_SAMPLES; i++)
    {
      const char *text = diag->text + i * DIAG_TEXT;
      const char *sample = strstr(text, "S31SAMPLE: ");
      uint64_t begin;
      uint64_t end;

      if (strstr(text, "S31STAT: truncated=1") != NULL ||
          strstr(text, "S31TIME: valid=1 ") == NULL || sample == NULL ||
          sscanf(sample, "S31SAMPLE: begin_us=%" SCNu64 " end_us=%" SCNu64,
                 &begin, &end) != 2 || end < begin ||
          (i > 0 && begin < previous_end))
        {
          fprintf(stderr, "FSBENCH invalid diagnostic snapshot=%u\n", i);
          return -1;
        }

      if (i > 0)
        {
          printf("FSBENCH diag_phase=%s mono_work_us=%" PRIi64
                 " hw_gap_us=%" PRIu64 "\n", names[i - 1],
                 diag->phase_us[i - 1], begin - previous_end);
        }

      printf("FSBENCH snapshot=%u mono_read_us=%" PRIi64
             " hw_format_us=%" PRIu64 "\n",
             i, diag->sample_us[i], end - begin);
      for (const char *line = text; *line != '\0'; )
        {
          const char *next = strchr(line, '\n');
          if (next == NULL)
            {
              return -1;
            }

          if (strncmp(line, "S31TIME:", 8) == 0 ||
              strncmp(line, "FLASH", 5) == 0)
            {
              fwrite(line, 1, next + 1 - line, stdout);
            }

          line = next + 1;
        }

      previous_end = end;
    }

  return 0;
}
#endif

static int phase(const char *name, int64_t *start, struct fs_diag_s *diag)
{
  int64_t end = now_us();

  if (end < 0 || end < *start)
    {
      return -1;
    }

#ifdef CONFIG_ESP32S31_FLASH_DIAGNOSTICS
  if (diag != NULL)
    {
      if (diag->count == 0 || diag->count >= DIAG_SAMPLES)
        {
          return -1;
        }

      diag->phase_us[diag->count - 1] = end - *start;
      if (diag_sample(diag) < 0)
        {
          return -1;
        }

      *start = now_us();
      return *start < 0 ? -1 : 0;
    }
#else
  (void)diag;
#endif

  printf("FSBENCH phase=%s elapsed_us=%" PRIi64 "\n", name, end - *start);
  *start = end;
  return 0;
}

static int verify(FILE *file, int quiet)
{
  struct fs_record_s buffer[RECORDS];
  struct fs_record_s expected;
  struct stat st;
  unsigned int changed[WRITES];

  if (fstat(fileno(file), &st) < 0 ||
      st.st_size != (off_t)(RECORDS * BLOCKS * sizeof(expected)) ||
      fseek(file, 0, SEEK_SET) != 0)
    {
      fprintf(stderr, "FSBENCH invalid size or seek\n");
      return -1;
    }

  for (unsigned int i = 0; i < WRITES; i++)
    {
      changed[i] = random_index(i);
    }

  for (unsigned int b = 0; b < BLOCKS; b++)
    {
      if (fread(buffer, sizeof(buffer), 1, file) != 1)
        {
          return -1;
        }

      for (unsigned int i = 0; i < RECORDS; i++)
        {
          int replaced = 0;
          for (unsigned int j = 0; b == 0 && j < WRITES; j++)
            {
              replaced |= changed[j] == i;
            }

          record_make(&expected, i, replaced);
          if (memcmp(&buffer[i], &expected, sizeof(expected)) != 0)
            {
              fprintf(stderr, "FSBENCH mismatch block=%u record=%u\n", b, i);
              return -1;
            }
        }
    }

  if (!quiet)
    {
      printf("FSBENCH verified_bytes=800000 records=40000\n");
    }
  return 0;
}

static int run(FILE *file, struct fs_diag_s *diag)
{
  struct fs_record_s block[RECORDS];
  struct fs_record_s value;
  struct fs_record_s expected;
  int64_t start = now_us();

  if (start < 0)
    {
      return -1;
    }

#ifdef CONFIG_ESP32S31_FLASH_DIAGNOSTICS
  if (diag != NULL)
    {
      if (diag_sample(diag) < 0 || (start = now_us()) < 0)
        {
          return -1;
        }
    }
#endif

  for (unsigned int i = 0; i < RECORDS; i++)
    {
      record_make(&block[i], i, 0);
    }

  for (unsigned int b = 0; b < BLOCKS; b++)
    {
      if (fwrite(block, sizeof(block), 1, file) != 1 ||
          fflush(file) != 0 || fsync(fileno(file)) < 0)
        {
          return -1;
        }
    }

  if (phase("setup_200_flush_sync", &start, diag) < 0)
    {
      return -1;
    }

  for (unsigned int i = 0; i < READS; i++)
    {
      unsigned int index = random_index(i);
      record_make(&expected, index, 0);
      if (fseek(file, index * sizeof(value), SEEK_SET) != 0 ||
          fread(&value, sizeof(value), 1, file) != 1 ||
          memcmp(&value, &expected, sizeof(value)) != 0)
        {
          return -1;
        }
    }

  if (phase("random_read_1000", &start, diag) < 0)
    {
      return -1;
    }

  record_make(&value, 0, 1);
  for (unsigned int i = 0; i < WRITES; i++)
    {
      unsigned int index = random_index(i);
      if (fseek(file, index * sizeof(value), SEEK_SET) != 0 ||
          fwrite(&value, sizeof(value), 1, file) != 1)
        {
          return -1;
        }
    }

  if (phase("random_write_10_before_final_sync", &start, diag) < 0 ||
      fflush(file) != 0 || fsync(fileno(file)) < 0 ||
      phase("final_flush_sync", &start, diag) < 0)
    {
      return -1;
    }

  if (verify(file, diag != NULL) < 0)
    {
      return -1;
    }

  return diag == NULL ? 0 : phase("full_verify", &start, diag);
}

#include "cow_workload.inc"

#include "readpath.inc"

#include "qualification.inc"

#include "bulkread.inc"

int main(int argc, char **argv)
{
  if (argc == 3 && strcmp(argv[1], "bulkread") == 0)
    {
      return qb_main(argv[2]);
    }

  if (argc >= 2 && (strcmp(argv[1], "qualify") == 0 ||
                    strcmp(argv[1], "qualify-verify") == 0 ||
                    strcmp(argv[1], "qualify-clean") == 0 ||
                    strcmp(argv[1], "streams") == 0 ||
                    strcmp(argv[1], "streams-verify") == 0 ||
                    strcmp(argv[1], "streams-clean") == 0))
    {
      return q_main(argc, argv);
    }

#ifdef CONFIG_ESP32S31_FLASH_DIAGNOSTICS
  if (argc == 3 && (strcmp(argv[1], "readpath") == 0 ||
                    strcmp(argv[1], "readpath-extra") == 0 ||
                    strcmp(argv[1], "readpath-verify") == 0 ||
                    strcmp(argv[1], "readpath-clean") == 0))
    {
      return rp_main(argv[1], argv[2]);
    }
#endif

  if (argc == 3 && (strcmp(argv[1], "cowrun") == 0 ||
                    strcmp(argv[1], "cowverify") == 0))
    {
      return cow_main(argv[2], strcmp(argv[1], "cowrun") == 0);
    }

  char path[256];
  FILE *file;
  int fd;
  int result;
  int length;
  int diagnostic = 0;
  int create;
  struct fs_diag_s *diag = NULL;

#ifdef CONFIG_ESP32S31_FLASH_DIAGNOSTICS
  struct fs_diag_s state = {0};
  diagnostic = argc == 3 && strcmp(argv[1], "diag") == 0;
#endif

  if (argc != 3 || (strcmp(argv[1], "run") != 0 &&
                    strcmp(argv[1], "verify") != 0 && !diagnostic) ||
      argv[2][0] != '/')
    {
      fprintf(stderr, "Usage: s31fsbench run|verify /existing-directory\n");
#ifdef CONFIG_ESP32S31_FLASH_DIAGNOSTICS
      fprintf(stderr, "Diagnostic: s31fsbench diag /existing-directory\n");
#endif
      return EXIT_FAILURE;
    }

  length = snprintf(path, sizeof(path), "%s/safe-random.bin", argv[2]);
  if (length < 0 || (size_t)length >= sizeof(path))
    {
      return EXIT_FAILURE;
    }

  create = strcmp(argv[1], "run") == 0 || diagnostic;
  fd = open(path, create ?
            O_CREAT | O_EXCL | O_RDWR : O_RDONLY, 0600);
  if (fd < 0)
    {
      perror("s31fsbench: open");
      return EXIT_FAILURE;
    }

  file = fdopen(fd, create ? "wb+" : "rb");
  if (file == NULL)
    {
      close(fd);
      return EXIT_FAILURE;
    }

#ifdef CONFIG_ESP32S31_FLASH_DIAGNOSTICS
  if (diagnostic)
    {
      state.text = malloc(DIAG_SAMPLES * DIAG_TEXT);
      if (state.text == NULL)
        {
          fclose(file);
          return EXIT_FAILURE;
        }

      diag = &state;
    }
#endif

  printf("FSBENCH mode=%s file=%s bytes=800000 read_count=1000 write_count=10\n",
         argv[1], path);
  result = create ? run(file, diag) : verify(file, 0);
  if (fclose(file) != 0)
    {
      result = -1;
    }

#ifdef CONFIG_ESP32S31_FLASH_DIAGNOSTICS
  if (diagnostic)
    {
      if (result == 0)
        {
          result = diag_report(diag);
        }

      free(state.text);
    }
#endif

  if (result < 0)
    {
      fprintf(stderr, "FSBENCH failed errno=%d\n", errno);
      return EXIT_FAILURE;
    }

  puts("FSBENCH result=PASS");
  return EXIT_SUCCESS;
}
