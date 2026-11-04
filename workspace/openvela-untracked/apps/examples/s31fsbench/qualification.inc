/* SPDX-License-Identifier: Apache-2.0
 * Qualification supplement; original modes and numbered sources are unchanged.
 * No mount/format operations. Root supplies each explicit remount boundary.
 */
#define Q_OWNER "korvo FS qualification v1\n"
#define QS_OWNER "korvo eight-stream qualification v1\n"
#define QS_SIZE 8237
#define QS_TRUNC 4113

/* Private comparison arithmetic from Espressif Picolibc rand.c (2023,
 * Apache-2.0), SHA256 663da9b5eec6684197add6c72af3a2668e36d24646f01c94aa5013ae965d9e2e.
 * uint64 LCG, RV32 RAND_MAX=0x7fffffff. Never changes system srand/rand.
 */
static unsigned int q_rand(uint64_t *state)
{
  *state = *state * UINT64_C(6364136223846793005) + 1;
  return (unsigned int)((*state >> 32) & UINT64_C(0x7fffffff));
}

static unsigned int q_index(unsigned int seed)
{
  uint64_t state = seed;
  return q_rand(&state) % RECORDS;
}

static uint32_t q_digest(const unsigned int *offsets, unsigned int count)
{
  uint32_t hash = UINT32_C(2166136261);
  for (unsigned int i = 0; i < count; i++)
    {
      for (unsigned int b = 0; b < 4; b++)
        {
          hash = (hash ^ ((offsets[i] >> (8 * b)) & 255)) *
                 UINT32_C(16777619);
        }
    }
  return hash;
}

static int q_error(const char *op)
{
  printf("QFS event=error op=%s errno=%d\n", op, errno ? errno : EIO);
  return -1;
}

static int q_close(int fd)
{
  return close(fd) == 0 ? 0 : q_error("close");
}

static int q_fclose(FILE *fp)
{
  return fclose(fp) == 0 ? 0 : q_error("fclose");
}

static int q_path(char path[256], const char *dir, const char *name)
{
  int n = snprintf(path, 256, "%s/%s", dir, name);
  return n > 0 && n < 256 ? 0 : q_error("path_length");
}

static int q_regular(const char *path)
{
  struct stat st;
  return lstat(path, &st) == 0 && S_ISREG(st.st_mode) ?
         0 : q_error("regular_file_guard");
}

static int q_owner(const char *dir, const char *signature, int create)
{
  char path[256];
  char text[80];
  struct stat st;
  size_t len = strlen(signature);
  int result = -1;
  int fd;
  size_t n = strlen(dir);

  /* Short absolute owned paths; reject traversal and aliases before mutation.
   * Root exclusively owns the parent; this is not an adversarial-race API.
   */
  if (n < 2 || dir[0] != '/' || dir[n - 1] == '/' || strstr(dir, "//") ||
      strstr(dir, "/./") || strstr(dir, "/../") ||
      strcmp(dir + n - 2, "/.") == 0 ||
      (n >= 3 && strcmp(dir + n - 3, "/..") == 0) ||
      q_path(path, dir, "owner.txt") < 0 ||
      (create && mkdir(dir, 0700) < 0) ||
      lstat(dir, &st) < 0 || !S_ISDIR(st.st_mode))
    {
      return q_error("directory_guard");
    }

  if (!create && q_regular(path) < 0)
    {
      return -1;
    }
  fd = open(path, create ? O_WRONLY | O_CREAT | O_EXCL : O_RDONLY, 0600);
  if (fd < 0)
    {
      return q_error("owner_open");
    }
  if (create)
    {
      int written = write(fd, signature, len) == (ssize_t)len;
      int synced = fsync(fd) == 0;
      result = written && synced ? 0 : q_error("owner_write_sync");
    }
  else if (read(fd, text, sizeof(text)) == (ssize_t)len &&
           memcmp(text, signature, len) == 0)
    {
      result = 0;
    }
  else
    {
      q_error("owner_signature");
    }
  if (q_close(fd) < 0)
    {
      result = -1;
    }
  return result;
}

/* Only two existing s31stat snapshots bracket an outer interval. No status
 * formatting, allocation, byte verification or logging inside t0..t3.
 * Internal endpoints use the unchanged 10 ms target monotonic clock.
 */
