/****************************************************************************
 * Diagnostic-only E70 small-read experiment. SPDX-License-Identifier: Apache-2.0
 * Included by the frozen s31fsbench; original numbered test stays untouched.
 ****************************************************************************/

#ifdef CONFIG_ESP32S31_FLASH_DIAGNOSTICS
#define RP_INTERVALS 9
#define RP_OWNER "E70 stdio seek diagnostic v1\n"

struct rp_interval_s
{
  const char *name;
  clock_t ticks;
  int64_t mono;
};

struct rp_state_s
{
  char *text;
  struct rp_interval_s interval[RP_INTERVALS];
  unsigned int count;
  unsigned int snapshots;
};

/* Each interval has independent before/after reports. No parsing, comparison,
 * checksum, formatting or UART occurs between its two snapshots. The read
 * destination array retains all bytes for checking after the timed interval.
 */

static int rp_sample(struct rp_state_s *state)
{
  char *text;
  ssize_t length;
  int fd;
  int result;
  if (state->snapshots >= 2 * RP_INTERVALS)
    {
      return -1;
    }

  text = state->text + state->snapshots * DIAG_TEXT;
  fd = open("/dev/s31stat", O_RDONLY);
  if (fd < 0)
    {
      return -1;
    }

  length = read(fd, text, DIAG_TEXT - 1);
  result = close(fd);
  if (length <= 0 || length >= DIAG_TEXT - 1 || result < 0)
    {
      return -1;
    }

  text[length] = '\0';
  state->snapshots++;
  return 0;
}

static int rp_begin(struct rp_state_s *state, const char *name)
{
  struct rp_interval_s *item;
  if (state->count >= RP_INTERVALS || rp_sample(state) < 0)
    {
      return -1;
    }

  item = &state->interval[state->count];
  item->name = name;
  item->mono = now_us();
  item->ticks = clock();
  return item->mono < 0 || item->ticks == (clock_t)-1 ? -1 : 0;
}

static int rp_end(struct rp_state_s *state)
{
  struct rp_interval_s *item = &state->interval[state->count];
  clock_t end = clock();
  int64_t mono = now_us();
  if (end == (clock_t)-1 || mono < item->mono ||
      rp_sample(state) < 0)
    {
      return -1;
    }

  item->ticks = end - item->ticks;
  item->mono = mono - item->mono;
  state->count++;
  return 0;
}

static uint32_t rp_checksum(const void *buffer, size_t size)
{
  const unsigned char *bytes = buffer;
  uint32_t sum = UINT32_C(2166136261);
  for (size_t i = 0; i < size; i++)
    {
      sum = (sum ^ bytes[i]) * UINT32_C(16777619);
    }

  return sum;
}

static int rp_check_reads(const struct fs_record_s *values,
                          const unsigned int *indices)
{
  struct fs_record_s expected;
  for (unsigned int i = 0; i < READS; i++)
    {
      record_make(&expected, indices[i], 0);
      if (memcmp(&expected, &values[i], sizeof(expected)) != 0)
        {
          return -1;
        }
    }

  return 0;
}

static int rp_report(const struct rp_state_s *state)
{
  puts("READPATH hw_gap=after.begin_us-before.end_us; includes status "
       "read-return/close/open, final status snprintf/copy, clock calls, "
       "control flow and scheduling; excludes formatter intervals. "
       "Empty uses identical measurement calls; do not silently subtract it.");
  for (unsigned int i = 0; i < state->count; i++)
    {
      uint64_t bounds[4];
      for (unsigned int j = 0; j < 2; j++)
        {
          const char *text = state->text + (2 * i + j) * DIAG_TEXT;
          const char *sample = strstr(text, "S31SAMPLE: ");
          if (sample == NULL || strstr(text, "S31TIME: valid=1 ") == NULL ||
              strstr(text, "FLASHIO: ") == NULL ||
              strstr(text, "S31STAT: truncated=1") != NULL ||
              sscanf(sample, "S31SAMPLE: begin_us=%" SCNu64
                     " end_us=%" SCNu64, &bounds[j * 2],
                     &bounds[j * 2 + 1]) != 2 ||
              bounds[j * 2 + 1] < bounds[j * 2])
            {
              return -1;
            }
        }

      if (bounds[2] < bounds[1])
        {
          return -1;
        }

      printf("READPATH phase=%s hw_gap_us=%" PRIu64
             " clock_ticks=%jd clocks_per_sec=%ld mono_us=%" PRIi64 "\n",
             state->interval[i].name, bounds[2] - bounds[1],
             (intmax_t)state->interval[i].ticks, (long)CLOCKS_PER_SEC,
             state->interval[i].mono);
      for (unsigned int j = 0; j < 2; j++)
        {
          printf("READPATH snapshot phase=%s edge=%s\n",
                 state->interval[i].name, j == 0 ? "before" : "after");
          fputs(state->text + (2 * i + j) * DIAG_TEXT, stdout);
        }
    }

  return 0;
}

