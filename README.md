# OT Power Plant Security Testbed with Agentic AI

An Operational Technology (OT) testbed simulating a 500 MW critical infrastructure power plant using an adapted **Purdue Model (ISA-95 / IEC 62443)** across a multi-machine network topology. Built completely in **modern C++ (C++17/C++20)**, this environment pairs high-fidelity thermodynamic and electromechanical simulation with real-time industrial protocols (Modbus/TCP) and an interactive web-based SCADA HMI workstation.

---

## 1. Purdue Layer Architecture & Network Topology

The environment is distributed across isolated machines on a private, host-only virtual network. In this deployment we used **3 machines** (Layer 1 and Layer 2 on separate VMs, Layer 3 on the host); a 4th machine (Security node) is planned but not yet running.

```
+---------------------------------------------------------------------------------------+
|  Layer 3 (Purdue Level 3 - SCADA / HMI Operator Workstation)                          |
|  Host machine (e.g. Windows), connects OUT to the Layer 2 PLC over Modbus/TCP         |
|  - C++ Asynchronous Modbus Client Daemon + Embedded Web SCADA Server                  |
|  - Interactive Dark-Mode P&ID Plant Schematic (SVG/Canvas), Real-Time Gauges & Trends  |
|  - Operator Controls: Megawatt demand, drum setpoints, breaker toggle, emergency ESD  |
+-------------------------------------------▲-------------------------------------------+
                                            │  Modbus/TCP (Port 5020)
                                            ▼
+---------------------------------------------------------------------------------------+
|  Layer 2 (Purdue Level 2 - Industrial PLC Controller Runtime) — VM                    |
|  Listens on 0.0.0.0:5020, reachable at this VM's host-only IP                         |
|  - Multi-Client C++ Modbus TCP Server Daemon (Holding Registers 40001 - 40064)        |
|  - Deterministic 10 Hz Cyclic Scan Engine (Input Scan -> Logic/PID -> SIS -> Outputs) |
|  - Regulatory Control: Drum Level 3-element PID, Turbine DEH Governor, Draft Control  |
|  - Safety Instrumented System (SIS / 2oo3): MFT, Turbine ETS Trip, First-Out Latch    |
+-------------------------------------------▲-------------------------------------------+
                                            │  Modbus/TCP (Port 5020)
                                            ▼
+---------------------------------------------------------------------------------------+
|  Layer 1 (Purdue Level 1 - Physics Digital Twin Engine) — VM                          |
|  Connects OUT to the Layer 2 PLC's IP; writes sensor values into its registers        |
|  - Modular C++ Digital Twin stepping coupled non-linear differential equations (2 Hz) |
|  - Steam Drum (Shrink & swell bubble dynamics, Rankine saturation curve, ASME relief) |
|  - Combustion & Draft (Forced/Induced draft fans, furnace draft pressure, flame scan) |
|  - Steam Turbine (Rotor inertia, governor CV throttle, 1800 RPM resonance, ETS trip)  |
|  - Synchronous Generator (Terminal kV, AVR excitation, grid sync permissive, 52G)     |
|  - Condenser & Feedwater (Vacuum inHg, hotwell cooling, redundant BFP A/B pumps, CEP) |
+---------------------------------------------------------------------------------------+

+---------------------------------------------------------------------------------------+
|  Layer 4 (Purdue Level 2/3 Security Node - Red & Blue Team AI Agents) — NOT YET UP     |
|  - Promiscuous packet sniffing (PyShark/Scapy) on the Host-Only network interface     |
|  - Tier 1 High-Speed ML Filter (Isolation Forest anomaly detector < 1 ms latency)     |
|  - Tier 2 Agentic LLM Orchestrator (Reasoning, autonomous mitigation & OT interlocks) |
|  - Red Team offensive agent executing Modbus register spoofing & exploit injection   |
+---------------------------------------------------------------------------------------+
```

**Important architectural note:** the HMI (Layer 3) only ever talks to the PLC (Layer 2). It has no direct connection to Layer 1. Layer 1 writes values into the PLC's registers; the HMI reads them back. This means:
- If Layer 1 dies, the PLC keeps answering the HMI, so the dashboard keeps displaying the *last known values* instead of erroring — it will look deceptively normal.
- If Layer 2 (the PLC) dies, the HMI loses its only upstream connection and stops entirely, since the PLC is a single point of failure for the whole visualization layer.

---

## 2. Directory Structure

```
OT-security/
├── common/                     # Cross-platform shared C++ headers
│   ├── PlantRegisters.hpp      # 64-register master Modbus mapping & bitmasks
│   ├── SocketUtils.hpp         # Cross-platform RAII socket wrappers (WinSock & POSIX)
│   └── ModbusProtocol.hpp      # Zero-dependency Modbus TCP client/server serializer
├── Layer_1/                    # Level 1: Physics Digital Twin Engine (C++)
│   ├── include/                # Boiler, Combustion, Turbine, Generator, Condenser headers
│   ├── src/                    # Differential equation implementations & main loop
│   ├── Makefile                # GCC/MinGW build configuration
│   └── CMakeLists.txt          # CMake build configuration
├── Layer_2/                    # Level 2: Industrial PLC Controller Runtime (C++)
│   ├── include/                # DataBank, PID Controller, SafetyInterlocks, ScanEngine
│   ├── src/                    # Multi-client Modbus server & scan cycle execution
│   ├── Makefile                # GCC/MinGW build configuration
│   └── CMakeLists.txt          # CMake build configuration
├── Layer_3/                    # Level 3: SCADA / HMI Operator Workstation (C++ & Web)
│   ├── include/                # Asynchronous ScadaPoller & embedded HttpScadaServer
│   ├── src/                    # Telemetry caching, REST API, command dispatch
│   ├── web/                    # Rich Dark-Mode Industrial Web HMI
│   │   ├── index.html          # Animated SVG P&ID diagram, gauges, annunciator
│   │   ├── style.css           # Glassmorphism cyber-industrial theme
│   │   └── app.js              # Real-time WebSocket/polling telemetry & trend charts
│   ├── Makefile                # GCC/MinGW build configuration
│   └── CMakeLists.txt          # CMake build configuration
├── build_all.bat               # Windows batch script to compile all 3 layers
├── build_all.sh                # Linux bash script to compile all 3 layers
└── CMakeLists.txt              # Root CMake project file
```

