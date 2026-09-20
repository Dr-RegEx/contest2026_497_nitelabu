"""Paced, unicast-only sender for the existing NuttX examples/udp server.

No credentials, capture, firewall changes, or host listening socket. Sending
alone is not a pass: the serial-side caller must validate every received frame.
"""
import ipaddress
import re
import socket
import sys
import time


def payload(offset):
    if not 0 <= offset < 256:
        raise ValueError('offset must be a byte')
    packet = bytearray(96)
    packet[0] = offset
    position = offset + 1
    for value in range(0x20, 0x7f):
        if position >= len(packet):
            position = 1
        packet[position] = value
        position += 1
    return bytes(packet)


def validate_transcript(text):
    if re.search(r'packets lost|bad |error|failure|failed|incorrect', text,
                 re.IGNORECASE):
        return False
    frames = re.findall(r'server: (\d+)\. Received (\d+) bytes from '
                        r'((?:\d+\.){3}\d+):(\d+)', text)
    if [(int(index), int(size)) for index, size, _, _ in frames] != [
            (index, 96) for index in range(256)]:
        return False
    # An unrelated sender must not be combined with our sequence.
    return len({(address, port) for _, _, address, port in frames}) == 1


def main():
    address = ipaddress.IPv4Address(sys.argv[1])
    if address.is_multicast or address.is_unspecified or address.is_loopback:
        raise SystemExit('An assigned unicast board IPv4 address is required')
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as stream:
        stream.settimeout(2)
        for offset in range(256):
            packet = payload(offset)
            if stream.sendto(packet, (str(address), 5471)) != len(packet):
                raise RuntimeError('partial UDP send')
            time.sleep(0.04)
    print('UDP_SENT=256 bytes_per_packet=96', flush=True)


if __name__ == '__main__':
    main()
