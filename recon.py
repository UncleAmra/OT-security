#!/usr/bin/env python3
"""
recon.py - passive reconnaissance against the OT plant PLC (Layer 2).

Read-only. Connects to the PLC's Modbus/TCP port, reads all 64 holding
registers with function code 03, and prints each one decoded to engineering
units. No writes are sent, so this does not change plant state.

Usage:
    python recon.py [plc_ip] [plc_port]
    (defaults: 127.0.0.1 5020)
"""

import socket
import struct
import sys

# (name, unit, scale, kind)  value shown = raw / scale, plus any offset handled below.
# kind: "" = analog, "bool" = 0/1, "enum" = coded, "bits" = bitmask (shown hex)
REGS = {
    0:  ("Boiler drum temp",        "C",     1,  ""),
    1:  ("Boiler drum pressure",    "PSI",   1,  ""),
    2:  ("Boiler drum level",       "%",     1,  ""),
    3:  ("Superheater steam temp",  "C",     1,  ""),
    4:  ("Main steam mass flow",    "kg/s",  1,  ""),
    5:  ("Feedwater mass flow",     "kg/s",  1,  ""),
    6:  ("Safety relief valve 1",   "",      1,  "bool"),
    7:  ("Safety relief valve 2",   "",      1,  "bool"),
    8:  ("Attemperator spray",      "%",     1,  ""),
    9:  ("Blowdown valve",          "",      1,  "bool"),
    10: ("Fuel feed rate",          "kg/min",1,  ""),
    11: ("FD fan speed",            "%",     1,  ""),
    12: ("ID fan speed",            "%",     1,  ""),
    13: ("Furnace draft",           "mmH2O", 10, "offset500"),
    14: ("Flue gas O2",             "%",     100,""),
    15: ("Air/fuel ratio",          "",      10, ""),
    16: ("Burner firing state",     "",      1,  "burner"),
    17: ("Flame detector",          "",      1,  "bool"),
    18: ("Sootblower",              "",      1,  "bool"),
    19: ("Combustion efficiency",   "%",     10, ""),
    20: ("Turbine speed",           "RPM",   1,  ""),
    21: ("Main stop valve",         "",      1,  "bool"),
    22: ("Governor control valve",  "%",     1,  ""),
    23: ("Bearing 1 vibration",     "mm/s",  10, ""),
    24: ("Bearing 2 vibration",     "mm/s",  10, ""),
    25: ("Rotor diff expansion",    "mm",    100,""),
    26: ("Lube oil pressure",       "Bar",   10, ""),
    27: ("Lube oil temp",           "C",     1,  ""),
    28: ("Turbine trip solenoid",   "",      1,  "bool"),
    29: ("HP chest metal temp",     "C",     1,  ""),
    30: ("Generator active power",  "MW",    1,  ""),
    31: ("Generator reactive pwr",  "MVAR",  10, ""),
    32: ("Grid frequency",          "Hz",    100,""),
    33: ("Terminal voltage",        "kV",    10, ""),
    34: ("Grid breaker 52G",        "",      1,  "bool"),
    35: ("Synchroscope phase",      "deg",   10, ""),
    36: ("Exciter field current",   "A",     10, ""),
    37: ("Power factor",            "cos",   100,""),
    38: ("Stator winding temp",     "C",     1,  ""),
    39: ("Grid sync permit",        "",      1,  "bool"),
    40: ("Condensate hotwell temp", "C",     1,  ""),
    41: ("Condenser vacuum",        "inHg",  10, ""),
    42: ("Circulating water flow",  "%",     1,  ""),
    43: ("Circ water inlet temp",   "C",     1,  ""),
    44: ("Boiler feed pump A",      "%",     1,  ""),
    45: ("Boiler feed pump B",      "%",     1,  ""),
    46: ("Deaerator pressure",      "Bar",   10, ""),
    47: ("Deaerator level",         "%",     1,  ""),
    48: ("Condensate pump",         "",      1,  "bool"),
    49: ("Cooling tower fan",       "%",     1,  ""),
    50: ("MASTER FUEL TRIP",        "",      1,  "trip"),
    51: ("TURBINE TRIP",            "",      1,  "trip"),
    52: ("GENERATOR TRIP",          "",      1,  "trip"),
    53: ("Emergency shutdown cmd",  "",      1,  "bool"),
    54: ("Alarm word HIGH",         "",      1,  "bits"),
    55: ("Alarm word LOW",          "",      1,  "bits"),
    56: ("First-out trip code",     "",      1,  "firstout"),
    57: ("Operator target MW",      "MW",    1,  ""),
    58: ("Operator target drum lvl","%",     1,  ""),
    59: ("Watchdog heartbeat",      "",      1,  ""),
    60: ("PLC mode",                "",      1,  "mode"),
    61: ("Anomaly status",          "",      1,  "bits"),
    62: ("Security alert level",    "",      1,  "alert"),
    63: ("Reset simulation cmd",    "",      1,  "bool"),
}

