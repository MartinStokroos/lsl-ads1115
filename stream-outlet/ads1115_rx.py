#!/usr/bin/python3

# file: ads1115_rx.py

"""
Example program for receiving a sampled stream
from the Arduino ads1115-sampler sketch and print
the data to the screen.
"""

from time import perf_counter
import serial
import sys
import select

BAUDRATE = 115200
START_BYTE = 0xAA
PAYLOAD_SIZE = 6  # counter + ch1L + ch1H + ch2L + ch2H + checksum


def bytes_to_int16(lsb, msb):
    """Reconstruct signed 16-bit integer from LSB and MSB."""
    value = (msb << 8) | lsb
    if value >= 0x8000:
        value -= 0x10000
    return value


def key_pressed():
    return select.select([sys.stdin], [], [], 0)[0]


def main():

    # -------------------------
    # Command-line argument
    # -------------------------
    if len(sys.argv) != 2:
        print(f"Usage: {sys.argv[0]} <serial_port>")
        print(f"Example: {sys.argv[0]} /dev/ttyACM0")
        sys.exit(1)

    port = sys.argv[1]

    # -------------------------
    # Open serial port safely
    # -------------------------
    try:
        ser = serial.Serial(port, BAUDRATE, timeout=1)
        print(f"Opened serial port: {port}")
    except serial.SerialException as e:
        print(f"Error: Could not open serial port {port}")
        print(f"Reason: {e}")
        sys.exit(1)

    print("Receiving data... Press Ctrl-Z to stop.")

    state = "WAIT_START"
    buffer = bytearray()

    prev_time = perf_counter()
    expected_counter = None
    total_packets = 0
    lost_packets = 0

    try:
        while True:

            if key_pressed():
                break

            byte = ser.read(1)
            if not byte:
                continue

            b = byte[0]

            # -------------------------
            # STATE: WAIT FOR START
            # -------------------------
            if state == "WAIT_START":
                if b == START_BYTE:
                    current_time = perf_counter()
                    buffer.clear()
                    state = "READ_PAYLOAD"

            # -------------------------
            # STATE: READ PAYLOAD
            # -------------------------
            elif state == "READ_PAYLOAD":
                buffer.append(b)

                if len(buffer) == PAYLOAD_SIZE:

                    counter, ch1L, ch1H, ch2L, ch2H, rx_checksum = buffer

                    calc_checksum = (
                        counter + ch1L + ch1H + ch2L + ch2H
                    ) & 0xFF

                    if calc_checksum == rx_checksum:

                        ch1 = bytes_to_int16(ch1L, ch1H)
                        ch2 = bytes_to_int16(ch2L, ch2H)

                        # --- Packet loss detection ---
                        if expected_counter is None:
                            expected_counter = (counter + 1) & 0xFF
                        else:
                            if counter != expected_counter:
                                diff = (counter - expected_counter) & 0xFF
                                lost_packets += diff
                                print(f"! lost {diff} packet(s)")
                                expected_counter = (counter + 1) & 0xFF
                            else:
                                expected_counter = (expected_counter + 1) & 0xFF

                        total_packets += 1
                        interval = 1000 * (current_time - prev_time)
                        prev_time = current_time
                        print(
                            f"#{counter:3d} | "
                            f"{ch1:6d}, {ch2:6d} | "
                            f"dt={interval:6.1f} ms | "
                            f"lost={lost_packets}"
                        )

                    else:
                        print("Checksum error — packet discarded")

                    # Always resync
                    state = "WAIT_START"

    finally:
        ser.close()
        print("\nStopped.")
        print(f"Total valid packets: {total_packets}")
        print(f"Total lost packets:  {lost_packets}")


if __name__ == "__main__":
    main()
