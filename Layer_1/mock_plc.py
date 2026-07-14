#!/usr/bin/env python3
"""
Mock PLC Server (pyModbusTCP) for local Layer 1 Physics Engine testing.
Runs on localhost:5020. This allows development of the physics engine 
without setting up virtual machines.

Install dependency:
    pip install pyModbusTCP
"""

import time
import argparse
from pyModbusTCP.server import ModbusServer, DataBank

def main():
    parser = argparse.ArgumentParser(description="Mock PLC Modbus Server")
    parser.add_argument("--host", default="127.0.0.1", help="Host IP to bind to")
    parser.add_argument("--port", type=int, default=5020, help="Modbus port to run on")
    args = parser.parse_args()

    print(f"\033[1;36m========================================================\n"
          f"       MOCK PLC MODBUS SERVER FOR LOCAL TESTING         \n"
          f"========================================================\033[0m")
    
    server = ModbusServer(host=args.host, port=args.port, no_block=True)
    
    # Initialize holding registers (16 registers, default values)
    # Reg 3: Relief Valve State (0=Closed, 1=Open)
    # Reg 4: Fuel Feed Rate (kg/min)
    # Reg 8: Main Steam Valve State (0=Closed, 1=Open)
    # Reg 12: Grid Breaker State (0=Open, 1=Closed)
    # Reg 15: Cooling Water Flow (%)
    
    db = DataBank()
    db.set_holding_registers(3, [0])    # Reg 3: Boiler relief valve closed
    db.set_holding_registers(4, [40])   # Reg 4: Fuel feed rate at 40 kg/min (nominal)
    db.set_holding_registers(8, [1])    # Reg 8: Main steam valve open
    db.set_holding_registers(12, [0])   # Reg 12: Grid Breaker open
    db.set_holding_registers(15, [60])  # Reg 15: Cooling Water Flow at 60%

    server.data_bank = db

    try:
        server.start()
        print(f"[INFO] Mock PLC Server active on {args.host}:{args.port}")
        print(f"[INFO] Modbus Register defaults configured:")
        print(f"  - Register 0003 (Relief Valve) : {db.get_holding_registers(3, 1)[0]} (Closed)")
        print(f"  - Register 0004 (Fuel Feed)     : {db.get_holding_registers(4, 1)[0]} kg/min")
        print(f"  - Register 0008 (Main Steam Vlv): {db.get_holding_registers(8, 1)[0]} (Open)")
        print(f"  - Register 0012 (Grid Breaker)  : {db.get_holding_registers(12, 1)[0]} (Open)")
        print(f"  - Register 0015 (Cooling Flow)  : {db.get_holding_registers(15, 1)[0]} %")
        print(f"\n[INFO] Listening for connections from the Physics Engine...")

        while True:
            # Read current values in DataBank
            regs = db.get_holding_registers(0, 16)
            
            # Print status periodically
            print(f"\033[H\033[J", end="") # Clear terminal screen
            print(f"\033[1;32m========================================================\n"
                  f"               MOCK PLC DATABASE VALUES                 \n"
                  f"========================================================\033[0m")
            print(f" [SENSORS - Written by Physics Engine]")
            print(f"   Boiler Temp (Reg 0)      : {regs[0]} °C")
            print(f"   Boiler Pressure (Reg 1)  : {regs[1]} PSI")
            print(f"   Boiler Water Level (Reg 2): {regs[2]} %")
            print(f"   Turbine RPM (Reg 5)      : {regs[5]} RPM")
            print(f"   Steam Inflow (Reg 6)     : {regs[6]} %")
            print(f"   Vibration x10 (Reg 7)    : {regs[7]} ({regs[7]/10.0} mm/s)")
            print(f"   Power Gen (Reg 9)        : {regs[9]} MW")
            print(f"   Freq x100 (Reg 10)       : {regs[10]} ({regs[10]/100.0} Hz)")
            print(f"   Volt x10 (Reg 11)        : {regs[11]} ({regs[11]/10.0} kV)")
            print(f"   Hotwell Temp (Reg 13)    : {regs[13]} °C")
            print(f"   Vacuum x10 (Reg 14)      : {regs[14]} ({regs[14]/10.0} inHg)")
            print(f"\n [CONTROLS - Change these to test physics responses]")
            print(f"   Relief Valve (Reg 3)     : {regs[3]} (0=Closed, 1=Open)")
            print(f"   Fuel Feed (Reg 4)        : {regs[4]} kg/min")
            print(f"   Main Steam Valve (Reg 8) : {regs[8]} (0=Closed, 1=Open)")
            print(f"   Grid Breaker (Reg 12)    : {regs[12]} (0=Open, 1=Closed)")
            print(f"   Cooling Water Flow (Reg 15): {regs[15]} %")
            print(f"\n\033[1;30mPress Ctrl+C to terminate the Mock PLC.\033[0m")
            
            time.sleep(1)

    except KeyboardInterrupt:
        print("\n[SHUTDOWN] Stopping mock PLC server...")
        server.stop()
        print("[SHUTDOWN] Exited.")

if __name__ == "__main__":
    main()
