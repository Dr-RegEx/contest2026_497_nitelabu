/****************************************************************************
 * apps/testing/drivers/drivertest/drivertest_spidev_slave.c
 *
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.  The
 * ASF licenses this file to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance with the
 * License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.  See the
 * License for the specific language governing permissions and limitations
 * under the License.
 *
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <stdint.h>
#include <ctype.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stddef.h>
#include <errno.h>
#include <getopt.h>
#include <fcntl.h>
#include <string.h>
#include <fcntl.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <setjmp.h>
#include <cmocka.h>
#include <nuttx/crc32.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define SPI_SLAVE_PATH          "/dev/spislv0"
#define SPISLV_TX_BUFSIZE       8
#define SPISLV_RX_BUFSIZE       8

/****************************************************************************
 * Private Type Declarations
 ****************************************************************************/

struct spislv_test_ctx_s
{
  char dev_path[PATH_MAX];
  int fd;
  void *tx_buffer;
  void *rx_buffer;
  int rx_size;
  int verbose;
};

/****************************************************************************
 * Name: print_usage
 ****************************************************************************/

static void print_usage(const char *prog)
{
  printf("Usage: %s [-d?]\n", prog);
  puts("  -d --device device to use (default /dev/spislv0)\n"
       "  -w --write char to transfer\n"
       "  -v --verbose  Verbose (show tx buffer)\n"
       "  -? --help print help\n"
       "\n");
  exit(1);
}

/****************************************************************************
 * Name: parse_commandline
 ****************************************************************************/

static void parse_commandline(int argc, FAR char **argv,
                              FAR struct spislv_test_ctx_s *spislv)
{
  int option;

  while ((option = getopt(argc, argv, "d:i:o:t:v")) != ERROR)
    {
      switch (option)
        {
          case 'd':
            snprintf(spislv->dev_path, sizeof(spislv->dev_path), "%s",
                     optarg);
            break;
          case 'w':
            spislv->tx_buffer = optarg;
            break;
          case 'v':
            spislv->verbose = 1;
            break;
          case '?':
            printf("Unknown option: %c\n", optopt);
            print_usage(argv[0]);
            break;
        }
    }
}

/****************************************************************************
 * Name: setup
 ****************************************************************************/

static int setup(FAR void **state)
{
  FAR struct spislv_test_ctx_s *spislv;

  spislv = (FAR struct spislv_test_ctx_s *)*state;
  spislv->fd = open(spislv->dev_path, O_RDWR);
  assert_false(spislv->fd < 0);

  if (spislv->verbose)
    {
      printf("Open:%s success\n", spislv->dev_path);
    }

  return 0;
}

/****************************************************************************
 * Name: teardown
 ****************************************************************************/

static int teardown(FAR void **state)
{
  FAR struct spislv_test_ctx_s *spislv;

  spislv = (FAR struct spislv_test_ctx_s *)*state;

  if (spislv->tx_buffer)
    {
      free(spislv->tx_buffer);
      spislv->tx_buffer = NULL;
    }

  if (spislv->rx_buffer)
    {
      free(spislv->rx_buffer);
      spislv->rx_buffer = NULL;
    }

  close(spislv->fd);
  return 0;
}

/****************************************************************************
 * Name: spidev_slave_write
 ****************************************************************************/

static int spidev_slave_write(FAR struct spislv_test_ctx_s *spislv,
                              void *buffer, int size)
{
  struct pollfd rfds;
  char *tmp_buf = buffer;
  int tmp_size = size;
  int buf_size;
  int count;
  int icnt = 0;
  int ret;

  assert_false(size <= 0);

  rfds.fd = spislv->fd;
  count = (size + SPISLV_TX_BUFSIZE - 1) / SPISLV_TX_BUFSIZE;
  for (icnt = 0; icnt < count; icnt++)
    {
      buf_size = (tmp_size - SPISLV_TX_BUFSIZE) > 0 ?
                  SPISLV_TX_BUFSIZE : tmp_size;
      ret = write(rfds.fd, tmp_buf, buf_size);
      assert_false(ret < 0);

      rfds.events = POLLOUT;
      rfds.revents = 0;

      while (true)
        {
          int n = poll(&rfds, 1, -1);
          if (n == 0)
            {
              printf("spi slave write time out\n");
              return -1;
            }

          if (n < 0)
            {
              printf("spi slave poll err n= %d\n", n);
              return -1;
            }

          if (rfds.revents == POLLOUT)
            {
              break;
            }
        }

      if (spislv->verbose)
        {
          printf("slave write: \t");
          for (int i = 0; i < buf_size; i++)
            {
              printf("0x%x\t", tmp_buf[i]);
            }

          printf("\n");
        }

      tmp_buf += buf_size;
    }

  return 0;
}

/****************************************************************************
 * Name: spidev_slave_read
 ****************************************************************************/