---

## 3. Multi-VM Deployment Guide

This is the deployment that matters for the project goal: three **separate machines**, communicating only over the network, so traffic between Purdue layers is genuinely interceptable — Option A (running everything on one machine) does not satisfy this and is intentionally omitted here.

### 3.1 Machine roles

| Machine | Role | Runs |
|---|---|---|
| VM A | Level 1 — Physics Digital Twin | `Layer_1` |
| VM B | Level 2 — PLC | `Layer_2` |
| Host (or VM C) | Level 3 — HMI | `Layer_3` |

Clone the **entire repository** onto both VMs (not just the one layer folder) — each layer's build includes headers from `../common`, so `common/` must sit next to the layer being built.

### 3.2 Hypervisor network setup (VMware or VirtualBox)

1. Shut each VM down fully before changing network settings.
2. Give each VM a **Host-only** network adapter (VMware: VM → Settings → Network Adapter → Host-only). A second NAT adapter is optional and only needed for internet access (e.g. `apt install`).
3. Power the VMs on and find their host-only IP on each:
   ```bash
   ip -br a
   ```
   Look for the interface in the same subnet as the host's VMware/VirtualBox host-only adapter (check with `ipconfig` on Windows — look for "VMware Network Adapter VMnet1/VMnet8" or "VirtualBox Host-Only Adapter"). DHCP-assigned addresses are fine; static IPs are not required.
4. Confirm all three machines can ping each other in every direction before moving on. If a ping fails, the adapters are not on the same host-only network.

### 3.3 Install build tools on both VMs

```bash
sudo apt update
sudo apt install -y build-essential cmake openssh-server dos2unix
```
`build-essential` provides `g++` and `make`, which is all the Makefiles need; CMake is an optional alternate build path. If a cloned/copied file has Windows line endings and `make` fails strangely, run:
```bash
find . -type f \( -name "*.sh" -o -name "Makefile" \) -exec dos2unix {} +
```

### 3.4 Build and start the PLC (VM B, Layer 2)

```bash
cd OT-security/Layer_2
make
./plc_controller 0.0.0.0 5020 100
```
`0.0.0.0` is required so the server listens on all interfaces, not just localhost — otherwise VM A and the host can't reach it. Leave this terminal running; it's the long-lived server.

### 3.5 Build and start the physics engine (VM A, Layer 1)

```bash
cd OT-security/Layer_1
make
./physics_engine <PLC_IP> 5020 500
```
Replace `<PLC_IP>` with VM B's host-only IP from step 3.2.

### 3.6 Build and start the HMI (Host, Layer 3)

On Windows, install a C++ toolchain (MinGW-w64) if you don't have one, then:
```powershell
cd OT-security\Layer_3
mingw32-make
.\scada_hmi.exe <PLC_IP> 5020 8080 web
```
Run this from inside `Layer_3`, since the `web` argument is a relative path to the static HTML/CSS/JS folder.

**Port conflicts:** if port 8080 is already in use by something else on your machine, the program falls back to the next free port and prints the actual URL it bound to (e.g. `http://localhost:8081`) — always read that line rather than assuming 8080. You can also force a specific port by passing a different number as the third argument.

Then open the printed URL in a browser. You should see the P&ID dashboard, with gauges updating once Layer 1 and Layer 2 are both running.

### 3.7 Verifying the full pipeline is live

Don't just trust that the page loaded — confirm data is actually flowing end-to-end:
- Watch the HMI gauges for a few seconds; the values should be visibly changing, not static.
- Stop Layer 1 (Ctrl+C) and confirm the gauges freeze at their last values rather than erroring — this is expected given the architecture note in Section 1, and confirms the write path works.
- Restart Layer 1 and confirm the gauges resume moving within a couple seconds.
- Stop Layer 2 and confirm the HMI loses connection — this confirms the PLC is the HMI's only upstream link.

### 3.8 Firewall notes

Debian ships with no firewall enabled by default. Ubuntu sometimes has `ufw` active — if a connection to port 5020 is refused rather than timing out, check with `sudo ufw status` and open it if needed:
```bash
sudo ufw allow 5020/tcp
```

### 3.9 Launch order summary

1. VM B: start the PLC (`Layer_2`) first — everything else connects to it.
2. VM A: start the physics engine (`Layer_1`), pointed at VM B's IP.
3. Host: build and start the HMI (`Layer_3`), pointed at VM B's IP, then open it in a browser.

### 3.10 Next step: Layer 4 (Security node)

Not yet deployed. Per the project proposal, this is deliberately deferred until the 3-layer integration above is stable and verified. It will add a 4th machine running packet capture (PyShark/Scapy) on the host-only interface, an anomaly detector, and Red/Blue Team agents that interact with the PLC over the same Modbus/TCP link.
