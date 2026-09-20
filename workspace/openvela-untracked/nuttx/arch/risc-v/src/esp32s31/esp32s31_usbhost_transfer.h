/* SPDX-License-Identifier: Apache-2.0 */
#ifndef __ARCH_RISCV_SRC_ESP32S31_USBHOST_TRANSFER_H
#define __ARCH_RISCV_SRC_ESP32S31_USBHOST_TRANSFER_H
#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

/* Packet callback returns actual data bytes, or a negative errno. It must
 * retire the hardware channel before returning success/retry. NAK retries
 * and a whole-request deadline belong to that callback, never to this loop.
 */
typedef ssize_t (*s31_usb_packet_t)(void *, bool, uint8_t, uint8_t *, size_t);

static inline ssize_t s31_usb_data(s31_usb_packet_t packet, void *arg,
                                  bool in, uint16_t maxpacket, uint8_t *pid,
                                  uint8_t *buffer, size_t length)
{
  size_t done = 0;
  ssize_t ret;
  size_t amount;

  if (!maxpacket || maxpacket > 512 || !pid ||
      (*pid != 0 && *pid != 2) || (length && !buffer))
    return -EINVAL;
  do
    {
      amount = length - done;
      if (amount > maxpacket)
        amount = maxpacket;
      ret = packet(arg, in, *pid, buffer ? buffer + done : NULL, amount);
      if (ret < 0)
        return ret;
      if ((size_t)ret > amount || (!in && (size_t)ret != amount))
        return -EPROTO;
      *pid ^= 2; /* DATA0=0, DATA1=2 in DWC2 HCTSIZ. */
      done += ret;
      if (in && (size_t)ret < amount)
        break;
    }
  while (done < length);
  return done;
}

static inline int s31_usb_control(s31_usb_packet_t packet, void *arg,
                                  uint16_t maxpacket, const uint8_t setup[8],
                                  uint8_t *buffer)
{
  uint8_t pid = 2; /* Control data always starts at DATA1. */
  uint8_t copy[8];
  size_t length;
  bool in;
  ssize_t ret;
  unsigned int i;

  if (!setup || !maxpacket || maxpacket > 64)
    return -EINVAL;
  length = setup[6] | (size_t)setup[7] << 8;
  in = (setup[0] & 0x80) != 0;
  if (length && !buffer)
    return -EINVAL;
  for (i = 0; i < 8; i++)
    copy[i] = setup[i];
  ret = packet(arg, false, 3, copy, 8); /* SETUP PID */
  if (ret != 8)
    return ret < 0 ? ret : -EPROTO;
  if (length)
    {
      ret = s31_usb_data(packet, arg, in, maxpacket, &pid, buffer, length);
      if (ret < 0)
        return ret;
    }
  /* No-data requests always use IN status, regardless of bmRequestType. */
  ret = packet(arg, length ? !in : true, 2, NULL, 0);
  return ret == 0 ? 0 : (ret < 0 ? ret : -EPROTO);
}
#endif
