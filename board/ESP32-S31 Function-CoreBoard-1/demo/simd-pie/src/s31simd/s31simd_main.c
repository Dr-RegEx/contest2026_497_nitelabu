/* SPDX-License-Identifier: Apache-2.0 */
#include <nuttx/config.h>
#include <pthread.h>
#include <sched.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

extern void s31simd_add(const uint32_t *, const uint32_t *, uint32_t *);
extern int32_t s31simd_dot16(const int8_t *, const int8_t *);
extern int s31simd_hold(const uint32_t *, uint32_t *);

struct result_s
{
  unsigned int id;
  unsigned int completed;
  int error;
};

static void *run_worker(void *arg)
{
  struct result_s *r = arg;
  uint32_t input[56] __attribute__((aligned(16)));
  uint32_t output[56] __attribute__((aligned(16)));
  uint32_t other[4] __attribute__((aligned(16))) = {1, 7, 19, 31};
  uint32_t sum[4] __attribute__((aligned(16)));
  int8_t features[16] __attribute__((aligned(16)));
  int8_t weights[16] __attribute__((aligned(16)));
  int32_t scalar;
  int32_t vector;
  cpu_set_t cpus;
  unsigned int i;
  unsigned int round;

  printf("SIMD worker=%u initial_cpu=%d\n", r->id, sched_getcpu());
  for (round = 0; round < 20; round++)
    {
      CPU_ZERO(&cpus);
      CPU_SET(round % 2, &cpus);
      r->error = pthread_setaffinity_np(pthread_self(), sizeof(cpus), &cpus);
      if (r->error != 0 || sched_getcpu() != (int)(round % 2))
        {
          r->error = 4;
          return NULL;
        }
    }

  printf("SIMD worker=%u affinity_switches=20 PASS\n", r->id);
  CPU_ZERO(&cpus);
  CPU_SET(1, &cpus);
  r->error = pthread_setaffinity_np(pthread_self(), sizeof(cpus), &cpus);
  if (r->error != 0)
    {
      return NULL;
    }

  printf("SIMD worker=%u cpu=%d\n", r->id, sched_getcpu());
  if (sched_getcpu() != 1)
    {
      r->error = 3;
      return NULL;
    }

  for (round = 0; round < 100; round++)
    {
      for (i = 0; i < 56; i++)
        {
          input[i] = 0x12340000 + (r->id << 16) + round * 32 + i;
        }

      /* XACC occupies five bytes; SAR uses six bits. */

      ((uint8_t *)input)[214] &= 0x3f;
      memset((uint8_t *)input + 215, 0, 9);
      s31simd_add(input, other, sum);
      for (i = 0; i < 4; i++)
        {
          if (sum[i] != input[i] + other[i])
            {
              r->error = 1;
              return NULL;
            }
        }

      scalar = 0;
      for (i = 0; i < 16; i++)
        {
          features[i] = (int)((round * 17 + i * 13 + r->id) % 256) - 128;
          weights[i] = (int)((round * 11 + i * 7) % 256) - 128;
          scalar += (int32_t)features[i] * weights[i];
        }

      vector = s31simd_dot16(features, weights);
      if (vector != scalar)
        {
          printf("INT8 dot mismatch worker=%u round=%u scalar=%ld simd=%ld\n",
                 r->id, round, (long)scalar, (long)vector);
          r->error = 5;
          return NULL;
        }

      memset(output, 0, sizeof(output));
      if (s31simd_hold(input, output) != 0 ||
          memcmp(input, output, sizeof(input)) != 0)
        {
          r->error = 2;
          return NULL;
        }

      r->completed++;
    }

  printf("SIMD INT8 dot16 worker=%u checked=100 PASS\n", r->id);
  return NULL;
}

int main(int argc, char *argv[])
{
  struct result_s results[2] = {{.id = 1}, {.id = 2}};
  pthread_t threads[2];
  pthread_attr_t attr;
  cpu_set_t cpus;
  int created = 0;
  int ret;
  int i;
  int failed = 0;

  ret = pthread_attr_init(&attr);
  if (ret != 0)
    {
      return 1;
    }

  CPU_ZERO(&cpus);
  CPU_SET(0, &cpus);
  ret = pthread_attr_setaffinity_np(&attr, sizeof(cpus), &cpus);
  if (ret == 0)
    {
      ret = pthread_attr_setstacksize(&attr, 8192);
    }
  if (ret == 0)
    {
      for (i = 0; i < 2; i++)
        {
          ret = pthread_create(&threads[i], &attr, run_worker, &results[i]);
          if (ret != 0)
            {
              break;
            }

          created++;
        }
    }

  pthread_attr_destroy(&attr);
  for (i = 0; i < created; i++)
    {
      if (pthread_join(threads[i], NULL) != 0)
        {
          failed = 1;
        }

      printf("SIMD CPU1 worker=%u completed=%u error=%d\n",
             results[i].id, results[i].completed, results[i].error);
      failed |= results[i].error != 0 || results[i].completed != 100;
    }

  failed |= created != 2;
  printf("SIMD arithmetic and full-bank sleep-switch: %s\n",
         failed ? "FAIL" : "PASS");
  return failed ? 1 : 0;
}