struct q_time_s
{
  int64_t t[4];
  int loop_error;
  int flush_error;
  int sync_error;
  int close_error;
  unsigned int bytes;
#ifdef CONFIG_ESP32S31_FLASH_DIAGNOSTICS
  struct rp_state_s sample;
#endif
};

static int q_begin(struct q_time_s *t)
{
  memset(t, 0, sizeof(*t));
#ifdef CONFIG_ESP32S31_FLASH_DIAGNOSTICS
  t->sample.text = malloc(2 * DIAG_TEXT);
  if (t->sample.text == NULL || rp_sample(&t->sample) < 0)
    {
      free(t->sample.text);
      t->sample.text = NULL;
      return q_error("timer_begin");
    }
#endif
  t->t[0] = now_us();
  return 0;
}

static void q_finish(struct q_time_s *t, FILE *fp, int fd, int writing)
{
  t->t[1] = now_us();
  if (writing && fp != NULL && fflush(fp) != 0)
    {
      t->flush_error = errno ? errno : EIO;
    }
  if (writing && fsync(fd) != 0)
    {
      t->sync_error = errno ? errno : EIO;
    }
  t->t[2] = now_us();
  if ((fp != NULL ? fclose(fp) : close(fd)) != 0)
    {
      t->close_error = errno ? errno : EIO;
    }
  t->t[3] = now_us();
}

static int q_report(struct q_time_s *t, const char *phase,
                    const unsigned int *offsets, unsigned int count)
{
  int ok = !t->loop_error && !t->flush_error && !t->sync_error &&
           !t->close_error && t->t[0] >= 0;
  int64_t elapsed = t->t[3] - t->t[0];
  const char *clock_name = "monotonic";
  unsigned int resolution = 10000;
  for (unsigned int i = 1; i < 4; i++)
    {
      ok &= t->t[i] >= t->t[i - 1];
    }
#ifdef CONFIG_ESP32S31_FLASH_DIAGNOSTICS
  uint64_t bounds[4] = {0};
  if (rp_sample(&t->sample) < 0)
    {
      ok = 0;
    }
  for (unsigned int j = 0; j < t->sample.snapshots; j++)
    {
      const char *text = t->sample.text + j * DIAG_TEXT;
      const char *s = strstr(text, "S31SAMPLE: ");
      if (s == NULL || strstr(text, "S31TIME: valid=1 ") == NULL ||
          strstr(text, "S31STAT: truncated=1") != NULL ||
          sscanf(s, "S31SAMPLE: begin_us=%" SCNu64 " end_us=%" SCNu64,
                 &bounds[2 * j], &bounds[2 * j + 1]) != 2 ||
          bounds[2 * j + 1] < bounds[2 * j])
        {
          ok = 0;
        }
    }
  if (t->sample.snapshots != 2 || bounds[2] < bounds[1])
    {
      ok = 0;
    }
  elapsed = bounds[2] >= bounds[1] ? (int64_t)(bounds[2] - bounds[1]) : -1;
  clock_name = "s31stat_hw_gap";
  resolution = 1;
  printf("QFS event=bounds phase=%s before_begin_us=%" PRIu64
         " before_end_us=%" PRIu64 " after_begin_us=%" PRIu64
         " after_end_us=%" PRIu64 "\n", phase,
         bounds[0], bounds[1], bounds[2], bounds[3]);
  free(t->sample.text);
#endif
  printf("QFS event=timing phase=%s bytes=%u clock=%s resolution_us=%u"
         " elapsed_us=%" PRIi64 " ms=%.3f kib_s=%.6f"
         " t0_us=%" PRIi64 " t1_us=%" PRIi64 " t2_us=%" PRIi64
         " t3_us=%" PRIi64 " loop_us=%" PRIi64 " commit_us=%" PRIi64
         " close_us=%" PRIi64 " continuous_total_us=%" PRIi64
         " endpoint_resolution_us=10000 loop_errno=%d flush_errno=%d"
         " sync_errno=%d close_errno=%d ok=%d\n", phase, t->bytes,
         clock_name, resolution, elapsed, elapsed / 1000.0,
         elapsed > 0 ? t->bytes * 1000000.0 / (1024.0 * elapsed) : 0,
         t->t[0], t->t[1], t->t[2], t->t[3], t->t[1] - t->t[0],
         t->t[2] - t->t[1], t->t[3] - t->t[2], t->t[3] - t->t[0],
         t->loop_error, t->flush_error, t->sync_error, t->close_error, ok);
  printf("QFS event=addresses phase=%s count=%u encoding=u32le_byte_offsets"
         " fnv32=%08" PRIx32 "\n", phase, count, q_digest(offsets, count));
  return ok ? 0 : -1;
}

