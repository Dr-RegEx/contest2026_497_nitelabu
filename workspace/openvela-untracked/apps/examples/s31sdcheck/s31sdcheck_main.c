/****************************************************************************
 * apps/examples/s31sdcheck/s31sdcheck_main.c
 * SPDX-License-Identifier: Apache-2.0
 *
 * Bounded synthetic VFS integrity qualification. No raw block/format access.
 ****************************************************************************/

#define _GNU_SOURCE 1
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <sched.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define FILE_BYTES 65536u
#define ROUNDS 12u
#define BUFFER_BYTES 8192u
#define PATH_BYTES 256u
#define TIME_LIMIT 120
#define WORKER_STACK 16384u

static const char g_marker[] =
  "s31sdcheck-v1\nbytes=65536\nrounds=12\n";
static const char *const g_files[] =
  {"owner.txt", "cpu0.bin", "cpu1.bin", ".next0", ".next1"};

struct run_s
{
  pthread_mutex_t lock;
  pthread_cond_t changed;
  struct timespec deadline;
  const char *directory;
  unsigned arrivals;
  unsigned generation;
  int first_error;
};

struct worker_s
{
  struct run_s *run;
  unsigned index;
  int cpu;
  int result;
  unsigned rounds;
  unsigned cpu_checks;
  unsigned written;
  unsigned verified;
};

static int path_join(char *path, const char *directory, const char *name)
{
  int n = snprintf(path, PATH_BYTES, "%s/%s", directory, name);
  return n < 0 || (size_t)n >= PATH_BYTES ? ENAMETOOLONG : 0;
}

static int guard(struct worker_s *worker)
{
  struct timespec now;
  if (worker == NULL) return 0;
  if (clock_gettime(CLOCK_MONOTONIC, &now) != 0) return errno;
  if (now.tv_sec > worker->run->deadline.tv_sec ||
      (now.tv_sec == worker->run->deadline.tv_sec &&
       now.tv_nsec >= worker->run->deadline.tv_nsec)) return ETIMEDOUT;
  if (sched_getcpu() != worker->cpu) return EXDEV;
  worker->cpu_checks++;
  return 0;
}

static int transfer(int fd, void *data, size_t length, off_t offset,
                    bool writing, struct worker_s *worker)
{
  size_t done = 0;
  while (done < length)
    {
      int ret = guard(worker);
      if (ret != 0) return ret;
      ssize_t n = writing ? pwrite(fd, (char *)data + done, length - done,
                                    offset + done) :
                            pread(fd, (char *)data + done, length - done,
                                  offset + done);
      if (n < 0)
        {
          if (errno == EINTR) continue;
          return errno;
        }

      if (n == 0) return EIO;
      done += n;
    }

  return 0;
}

static int close_result(int fd, int result)
{
  /* Do not retry close after EINTR: the descriptor may already be consumed. */
  int ret = close(fd);
  return result != 0 ? result : ret < 0 ? errno : 0;
}

static unsigned char pattern(unsigned owner, unsigned round, unsigned offset)
{
  uint32_t value = (offset * UINT32_C(2654435761)) ^
                   (owner * UINT32_C(0x923fd721)) ^
                   (round * UINT32_C(0x1b87e593));
  value ^= value >> 16;
  value ^= value >> 8;
  return (unsigned char)value;
}

static int regular_file(const char *path, off_t size)
{
  struct stat st;
  if (lstat(path, &st) != 0) return errno;
  if (!S_ISREG(st.st_mode) || (size >= 0 && st.st_size != size)) return EINVAL;
  return 0;
}

static int marker_check(const char *directory)
{
  struct stat st;
  char path[PATH_BYTES];
  char data[sizeof(g_marker) - 1];
  if (lstat(directory, &st) != 0) return errno;
  if (!S_ISDIR(st.st_mode)) return EINVAL;
  int ret = path_join(path, directory, g_files[0]);
  if (ret == 0) ret = regular_file(path, sizeof(data));
  if (ret != 0) return ret;
  int fd = open(path, O_RDONLY);
  if (fd < 0) return errno;
  ret = transfer(fd, data, sizeof(data), 0, false, NULL);
  if (ret == 0 && memcmp(data, g_marker, sizeof(data)) != 0) ret = EINVAL;
  return close_result(fd, ret);
}

