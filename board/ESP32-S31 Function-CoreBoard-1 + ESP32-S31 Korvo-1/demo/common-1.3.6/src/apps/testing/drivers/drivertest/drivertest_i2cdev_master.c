/****************************************************************************
 * apps/testing/drivers/drivertest/drivertest_i2cdev_master.c
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
#include <sys/stat.h>
#include <sys/types.h>
#include <setjmp.h>
#include <cmocka.h>
#include <nuttx/i2c/i2c_master.h>
#include <nuttx/crc32.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define I2C_MASTER_PATH         "/dev/i2c0"
#define I2C_WRITEBUFSIZE        4
#define I2C_SLAVE_WRITEBUFSIZE  4

/****************************************************************************
 * Private Type Declarations
 ****************************************************************************/

struct i2c_test_ctx_s
{
  char dev_path[PATH_MAX];
  int fd;
  char *input_file;
  char *output_file;
  void *tx_buffer;
  void *rx_buffer;
  int tx_size;
  int rx_size;
  char slave_addr;
  int frequency;
  int verbose;
};

static const char g_default_tx[] =
{
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0x40, 0x00, 0x00, 0x00, 0x00, 0x95,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xf0, 0x0d,
};

/****************************************************************************
 * Name: print_usage
 ****************************************************************************/

static void print_usage(const char *prog)
{
  printf("Usage: %s [-d?]\n", prog);
  puts("  -d --device device to use (default /dev/i2c0)\n"
       "  -i --read file to transfer\n"
       "  -o --receive data to output file\n"
       "  -w --write char to transfer\n"
       "  -a --slave addr set, eg: 0xab\n"
       "  -s --size to read t, eg: 0x1000\n"
       "  -f --frequency set\n"
       "  -v --verbose  Verbose (show tx buffer)\n"
       "  -? --help print help\n"
       "\n");
  exit(1);
}

/****************************************************************************
 * Name: parse_commandline
 ****************************************************************************/

static void parse_commandline(int argc, FAR char **argv,
                              FAR struct i2c_test_ctx_s *i2c)
{
  int option;
  char *end_ptr;

  while ((option = getopt(argc, argv, "d:i:o:t:a:f:s:v")) != ERROR)
    {
      switch (option)
        {
          case 'd':
            snprintf(i2c->dev_path, sizeof(i2c->dev_path), "%s",
                     optarg);
            break;
          case 'w':
            i2c->tx_buffer = optarg;
            break;
          case 'a':
            i2c->slave_addr = strtol(optarg, &end_ptr, 16);
            break;
          case 's':
            i2c->rx_size = strtol(optarg, &end_ptr, 16);
            break;
          case 'f':
            i2c->frequency = atoi(optarg);
            break;
          case 'v':
            i2c->verbose = 1;
            break;
          case 'i':
            i2c->input_file = optarg;
            break;
          case 'o':
            i2c->output_file = optarg;
            break;
          case '?':
            printf("Unknown option: %c\n", optopt);
            print_usage(argv[0]);
            break;
        }
    }
}

/****************************************************************************
 * Name: str_unescape
 * Unescape - process hexadecimal escape character
 *      converts shell input "\x23" -> 0x23
 ****************************************************************************/

static int str_unescape(char *dst, char *src)
{
  int ret = 0;
  unsigned int ch;

  while (*src)
    {
      if (*src == '\\' && *(src + 1) == 'x')
        {
          if (sscanf(src + 2, "%2x", &ch) != 1)
            {
              printf("malformed input string");
              return -1;
            }

          src += 4;
          *dst++ = (unsigned char)ch;
        }
      else
        {
          *dst++ = *src++;
        }

      ret++;
    }

  return ret;
}

/****************************************************************************
 * Name: setup
 ****************************************************************************/

static int setup(FAR void **state)
{
  FAR struct i2c_test_ctx_s *i2c;

  i2c = (FAR struct i2c_test_ctx_s *)*state;
  i2c->fd = open(i2c->dev_path, O_RDWR);
  assert_false(i2c->fd < 0);

  if (i2c->verbose)
    {
      printf("Open:%s success\n", i2c->dev_path);
    }

  return 0;
}

/****************************************************************************
 * Name: teardown
 ****************************************************************************/

static int teardown(FAR void **state)
{
  FAR struct i2c_test_ctx_s *i2c;

  i2c = (FAR struct i2c_test_ctx_s *)*state;
  if (i2c->tx_buffer != NULL)
    {
      free(i2c->tx_buffer);
    }

    if (i2c->rx_buffer != NULL)
    {
      free(i2c->rx_buffer);
    }

  close(i2c->fd);
  return 0;
}

