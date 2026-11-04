/* SPDX-License-Identifier: Apache-2.0
 * Diagnostic for initial bulk reads followed by small reads. Synthetic data
 * only; no mounting/formatting. FILE internals are inspected only here.
 */

#if !defined(CONFIG_STDIO_DISABLE_BUFFERING) && CONFIG_STDIO_BUFFER_SIZE > 0

#define QB_OWNER "korvo bulk-read window qualification v1\n"
#define QB_SIZE (3 * 4096 + 73)

static unsigned char qb_byte(unsigned int pos)
{
  return (unsigned char)((pos * 37u) ^ (pos >> 5) ^ 0x5a);
}

static int qb_read(FILE *fp, unsigned int pos, unsigned int count,
                   unsigned char *buffer)
{
  if (fread(buffer, 1, count, fp) != count || ferror(fp) ||
      ftell(fp) != pos + count)
    {
      return q_error("bulk_read_position");
    }

  for (unsigned int i = 0; i < count; i++)
    {
      if (buffer[i] != qb_byte(pos + i))
        {
          return q_error("bulk_read_content");
        }
    }

  return 0;
}

static int qb_run(const char *dir)
{
  char path[256];
  FILE *fp[8] = {0};
  unsigned char *buffer = NULL;
  int fd = -1;
  int result = -1;

  if (q_path(path, dir, "bulk.bin") < 0 ||
      (buffer = malloc(8192)) == NULL ||
      (fd = open(path, O_CREAT | O_EXCL | O_WRONLY, 0600)) < 0)
    {
      goto out;
    }

  for (unsigned int pos = 0; pos < QB_SIZE; )
    {
      unsigned int n = QB_SIZE - pos < 4096 ? QB_SIZE - pos : 4096;
      for (unsigned int i = 0; i < n; i++)
        {
          buffer[i] = qb_byte(pos + i);
        }

      if (write(fd, buffer, n) != n)
        {
          goto out;
        }

      pos += n;
    }

  if (fsync(fd) < 0)
    {
      goto out;
    }

  result = q_close(fd);
  fd = -1;
  if (result < 0)
    {
      goto out;
    }

  result = -1;
  for (unsigned int i = 0; i < 8; i++)
    {
      fp[i] = fopen(path, "rb");
      if (fp[i] == NULL || qb_read(fp[i], 0, 4096, buffer) < 0 ||
          qb_read(fp[i], 4096, 8192, buffer) < 0 ||
          fp[i]->fs_bufstart != fp[i]->fs_buffer ||
          fp[i]->fs_bufend - fp[i]->fs_bufstart != CONFIG_STDIO_BUFFER_SIZE)
        {
          q_error("bulk_default_buffer");
          goto out;
        }
    }

  printf("BULKREAD phase=bulk handles=8 bytes_per_handle=12288 "
         "default_capacity=%d promoted=0\n", CONFIG_STDIO_BUFFER_SIZE);
  for (unsigned int i = 0; i < 8; i++)
    {
      if (qb_read(fp[i], 12288, 73, buffer) < 0 ||
          fgetc(fp[i]) != EOF || !feof(fp[i]) || ferror(fp[i]) ||
          fseek(fp[i], 37 + i, SEEK_SET) != 0 ||
          qb_read(fp[i], 37 + i, 20, buffer) < 0 ||
          fp[i]->fs_bufstart == fp[i]->fs_buffer ||
          fp[i]->fs_bufend - fp[i]->fs_bufstart != 4096 ||
          fseek(fp[i], 0, SEEK_SET) != 0 ||
          qb_read(fp[i], 0, 8192, buffer) < 0)
        {
          q_error("bulk_then_small_window");
          goto out;
        }
    }

  puts("BULKREAD phase=mixed handles=8 window_capacity=4096 "
       "content_position_eof=1");
  result = 0;
out:
  if (fd >= 0 && q_close(fd) < 0)
    {
      result = -1;
    }

  if (qs_close_all(fp) < 0)
    {
      result = -1;
    }

  free(buffer);
  return result;
}

static int qb_main(const char *dir)
{
  static const char *const files[] = {"bulk.bin", "owner.txt"};
  int result = q_owner(dir, QB_OWNER, 1);
  if (result == 0)
    {
      result = qb_run(dir);
      if (result == 0)
        {
          result = q_clean(dir, files, 2);
        }
    }

  printf("BULKREAD result=%s cleanup=%d\n", result == 0 ? "PASS" : "FAIL",
         result == 0);
  return result == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}

#else

static int qb_main(const char *dir)
{
  (void)dir;
  puts("BULKREAD unavailable: stdio buffering disabled");
  return EXIT_FAILURE;
}

#endif