static int create_directory(const char *directory)
{
  char path[PATH_BYTES];
  int ret = path_join(path, directory, g_files[0]);
  if (ret != 0) return ret;
  /* EEXIST is a refusal, including an earlier incomplete qualification. */
  if (mkdir(directory, 0700) != 0) return errno;
  int fd = open(path, O_CREAT | O_EXCL | O_WRONLY, 0600);
  if (fd < 0)
    {
      ret = errno;
      rmdir(directory);
      return ret;
    }

  ret = transfer(fd, (void *)g_marker, sizeof(g_marker) - 1, 0, true, NULL);
  if (ret == 0 && fsync(fd) != 0) ret = errno;
  ret = close_result(fd, ret);
  if (ret != 0)
    {
      unlink(path);
      rmdir(directory);
    }

  return ret;
}

static void abort_run(struct run_s *run, int error)
{
  pthread_mutex_lock(&run->lock);
  if (run->first_error == 0) run->first_error = error;
  pthread_cond_broadcast(&run->changed);
  pthread_mutex_unlock(&run->lock);
}

static int rendezvous(struct worker_s *worker)
{
  struct run_s *run = worker->run;
  int ret = guard(worker);
  if (ret != 0)
    {
      abort_run(run, ret);
      return ret;
    }

  pthread_mutex_lock(&run->lock);
  unsigned generation = run->generation;
  if (run->first_error == 0 && ++run->arrivals == 2)
    {
      run->arrivals = 0;
      run->generation++;
      pthread_cond_broadcast(&run->changed);
    }

  while (run->first_error == 0 && generation == run->generation)
    {
      ret = pthread_cond_timedwait(&run->changed, &run->lock, &run->deadline);
      if (ret != 0)
        {
          run->first_error = ret;
          pthread_cond_broadcast(&run->changed);
        }
    }

  ret = run->first_error;
  pthread_mutex_unlock(&run->lock);
  return ret;
}

static int write_file(struct worker_s *worker, unsigned round,
                      unsigned char *buffer)
{
  static const unsigned chunks[] = {8192, 4093, 257, 1};
  char path[PATH_BYTES];
  char final[PATH_BYTES];
  unsigned offset = 0;
  unsigned chunk = 0;
  int ret = path_join(path, worker->run->directory, g_files[3 + worker->index]);
  if (ret == 0) ret = path_join(final, worker->run->directory,
                              g_files[1 + worker->index]);
  if (ret != 0) return ret;
  int fd = open(path, O_CREAT | O_EXCL | O_WRONLY, 0600);
  if (fd < 0) return errno;
  while (offset < FILE_BYTES)
    {
      unsigned length = chunks[chunk++ % 4];
      if (length > FILE_BYTES - offset) length = FILE_BYTES - offset;
      for (unsigned n = 0; n < length; n++)
        buffer[n] = pattern(worker->index, round, offset + n);
      ret = transfer(fd, buffer, length, offset, true, worker);
      if (ret != 0) break;
      offset += length;
      worker->written += length;
    }

  if (ret == 0 && fsync(fd) != 0) ret = errno;
  ret = close_result(fd, ret);
  if (ret == 0 && rename(path, final) != 0) ret = errno;
  return ret;
}

static int verify_file(const char *directory, unsigned owner, unsigned round,
                       unsigned char *buffer, struct worker_s *worker)
{
  char path[PATH_BYTES];
  int ret = path_join(path, directory, g_files[1 + owner]);
  if (ret == 0) ret = regular_file(path, FILE_BYTES);
  if (ret != 0) return ret;
  int fd = open(path, O_RDONLY);
  if (fd < 0) return errno;
  for (unsigned offset = 0; offset < FILE_BYTES; )
    {
      unsigned length = FILE_BYTES - offset;
      if (length > 4093) length = 4093;
      ret = transfer(fd, buffer, length, offset, false, worker);
      if (ret != 0) break;
      for (unsigned n = 0; n < length; n++)
        {
          if (buffer[n] != pattern(owner, round, offset + n))
            {
              ret = EILSEQ;
              break;
            }
        }

      if (ret != 0) break;
      offset += length;
      if (worker != NULL) worker->verified += length;
    }

  return close_result(fd, ret);
}

