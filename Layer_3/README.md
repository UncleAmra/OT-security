# Layer 3: SCADA / HMI Operator Workstation

The **Layer 3 SCADA / HMI Operator Workstation** provides a full supervisory control dashboard for human operators running in an isolated VM (Purdue Level 3).

It pairs a high-performance C++ asynchronous Modbus client daemon with an embedded cross-platform HTTP web server and a responsive, futuristic, dark-mode industrial Web HMI.

---

## Features

1. **Live Animated P&ID Diagram (Process & Instrumentation)**:
   * Dynamic steam drum water level with bubbling evaporation.
   * Superheated steam piping with live velocity particle flow.
   * Real-time rotating turbine blades and generator rotor.
   * Live 52G breaker switch position and synchroscope alignment.
   * Boiler feed pumps (BFP A/B) and surface condenser vacuum dynamics.
2. **Precision Telemetry Gauges**:
   * Drum pressure, temperature, water level %, and superheater live steam temperature.
   * Turbine RPM tachometer, Bearing 1 & 2 vibration levels (mm/s), lube oil pressure.
   * Generator active power (MW), reactive power (MVAR), frequency (Hz), terminal voltage (kV), power factor.
   * Condenser vacuum (inHg), hotwell temp (°C), and flue gas O2 %.
3. **Real-Time Strip Chart**:
   * Continuous Canvas plot tracking Drum Pressure, Drum Level, and Megawatt Generation over time.
4. **ISA-18.2 Alarm Annunciator Matrix**:
   * 12 flashing alarm annunciator tiles for critical and high-priority alarms.
   * Dedicated First-Out Root Cause Banner identifying the initiate event that triggered cascade trips.
5. **Interactive Operator Desk Setpoints & Interlocks**:
   * Target Megawatt (MW) demand slider.
   * Target Drum Level (%) setpoint slider.
   * 52G Generator Breaker Connect / Disconnect button.
   * Safety Relief Valve pop test button.
   * Auto / Manual PLC regulation mode toggle.
   * **Emergency Plant Trip (ESD / MFT) button** with confirmation guardrail.

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
# Run pointing to Layer 2 PLC (e.g. 192.168.56.12:5020) and web port 8080
./scada_hmi 192.168.56.12 5020 8080 web

# Localhost testing:
./scada_hmi 127.0.0.1 5020 8080 web
```

Once running, open your web browser to:
`http://localhost:8080` (or `http://<VM3_IP>:8080` from another machine on the network).