/****************************************************************************
 * Name: i2c_transfer_file
 ****************************************************************************/

static int i2c_msg_transfer(FAR struct i2c_test_ctx_s *i2c, void *buffer,
                            int size, int flags)
{
  struct i2c_transfer_s i2c_transfer;
  struct i2c_msg_s i2c_msg[2];
  uint8_t *tmp_buf = buffer;
  int ret;
  int i;

  i2c_msg[0].addr = i2c->slave_addr;
  i2c_msg[0].flags = flags;
  i2c_msg[0].buffer = buffer;
  i2c_msg[0].length = size;
  i2c_msg[0].frequency = i2c->frequency;

  i2c_transfer.msgv = (struct i2c_msg_s *)i2c_msg;
  i2c_transfer.msgc = 1;

  ret = ioctl(i2c->fd, I2CIOC_TRANSFER, (unsigned long)&i2c_transfer);
  assert_false(ret < 0);

  if (i2c->verbose && !i2c->output_file)
    {
      for (i = 0; i < size; i++)
      {
        printf("0x%x\t", tmp_buf[i]);
      }
      printf("\n");
    }

  return 0;
}

/****************************************************************************
 * Name: i2cdev_master_read
 ****************************************************************************/

static int i2cdev_master_read(FAR struct i2c_test_ctx_s *i2c, void *buffer,
                              int size)
{
  uint8_t *tmp_buf = buffer;
  int trans_size = size;
  int tmp_size;
  int count;
  int ret;
  int i;

  assert_false (size < 0);

  count = (size + I2C_SLAVE_WRITEBUFSIZE - 1) / I2C_SLAVE_WRITEBUFSIZE;
  for (i = 0; i < count; i++)
    {
      sleep(1);
      tmp_size = (trans_size - I2C_SLAVE_WRITEBUFSIZE) > 0 ?
                 I2C_SLAVE_WRITEBUFSIZE : trans_size;
      ret = i2c_msg_transfer(i2c, tmp_buf, tmp_size, 1);
      assert_false(ret < 0);
      tmp_buf += tmp_size;
      trans_size -= tmp_size;
    }

  return 0;
}

/****************************************************************************
 * Name: drivertest_i2cdev_read
 ****************************************************************************/

static int i2cdev_transfer_read(FAR struct i2c_test_ctx_s *i2c)
{
  int trans_size = i2c->rx_size;
  int crc32;
  int crc_check;
  int ret;

  if (i2c->rx_size <= 0)
    {
      return -1;
    }

  /* trans size to read */

  printf("trans size to read %d\n", trans_size);
  sleep(1);
  ret = i2c_msg_transfer(i2c, &trans_size, sizeof(int), 0);
  assert_false(ret < 0);

  /* read data from slave */

  sleep(1);
  printf("read data from slave\n");
  i2c->rx_buffer = (char *)malloc(i2c->rx_size);
  memset(i2c->rx_buffer, 0, i2c->rx_size);
  ret = i2cdev_master_read(i2c, i2c->rx_buffer, i2c->rx_size);
  assert_false(ret < 0);

  /* get read crc32 */

  sleep(1);
  printf("get read crc32\n");
  ret = i2cdev_master_read(i2c, &crc_check, sizeof(int));
  assert_false(ret < 0);
  crc32 = crc32part((uint8_t *)i2c->rx_buffer, i2c->rx_size, ~0);

  if (i2c->verbose)
    {
      printf("crc32: 0x%x, crc_check: 0x%x\n", crc32, crc_check);
    }

  assert_int_equal(crc32, crc_check);

  return 0;
}

/****************************************************************************
 * Name: drivertest_i2cdev_read
 ****************************************************************************/

static void drivertest_i2cdev_read(FAR void **state)
{
  FAR struct i2c_test_ctx_s *i2c;
  int ret;

  i2c = (FAR struct i2c_test_ctx_s *)*state;

  assert_false(i2c->rx_size <= 0);

  ret = i2cdev_transfer_read(i2c);
  assert_false(ret < 0);
}

/****************************************************************************
 * Name: i2c_transfer_file
 ****************************************************************************/

