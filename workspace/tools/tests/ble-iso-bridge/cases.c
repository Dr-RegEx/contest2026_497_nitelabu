int main(void)
{
  uint8_t packet[16388] = { H4_ISO, 0x01, 0x40, 0x2c, 0x01 };
  uint8_t command[] = { 3, 12, 0 };
  uint8_t acl[] = { 1, 0, 1, 0, 0xaa };
  int calls;
  memset(packet + 5, 0xab, 300);
  g_open = true;
  g_driver.receive = host_rx;
  assert(s31_send(&g_driver, BT_ISO_OUT, packet + 1, 304) == 304);
  assert(captured_type == H4_ISO && captured_len == 304);
  assert(memcmp(captured, packet + 1, 304) == 0);
  assert(s31_receive(packet, 305) == 0);
  assert(captured_type == BT_ISO_IN && captured_len == 304);
  assert(memcmp(captured, packet + 1, 304) == 0);

  /* Truncated headers, inconsistent lengths and RFU bits never reach HCI. */
  calls = tx_calls;
  assert(s31_send(&g_driver, BT_ISO_OUT, packet + 1, 3) == -EINVAL);
  assert(s31_send(&g_driver, BT_ISO_OUT, packet + 1, 303) == -EINVAL);
  packet[4] |= 0xc0;
  assert(s31_send(&g_driver, BT_ISO_OUT, packet + 1, 304) == -EINVAL);
  assert(tx_calls == calls);
  calls = rx_calls;
  assert(s31_receive(packet, 4) == -EINVAL);
  assert(s31_receive(packet, 305) == -EINVAL);
  packet[4] &= 0x3f;
  assert(s31_receive(packet, 304) == -EINVAL);
  assert(rx_calls == calls);

  tx_result = -ENOBUFS;
  assert(s31_send(&g_driver, BT_ISO_OUT, packet + 1, 304) == -ENOBUFS);
  tx_result = 1;
  assert(s31_send(&g_driver, BT_ISO_OUT, packet + 1, 304) == -EIO);
  tx_result = 0;
  rx_result = -ENOBUFS;
  assert(s31_receive(packet, 305) == -ENOBUFS);
  rx_result = 0;

  /* Protocol maximum and zero payload, including kernel address copy. */
  packet[3] = 0xff; packet[4] = 0x3f;
  assert(s31_send(&g_driver, BT_ISO_OUT, packet + 1, 16387) == 16387);
  assert(s31_receive(packet, 16388) == 0);
  assert(s31_send(&g_driver, BT_ISO_OUT, packet + 1, 16388) == -EINVAL);
  packet[3] = 0; packet[4] = 0;
  assert(s31_send(&g_driver, BT_ISO_OUT, packet + 1, 4) == 4);
  assert(s31_receive(packet, 5) == 0);
  assert(s31_send(&g_driver, BT_CMD, command, sizeof(command)) == 3);
  assert(captured_type == H4_CMD);
  assert(s31_send(&g_driver, BT_ACL_OUT, acl, sizeof(acl)) == 5);
  assert(captured_type == H4_ACL);
  g_open = false;
  assert(s31_send(&g_driver, BT_ISO_OUT, packet + 1, 4) == -ENODEV);
  return 0;
}