static int q_verify(const char *dir, const char *name, int kind, int marked)
{
  char path[256];
  unsigned char buf[256];
  struct fs_record_s actual;
  struct fs_record_s expected;
  struct stat st;
  unsigned int size = kind == 0 ? 800000 : 262144;
  FILE *fp;
  int ok = 1;

  if (q_path(path, dir, name) < 0 || q_regular(path) < 0 ||
      (fp = fopen(path, "rb")) == NULL)
    {
      return q_error("verify_open");
    }
  if (fstat(fileno(fp), &st) < 0 || st.st_size != size)
    {
      ok = 0;
    }
  for (unsigned int pos = 0; ok && pos < size; )
    {
      if (kind == 0)
        {
          unsigned int record = pos / sizeof(actual);
          int changed = 0;
          for (unsigned int i = 0; marked && record < RECORDS && i < WRITES; i++)
            {
              changed |= q_index(i) == record;
            }
          record_make(&expected, record % RECORDS, changed);
          ok = fread(&actual, sizeof(actual), 1, fp) == 1 &&
               memcmp(&actual, &expected, sizeof(actual)) == 0;
          pos += sizeof(actual);
          if (record % 1000 == 0)
            {
              usleep(10000); /* Reference verify yields one 10 ms tick. */
            }
        }
      else
        {
          ok = fread(buf, 1, sizeof(buf), fp) == sizeof(buf);
          for (unsigned int j = 0; ok && j < sizeof(buf); j++)
            {
              ok = buf[j] == (kind == 1 ? 'A' : 0);
            }
          pos += sizeof(buf);
        }
    }
  if (ok)
    {
      ok = fgetc(fp) == EOF && feof(fp) && !ferror(fp);
    }
  if (q_fclose(fp) < 0)
    {
      ok = 0;
    }
  printf("QFS event=verify file=%s expected_bytes=%u size_eof=1 ok=%d\n",
         name, size, ok);
  return ok ? 0 : q_error("verify_content_size_eof");
}

static int q_prepare(const char *dir)
{
  char path[256];
  struct fs_record_s block[RECORDS];
  struct q_time_s t;
  FILE *fp;
  int fd;
  if (q_path(path, dir, "random.bin") < 0 ||
      (fd = open(path, O_CREAT | O_EXCL | O_RDWR, 0600)) < 0)
    {
      return q_error("prepare_open");
    }
  fp = fdopen(fd, "wb+");
  if (fp == NULL)
    {
      q_close(fd);
      return q_error("prepare_fdopen");
    }
  for (unsigned int i = 0; i < RECORDS; i++)
    {
      record_make(&block[i], i, 0);
    }
  if (q_begin(&t) < 0)
    {
      q_fclose(fp);
      return -1;
    }
  for (unsigned int i = 0; i < BLOCKS; i++)
    {
      if (fwrite(block, sizeof(*block), RECORDS, fp) != RECORDS || fsync(fd) < 0)
        {
          t.loop_error = errno ? errno : EIO;
          break;
        }
      t.bytes += sizeof(block);
      usleep(10000); /* Same one-tick setup yield as the fixed reference. */
    }
  q_finish(&t, fp, fd, 1);
  return q_report(&t, "setup_200x4000_200fsync", NULL, 0);
}

