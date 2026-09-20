/****************************************************************************
 * apps/testing/drivers/drivertest/drivertest_spidev_master.c
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
#include <nuttx/spi/spi_transfer.h>
#include <nuttx/crc32.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define SPI_MASTER_PATH         "/dev/spi0"
#define SPI_TX_BUFSIZE          8
#define SPI_RX_BUFSIZE          8

/****************************************************************************
 * Private Type Declarations
 ****************************************************************************/

struct spi_test_ctx_s
{
  char dev_path[PATH_MAX];
  int fd;
  char *input_file;
  char *output_file;
  void *tx_buffer;
  void *rx_buffer;
  int trans_size;
  int bits;
  int loop;
  int speed;
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
  puts("  -d --device device to use (default /dev/spi0)\n"
       "  -i --read file to transfer\n"
       "  -o --receive data to output file\n"
       "  -b --bits per word\n"
       "  -w --send data (e.g. \"1234\\xde\\xad\")\n"
       "  -l --loopback\n"
       "  -s --speed set\n"
       "  -S --size data to transfer\n"
       "  -v --verbose  Verbose (show tx buffer)\n"
       "  -? --help print help\n"
       "\n");
  exit(1);
}

/****************************************************************************
 * Name: parse_commandline
 ****************************************************************************/

