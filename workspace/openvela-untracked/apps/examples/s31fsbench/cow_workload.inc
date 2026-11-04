/* Supplement to the unchanged numbered workload: changed data, real fsync,
 * complete file verification after a separate close/reopen or remount.
 */

#define COW_BLOCK 4096
#define COW_COUNT 64

static void cow_pattern(unsigned char *buffer, unsigned int block,
                        unsigned int version)
{
  for (unsigned int i = 0; i < COW_BLOCK; i++)
    {
      buffer[i] = (unsigned char)((i * 17u) ^ (i >> 3) ^
                                  (block * 31u) ^ (version * 23u));
    }
}

static int cow_verify_fd(int fd, unsigned char *buffer,
                         unsigned char *expected,
                         const unsigned int versions[COW_COUNT])
{
  struct stat st;
  if (fstat(fd, &st) < 0 || st.st_size != COW_BLOCK * COW_COUNT ||
      lseek(fd, 0, SEEK_SET) != 0)
    {
      return -1;
    }

  for (unsigned int b = 0; b < COW_COUNT; b++)
    {
      cow_pattern(expected, b, versions[b]);
      if (read(fd, buffer, COW_BLOCK) != COW_BLOCK ||
          memcmp(buffer, expected, COW_BLOCK) != 0)
        {
          fprintf(stderr, "COWBENCH mismatch block=%u\n", b);
          return -1;
        }
    }

  return 0;
}

static int cow_main(const char *directory, int create)
{
  char path[256];
  unsigned int versions[COW_COUNT] = {0};
  unsigned int offsets[COW_COUNT];
  unsigned char *buffer = NULL;
  unsigned char *expected = NULL;
  int fd = -1;
  int peer = -1;
  int result = -1;
  int64_t begin;
  int64_t write_us = -1;
  int64_t sync_us = -1;
  int n = snprintf(path, sizeof(path), "%s/cow-random.bin", directory);
  if (directory[0] != '/' || n < 0 || (size_t)n >= sizeof(path))
    {
      return EXIT_FAILURE;
    }

  srand(1);
  for (unsigned int i = 0; i < COW_COUNT; i++)
    {
      offsets[i] = (unsigned int)rand() % COW_COUNT;
      versions[offsets[i]] = i + 1;
    }

  buffer = malloc(COW_BLOCK);
  expected = malloc(COW_BLOCK);
  if (buffer == NULL || expected == NULL)
    {
      goto out;
    }

  fd = open(path, create ? O_CREAT | O_EXCL | O_RDWR : O_RDONLY, 0600);
  if (fd < 0)
    {
      goto out;
    }

  if (create)
    {
      for (unsigned int b = 0; b < COW_COUNT; b++)
        {
          cow_pattern(buffer, b, 0);
          if (write(fd, buffer, COW_BLOCK) != COW_BLOCK)
            {
              goto out;
            }
        }

      if (fsync(fd) < 0 || (begin = now_us()) < 0)
        {
          goto out;
        }

      for (unsigned int i = 0; i < COW_COUNT; i++)
        {
          unsigned int b = offsets[i];
          cow_pattern(buffer, b, i + 1);
          if (lseek(fd, b * COW_BLOCK, SEEK_SET) != b * COW_BLOCK ||
              write(fd, buffer, COW_BLOCK) != COW_BLOCK)
            {
              goto out;
            }
        }

      write_us = now_us() - begin;
      /* A separate open must see all pending changes. Verification is outside
       * the measured write and fsync intervals, and is explicitly reported.
       */

      peer = open(path, O_RDONLY);
      if (peer < 0 || cow_verify_fd(peer, buffer, expected, versions) < 0)
        {
          goto out;
        }

      /* Keep peer open: close is itself a sync boundary in this candidate. */

      begin = now_us();
      if (begin < 0 || fsync(fd) < 0)
        {
          goto out;
        }

      sync_us = now_us() - begin;
      if (write_us < 0 || sync_us < 0 || write_us + sync_us <= 0)
        {
          goto out;
        }

      printf("COWBENCH seed=1 bytes=262144 writes=64 block=4096 "
             "write_us=%" PRIi64 " fsync_us=%" PRIi64 " "
             "durable_total_us=%" PRIi64 " verify_gap_excluded=1\n",
             write_us, sync_us, write_us + sync_us);
    }

  if (cow_verify_fd(fd, buffer, expected, versions) < 0)
    {
      goto out;
    }

  result = 0;

out:
  if (peer >= 0 && close(peer) < 0)
    {
      result = -1;
    }

  if (fd >= 0 && close(fd) < 0)
    {
      result = -1;
    }

  free(buffer);
  free(expected);
  printf("COWBENCH mode=%s result=%s errno=%d\n", create ? "run" : "verify",
         result == 0 ? "PASS" : "FAIL", result == 0 ? 0 : errno);
  return result == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