static int q_small(const char *dir)
{
  char path[256];
  struct fs_record_s *values = NULL;
  unsigned int *offsets = NULL;
  struct fs_record_s expected;
  struct q_time_s t;
  FILE *fp = NULL;
  int result = -1;
  int fd;
  if (q_verify(dir, "random.bin", 0, 0) < 0 ||
      q_path(path, dir, "random.bin") < 0)
    {
      return -1;
    }
  values = malloc(READS * sizeof(*values));
  offsets = malloc(READS * sizeof(*offsets));
  fp = fopen(path, "rb+");
  if (values == NULL || offsets == NULL || fp == NULL || q_begin(&t) < 0)
    {
      goto out;
    }
  for (unsigned int i = 0; i < READS; i++)
    {
      uint64_t state = i;
      offsets[i] = (q_rand(&state) % RECORDS) * sizeof(*values);
      if (fseek(fp, offsets[i], SEEK_SET) != 0 ||
          fread(&values[i], sizeof(*values), 1, fp) != 1)
        {
          t.loop_error = errno ? errno : EIO;
          break;
        }
      t.bytes += sizeof(*values);
    }
  /* Read timing leaves this same FILE open for the following reference write.
   * q_report takes the outer after sample before checks/logging.
   */
  t.t[1] = t.t[2] = t.t[3] = now_us();
  if (q_report(&t, "small_read", offsets, t.bytes / sizeof(*values)) < 0)
    {
      goto out;
    }
  for (unsigned int i = 0; i < READS; i++)
    {
      record_make(&expected, offsets[i] / sizeof(*values), 0);
      if (memcmp(&values[i], &expected, sizeof(expected)) != 0)
        {
          q_error("small_read_bytes");
          goto out;
        }
    }
  record_make(&expected, 0, 1);
  fd = fileno(fp);
  if (q_begin(&t) < 0)
    {
      goto out;
    }
  for (unsigned int i = 0; i < WRITES; i++)
    {
      uint64_t state = i;
      offsets[i] = (q_rand(&state) % RECORDS) * sizeof(expected);
      if (fseek(fp, offsets[i], SEEK_SET) != 0 ||
          fwrite(&expected, sizeof(expected), 1, fp) != 1)
        {
          t.loop_error = errno ? errno : EIO;
          break;
        }
      t.bytes += sizeof(expected);
    }
  q_finish(&t, fp, fd, 1);
  fp = NULL;
  result = q_report(&t, "small_write", offsets, t.bytes / sizeof(expected));
out:
  if (fp != NULL && q_fclose(fp) < 0)
    {
      result = -1;
    }
  free(offsets);
  free(values);
  return result;
}

static int q_bulk(const char *dir, const char *step)
{
  char path[256];
  unsigned char buffer[4096];
  unsigned int offsets[64];
  struct q_time_s t;
  uint64_t state = 1;
  int dd = strncmp(step, "dd", 2) == 0;
  int random = strncmp(step, "random", 6) == 0;
  int writing = strcmp(step, "seq") == 0 || strcmp(step, "random") == 0 ||
                strcmp(step, "dd") == 0;
  const char *name = dd ? "dd.bin" : "performance.bin";
  int flags = writing ? (random ? O_WRONLY : O_WRONLY | O_CREAT | O_EXCL) : O_RDONLY;
  int fd;
  if ((!writing || random) && q_verify(dir, name, dd ? 2 : 1, 0) < 0)
    {
      return -1;
    }
  if (q_path(path, dir, name) < 0 || (fd = open(path, flags, 0600)) < 0)
    {
      return q_error("bulk_open");
    }
  memset(buffer, dd ? 0 : 'A', sizeof(buffer));
  if (q_begin(&t) < 0)
    {
      q_close(fd);
      return -1;
    }
  for (unsigned int i = 0; i < 64; i++)
    {
      offsets[i] = (random ? q_rand(&state) % 64 : i) * sizeof(buffer);
      if ((random && lseek(fd, offsets[i], SEEK_SET) != offsets[i]) ||
          (writing ? write(fd, buffer, sizeof(buffer)) :
                     read(fd, buffer, sizeof(buffer))) != sizeof(buffer))
        {
          t.loop_error = errno ? errno : EIO;
          break;
        }
      t.bytes += sizeof(buffer);
    }
  if (writing)
    {
      q_finish(&t, NULL, fd, 1);
      return q_report(&t, step, offsets, t.bytes / sizeof(buffer));
    }
  t.t[1] = t.t[2] = t.t[3] = now_us();
  /* As in the reference, read rate excludes descriptor close. */
  int result = q_report(&t, step, offsets, t.bytes / sizeof(buffer));
  if (q_close(fd) < 0)
    {
      result = -1;
    }
  return result;
}

