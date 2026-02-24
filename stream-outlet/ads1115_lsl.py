#!/usr/bin/python3

# file: ads1115_lsl.py

"""
Example program for receiving a sampled stream
from the Arduino ads1115-sampler sketch and send
it over LSL.
"""

import serial
import sys
import select
import time
from time import perf_counter
from pylsl import StreamInfo, StreamOutlet, local_clock

BAUDRATE = 115200
START_BYTE = 0xAA
PAYLOAD_SIZE = 6  # counter + ch1L + ch1H + ch2L + ch2H + checksum
SAMPLE_RATE = 100  # Hz


def bytes_to_int16(lsb, msb):
    """Reconstruct signed 16-bit integer from LSB and MSB."""
    value = (msb << 8) | lsb
    if value >= 0x8000:
        value -= 0x10000
    return value

def key_pressed():
    return select.select([sys.stdin], [], [], 0)[0]

def create_lsl_outlet():
    info = StreamInfo(
        name="ADCStream",
        type="EEG",
        channel_count=2,
        nominal_srate=SAMPLE_RATE,
        channel_format="float32",
        source_id="ads_1115_001"
    )
    # Optional: add channel labels
    chns = info.desc().append_child("channels")

    ch1 = chns.append_child("channel")
    ch1.append_child_value("label", "Channel1")
    ch1.append_child_value("unit", "adc")
    ch1.append_child_value("type", "EEG")

    ch2 = chns.append_child("channel")
    ch2.append_child_value("label", "Channel2")
    ch2.append_child_value("unit", "adc")
    ch2.append_child_value("type", "EEG")

    outlet = StreamOutlet(info)
    return outlet


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
        
    # Create LSL outlet    
    outlet = create_lsl_outlet()

    print("Streaming to LSL... Press Ctrl-Z to stop.")

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

                        # do unit conversion here:
                        ch1 = ch1 * 2048/65536.0; # converting to miliVolts
                        ch2 = ch2 * 2048/65536.0;  
                        # Push sample with precise LSL timestamp
                        outlet.push_sample([ch1, ch2], local_clock())                        

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
#                        print(
#                            f"#{counter:3d} | "
#                            f"{ch1:6d}, {ch2:6d} | "
#                            f"dt={interval:6.1f} ms | "
#                            f"lost={lost_packets}"
#                        )

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
