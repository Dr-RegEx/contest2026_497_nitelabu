/* SPDX-License-Identifier: Apache-2.0 */

#include <nuttx/config.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

static int lp_transfer(int fd, int round, unsigned int baud,
                       tcflag_t format, unsigned int size)
{
  struct termios term;
  struct pollfd pfd = {.fd = fd};
  uint8_t tx[257];
  uint8_t rx[257];
  unsigned int sent = 0;
  unsigned int received = 0;
  unsigned int tries;
  unsigned int i;
  ssize_t n;

  if (tcgetattr(fd, &term) < 0)
    {
      return -errno;
    }

  cfmakeraw(&term);
  term.c_cflag = format;
  cfsetispeed(&term, baud);
  if (tcsetattr(fd, TCSANOW, &term) < 0 ||
      tcgetattr(fd, &term) < 0)
    {
      return -errno;
    }

  if (cfgetispeed(&term) != baud ||
      (term.c_cflag & (CSIZE | PARENB | PARODD | CSTOPB)) != format)
    {
      return -EIO;
    }

  for (i = 0; i < size; i++)
    {
      tx[i] = (i * 37 + round * 11) & ((format & CSIZE) == CS7 ? 127 : 255);
    }

  memset(rx, 0, sizeof(rx));
  for (tries = 0; tries < 300 && received < size; tries++)
    {
      pfd.events = POLLIN | (sent < size ? POLLOUT : 0);
      pfd.revents = 0;
      if (poll(&pfd, 1, 10) < 0)
        {
          return -errno;
        }

      if (pfd.revents & (POLLERR | POLLHUP | POLLNVAL))
        {
          return -EIO;
        }

      if ((pfd.revents & POLLOUT) && sent < size)
        {
          n = write(fd, tx + sent, size - sent);
          if (n < 0 && errno != EAGAIN)
            {
              return -errno;
            }

          if (n > 0)
            {
              sent += n;
            }
        }

      if (pfd.revents & POLLIN)
        {
          n = read(fd, rx + received, size - received);
          if (n < 0 && errno != EAGAIN)
            {
              return -errno;
            }

          if (n > 0)
            {
              received += n;
            }
        }

      usleep(1000);
    }

  if (sent != size || received != size || memcmp(tx, rx, size) != 0)
    {
      printf("S31LPUART transfer-failed round=%d sent=%u received=%u\n",
             round, sent, received);
      return -EIO;
    }

  if (tcdrain(fd) < 0)
    {
      return -errno;
    }

  printf("S31LPUART round=%d baud=%u format=%s bytes=%u match=1\n",
         round, baud, (format & CSIZE) == CS7 ? "7E2" : "8N1", size);
  return OK;
}

int esp32s31_lpuart_test(void)
{
  struct termios term;
  int fd;
  int ret;
  int close_ret;

  fd = open("/dev/ttyLP0", O_RDWR | O_NONBLOCK);
  if (fd < 0)
    {
      return -errno;
    }

  ret = lp_transfer(fd, 0, 9600, CS8, 257);
  if (ret == OK)
    {
      ret = lp_transfer(fd, 1, 57600, CS8, 257);
    }

  if (ret == OK)
    {
      ret = lp_transfer(fd, 2, 115200, CS8, 257);
    }

  if (ret == OK)
    {
      ret = lp_transfer(fd, 3, 57600, CS7 | PARENB | CSTOPB, 127);
    }

  if (ret == OK)
    {
      ret = tcgetattr(fd, &term) < 0 ? -errno : OK;
      if (ret == OK)
        {
          cfsetispeed(&term, 0);
          errno = 0;
          if (tcsetattr(fd, TCSANOW, &term) != -1 || errno != EINVAL)
            {
              ret = -EIO;
            }
        }
    }

  close_ret = close(fd);
  if (ret < 0 || close_ret < 0)
    {
      return ret < 0 ? ret : -errno;
    }

  fd = open("/dev/ttyLP0", O_RDWR | O_NONBLOCK);
  if (fd < 0)
    {
      return -errno;
    }

  ret = lp_transfer(fd, 4, 115200, CS8, 33);
  close_ret = close(fd);
  if (ret < 0 || close_ret < 0)
    {
      return ret < 0 ? ret : -errno;
    }

  printf("S31LPUART PASS rounds=5 bytes=931 reopen=1 invalid-baud=EINVAL\n");
  return OK;
}