static int q_all(const char *dir)
{
  int result = q_verify(dir, "random.bin", 0, 1);
  if (q_verify(dir, "performance.bin", 1, 0) < 0)
    {
      result = -1;
    }
  if (q_verify(dir, "dd.bin", 2, 0) < 0)
    {
      result = -1;
    }
  printf("QFS event=full_verify expected_bytes=1324288 ok=%d\n", result == 0);
  return result;
}

static int q_clean(const char *dir, const char *const *names, unsigned int count)
{
  char path[256];
  /* Called only after owner and exact full-content/size/EOF validation. */
  for (unsigned int i = 0; i < count; i++)
    {
      if (q_path(path, dir, names[i]) < 0 || q_regular(path) < 0 ||
          unlink(path) < 0)
        {
          return q_error("owned_unlink");
        }
    }
  return rmdir(dir) == 0 ? 0 : q_error("owned_rmdir");
}

static const unsigned int qs_offsets[8] = {4090, 8217, 17, 4080, 8190, 31, 4100, 8200};

static unsigned char qs_byte(unsigned int pos, int second, unsigned int epochs)
{
  unsigned char value = (unsigned char)((pos * 37u) ^ (pos >> 5) ^ 0x5a);
  for (unsigned int e = 0; e < epochs; e++)
    {
      unsigned int offset = second ? e * 29 : qs_offsets[e];
      if ((!second || (e & 1)) && pos >= offset && pos < offset + 20)
        {
          value = (unsigned char)((second ? 0x70 : 0xc0) + e);
        }
    }
  return value;
}

static int qs_verify(FILE *fp, unsigned int size, int second)
{
  struct stat st;
  unsigned char buffer[256];
  if (fstat(fileno(fp), &st) < 0 || st.st_size != size || fseek(fp, 0, SEEK_SET))
    {
      return q_error("stream_size_seek");
    }
  for (unsigned int pos = 0; pos < size; )
    {
      unsigned int count = size - pos < sizeof(buffer) ? size - pos : sizeof(buffer);
      if (fread(buffer, 1, count, fp) != count)
        {
          return q_error("stream_full_read");
        }
      for (unsigned int i = 0; i < count; i++)
        {
          if (buffer[i] != qs_byte(pos + i, second, 8))
            {
              return q_error("stream_full_bytes");
            }
        }
      pos += count;
    }
  return fgetc(fp) == EOF && feof(fp) && !ferror(fp) ?
         0 : q_error("stream_eof");
}

static int qs_create(const char *path)
{
  unsigned char buffer[256];
  int fd = open(path, O_CREAT | O_EXCL | O_RDWR, 0600);
  int result = 0;
  if (fd < 0)
    {
      return q_error("stream_create");
    }
  for (unsigned int pos = 0; pos < QS_SIZE; )
    {
      unsigned int n = QS_SIZE - pos < sizeof(buffer) ? QS_SIZE - pos : sizeof(buffer);
      for (unsigned int i = 0; i < n; i++)
        {
          buffer[i] = qs_byte(pos + i, 0, 0);
        }
      if (write(fd, buffer, n) != n)
        {
          result = q_error("stream_setup_write");
          break;
        }
      pos += n;
    }
  if (fsync(fd) < 0)
    {
      result = q_error("stream_setup_sync");
    }
  if (q_close(fd) < 0)
    {
      result = -1;
    }
  return result;
}

static int qs_read(FILE *fp, unsigned int pos, unsigned int size, unsigned int epochs)
{
  unsigned char bytes[32];
  int result = 0;
  flockfile(fp);
  if (size > sizeof(bytes) || fseek(fp, pos, SEEK_SET) ||
      fread(bytes, 1, size, fp) != size)
    {
      result = -1;
    }
  for (unsigned int i = 0; result == 0 && i < size; i++)
    {
      if (bytes[i] != qs_byte(pos + i, 0, epochs))
        {
          result = -1;
        }
    }
  funlockfile(fp);
  return result == 0 ? 0 : q_error("stream_peer_bytes");
}

static int qs_close_all(FILE **fp)
{
  int result = 0;
  for (unsigned int i = 0; i < 8; i++)
    {
      if (fp[i] != NULL && q_fclose(fp[i]) < 0)
        {
          result = -1;
        }
      fp[i] = NULL;
    }
  return result;
}