static int rp_work(FILE *file, const char *path, int extra,
                   struct rp_state_s *state)
{
  struct fs_record_s block[RECORDS];
  struct fs_record_s value;
  struct fs_record_s *values = NULL;
  unsigned int *indices = NULL;
  uint32_t read_sum = 0;
  uint32_t file_sum = 0;
  uint32_t fd_sum = 0;
  int fd = -1;
  int result = -1;

  values = malloc(READS * sizeof(*values));
  indices = malloc(READS * sizeof(*indices));
  if (values == NULL || indices == NULL ||
      rp_begin(state, "empty") < 0 || rp_end(state) < 0)
    {
      goto out;
    }

  /* Same 200 x (200 x 20 B fwrite + fsync), with no added fflush. */

  if (rp_begin(state, "setup_800000_200fsync") < 0)
    {
      goto out;
    }

  for (unsigned int i = 0; i < RECORDS; i++)
    {
      record_make(&block[i], i, 0);
    }

  for (unsigned int i = 0; i < BLOCKS; i++)
    {
      if (fwrite(block, sizeof(*block), RECORDS, file) != RECORDS ||
          fsync(fileno(file)) < 0)
        {
          goto out;
        }
    }

  if (rp_end(state) < 0 || rp_begin(state, "original_read_1000") < 0)
    {
      goto out;
    }

  rewind(file);
  for (unsigned int i = 0; i < READS; i++)
    {
      srand(i);
      if (fseek(file, sizeof(value) * (rand() % RECORDS), SEEK_SET) != 0 ||
          fread(&values[i], sizeof(value), 1, file) != 1)
        {
          goto out;
        }
    }

  if (rp_end(state) < 0)
    {
      goto out;
    }

  /* Regenerate addresses and validate/checksum only after timed reads. */

  for (unsigned int i = 0; i < READS; i++)
    {
      indices[i] = random_index(i);
    }

  if (rp_check_reads(values, indices) < 0)
    {
      goto out;
    }

  read_sum = rp_checksum(values, READS * sizeof(*values));
  if (extra)
    {
      if (rp_begin(state, "diagnostic_prng_only") < 0)
        {
          goto out;
        }

      for (unsigned int i = 0; i < READS; i++)
        {
          srand(i);
          indices[i] = rand() % RECORDS;
        }

      if (rp_end(state) < 0 ||
          rp_begin(state, "diagnostic_FILE_address_replay") < 0)
        {
          goto out;
        }

      rewind(file);
      for (unsigned int i = 0; i < READS; i++)
        {
          if (fseek(file, sizeof(value) * indices[i], SEEK_SET) != 0 ||
              fread(&values[i], sizeof(value), 1, file) != 1)
            {
              goto out;
            }
        }

      if (rp_end(state) < 0 || rp_check_reads(values, indices) < 0)
        {
          goto out;
        }

      file_sum = rp_checksum(values, READS * sizeof(*values));
      fd = open(path, O_RDONLY);
      if (fd < 0 || rp_begin(state, "diagnostic_fd_address_replay") < 0)
        {
          goto out;
        }

      for (unsigned int i = 0; i < READS; i++)
        {
          if (lseek(fd, sizeof(value) * indices[i], SEEK_SET) < 0 ||
              read(fd, &values[i], sizeof(value)) != sizeof(value))
            {
              goto out;
            }
        }

      if (rp_end(state) < 0 || rp_check_reads(values, indices) < 0)
        {
          goto out;
        }

      fd_sum = rp_checksum(values, READS * sizeof(*values));
      close(fd);
      fd = -1;
    }

  if (rp_begin(state, "original_write_10") < 0)
    {
      goto out;
    }