BURNER   = {0: "Off", 1: "Igniting", 2: "Firing"}
MODE     = {0: "MANUAL", 1: "AUTO/cascade", 2: "Maintenance"}
ALERT    = {0: "NORMAL", 1: "WARNING", 2: "CRITICAL", 3: "TRIPPED"}
FIRSTOUT = {0: "none", 1: "operator ESD", 2: "boiler overpressure", 3: "drum level lo-lo",
            4: "drum level hi-hi", 5: "flame failure", 6: "turbine overspeed",
            7: "high bearing vib", 8: "loss of lube oil", 9: "condenser vacuum loss",
            10: "generator reverse power", 11: "loss of excitation", 12: "electrical fault 87G"}


def recv_exact(sock, n):
    """Read exactly n bytes. TCP recv may return fewer, so loop until full."""
    buf = b""
    while len(buf) < n:
        chunk = sock.recv(n - len(buf))
        if not chunk:
            raise IOError("PLC closed the connection mid-response")
        buf += chunk
    return buf


# Meanings for a Modbus exception response (function code | 0x80).
MB_EXCEPTIONS = {1: "illegal function", 2: "illegal data address",
                 3: "illegal data value", 4: "slave device failure"}


def modbus_read_all(sock, count=64):
    """FC03: read `count` holding registers starting at address 0."""
    # MBAP header (txid, proto, len, unit) + PDU (func, start, count)
    req = struct.pack(">HHHBBHH", 1, 0, 6, 1, 3, 0, count)
    sock.sendall(req)
    # MBAP header is 7 bytes; its length field counts the unit id + PDU.
    _, _, length, _ = struct.unpack(">HHHB", recv_exact(sock, 7))
    pdu = recv_exact(sock, length - 1)
    func = pdu[0]
    if func == (3 | 0x80):  # exception response
        code = pdu[1] if len(pdu) > 1 else 0
        raise IOError(f"PLC returned Modbus exception {code} ({MB_EXCEPTIONS.get(code, 'unknown')})")
    if func != 3:
        raise IOError(f"unexpected Modbus function in response: 0x{func:02x}")
    byte_count = pdu[1]
    if byte_count != count * 2:
        raise IOError(f"expected {count * 2} data bytes, PLC sent {byte_count}")
    return list(struct.unpack(f">{count}H", pdu[2:2 + byte_count]))


def decode(addr, raw):
    name, unit, scale, kind = REGS[addr]
    if kind == "bool":
        return "ON" if raw else "off"
    if kind == "trip":
        return "*** TRIPPED ***" if raw else "healthy"
    if kind == "burner":
        return BURNER.get(raw, raw)
    if kind == "mode":
        return MODE.get(raw, raw)
    if kind == "alert":
        return ALERT.get(raw, raw)
    if kind == "firstout":
        return FIRSTOUT.get(raw, raw)
    if kind == "bits":
        return f"0x{raw:04x}"
    if kind == "offset500":
        return f"{(raw - 500) / scale:.1f} {unit}"
    val = raw / scale
    return f"{val:g} {unit}".strip()


def main():
    ip = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
    port = int(sys.argv[2]) if len(sys.argv) > 2 else 5020

    print(f"[recon] target PLC {ip}:{port} (Modbus/TCP, FC03, read-only)")
    try:
        sock = socket.create_connection((ip, port), timeout=3)
    except OSError as e:
        print(f"[recon] cannot reach PLC: {e}")
        print("        Is Layer 2 running?  Start it with plc_controller 0.0.0.0 5020 100")
        return 1

    print(f"[recon] port {port} OPEN - no auth, no encryption. Reading 64 registers...\n")
    with sock:
        regs = modbus_read_all(sock, 64)

    print(f"{'REG':>3}  {'NAME':<24} {'RAW':>6}  VALUE")
    print("-" * 60)
    for addr in range(64):
        name = REGS[addr][0]
        print(f"{addr:>3}  {name:<24} {regs[addr]:>6}  {decode(addr, regs[addr])}")

    print("\n[recon] done. Interesting write targets for later:")
    print("        REG 60 = PLC mode (set 0 = MANUAL to stop auto-correction)")
    print("        REG 10 = fuel feed rate   REG 22 = turbine control valve")
    print("        REG 34 = grid breaker     REG 53 = emergency shutdown")
    return 0


if __name__ == "__main__":
    sys.exit(main())