static void parse_commandline(int argc, FAR char **argv,
                              FAR struct spi_test_ctx_s *spi)
{
  int option;
  char *end_ptr;

  while ((option = getopt(argc, argv, "d:i:o:b:w:S:s:lv")) != ERROR)
    {
      switch (option)
        {
          case 'd':
            snprintf(spi->dev_path, sizeof(spi->dev_path), "%s",
                     optarg);
            break;
          case 'w':
            spi->tx_buffer = optarg;
            break;
          case 'b':
            spi->bits = atoi(optarg);
            break;
          case 'S':
            spi->trans_size = strtol(optarg, &end_ptr, 16);
            break;
          case 'l':
            spi->loop = 1;
            break;
          case 's':
            spi->speed = atoi(optarg);
            break;
          case 'v':
            spi->verbose = 1;
            break;
          case 'i':
            spi->input_file = optarg;
            break;
          case 'o':
            spi->output_file = optarg;
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
  FAR struct spi_test_ctx_s *spi;

  spi = (FAR struct spi_test_ctx_s *)*state;
  spi->fd = open(spi->dev_path, O_RDWR);
  assert_false(spi->fd < 0);

  if (spi->verbose)
    {
      printf("Open:%s success\n", spi->dev_path);
    }

  return 0;
}

/****************************************************************************
 * Name: teardown
 ****************************************************************************/

static int teardown(FAR void **state)
{
  FAR struct spi_test_ctx_s *spi;

  spi = (FAR struct spi_test_ctx_s *)*state;
  if (spi->tx_buffer != NULL)
    {
      free(spi->tx_buffer);
      spi->tx_buffer = NULL;
    }

  if (spi->rx_buffer != NULL)
    {
      free(spi->rx_buffer);
      spi->rx_buffer = NULL;
    }

  close(spi->fd);
  return 0;
}

/****************************************************************************
 * Name: spi_transfer_file
 ****************************************************************************/

static int spi_msg_transfer(FAR struct spi_test_ctx_s *spi, void *tx_buf,
                            void *rx_buf, int size, int flags)
{
  int ret;
  int out_fd;

  struct spi_trans_s trans =
  {
    .txbuffer = tx_buf,
    .rxbuffer = rx_buf,
    .nwords = size,
  };

  struct spi_sequence_s seq =
  {
    .mode = 0,
    .nbits = spi->bits,
    .frequency = spi->speed,
    .ntrans = 1,
    .trans = &trans,
  };

  ret = ioctl(spi->fd, SPIIOC_TRANSFER, &seq);
  assert_false(ret < 0);

  if (spi->output_file)
    {
      out_fd = open(spi->output_file, O_WRONLY | O_CREAT | O_TRUNC, 0666);
      assert_false(out_fd < 0);

      ret = write(out_fd, rx_buf, size);
      close(out_fd);
    }

  if (spi->verbose && !spi->output_file)
    {
      for (int i = 0; i < size; i++)
      {
        if (flags)
          {
            printf("0x%x\t", *((uint8_t *)rx_buf + i));
          }
        else
          {
            printf("0x%x\t", *((uint8_t *)tx_buf + i));
          }
      }
      printf("\n");
    }

  return 0;
}

/****************************************************************************
 * Name: spidev_master_read
 ****************************************************************************/

static int spidev_master_read(FAR struct spi_test_ctx_s *spi, void *buffer,
                              int size)
{
  uint8_t *tx_buf = spi->tx_buffer;
  uint8_t *rx_buf = buffer;
  int tmp_size;
  int count;
  int ret;
  int i;

  assert_false (size < 0);

  count = (size + SPI_TX_BUFSIZE - 1) / SPI_TX_BUFSIZE;
  for (i = 0; i < count; i++)
    {
      sleep(1);
      tmp_size = (size - SPI_TX_BUFSIZE) > 0 ?
                  SPI_TX_BUFSIZE : size;
      ret = spi_msg_transfer(spi, tx_buf, rx_buf, tmp_size, 1);
      assert_false(ret < 0);

      rx_buf += tmp_size;
      tx_buf += tmp_size;
      size -= tmp_size;
    }

  return 0;
}

/****************************************************************************
 * Name: drivertest_spidev_read
 ****************************************************************************/

static int spidev_transfer_read(FAR struct spi_test_ctx_s *spi)
{
  int trans_size = spi->trans_size;
  int crc32;
  int crc_check;
  int ret;

  assert_false(spi->trans_size <= 0);

  /* trans size to read */

  printf("trans size to read %d\n", trans_size);
  sleep(1);
  ret = spi_msg_transfer(spi, &trans_size, NULL, sizeof(int), 0);
  assert_false(ret < 0);

  /* read data from slave */

  sleep(1);
  spi->rx_buffer = (char *)malloc(spi->trans_size);
  spi->tx_buffer = (char *)malloc(spi->trans_size);
  memset(spi->rx_buffer, 0, spi->trans_size);
  ret = spidev_master_read(spi, spi->rx_buffer, spi->trans_size);
  assert_false(ret < 0);

  /* get read crc32 */

  sleep(1);
  ret = spidev_master_read(spi, &crc_check, sizeof(int));
  assert_false(ret < 0);
  crc32 = crc32part((uint8_t *)spi->rx_buffer, spi->trans_size, ~0);

  if (spi->verbose)
    {
      printf("crc32: 0x%x, crc_check: 0x%x\n", crc32, crc_check);
    }

  assert_int_equal(crc32, crc_check);

  return 0;
}

/****************************************************************************
 * Name: drivertest_spidev_read
 ****************************************************************************/

static void drivertest_spidev_read(FAR void **state)
{
  FAR struct spi_test_ctx_s *spi;
  int ret;

  spi = (FAR struct spi_test_ctx_s *)*state;

  assert_false(spi->trans_size <= 0);

  ret = spidev_transfer_read(spi);
  assert_false(ret < 0);
}

/****************************************************************************
 * Name: spi_transfer_file
 ****************************************************************************/

static int spidev_master_write(FAR struct spi_test_ctx_s *spi)
{
  int trans_size = spi->trans_size;
  uint8_t *tx_buf = spi->tx_buffer;
  int count;
  int size;
  int crc32;
  int ret;
  int i;

  /* trans write count */

  ret = spi_msg_transfer(spi, &trans_size, NULL, sizeof(int), 0);
  assert_false(ret < 0);

  /* trans write data */

  count = (spi->trans_size + SPI_TX_BUFSIZE - 1) / SPI_TX_BUFSIZE;
  for (i = 0; i < count; i++)
    {
      sleep(1);
      size = (trans_size - SPI_TX_BUFSIZE) > 0 ?
              SPI_TX_BUFSIZE : trans_size;
      ret = spi_msg_transfer(spi, tx_buf, NULL, size, 0);
      assert_false(ret < 0);

      tx_buf += size;
      trans_size -= size;
    }

  /* trans write data crc */

  sleep(1);
  crc32 = crc32part((uint8_t *)spi->tx_buffer, spi->trans_size, ~0);
  if (spi->verbose)
    {
      printf("crc32: 0x%x\n", crc32);
    }

  ret = spi_msg_transfer(spi, (uint8_t *)&crc32, NULL, sizeof(int), 0);
  assert_false(ret < 0);

  return 0;
}

/****************************************************************************
 * Name: spi_transfer_file
 ****************************************************************************/

static int spi_transfer_file(FAR struct spi_test_ctx_s *spi)
{
  ssize_t bytes;
  struct stat sb;
  int tx_fd;
  int ret;

  ret = stat(spi->input_file, &sb);
  assert_false(ret < 0);

  tx_fd = open(spi->input_file, O_RDONLY);
  assert_false(tx_fd < 0);

  spi->tx_buffer = malloc(sb.st_size);
  if (!spi->tx_buffer)
    {
      return -ENOMEM;
    }

  spi->rx_buffer = malloc(sb.st_size);
  if (!spi->rx_buffer)
    {
      return -ENOMEM;
    }

  bytes = read(tx_fd, spi->tx_buffer, sb.st_size);
  assert_int_equal(bytes, sb.st_size);

  spi->trans_size = bytes;
  ret = spidev_master_write(spi);
  assert_false(ret < 0);

  close(tx_fd);

  return 0;
}

/****************************************************************************
 * Name: spi_transfer_buffer
 ****************************************************************************/

static int spi_transfer_buffer(FAR struct spi_test_ctx_s *spi)
{
  int ret;
  char *tx_str = spi->tx_buffer;
  size_t size = strlen(tx_str);

  spi->tx_buffer = malloc(size);
  if (!spi->tx_buffer)
    {
      return -ENOMEM;
    }

  spi->rx_buffer = malloc(size);
  if (!spi->rx_buffer)
    {
      return -ENOMEM;
    }

  size = str_unescape((char *)spi->tx_buffer, tx_str);
  if (spi->verbose)
    {
      printf("tx size %d\n", size);
    }

  spi->trans_size = size;
  ret = spidev_master_write(spi);
  assert_false(ret < 0);

  return 0;
}

/****************************************************************************
 * Name: spi_transfer_default
 ****************************************************************************/

static int spi_transfer_default(FAR struct spi_test_ctx_s *spi)
{
  int ret;
  size_t size = sizeof(g_default_tx);

  spi->tx_buffer = malloc(size);
  if (!spi->tx_buffer)
    {
      return -ENOMEM;
    }

  spi->rx_buffer = malloc(size);
  if (!spi->rx_buffer)
    {
      return -ENOMEM;
    }

  memcpy(spi->tx_buffer, g_default_tx, size);
  spi->trans_size = size;
  ret = spidev_master_write(spi);
  assert_false(ret < 0);

  return 0;
}

/****************************************************************************
 * Name: drivertest_spidev_write
 ****************************************************************************/

static void drivertest_spidev_write(FAR void **state)
{
  FAR struct spi_test_ctx_s *spi;
  int ret;

  spi = (FAR struct spi_test_ctx_s *)*state;

  if (spi->input_file)
    {
      ret = spi_transfer_file(spi);
    }
  else if (spi->tx_buffer)
    {
      ret = spi_transfer_buffer(spi);
    }
  else
    {
      ret = spi_transfer_default(spi);
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
  struct spi_test_ctx_s spi;

  memset(&spi, 0, sizeof(struct spi_test_ctx_s));
  snprintf(spi.dev_path, sizeof(spi.dev_path), "%s",
           SPI_MASTER_PATH);
  spi.speed = 1000000;
  spi.verbose = 1;
  spi.bits = 8;
  spi.trans_size = 0x20;
  parse_commandline(argc, argv, &spi);

  const struct CMUnitTest tests[] =
  {
    cmocka_unit_test_prestate_setup_teardown(drivertest_spidev_write, setup,
                                             teardown, &spi),
    cmocka_unit_test_prestate_setup_teardown(drivertest_spidev_read, setup,
                                             teardown, &spi),
  };

  return cmocka_run_group_tests(tests, NULL, NULL);
}
