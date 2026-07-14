# Layer 1: Modular Physics Engine Simulation

The **Physics Engine** is a modular C-based digital twin that simulates the thermodynamic, rotational, electrical, and cooling cycles of a critical power plant. It calculates real-time dynamics and interacts directly with the **Level 2 PLC (Programmable Logic Controller)** using standard Modbus/TCP.

---

## Modular Architecture

The simulator is separated into distinct physical components and network layers:

### Header Files (`include/`)
- [config.h](file:///C:/Users/edgar/OneDrive/Desktop/project_1/OT-security/Layer_1/include/config.h): Central configuration for network details, PLC IPs/ports, tick rates, and Modbus holding registers.
- [boiler.h](file:///C:/Users/edgar/OneDrive/Desktop/project_1/OT-security/Layer_1/include/boiler.h): Holds the Boiler state and thermodynamic equations declarations.
- [turbine.h](file:///C:/Users/edgar/OneDrive/Desktop/project_1/OT-security/Layer_1/include/turbine.h): Holds the Turbine state (RPM, vibration) and rotational dynamics.
- [generator.h](file:///C:/Users/edgar/OneDrive/Desktop/project_1/OT-security/Layer_1/include/generator.h): Holds the Generator state (MW output, voltage, frequency) and electrical synchronization dynamics.
- [condenser.h](file:///C:/Users/edgar/OneDrive/Desktop/project_1/OT-security/Layer_1/include/condenser.h): Holds the Condenser state (hotwell temp, vacuum) and cooling dynamics.
- [modbus_client.h](file:///C:/Users/edgar/OneDrive/Desktop/project_1/OT-security/Layer_1/include/modbus_client.h): Lightweight, dependency-free Modbus/TCP client interface definitions.

### Source Files (`src/`)
- [boiler.c](file:///C:/Users/edgar/OneDrive/Desktop/project_1/OT-security/Layer_1/src/boiler.c): Temperature heating dynamics based on fuel feed, water level boil-off, and pressure accumulation.
- [turbine.c](file:///C:/Users/edgar/OneDrive/Desktop/project_1/OT-security/Layer_1/src/turbine.c): Converts boiler steam pressure into rotational RPM. Models critical resonance vibration around $1800 \text{ RPM}$ and structural damage vibration during overspeed ($>3600 \text{ RPM}$).
- [generator.c](file:///C:/Users/edgar/OneDrive/Desktop/project_1/OT-security/Layer_1/src/generator.c): Simulates magnetic excitation (voltage) and grid synchronization. If breaker is closed, frequency locks to the grid ($60 \text{ Hz}$) and power is generated. If open, the generator runs without load (creating overspeed risk).
- [condenser.c](file:///C:/Users/edgar/OneDrive/Desktop/project_1/OT-security/Layer_1/src/condenser.c): Condensation loop. Cooling water flow reduces hotwell temperature, creating a vacuum ($0\text{-}30 \text{ inHg}$). Loss of cooling flow collapses vacuum back to atmospheric levels.
- [modbus_client.c](file:///C:/Users/edgar/OneDrive/Desktop/project_1/OT-security/Layer_1/src/modbus_client.c): Custom Modbus TCP packet serialization (MBAP + PDU serialization for FC 03 and FC 16).
- [main.c](file:///C:/Users/edgar/OneDrive/Desktop/project_1/OT-security/Layer_1/src/main.c): Main execution loop. Reads commands from the PLC, drives sequential updates, executes Modbus writes back to the PLC, and displays visual console metrics.

---

## Modbus Register Specifications

All components communicate telemetry state and control inputs in a single contiguous block of 16 holding registers:

| Register | Name | Scale | Direction | Description |
|---|---|---|---|---|
| **0000** | Boiler Temperature | $1$ | Write (to PLC) | Boiler shell temperature (°C) |
| **0001** | Boiler Pressure | $1$ | Write (to PLC) | Boiler internal pressure (PSI) |
| **0002** | Water Level | $1$ | Write (to PLC) | Boiler liquid level percentage (%) |
| **0003** | Relief Valve State | $1$ | Read (from PLC) | Emergency relief valve (0 = Closed, 1 = Open) |
| **0004** | Fuel Feed Rate | $1$ | Read (from PLC) | Burner fuel flow input (kg/min) |
| **0005** | Turbine RPM | $1$ | Write (to PLC) | Turbine shaft speed (0 - 3600 RPM) |
| **0006** | Steam Inflow Rate | $1$ | Write (to PLC) | Valve throttled steam inflow rate (%) |
| **0007** | Vibration Level | $10$ | Write (to PLC) | Shaft displacement (mm/s * 10) |
| **0008** | Main Steam Valve State | $1$ | Read (from PLC) | Steam throttle valve (0 = Closed, 1 = Open) |
| **0009** | Power Output | $1$ | Write (to PLC) | Generator active grid power output (MW) |
| **0010** | Grid Frequency | $100$ | Write (to PLC) | AC Frequency (Hz * 100) |
| **0011** | Output Voltage | $10$ | Write (to PLC) | Terminal voltage (kV * 10) |
| **0012** | Grid Breaker State | $1$ | Read (from PLC) | Synchronization breaker (0 = Open, 1 = Closed) |
| **0013** | Hotwell Temperature | $1$ | Write (to PLC) | Condensate hotwell temp (°C) |
| **0014** | Vacuum Pressure | $10$ | Write (to PLC) | Condenser vacuum level (inHg * 10) |
| **0015** | Cooling Water Flow | $1$ | Read (from PLC) | Auxiliary cooling water flow (%) |

---

## Compilation

Ensure you have a C compiler (`gcc` or `clang`) and `make` installed.

### Build Target
```bash
make
```

### Clean Target
```bash
make clean
```

---

## Execution

Run the compiled binary. You can supply the optional PLC host IP and port (defaults to `127.0.0.1:5020`).

```bash
# Run with defaults
./physics_engine

# Run pointing to a specific PLC IP
./physics_engine 192.168.56.10 502
```