static int i2cdev_master_write(FAR struct i2c_test_ctx_s *i2c)
{
  uint8_t *buffer = i2c->tx_buffer;
  int trans_size = i2c->tx_size;
  int count = 0;
  int size;
  int crc32;
  int ret;
  int i;

  if (i2c->tx_size <= 0)
    {
      return -1;
    }

  /* trans write count */

  ret = i2c_msg_transfer(i2c, &trans_size, sizeof(int), 0);
  assert_false(ret < 0);

  /* trans write data */

  count = (i2c->tx_size + I2C_WRITEBUFSIZE - 1) / I2C_WRITEBUFSIZE;
  for (i = 0; i < count; i++)
    {
      sleep(1);
      size = (trans_size - I2C_WRITEBUFSIZE) > 0 ?
              I2C_WRITEBUFSIZE : trans_size;
      ret = i2c_msg_transfer(i2c, buffer, size, 0);
      assert_false(ret < 0);

      buffer += size;
      trans_size -= size;
    }

  /* trans write data crc */

  sleep(1);
  crc32 = crc32part((uint8_t *)i2c->tx_buffer, i2c->tx_size, ~0);
  if (i2c->verbose)
    {
      printf("crc32: 0x%x\n", crc32);
    }

  ret = i2c_msg_transfer(i2c, (uint8_t *)&crc32, sizeof(int), 0);
  assert_false(ret < 0);

  return 0;
}

/****************************************************************************
 * Name: i2c_transfer_file
 ****************************************************************************/

static int i2c_transfer_file(FAR struct i2c_test_ctx_s *i2c)
{
  ssize_t bytes;
  struct stat sb;
  int tx_fd;
  int ret;

  ret = stat(i2c->input_file, &sb);
  assert_false(ret < 0);

  tx_fd = open(i2c->input_file, O_RDONLY);
  assert_false(tx_fd < 0);

  i2c->tx_buffer = malloc(sb.st_size);
  if (!i2c->tx_buffer)
    {
      return -ENOMEM;
    }

  bytes = read(tx_fd, i2c->tx_buffer, sb.st_size);
  assert_int_equal(bytes, sb.st_size);

  i2c->tx_size = bytes;
  ret = i2cdev_master_write(i2c);
  assert_false(ret < 0);

  close(tx_fd);

  return 0;
}

/****************************************************************************
 * Name: i2c_transfer_buffer
 ****************************************************************************/

static int i2c_transfer_buffer(FAR struct i2c_test_ctx_s *i2c)
{
  int ret;
  char *tx_str = i2c->tx_buffer;
  size_t size = strlen(tx_str);

  i2c->tx_buffer = malloc(size);
  if (!i2c->tx_buffer)
    return -ENOMEM;

  size = str_unescape((char *)i2c->tx_buffer, tx_str);
  if (i2c->verbose)
    printf("tx size %d\n", size);

  i2c->tx_size = size;
  ret = i2cdev_master_write(i2c);
  assert_false(ret < 0);

  return 0;
}

/****************************************************************************
 * Name: i2c_transfer_default
 ****************************************************************************/

static int i2c_transfer_default(FAR struct i2c_test_ctx_s *i2c)
{
  int ret;
  size_t size = sizeof(g_default_tx);

  i2c->tx_buffer = malloc(size);
  if (!i2c->tx_buffer)
    return -ENOMEM;

  memcpy(i2c->tx_buffer, g_default_tx, size);
  i2c->tx_size = size;
  ret = i2cdev_master_write(i2c);
  assert_false(ret < 0);

  return 0;
}

/****************************************************************************
 * Name: drivertest_i2cdev_write
 ****************************************************************************/

static void drivertest_i2cdev_write(FAR void **state)
{
  FAR struct i2c_test_ctx_s *i2c;
  int ret;

  i2c = (FAR struct i2c_test_ctx_s *)*state;

  if (i2c->input_file)
    {
      ret = i2c_transfer_file(i2c);
    }
  else if (i2c->tx_buffer)
    {
      ret = i2c_transfer_buffer(i2c);
    }
  else
    {
      ret = i2c_transfer_default(i2c);
    }

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
  struct i2c_test_ctx_s i2c;

  memset(&i2c, 0, sizeof(struct i2c_test_ctx_s));
  snprintf(i2c.dev_path, sizeof(i2c.dev_path), "%s",
           I2C_MASTER_PATH);
  i2c.frequency = 100000;
  i2c.verbose = 1;
  i2c.slave_addr = 0x28;
  i2c.rx_size = 32;
  parse_commandline(argc, argv, &i2c);

  const struct CMUnitTest tests[] =
  {
    cmocka_unit_test_prestate_setup_teardown(drivertest_i2cdev_read, setup,
                                             teardown, &i2c),
    cmocka_unit_test_prestate_setup_teardown(drivertest_i2cdev_write, setup,
                                             teardown, &i2c),
  };

  return cmocka_run_group_tests(tests, NULL, NULL);
}
