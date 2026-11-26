# OT Power Plant Security Testbed

A small Operational Technology (OT) testbed that simulates a 500 MW steam power plant, split across the levels of the **Purdue model**. Each layer is a separate C++17 program, and they talk over plain **Modbus/TCP**, so you can capture, study and later attack or defend the traffic between them.

![Layer architecture](docs/architecture.svg)

## How it works

**Layer 1: Physics Digital Twin** (`physics_engine`) models the plant — combustion, boiler, turbine, generator and condenser — as coupled equations stepped twice a second. Each tick it reads the actuator registers from the PLC and writes back the new sensor values.

**Layer 2: Soft PLC** (`plc_controller`) is the hub and the **only server**. It holds 64 Modbus holding registers and runs a 10 Hz scan: read inputs → control loops → safety interlocks (Master Fuel Trip, trips, ESD) → write outputs. Layers 1 and 3 connect to it as Modbus clients on port **5020**.

**Layer 3: SCADA / HMI** (`scada_hmi`) is the operator workstation. It polls all 64 registers 4 times a second and serves a web dashboard on port **8085**; operator actions become Modbus writes to the PLC.

**Layer 4: Offensive Security / Docs** (`Layer_4/`) holds the Python attack toolkit (`recon.py`, `attack.py`) used to test the network from a separate attacker machine, plus the penetration-test report and its evidence.

**How they are connected:** Layers 1 and 3 never talk directly — every value goes through the PLC's registers. If Layer 1 stops, the HMI freezes on the last values; if the PLC stops, both lose their only link until it comes back.

## Directory structure

Each C++ layer (`Layer_1`–`Layer_3`) has its own `include/`, `src/` and `Makefile` / `CMakeLists.txt`. Compiled binaries are not in the repo; build them with `build_all.bat` / `build_all.sh`.

```
OT-security/
├── common/      Shared headers: 64-register Modbus map, protocol, sockets
├── Layer_1/     Physics digital twin           (physics_engine)
├── Layer_2/     Soft PLC — Modbus server        (plc_controller)
├── Layer_3/     SCADA / HMI + web dashboard     (scada_hmi)
├── Layer_4/     Attack scripts, evidence, pentest report
├── docs/        Architecture diagram
├── run_all.bat / build_all.*   Run / build all layers
├── test_system.py              End-to-end test
└── README.md
```

## Requirements

- `g++` with C++17 support and `make` (MinGW-w64 on Windows, `build-essential` on Linux). CMake is optional.
- Clone the **whole repository** on each machine, because every layer includes headers from `../common`.

## Option A: Run everything on one machine

Good for development and testing. All traffic goes over `127.0.0.1`.

**Windows**

```powershell
.\build_all.bat
.\run_all.bat
```

`run_all.bat` opens three windows (PLC first, then physics, then HMI). Open **http://localhost:8085** in a browser.

**Linux**: build once, then use three terminals and start them in this order:

```bash
./build_all.sh
cd Layer_2 && ./plc_controller 0.0.0.0 5020 100            # terminal 1: PLC
cd Layer_1 && ./physics_engine 127.0.0.1 5020 500          # terminal 2: physics
cd Layer_3 && ./scada_hmi 127.0.0.1 5020 8085 web          # terminal 3: HMI
```

## Option B: Run across VMs

This is the real deployment. Each layer runs on its own machine, so the traffic between layers actually crosses the network and can be sniffed.

| Machine | Runs | Connects to |
|---|---|---|
| VM B | Layer 2 (PLC) | listens on `0.0.0.0:5020` |
| VM A | Layer 1 (physics) | `<PLC_IP>:5020` |
| Host | Layer 3 (HMI) | `<PLC_IP>:5020`, serves `:8085` |

**1. Network.** Give every VM a **Host-only** adapter (VMware: VMnet1, VirtualBox: Host-Only Adapter), plus an optional NAT adapter for internet. Find each IP with `ip -br a` (Linux) or `ipconfig` (Windows), and make sure every machine can `ping` every other one. To sniff traffic from another VM later (Wireshark / Scapy), set that adapter's **Promiscuous Mode** to **Allow All**.

**2. Install tools on both VMs.**

```bash
sudo apt update && sudo apt install -y build-essential cmake dos2unix
```

If `make` fails strangely after copying files from Windows, fix the line endings with `find . -name Makefile -o -name "*.sh" | xargs dos2unix`.

**3. Start the PLC first (VM B).**

```bash
cd Desktop/OT-security/Layer_2 && make
./plc_controller 0.0.0.0 5020 100
```

Bind to `0.0.0.0` so the other machines can reach it. If you use `ufw`, open the port with `sudo ufw allow 5020/tcp`.

**4. Start the physics engine (VM A).**

```bash
cd Desktop/OT-security/Layer_1 && make
./physics_engine <PLC_IP> 5020 500
```

**5. Start the HMI (Host, from inside `Layer_3`).**

```powershell
cd OT-security\Layer_3
mingw32-make
.\scada_hmi.exe <PLC_IP> 5020 8085 web
```

Open the URL it prints. If 8085 is busy it falls back to the next free port, so read the printed line.

**6. Verify the pipeline.** Gauges should move every second. Stop Layer 1 and the values freeze, start it again and they resume. Stop Layer 2 and the HMI shows disconnected.

## Command-line arguments

| Program | Arguments (defaults) |
|---|---|
| `plc_controller` | `[bind_ip=0.0.0.0] [port=5020] [scan_ms=100]` |
| `physics_engine` | `[plc_ip=127.0.0.1] [port=5020] [tick_ms=500]` |
| `scada_hmi` | `[plc_ip=127.0.0.1] [plc_port=5020] [http_port=8085] [web_dir=web]` |

## Layer 4: security testing

> **Authorized testbed use only** — run these only against this simulator on an isolated lab network.

From an attacker machine that can reach the PLC (`<PLC_IP>:5020`):

```bash
cd Layer_4
python3 recon.py <PLC_IP> 5020
python3 attack.py <PLC_IP> esd
python3 attack.py <PLC_IP> overfire
python3 attack.py <PLC_IP> breaker
python3 attack.py <PLC_IP> restore
```

`recon.py` reads all 64 registers (no auth); `attack.py` sends unauthenticated Modbus writes. Findings and evidence are in `Layer_4/OT_Security_Pentest_Report.pdf`.