  rewind(file);
  record_make(&value, 0, 1);
  for (unsigned int i = 0; i < WRITES; i++)
    {
      srand(i);
      if (fseek(file, sizeof(value) * (rand() % RECORDS), SEEK_SET) != 0 ||
          fwrite(&value, sizeof(value), 1, file) != 1)
        {
          goto out;
        }
    }

  if (rp_end(state) < 0 || rp_begin(state, "final_flush_sync") < 0 ||
      fflush(file) != 0 || fsync(fileno(file)) < 0 || rp_end(state) < 0 ||
      rp_begin(state, "full_verify_800000") < 0 || verify(file, 1) < 0 ||
      rp_end(state) < 0)
    {
      goto out;
    }

  printf("READPATH verified_bytes=800000 read_fnv32=%08" PRIx32
         " address_fnv32=%08" PRIx32 " FILE_fnv32=%08" PRIx32
         " fd_fnv32=%08" PRIx32 " extra=%d\n", read_sum,
         rp_checksum(indices, READS * sizeof(*indices)), file_sum, fd_sum,
         extra);
  result = 0;

out:
  if (fd >= 0)
    {
      close(fd);
    }

  free(indices);
  free(values);
  return result;
}

/* A fresh directory plus an exclusive owner marker protects existing data.
 * Verify/clean require the marker and a full exact-content/size check. There
 * is no recursive deletion, truncation, or deletion on failed verification.
 */

static int rp_main(const char *mode, const char *directory)
{
  char path[256];
  char owner[256];
  char signature[sizeof(RP_OWNER)];
  struct rp_state_s state = {0};
  FILE *file = NULL;
  int create = strcmp(mode, "readpath") == 0 ||
               strcmp(mode, "readpath-extra") == 0;
  int result = -1;
  int fd;
  int n;

  n = snprintf(path, sizeof(path), "%s/data.bin", directory);
  if (directory[0] != '/' || n < 0 || (size_t)n >= sizeof(path))
    {
      return EXIT_FAILURE;
    }

  n = snprintf(owner, sizeof(owner), "%s/owner.txt", directory);
  if (n < 0 || (size_t)n >= sizeof(owner) ||
      (create && mkdir(directory, 0700) < 0))
    {
      return EXIT_FAILURE;
    }

  fd = open(owner, create ? O_WRONLY | O_CREAT | O_EXCL : O_RDONLY, 0600);
  if (fd < 0)
    {
      return EXIT_FAILURE;
    }

  if (create)
    {
      result = write(fd, RP_OWNER, sizeof(RP_OWNER) - 1) ==
               sizeof(RP_OWNER) - 1 && fsync(fd) == 0 ? 0 : -1;
    }
  else
    {
      result = read(fd, signature, sizeof(signature)) ==
               sizeof(RP_OWNER) - 1 &&
               memcmp(signature, RP_OWNER, sizeof(RP_OWNER) - 1) == 0 ?
               0 : -1;
    }

  if (close(fd) < 0 || result < 0)
    {
      return EXIT_FAILURE;
    }

  result = -1;
  fd = open(path, create ? O_RDWR | O_CREAT | O_EXCL : O_RDONLY, 0600);
  if (fd < 0)
    {
      return EXIT_FAILURE;
    }

  file = fdopen(fd, create ? "wb+" : "rb");
  if (file == NULL)
    {
      close(fd);
      return EXIT_FAILURE;
    }

  if (create)
    {
      state.text = malloc(2 * RP_INTERVALS * DIAG_TEXT);
      if (state.text != NULL)
        {
          result = rp_work(file, path,
                           strcmp(mode, "readpath-extra") == 0, &state);
        }
    }
  else
    {
      result = verify(file, 0);
    }

  if (fclose(file) != 0)
    {
      result = -1;
    }

  if (create && result == 0)
    {
      result = rp_report(&state);
    }

  free(state.text);
  if (result == 0 && strcmp(mode, "readpath-clean") == 0)
    {
      result = unlink(path) == 0 && unlink(owner) == 0 &&
               rmdir(directory) == 0 ? 0 : -1;
    }

  printf("READPATH mode=%s directory=%s result=%s errno=%d\n", mode,
         directory, result == 0 ? "PASS" : "FAIL", errno);
  return result == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
#endif
