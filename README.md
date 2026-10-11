# OT Power Plant Security Testbed with Agentic AI

An Operational Technology (OT) testbed simulating a 500 MW critical infrastructure power plant using an adapted **Purdue Model (ISA-95 / IEC 62443)** across a 4-Virtual Machine network topology. Built completely in **modern C++ (C++17/C++20)**, this environment pairs high-fidelity thermodynamic and electromechanical simulation with real-time industrial protocols (Modbus/TCP) and an interactive web-based SCADA HMI workstation.

---

## 1. Purdue 4-Layer Architecture & VM Network Topology

The environment is distributed across 4 isolated Virtual Machines on a private, host-only virtual network (`192.168.56.0/24`):

```
+---------------------------------------------------------------------------------------+
|  VM 3 (Purdue Level 3 - SCADA / HMI Operator Workstation)                             |
|  IP: 192.168.56.13                                                                    |
|  - C++ Asynchronous Modbus Client Daemon + Embedded Web SCADA Server (Port 8080)       |
|  - Interactive Dark-Mode P&ID Plant Schematic (SVG/Canvas), Real-Time Gauges & Trends  |
|  - Operator Controls: Megawatt demand, drum setpoints, breaker toggle, emergency ESD  |
+-------------------------------------------▲-------------------------------------------+
                                            │  Modbus/TCP (Port 5020 or 502)
                                            ▼
+---------------------------------------------------------------------------------------+
|  VM 2 (Purdue Level 2 - Industrial PLC Controller Runtime)                            |
|  IP: 192.168.56.12                                                                    |
|  - Multi-Client C++ Modbus TCP Server Daemon (Holding Registers 40001 - 40064)        |
|  - Deterministic 10 Hz Cyclic Scan Engine (Input Scan -> Logic/PID -> SIS -> Outputs) |
|  - Regulatory Control: Drum Level 3-element PID, Turbine DEH Governor, Draft Control  |
|  - Safety Instrumented System (SIS / 2oo3): MFT, Turbine ETS Trip, First-Out Latch    |
+-------------------------------------------▲-------------------------------------------+
                                            │  Modbus/TCP (Port 5020 or 502)
                                            ▼
+---------------------------------------------------------------------------------------+
|  VM 1 (Purdue Level 1 - Physics Digital Twin Engine)                                  |
|  IP: 192.168.56.11                                                                    |
|  - Modular C++ Digital Twin stepping coupled non-linear differential equations (2 Hz) |
|  - Steam Drum (Shrink & swell bubble dynamics, Rankine saturation curve, ASME relief) |
|  - Combustion & Draft (Forced/Induced draft fans, furnace draft pressure, flame scan) |
|  - Steam Turbine (Rotor inertia, governor CV throttle, 1800 RPM resonance, ETS trip)  |
|  - Synchronous Generator (Terminal kV, AVR excitation, grid sync permissive, 52G)     |
|  - Condenser & Feedwater (Vacuum inHg, hotwell cooling, redundant BFP A/B pumps, CEP) |
+---------------------------------------------------------------------------------------+

+---------------------------------------------------------------------------------------+
|  VM 4 (Purdue Level 2/3 Security Node - Red & Blue Team AI Agents)                    |
|  IP: 192.168.56.14                                                                    |
|  - Promiscuous packet sniffing (PyShark/Scapy) on the Host-Only network interface     |
|  - Tier 1 High-Speed ML Filter (Isolation Forest anomaly detector < 1 ms latency)     |
|  - Tier 2 Agentic LLM Orchestrator (Reasoning, autonomous mitigation & OT interlocks) |
|  - Red Team offensive agent executing Modbus register spoofing & exploit injection   |
+---------------------------------------------------------------------------------------+
```

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

## 3. Quick Start & Execution

### Option A: Local Single-Machine Testing (Windows or Linux)

You can run the entire 3-layer industrial power plant locally on one machine:

1. **Build All Layers**:
   * On Windows: `build_all.bat`
   * On Linux: `chmod +x build_all.sh && ./build_all.sh`
   * Or with CMake: `cmake -B build && cmake --build build`

2. **Terminal 1: Start Layer 2 PLC Server**:
   ```bash
   cd Layer_2
   ./plc_controller 0.0.0.0 5020 100
   ```

3. **Terminal 2: Start Layer 1 Physics Twin**:
   ```bash
   cd Layer_1
   ./physics_engine 127.0.0.1 5020 500
   ```

4. **Terminal 3: Start Layer 3 SCADA HMI**:
   ```bash
   cd Layer_3
   ./scada_hmi 127.0.0.1 5020 8080 web
   ```

5. **Open Operator Console**:
   Navigate to [http://localhost:8080](http://localhost:8080) in your web browser.

---

### Option B: Multi-VM Network Deployment

1. **Hypervisor Setup (VirtualBox / VMware)**:
   * Create a **Host-Only Network** (e.g. `192.168.56.0/24`) with DHCP **disabled**.
   * Attach Adapter 1 of each VM to this Host-Only Network with **Promiscuous Mode: Allow All**.
2. **Assign Static IPs**:
   * VM 1 (Physics): `192.168.56.11`
   * VM 2 (PLC): `192.168.56.12`
   * VM 3 (HMI): `192.168.56.13`
   * VM 4 (Security): `192.168.56.14`
3. **Launch Sequence**:
   * **Phase 1 (VM 2)**: `./plc_controller 0.0.0.0 5020 100` (Listening on port 5020)
   * **Phase 2 (VM 1)**: `./physics_engine 192.168.56.12 5020 500`
   * **Phase 3 (VM 3)**: `./scada_hmi 192.168.56.12 5020 8080 web` -> Open browser at `http://localhost:8080`
   * **Phase 4 (VM 4)**: Launch packet sniffing / AI agent defense nodes