static void *worker_main(void *arg)
{
  struct worker_s *worker = arg;
  cpu_set_t set;
  int ret = pthread_getaffinity_np(pthread_self(), sizeof(set), &set);
  if (ret == 0 && (!CPU_ISSET(worker->cpu, &set) || CPU_COUNT(&set) != 1))
    ret = EXDEV;
  unsigned char *storage = malloc(BUFFER_BYTES + 1);
  if (ret == 0 && storage == NULL) ret = ENOMEM;
  for (unsigned round = 0; ret == 0 && round < ROUNDS; round++)
    {
      ret = write_file(worker, round, storage + 1);
      if (ret == 0) ret = rendezvous(worker);
      if (ret == 0) ret = verify_file(worker->run->directory, 1 - worker->index,
                                     round, storage + 1, worker);
      if (ret == 0) ret = rendezvous(worker);
      if (ret == 0) worker->rounds++;
    }

  worker->result = ret;
  if (ret != 0) abort_run(worker->run, ret);
  free(storage);
  return NULL;
}

static int write_test(const char *directory, int cpu0, int cpu1)
{
  struct run_s run = {.directory = directory};
  struct worker_s workers[2] = {{.run=&run, .index=0, .cpu=cpu0},
                                {.run=&run, .index=1, .cpu=cpu1}};
  pthread_t threads[2];
  pthread_condattr_t condattr;
  pthread_attr_t attr;
  struct timespec begin;
  struct timespec end;
  int created = 0;
  int ret = clock_gettime(CLOCK_MONOTONIC, &begin) == 0 ? 0 : errno;
  if (ret != 0) return ret;
  run.deadline = begin;
  run.deadline.tv_sec += TIME_LIMIT;
  ret = pthread_mutex_init(&run.lock, NULL);
  if (ret != 0) return ret;
  ret = pthread_condattr_init(&condattr);
  if (ret != 0) goto destroy_mutex;
  ret = pthread_condattr_setclock(&condattr, CLOCK_MONOTONIC);
  if (ret == 0) ret = pthread_cond_init(&run.changed, &condattr);
  pthread_condattr_destroy(&condattr);
  if (ret != 0) goto destroy_mutex;
  ret = pthread_attr_init(&attr);
  if (ret != 0) goto destroy_cond;
  ret = pthread_attr_setstacksize(&attr, WORKER_STACK);
  for (int n = 0; ret == 0 && n < 2; n++)
    {
      cpu_set_t set;
      CPU_ZERO(&set);
      CPU_SET(workers[n].cpu, &set);
      ret = pthread_attr_setaffinity_np(&attr, sizeof(set), &set);
      if (ret == 0) ret = pthread_create(&threads[n], &attr, worker_main, &workers[n]);
      if (ret == 0) created++;
    }

  pthread_attr_destroy(&attr);
  if (ret != 0) abort_run(&run, ret);
  for (int n = 0; n < created; n++)
    {
      int joined = pthread_join(threads[n], NULL);
      if (ret == 0) ret = joined;
    }

  if (ret == 0) ret = run.first_error;
  if (ret == 0)
    {
      for (int n = 0; n < 2; n++)
        if (workers[n].result != 0 || workers[n].rounds != ROUNDS ||
            workers[n].written != ROUNDS * FILE_BYTES ||
            workers[n].verified != ROUNDS * FILE_BYTES) ret = EIO;
    }

  if (clock_gettime(CLOCK_MONOTONIC, &end) != 0)
    {
      if (ret == 0) ret = errno;
      end = begin;
    }

  printf("S31SDCHECK workers cpu=%d,%d rounds=%u,%u checks=%u,%u "
         "written=%u,%u verified=%u,%u errors=%d,%d elapsed_ms=%lld\n",
         cpu0, cpu1, workers[0].rounds, workers[1].rounds,
         workers[0].cpu_checks, workers[1].cpu_checks,
         workers[0].written, workers[1].written,
         workers[0].verified, workers[1].verified,
         workers[0].result, workers[1].result,
         (long long)(end.tv_sec - begin.tv_sec) * 1000 +
         (end.tv_nsec - begin.tv_nsec) / 1000000);
destroy_cond:
  pthread_cond_destroy(&run.changed);
destroy_mutex:
  pthread_mutex_destroy(&run.lock);
  return ret;
}