static int qs_hold(unsigned int seconds)
{
  int64_t start = now_us();
  if (start < 0)
    {
      return q_error("hold_clock");
    }
  printf("QFS event=live_hold pid=%jd seconds=%u windows=8 requested_window_bytes=32800"
         " dirty_nodes=2 checkpoint=pre_writer_fsync\n", (intmax_t)getpid(), seconds);
  if (fflush(stdout) != 0)
    {
      return q_error("hold_stdout");
    }
  /* Finite deadline and bounded iterations, including interrupted sleeps. */
  for (unsigned int i = 0; i < seconds * 10; i++)
    {
      int64_t current = now_us();
      if (current < start)
        {
          return q_error("hold_clock_order");
        }
      if (current - start >= (int64_t)seconds * 1000000)
        {
          break;
        }
      struct timespec delay = {0, 100000000};
      if (nanosleep(&delay, NULL) < 0 && errno != EINTR)
        {
          return q_error("hold_sleep");
        }
    }
  return 0;
}

static int qs_run(const char *dir, unsigned int hold)
{
  char path[256];
  char other[256];
  FILE *fp[8] = {0};
  unsigned char changed[20];
  unsigned char actual[20];
  int second = -1;
  int result = -1;
  if (q_path(path, dir, "streams.bin") < 0 ||
      q_path(other, dir, "second.bin") < 0 ||
      qs_create(path) < 0 || qs_create(other) < 0)
    {
      goto out;
    }
  second = open(other, O_RDWR);
  if (second < 0)
    {
      q_error("second_open");
      goto out;
    }
  for (unsigned int i = 0; i < 8; i++)
    {
      fp[i] = fopen(path, "rb+");
      if (fp[i] == NULL || qs_read(fp[i], 17 + i * 37, 20, 0) < 0)
        {
          goto out;
        }
    }
  for (unsigned int e = 0; e < 8; e++)
    {
      /* Single-task barrier: all eight independent FILEs invalidated before
       * the chosen writer changes VFS bytes. No external coherence assumed.
       */
      for (unsigned int i = 0; i < 8; i++)
        {
          if (fflush(fp[i]) != 0)
            {
              q_error("stream_invalidate");
              goto out;
            }
        }
      memset(changed, 0xc0 + e, sizeof(changed));
      if (fseek(fp[e], qs_offsets[e], SEEK_SET) ||
          fwrite(changed, 1, sizeof(changed), fp[e]) != sizeof(changed) ||
          fflush(fp[e]) != 0)
        {
          q_error("stream_writer_flush");
          goto out;
        }
      if (e & 1)
        {
          memset(changed, 0x70 + e, sizeof(changed));
          if (lseek(second, e * 29, SEEK_SET) != e * 29 ||
              write(second, changed, sizeof(changed)) != sizeof(changed) ||
              lseek(second, e * 29, SEEK_SET) != e * 29 ||
              read(second, actual, sizeof(actual)) != sizeof(actual) ||
              memcmp(changed, actual, sizeof(actual)))
            {
              q_error("second_changed_bytes");
              goto out;
            }
        }
      for (unsigned int i = 0; i < 8; i++)
        {
          if (qs_read(fp[i], qs_offsets[e], 20, e + 1) < 0 ||
              qs_read(fp[i], 4088, 24, e + 1) < 0 ||
              qs_read(fp[i], QS_SIZE - 20, 20, e + 1) < 0)
            {
              goto out;
            }
        }
      /* Every changed-byte read precedes fsync; no peer close can commit it. */
      printf("QFS event=epoch epoch=%u peers=8 changed_read_before_writer_fsync=1\n", e);
      if (e == 1 && qs_hold(hold) < 0)
        {
          goto out;
        }
      int synced = fsync(fileno(fp[e]));
      int second_synced = fsync(second);
      if (synced < 0 || second_synced < 0)
        {
          q_error("stream_commit");
          goto out;
        }
    }
  if (qs_verify(fp[0], QS_SIZE, 0) < 0 || qs_close_all(fp) < 0)
    {
      goto out;
    }
  /* Second eight-open/read/close cycle, then coordinated invalidation and
   * truncate with all peers open. Reopen verifies exact final size and EOF.
   */
  for (unsigned int i = 0; i < 8; i++)
    {
      fp[i] = fopen(path, "rb+");
      if (fp[i] == NULL || qs_read(fp[i], 4088, 24, 8) < 0)
        {
          goto out;
        }
    }
  for (unsigned int i = 0; i < 8; i++)
    {
      if (fflush(fp[i]) != 0)
        {
          q_error("truncate_invalidate");
          goto out;
        }
    }
  if (ftruncate(fileno(fp[0]), QS_TRUNC) < 0)
    {
      q_error("truncate");
      goto out;
    }
  for (unsigned int i = 0; i < 8; i++)
    {
      if (qs_verify(fp[i], QS_TRUNC, 0) < 0)
        {
          goto out;
        }
    }
  if (fsync(fileno(fp[0])) < 0)
    {
      q_error("truncate_sync");
      goto out;
    }
  result = 0;
out:
  if (qs_close_all(fp) < 0)
    {
      result = -1;
    }
  if (second >= 0 && q_close(second) < 0)
    {
      result = -1;
    }
  return result;
}