static int spidev_slave_read(FAR struct spislv_test_ctx_s *spislv,
                             void *buffer, int size)
{
  struct pollfd rfds;
  int read_total;
  int icnt;
  int total_size = size;
  int tmp_size = 0;
  int read_size = 0;
  char *tmp_buf = buffer;

  rfds.fd = spislv->fd;
  rfds.events = POLLIN;
  rfds.revents = 0;

  read_total = (size + SPISLV_RX_BUFSIZE - 1) / SPISLV_RX_BUFSIZE;
  for (icnt = 0; icnt < read_total; icnt++)
    {
      int n = poll(&rfds, 1, -1);
      if (n == 0)
        {
          printf("spislv read data time out\n");
          return -1;
        }

      if (n < 0)
        {
          printf("spislv read data poll err n= %d\n", n);
          return -1;
        }

      if (rfds.revents == POLLIN)
        {
          tmp_size = (total_size - SPISLV_RX_BUFSIZE) > 0 ?
                      SPISLV_RX_BUFSIZE : total_size;
          read_size = read(rfds.fd, tmp_buf, tmp_size);
        }

      if (spislv->verbose)
        {
          printf("slave read: \t");
          for (int i = 0; i < tmp_size; i++)
            {
              printf("0x%x\t", tmp_buf[i]);
            }

          printf("\n");
        }

      tmp_buf += read_size;
    }

  return 0;
}

/****************************************************************************
 * Name: txbuffer_fill_init
 ****************************************************************************/

static void txbuffer_fill_init(FAR char *addr, size_t size)
{
  FAR char *tmp = addr;
  int idx;

  for (idx = 0; idx < size; idx++)
    {
      *(tmp + idx) = idx % 255;
    }
}

/****************************************************************************
 * Name: drivertest_spidev_read
 ****************************************************************************/

static void drivertest_spidev_read(FAR void **state)
{
  FAR struct spislv_test_ctx_s *spislv;
  int read_total = -1;
  int crc_check = 0;
  int crc32;
  int ret;

  spislv = (FAR struct spislv_test_ctx_s *)*state;

  /* get read data size */

  sleep(1);
  ret = spidev_slave_read(spislv, &read_total, sizeof(int));
  assert_false(ret < 0);

  /* read data */

  spislv->rx_buffer = malloc(read_total);
  memset(spislv->rx_buffer, 0, read_total);
  spislv->rx_size = read_total;
  ret = spidev_slave_read(spislv, spislv->rx_buffer, read_total);
  assert_false(ret < 0);

  /* get read crc32 */

  sleep(1);
  ret = spidev_slave_read(spislv, &crc_check, sizeof(int));
  assert_false(ret < 0);
  crc32 = crc32part(spislv->rx_buffer, spislv->rx_size, ~0);
  if (spislv->verbose)
    {
      printf("crc32: 0x%x, crc_check: 0x%x\n", crc32, crc_check);
    }

  assert_int_equal(crc32, crc_check);
}

/****************************************************************************
 * Name: drivertest_spidev_write
 ****************************************************************************/

static void drivertest_spidev_write(FAR void **state)
{
  FAR struct spislv_test_ctx_s *spislv;
  int write_byte;
  int crc32 = 0;
  int ret;

  spislv = (FAR struct spislv_test_ctx_s *)*state;

  /* get write data size */

  ret = spidev_slave_read(spislv, &write_byte, sizeof(int));
  assert_false(ret < 0);

  /* fill init data, then write data */

  sleep(1);
  spislv->tx_buffer = malloc(write_byte);
  txbuffer_fill_init(spislv->tx_buffer, write_byte);
  ret = spidev_slave_write(spislv, spislv->tx_buffer, write_byte);
  assert_false(ret < 0);

  /* trans crc */

  sleep(1);
  crc32 = crc32part(spislv->tx_buffer, write_byte, ~0);
  if (spislv->verbose)
    {
      printf("crc32: 0x%x\n", crc32);
    }

  ret = spidev_slave_write(spislv, &crc32, sizeof(int));
  assert_false(ret < 0);
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: main
 ****************************************************************************/

int main(int argc, char *argv[])
{
  struct spislv_test_ctx_s spislv;

  memset(&spislv, 0, sizeof(struct spislv_test_ctx_s));
  snprintf(spislv.dev_path, sizeof(spislv.dev_path), "%s",
           SPI_SLAVE_PATH);
  spislv.verbose = 1;
  parse_commandline(argc, argv, &spislv);

  const struct CMUnitTest tests[] =
  {
    cmocka_unit_test_prestate_setup_teardown(drivertest_spidev_read, setup,
                                             teardown, &spislv),
    cmocka_unit_test_prestate_setup_teardown(drivertest_spidev_write, setup,
                                             teardown, &spislv),
  };

  return cmocka_run_group_tests(tests, NULL, NULL);
}