static int verify_test(const char *directory)
{
  int ret = marker_check(directory);
  if (ret != 0) return ret;
  unsigned char *storage = malloc(BUFFER_BYTES + 1);
  if (storage == NULL) return ENOMEM;
  ret = verify_file(directory, 0, ROUNDS - 1, storage + 1, NULL);
  if (ret == 0) ret = verify_file(directory, 1, ROUNDS - 1, storage + 1, NULL);
  free(storage);
  return ret;
}

static int clean_test(const char *directory)
{
  int ret = marker_check(directory);
  bool present[5] = {false};
  char path[PATH_BYTES];
  if (ret != 0) return ret;
  DIR *dir = opendir(directory);
  if (dir == NULL) return errno;
  for (;;)
    {
      errno = 0;
      struct dirent *entry = readdir(dir);
      if (entry == NULL)
        {
          if (errno != 0) ret = errno;
          break;
        }

      if (!strcmp(entry->d_name, ".") || !strcmp(entry->d_name, "..")) continue;
      unsigned n;
      for (n = 0; n < 5 && strcmp(entry->d_name, g_files[n]); n++);
      if (n == 5) { ret = EEXIST; break; }
      ret = path_join(path, directory, g_files[n]);
      if (ret == 0) ret = regular_file(path, -1);
      if (ret != 0) break;
      present[n] = true;
    }

  int closed = closedir(dir);
  if (ret == 0 && closed != 0) ret = errno;
  if (ret != 0) return ret;
  /* Keep the ownership marker until every synthetic data file is removed. */
  for (unsigned n = 1; n < 5; n++)
    {
      if (!present[n]) continue;
      ret = path_join(path, directory, g_files[n]);
      if (ret != 0) return ret;
      if (unlink(path) != 0) return errno;
    }

  ret = path_join(path, directory, g_files[0]);
  if (ret != 0) return ret;
  if (unlink(path) != 0) return errno;
  return rmdir(directory) == 0 ? 0 : errno;
}

static int parse_cpu(const char *value)
{
  char *end;
  errno = 0;
  long n = strtol(value, &end, 10);
  return errno || !value[0] || *end || n < 0 || n >= CPU_SETSIZE ? -1 : (int)n;
}

int main(int argc, char **argv)
{
  bool writing = argc == 5 && !strcmp(argv[1], "write");
  bool verifying = argc == 3 && !strcmp(argv[1], "verify");
  bool cleaning = argc == 3 && !strcmp(argv[1], "clean");
  if (!writing && !verifying && !cleaning)
    {
      fprintf(stderr, "Usage: s31sdcheck write NEW_DIRECTORY CPU0 CPU1 | "
                      "verify DIRECTORY | clean DIRECTORY\n");
      return 2;
    }

  const char *directory = argv[2];
  size_t len = strlen(directory);
  if (len < 2 || directory[0] != '/' || directory[len - 1] == '/' ||
      len > PATH_BYTES - 16) return 2;
  int ret;
  if (writing)
    {
      int cpu0 = parse_cpu(argv[3]);
      int cpu1 = parse_cpu(argv[4]);
      if (cpu0 < 0 || cpu1 < 0 || cpu0 == cpu1) return 2;
      ret = create_directory(directory);
      if (ret == 0) ret = write_test(directory, cpu0, cpu1);
    }
  else ret = verifying ? verify_test(directory) : clean_test(directory);
  printf("S31SDCHECK %s %s error=%d\n", argv[1], ret == 0 ? "PASS" : "FAIL", ret);
  return ret == 0 ? 0 : 1;
}
