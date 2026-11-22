# OT Power Plant Security Testbed

A small Operational Technology (OT) testbed that simulates a 500 MW steam power plant, split across the levels of the **Purdue model**. Each layer is a separate C++17 program, and they talk over plain **Modbus/TCP**, so you can capture, study and later attack or defend the traffic between them.

![Layer architecture](docs/architecture.svg)

## How it works

**Layer 1: Physics Digital Twin** (`physics_engine`) plays the physical plant. It models combustion and draft, the boiler and steam drum, the steam turbine, the generator, and the condenser/feedwater loop as coupled equations, stepping them twice a second. On every tick it reads the actuator registers from the PLC (fuel rate, valve positions, pump speeds, breaker), computes the new plant state, and writes the sensor values (pressures, temperatures, RPM, MW, …) back into the PLC.

**Layer 2: Soft PLC** (`plc_controller`) is the hub of the system and the **only server**. It holds 64 Modbus holding registers (`common/PlantRegisters.hpp`) and runs a 10 Hz scan cycle: read inputs → run the startup sequence and PID loops (drum level, turbine speed/load, furnace draft) → evaluate the safety system (Master Fuel Trip, turbine trip, ESD, first-out latch) → write outputs. Layer 1 and Layer 3 both connect to it as Modbus clients on port **5020**.

**Layer 3: SCADA / HMI** (`scada_hmi`) is the operator workstation. A background poller reads all 64 registers from the PLC 4 times a second. An embedded HTTP server (default port **8085**) serves the web dashboard in `Layer_3/web/` and exposes `/api/telemetry` and `/api/command`. Operator actions such as MW target, drum level setpoint, breaker, ESD and reset become single-register Modbus writes to the PLC.

**How they are connected:** Layer 1 and Layer 3 **never talk to each other directly**; every value goes through the PLC's registers. So if Layer 1 stops, the HMI keeps showing the last known values (it looks normal but is frozen). If the PLC stops, both other layers lose their only link and keep retrying until it comes back. **Layer 4** (security node with packet capture, an ML anomaly detector, and Red/Blue AI agents) is planned and not built yet.

## Directory structure

Each `.hpp` in `include/` has a matching `.cpp` in `src/` (except `PidController.hpp`, which is header-only). Compiled binaries are not in the repo; build them with `build_all.bat` / `build_all.sh`.

```
OT-security/
├── common/                              Shared headers used by all three layers
│   ├── PlantRegisters.hpp               64-register Modbus map, alarm bits, first-out trip codes
│   ├── ModbusProtocol.hpp               Modbus/TCP client + request/response frame encoding
│   └── SocketUtils.hpp                  Cross-platform TCP socket wrappers (WinSock / POSIX)
│
├── Layer_1/                             LAYER 1: PHYSICS DIGITAL TWIN
│   ├── include/
│   │   ├── DigitalTwinPlant.hpp         Runs all subsystems; registers in → physics → registers out
│   │   ├── CombustionSystem.hpp         Fuel, FD/ID fans, furnace draft, flame, flue-gas O2
│   │   ├── BoilerSystem.hpp             Steam drum pressure/temp/level, superheater, safety valves
│   │   ├── TurbineSystem.hpp            Rotor speed, steam flow, vibration, lube oil, trip
│   │   ├── GeneratorSystem.hpp          MW/MVAR, frequency, voltage, excitation, 52G breaker, sync
│   │   └── CondenserFeedwaterSystem.hpp Vacuum, hotwell, deaerator, BFP A/B, condensate pump
│   ├── src/
│   │   ├── main.cpp                     Main loop: read PLC → step model → write sensors + console view
│   │   └── *.cpp                        Implementations of the headers above
│   ├── Makefile                         make build (Linux / MinGW)
│   └── CMakeLists.txt                   CMake build
│
├── Layer_2/                             LAYER 2: SOFT PLC
│   ├── include/
│   │   ├── ModbusTcpServer.hpp          Multi-client Modbus/TCP server (FC03, FC06, FC16)
│   │   ├── PlcDataBank.hpp              Mutex-protected store for the 64 holding registers
│   │   ├── PlcScanEngine.hpp            10 Hz scan cycle: startup sequence + control loops
│   │   ├── PidController.hpp            PID with output limits (header-only)
│   │   └── PlcSafetyInterlocks.hpp      MFT, turbine trip, ESD, alarms, first-out latch
│   ├── src/
│   │   ├── main.cpp                     Starts server + scan loop, console status view
│   │   └── *.cpp                        Implementations of the headers above
│   ├── Makefile                         make build (Linux / MinGW)
│   └── CMakeLists.txt                   CMake build
│
├── Layer_3/                             LAYER 3: SCADA / HMI
│   ├── include/
│   │   ├── ScadaPoller.hpp              Background Modbus poller (4 Hz) + command writes
│   │   └── HttpScadaServer.hpp          HTTP server: static files + JSON API
│   ├── src/
│   │   ├── main.cpp                     Starts poller + HTTP server
│   │   └── *.cpp                        Implementations of the headers above
│   ├── web/
│   │   ├── index.html                   Dashboard layout: P&ID, gauges, alarms, controls
│   │   ├── app.js                       Polls /api/telemetry, draws trends, sends /api/command
│   │   └── style.css                    Dashboard styling
│   ├── Makefile                         make build (Linux / MinGW)
│   └── CMakeLists.txt                   CMake build
│
├── docs/
│   └── architecture.svg                 Architecture diagram (top of this README)
├── build_all.bat                        Build all three layers (Windows)
├── build_all.sh                         Build all three layers (Linux)
├── run_all.bat                          Start all three layers on one Windows machine
├── test_system.py                       End-to-end test: startup, attack, ESD, restart
├── CMakeLists.txt                       Root CMake project (builds all layers)
├── OT-Security Plant Simulator How It Works.pdf   Explainer for teammates
├── .gitignore                           Keeps binaries, captures and secrets out of git
└── README.md                            This file
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
cd OT-security/Layer_2 && make
./plc_controller 0.0.0.0 5020 100
```

Bind to `0.0.0.0` so the other machines can reach it. If you use `ufw`, open the port with `sudo ufw allow 5020/tcp`.

**4. Start the physics engine (VM A).**

```bash
cd OT-security/Layer_1 && make
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