static int qs_all(const char *dir)
{
  char path[256];
  int result = 0;
  for (unsigned int i = 0; i < 2; i++)
    {
      FILE *fp;
      const char *name = i ? "second.bin" : "streams.bin";
      if (q_path(path, dir, name) < 0 || q_regular(path) < 0 ||
          (fp = fopen(path, "rb")) == NULL)
        {
          result = q_error("stream_verify_open");
          continue;
        }
      if (qs_verify(fp, i ? QS_SIZE : QS_TRUNC, i) < 0)
        {
          result = -1;
        }
      if (q_fclose(fp) < 0)
        {
          result = -1;
        }
    }
  printf("QFS event=stream_full_verify expected_bytes=12350 size_eof=1 ok=%d\n", result == 0);
  return result;
}

static int q_main(int argc, char **argv)
{
  static const char *const files[] = {"random.bin", "performance.bin", "dd.bin", "owner.txt"};
  static const char *const streams[] = {"streams.bin", "second.bin", "owner.txt"};
  const char *mode = argv[1];
  int stream = strncmp(mode, "streams", 7) == 0;
  int result = -1;
  int create = (strcmp(mode, "qualify") == 0 && argc == 4 && strcmp(argv[3], "prep") == 0) ||
               strcmp(mode, "streams") == 0;
  unsigned int hold = 10;
  if (argc < 3 || argc > 4 ||
      (strcmp(mode, "qualify") == 0 && argc != 4))
    {
      goto out;
    }
  if (stream && argc == 4)
    {
      char *end;
      unsigned long seconds = strtoul(argv[3], &end, 10);
      if (strcmp(mode, "streams") != 0 || *argv[3] == '\0' || *end || seconds > 30)
        {
          goto out;
        }
      hold = seconds;
    }
  else if (strcmp(mode, "qualify") != 0 && argc != 3)
    {
      goto out;
    }
  if (q_owner(argv[2], stream ? QS_OWNER : Q_OWNER, create) < 0)
    {
      goto out;
    }
  if (strcmp(mode, "qualify") == 0)
    {
      const char *step = argv[3];
      if (strcmp(step, "prep") == 0)
        {
          result = q_prepare(argv[2]);
        }
      else if (strcmp(step, "small") == 0)
        {
          result = q_small(argv[2]);
        }
      else if (strcmp(step, "seq") == 0 || strcmp(step, "seqread") == 0 ||
               strcmp(step, "random") == 0 || strcmp(step, "randomread") == 0 ||
               strcmp(step, "dd") == 0 || strcmp(step, "ddread") == 0)
        {
          result = q_bulk(argv[2], step);
        }
    }
  else if (stream)
    {
      result = strcmp(mode, "streams") == 0 ? qs_run(argv[2], hold) : 0;
      if (result == 0)
        {
          result = qs_all(argv[2]);
        }
      if (result == 0 && strcmp(mode, "streams-clean") == 0)
        {
          result = q_clean(argv[2], streams, 3);
        }
    }
  else
    {
      result = q_all(argv[2]);
      if (result == 0 && strcmp(mode, "qualify-clean") == 0)
        {
          result = q_clean(argv[2], files, 4);
        }
    }
out:
  printf("QFS event=result mode=%s ok=%d retained=%d\n", mode, result == 0,
         strcmp(mode, "qualify-clean") != 0 && strcmp(mode, "streams-clean") != 0);
  return result == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
