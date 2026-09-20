/****************************************************************************
 * apps/testing/drivers/drivertest/drivertest_i2cdev_slave.c
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

#define I2C_SLAVE_PATH        "/dev/i2cslv0"
#define I2C_WRITEBUFSIZE      4
#define I2C_SLAVE_WRITEBUFSIZE 4

/****************************************************************************
 * Private Type Declarations
 ****************************************************************************/

struct i2cslv_test_ctx_s
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
  puts("  -d --device device to use (default /dev/i2cslv0)\n"
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
                              FAR struct i2cslv_test_ctx_s *i2cslv)
{
  int option;

  while ((option = getopt(argc, argv, "d:i:o:t:v")) != ERROR)
    {
      switch (option)
        {
          case 'd':
            snprintf(i2cslv->dev_path, sizeof(i2cslv->dev_path), "%s",
                     optarg);
            break;
          case 'w':
            i2cslv->tx_buffer = optarg;
            break;
          case 'v':
            i2cslv->verbose = 1;
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
  FAR struct i2cslv_test_ctx_s *i2cslv;

  i2cslv = (FAR struct i2cslv_test_ctx_s *)*state;
  i2cslv->fd = open(i2cslv->dev_path, O_RDWR);
  assert_false(i2cslv->fd < 0);

  if (i2cslv->verbose)
    {
      printf("Open:%s success\n", i2cslv->dev_path);
    }

  return 0;
}

/****************************************************************************
 * Name: teardown
 ****************************************************************************/

static int teardown(FAR void **state)
{
  FAR struct i2cslv_test_ctx_s *i2cslv;

  i2cslv = (FAR struct i2cslv_test_ctx_s *)*state;
  if (i2cslv->tx_buffer != NULL)
    {
      free(i2cslv->tx_buffer);
    }

  if (i2cslv->rx_buffer != NULL)
    {
      free(i2cslv->rx_buffer);
    }

  close(i2cslv->fd);
  return 0;
}

/****************************************************************************
 * Name: i2cdev_slave_write
 ****************************************************************************/

static int i2cdev_slave_write(FAR struct i2cslv_test_ctx_s *i2cslv,
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

  rfds.fd = i2cslv->fd;
  count = (size + I2C_SLAVE_WRITEBUFSIZE - 1) / I2C_SLAVE_WRITEBUFSIZE;
  for (icnt = 0; icnt < count; icnt++)
    {
      buf_size = (tmp_size - I2C_SLAVE_WRITEBUFSIZE) > 0 ?
                  I2C_SLAVE_WRITEBUFSIZE : tmp_size;
      ret = write(rfds.fd, tmp_buf, buf_size);
      assert_false(ret < 0);

      rfds.events = POLLOUT;
      rfds.revents = 0;

      while (true)
        {
          int n = poll(&rfds, 1, -1);
          if (n == 0)
            {
              printf("i2c slave write time out\n");
              return -1;
            }

          if (n < 0)
            {
              printf("i2c slave poll err n= %d\n", n);
              return -1;
            }

          if (rfds.revents == POLLOUT)
            {
              printf("i2c slave can write success\n");
              break;
            }
        }

      if (i2cslv->verbose)
        {
          printf("slave write: \t");
          for (int i = 0; i < I2C_SLAVE_WRITEBUFSIZE; i++)
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
 * Name: i2cdev_slave_read
 ****************************************************************************/

static int i2cdev_slave_read(FAR struct i2cslv_test_ctx_s *i2cslv,
                             void *buffer, int size)
{
  struct pollfd rfds;
  int read_total;
  int icnt;
  int total_size = size;
  int tmp_size = 0;
  int read_size = 0;
  char *tmp_buf = buffer;
  int i;

  rfds.fd = i2cslv->fd;
  rfds.events = POLLIN;
  rfds.revents = 0;

  read_total = (size + I2C_WRITEBUFSIZE - 1) / I2C_WRITEBUFSIZE;
  for (icnt = 0; icnt < read_total; icnt++)
    {
      int n = poll(&rfds, 1, -1);
      if (n == 0)
        {
          printf("i2cslv read data time out\n");
          return -1;
        }

      if (n < 0)
        {
          printf("i2cslv read data poll err n= %d\n", n);
          return -1;
        }

      if (rfds.revents == POLLIN)
        {
          tmp_size = (total_size - I2C_WRITEBUFSIZE) > 0 ?
                      I2C_WRITEBUFSIZE : total_size;
          read_size = read(rfds.fd, tmp_buf, tmp_size);
        }

      if (i2cslv->verbose)
        {
          printf("slave read: \t");
          for (i = 0; i < I2C_WRITEBUFSIZE; i++)
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
 * Name: drivertest_i2cdev_read
 ****************************************************************************/

static void drivertest_i2cdev_read(FAR void **state)
{
  FAR struct i2cslv_test_ctx_s *i2cslv;
  int read_total = -1;
  int crc_check = 0;
  int crc32;
  int ret;

  i2cslv = (FAR struct i2cslv_test_ctx_s *)*state;

  /* get read data size */

  sleep(1);
  ret = i2cdev_slave_read(i2cslv, &read_total, sizeof(int));
  assert_false(ret < 0);

  /* read data */

  i2cslv->rx_buffer = malloc(read_total);
  memset(i2cslv->rx_buffer, 0, read_total);
  i2cslv->rx_size = read_total;
  ret = i2cdev_slave_read(i2cslv, i2cslv->rx_buffer, read_total);
  assert_false(ret < 0);

  /* get read crc32 */

  sleep(1);
  ret = i2cdev_slave_read(i2cslv, &crc_check, sizeof(int));
  assert_false(ret < 0);
  crc32 = crc32part(i2cslv->rx_buffer, i2cslv->rx_size, ~0);

  if (i2cslv->verbose)
    {
      printf("crc32: 0x%x, crc_check: 0x%x\n", crc32, crc_check);
    }

  assert_int_equal(crc32, crc_check);
}

/****************************************************************************
 * Name: drivertest_i2cdev_write
 ****************************************************************************/

static void drivertest_i2cdev_write(FAR void **state)
{
  FAR struct i2cslv_test_ctx_s *i2cslv;
  int write_byte;
  int crc32 = 0;
  int ret;

  i2cslv = (FAR struct i2cslv_test_ctx_s *)*state;

  /* get write data size */

  ret = i2cdev_slave_read(i2cslv, &write_byte, sizeof(int));
  printf("write_byte: %d\n", write_byte);
  assert_false(ret < 0);

  /* fill init data, then write data */

  sleep(1);
  i2cslv->tx_buffer = malloc(write_byte);
  txbuffer_fill_init(i2cslv->tx_buffer, write_byte);
  ret = i2cdev_slave_write(i2cslv, i2cslv->tx_buffer, write_byte);
  assert_false(ret < 0);

  /* trans crc */

  sleep(1);
  crc32 = crc32part(i2cslv->tx_buffer, write_byte, ~0);
  if (i2cslv->verbose)
    {
      printf("crc32: 0x%x\n", crc32);
    }

  ret = i2cdev_slave_write(i2cslv, &crc32, sizeof(int));
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
  struct i2cslv_test_ctx_s i2cslv;

  memset(&i2cslv, 0, sizeof(struct i2cslv_test_ctx_s));
  snprintf(i2cslv.dev_path, sizeof(i2cslv.dev_path), "%s",
           I2C_SLAVE_PATH);
  i2cslv.verbose = 1;
  parse_commandline(argc, argv, &i2cslv);

  const struct CMUnitTest tests[] =
  {
    cmocka_unit_test_prestate_setup_teardown(drivertest_i2cdev_read, setup,
                                             teardown, &i2cslv),
    cmocka_unit_test_prestate_setup_teardown(drivertest_i2cdev_write, setup,
                                             teardown, &i2cslv),
  };

  return cmocka_run_group_tests(tests, NULL, NULL);
}
