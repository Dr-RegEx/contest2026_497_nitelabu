/* SPDX-License-Identifier: Apache-2.0 */
#include <nuttx/config.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdint.h>
#include <string.h>
#include <sys/ioctl.h>
#include <syslog.h>
#include <unistd.h>
#include <nuttx/can/can.h>

static void frame(struct can_msg_s *msg, unsigned int index)
{
  static const uint8_t dlcs[] = {0, 8, 8, 9, 10, 11, 12, 13, 14, 15};
  unsigned int i;
  memset(msg, 0, sizeof(*msg));
  msg->cm_hdr.ch_id = index & 1 ? 0x1abc000 + index : 0x321 + index;
  msg->cm_hdr.ch_extid = index & 1;
  msg->cm_hdr.ch_dlc = dlcs[index];
  msg->cm_hdr.ch_edl = index >= 3;
  msg->cm_hdr.ch_brs = index >= 4;
  for (i = 0; i < can_dlc2bytes(dlcs[index]); i++)
    {
      msg->cm_data[i] = (index * 29 + i * 7) ^ 0xa5;
    }
}

static int receive(int fd, const struct can_msg_s *expected, unsigned int i)
{
  struct can_msg_s got;
  struct pollfd pfd = { .fd = fd, .events = POLLIN };
  int len = CAN_MSGLEN(can_dlc2bytes(expected->cm_hdr.ch_dlc));
  int ret = poll(&pfd, 1, 1000);
  if (ret != 1 || !(pfd.revents & POLLIN))
    {
      syslog(LOG_ERR, "S31CAN receive=%u poll=%d events=%lx\n",
             i, ret, (unsigned long)pfd.revents);
      return -ETIMEDOUT;
    }

  memset(&got, 0, sizeof(got));
  ret = read(fd, &got, sizeof(got));
  if (ret != sizeof(got) || memcmp(&got, expected, len) != 0)
    {
      syslog(LOG_ERR, "S31CAN receive=%u bytes=%d id=%lx error=%u dlc=%u\n",
             i, ret, (unsigned long)got.cm_hdr.ch_id,
             got.cm_hdr.ch_error, got.cm_hdr.ch_dlc);
      return -EIO;
    }

  syslog(LOG_INFO, "S31CAN frame=%u id=%lx dlc=%u fd=%u brs=%u bytes=%d match=1\n",
         i, (unsigned long)got.cm_hdr.ch_id, got.cm_hdr.ch_dlc,
         got.cm_hdr.ch_edl, got.cm_hdr.ch_brs,
         can_dlc2bytes(got.cm_hdr.ch_dlc));
  return OK;
}

int esp32s31_canfd_test(void)
{
  struct can_msg_s tx;
  struct canioc_bittiming_s timing = { .bt_type = CAN_BITTIMING_NOMINAL };
  struct canioc_connmodes_s modes;
  uint8_t batch[4 * sizeof(struct can_msg_s)];
  unsigned int used = 0;
  unsigned int i;
  int fd = open("/dev/can0", O_RDWR | O_NONBLOCK);
  int len;
  int ret = -EIO;
  if (fd < 0)
    {
      return -errno;
    }

  if (ioctl(fd, CANIOC_GET_CONNMODES, (unsigned long)&modes) < 0 ||
      !modes.bm_loopback ||
      ioctl(fd, CANIOC_GET_BITTIMING, (unsigned long)&timing) < 0 ||
      timing.bt_baud != 500000)
    {
      goto out;
    }

  timing.bt_type = CAN_BITTIMING_DATA;
  if (ioctl(fd, CANIOC_GET_BITTIMING, (unsigned long)&timing) < 0 ||
      timing.bt_baud != 2000000)
    {
      goto out;
    }

  frame(&tx, 9);
  errno = 0;
  if (write(fd, &tx, CAN_MSGLEN(8)) != -1 || errno != EINVAL)
    {
      goto out;
    }

  syslog(LOG_INFO, "S31CAN contract short-write=EINVAL nominal=500000 data=2000000\n");
  for (i = 0; i < 10; i++)
    {
      frame(&tx, i);
      len = CAN_MSGLEN(can_dlc2bytes(tx.cm_hdr.ch_dlc));
      ret = -EIO;
      if (write(fd, &tx, len) != len)
        {
          goto out;
        }

      ret = receive(fd, &tx, i);
      if (ret < 0)
        {
          goto out;
        }
    }

  /* Multiple messages in one write exercise the upper TX queue. */

  for (i = 6; i < 10; i++)
    {
      frame(&tx, i);
      len = CAN_MSGLEN(can_dlc2bytes(tx.cm_hdr.ch_dlc));
      memcpy(batch + used, &tx, len);
      used += len;
    }

  ret = -EIO;
  if (write(fd, batch, used) != used)
    {
      goto out;
    }

  for (i = 6; i < 10; i++)
    {
      frame(&tx, i);
      ret = receive(fd, &tx, i + 4);
      if (ret < 0)
        {
          goto out;
        }
    }

  close(fd);
  fd = open("/dev/can0", O_RDWR | O_NONBLOCK);
  if (fd < 0)
    {
      return -errno;
    }

  frame(&tx, 9);
  len = CAN_MSGLEN(64);
  ret = -EIO;
  if (write(fd, &tx, len) == len)
    {
      ret = receive(fd, &tx, 14);
    }

out:
  if (ret < 0)
    {
      ioctl(fd, CANIOC_IOFLUSH, 0);
    }

  close(fd);
  if (ret == OK)
    {
      syslog(LOG_INFO, "S31CAN PASS frames=15 reopen=1 loopback=gpio37\n");
    }

  return ret;
}
