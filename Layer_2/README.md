# Layer 2: Industrial C++ PLC Controller Runtime

The **Layer 2 Industrial PLC Controller** is a high-performance, deterministic C++ runtime that acts as the supervisory control node in the Purdue Model (ISA-95 Level 2).

It operates a multi-client Modbus/TCP server on Port 5020 (or standard 502), continuously executing deterministic industrial scan cycles (10 Hz / 100 ms) with closed-loop PID regulation and a Safety Instrumented System (SIS) enforcing 2-out-of-3 (2oo3) trip voting logic.

---

## Architecture Overview

```
                      [ Layer 3: SCADA / HMI Console ]
                                     ▲
                                     │  Modbus/TCP (FC 03, FC 06, FC 16)
                                     ▼
        +-----------------------------------------------------------+
        |                 LAYER 2: PLC CONTROLLER                   |
        |                                                           |
        |   +-------------------+      +------------------------+   |
        |   | Multi-Client      | <--> | Atomic 64-Register     |   |
        |   | Modbus TCP Server |      | Holding Register Table |   |
        |   +-------------------+      +------------------------+   |
        |                                           ▲               |
        |   +---------------------------------------+-----------+   |
        |   |             DETERMINISTIC SCAN CYCLE              |   |
        |   | 1. Input Image Scan (Sensors from Layer 1)        |   |
        |   | 2. Closed-Loop Regulatory PID Algorithms          |   |
        |   |    - 3-Element Drum Level Control -> BFP Speed    |   |
        |   |    - DEH Turbine Governor -> Control Valve        |   |
        |   |    - Balanced Furnace Draft -> ID Fan Speed       |   |
        |   | 3. Safety Instrumented System (SIS / 2oo3)        |   |
        |   |    - Master Fuel Trip (MFT) Interlocks            |   |
        |   |    - Turbine Emergency Trip System (ETS)          |   |
        |   |    - First-Out Root Cause Latching                |   |
        |   | 4. Output Image Flush                             |   |
        |   +---------------------------------------------------+   |
        +-----------------------------------------------------------+
                                     ▲
                                     │  Modbus/TCP (FC 03, FC 16)
                                     ▼
                     [ Layer 1: Physics Digital Twin ]
```

---

## Safety Instrumented System (SIS) & Trip Matrix

The controller implements critical infrastructure trip interlocks:

| Trigger Condition | Threshold | Root Cause Code | Safety Action |
|---|---|---|---|
| **Boiler Overpressure** | $P \ge 190 \text{ PSI}$ | `2` | MFT Active + Opens Safety Valves 1 & 2 |
| **Drum Level Low-Low** | $L \le 18\%$ | `3` | MFT Active (Protects boiler tubes from dry burnout) |
| **Drum Level High-High** | $L \ge 82\%$ | `4` | Turbine Trip (Prevents water induction blade destruction) |
| **Furnace Flame Loss** | Flame Dark with Fuel $> 5$ | `5` | MFT Active (Prevents explosive unburnt gas accumulation) |
| **Turbine Overspeed** | $\ge 3960 \text{ RPM}$ ($110\%$) | `6` | Turbine ETS Trip (Closes MSV & CV immediately) |
| **High Bearing Vibration** | $\ge 4.5 \text{ mm/s}$ | `7` | Turbine ETS Trip (Prevents rotor-casing contact) |
| **Loss of Lube Oil** | $< 1.5 \text{ Bar}$ | `8` | Turbine ETS Trip (Prevents babbitt bearing seizure) |
| **Condenser Vacuum Loss** | $< 8.0 \text{ inHg}$ | `9` | Turbine ETS Trip (Prevents blade overheating / backpressure) |
| **Operator Manual ESD** | Modbus Reg 53 = 1 | `1` | Complete Plant Trip (MFT + ETS + 52G Breaker) |

---

## Compilation

### Linux / MinGW (Makefile)
```bash
make
```

### CMake
```bash
mkdir build && cd build
cmake ..
cmake --build .
```

---

## Execution

```bash
# Run with defaults (0.0.0.0:5020, 100 ms scan cycle)
./plc_controller

# Custom bind IP, port, and scan cycle:
# ./plc_controller <bind_ip> <port> <scan_ms>
./plc_controller 0.0.0.0 5020 100
```
