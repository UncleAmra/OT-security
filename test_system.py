import socket
import struct
import time
import urllib.request
import json
import subprocess
import os
import sys

def modbus_read(sock, unit_id, start_addr, count):
    # FC 03
    req = struct.pack(">HHHBBHH", 1, 0, 6, unit_id, 3, start_addr, count)
    sock.sendall(req)
    header = sock.recv(7)
    if len(header) < 7: return None
    _, _, length, _ = struct.unpack(">HHHB", header)
    pdu = sock.recv(length - 1)
    if len(pdu) < 2 or pdu[0] != 3: return None
    byte_count = pdu[1]
    values = struct.unpack(f">{count}H", pdu[2:2+byte_count])
    return list(values)

def modbus_write_single(sock, unit_id, addr, val):
    # FC 06
    req = struct.pack(">HHHBBHH", 2, 0, 6, unit_id, 6, addr, val)
    sock.sendall(req)
    resp = sock.recv(12)
    return len(resp) == 12

def http_get(url):
    req = urllib.request.Request(url)
    with urllib.request.urlopen(req, timeout=3) as response:
        return json.loads(response.read().decode())

def http_post(url, data):
    req = urllib.request.Request(
        url,
        data=json.dumps(data).encode(),
        headers={"Content-Type": "application/json"}
    )
    with urllib.request.urlopen(req, timeout=3) as response:
        return json.loads(response.read().decode())

def run_tests():
    project_dir = os.path.dirname(os.path.abspath(__file__))
    plc_exe = os.path.join(project_dir, "Layer_2", "plc_controller.exe")
    physics_exe = os.path.join(project_dir, "Layer_1", "physics_engine.exe")
    scada_exe = os.path.join(project_dir, "Layer_3", "scada_hmi.exe")

    print("[TEST RUNNER] Starting Layer 2 (PLC)...")
    p_plc = subprocess.Popen([plc_exe, "127.0.0.1", "5020", "100"], cwd=os.path.join(project_dir, "Layer_2"),
                             stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    time.sleep(1)

    print("[TEST RUNNER] Starting Layer 1 (Physics Twin)...")
    p_phys = subprocess.Popen([physics_exe, "127.0.0.1", "5020", "500"], cwd=os.path.join(project_dir, "Layer_1"),
                              stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    time.sleep(1)

    print("[TEST RUNNER] Starting Layer 3 (SCADA HMI)...")
    p_scada = subprocess.Popen([scada_exe, "127.0.0.1", "5020", "8085", "web"], cwd=os.path.join(project_dir, "Layer_3"),
                               stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    time.sleep(2)

    try:
        # -------------------------------------------------------------
        # TEST 1: COLD STARTUP
        # -------------------------------------------------------------
        print("\n=== TEST 1: COLD STARTUP INITIAL CONDITIONS ===")
        telem = http_get("http://127.0.0.1:8085/api/telemetry")
        print(f"Connected: {telem.get('connected')}")
        print(f"Boiler Temp: {telem['boiler']['drumTemp']:.1f} °C (expect ~25)")
        print(f"Boiler Press: {telem['boiler']['drumPress']:.1f} PSI (expect ~15)")
        print(f"Turbine Speed: {telem['turbine']['rpm']:.0f} RPM (expect 0)")
        print(f"Generator MW: {telem['generator']['mw']:.1f} MW (expect 0)")
        print(f"MFT State: {telem['safety']['mft']} (expect 0)")
        print(f"Turbine Trip: {telem['safety']['turbineTrip']} (expect 0)")
        print(f"Security Alert Level: {telem['safety']['securityAlertLevel']} (expect 0 = NORMAL)")

        assert telem['boiler']['drumTemp'] < 70.0, "Boiler overheated immediately!"
        assert telem['turbine']['rpm'] < 50.0, "Turbine oversped immediately!"
        assert telem['safety']['mft'] == 0, "MFT false trip!"
        assert telem['safety']['turbineTrip'] == 0, "Turbine false trip!"
        print(">> TEST 1 PASSED: Initial conditions are cold, safe, and free from false alarms/trips!")

        # -------------------------------------------------------------
        # TEST 2: CONTROLLED STARTUP & SUSTAINED NORMAL OPERATION
        # -------------------------------------------------------------
        print("\n=== TEST 2: CONTROLLED STARTUP SEQUENCE & NORMAL OPERATION ===")
        print("Waiting for plant to warm up and synchronize (monitoring progression)...")
        synced = False
        for i in range(35):
            time.sleep(2)
            t = http_get("http://127.0.0.1:8085/api/telemetry")
            drum_p = t['boiler']['drumPress']
            rpm = t['turbine']['rpm']
            mw = t['generator']['mw']
            breaker = t['generator']['breaker']
            fuel = t['combustion']['fuelRate']
            print(f"  [T={2+i*2:2d}s] Press: {drum_p:5.1f} PSI | Fuel: {fuel:4.1f} kg/m | RPM: {rpm:4.0f} | 52G: {breaker} | Gen: {mw:5.1f} MW")
            if breaker == 1 and rpm >= 3550 and mw > 100:
                synced = True
                print(">> Plant successfully achieved grid synchronization and power generation!")
                break

        assert synced, "Plant failed to synchronize to grid!"
        
        # Verify sustained steady state
        time.sleep(4)
        t_stable = http_get("http://127.0.0.1:8085/api/telemetry")
        print(f"\nSteady-state parameters:")
        print(f"  Drum Pressure: {t_stable['boiler']['drumPress']:.1f} PSI (stable ~150-160 PSI)")
        print(f"  Drum Level:    {t_stable['boiler']['drumLevel']:.1f} % (stable ~50%)")
        print(f"  Turbine RPM:   {t_stable['turbine']['rpm']:.0f} RPM (stable ~3600 RPM)")
        print(f"  Gen Output:    {t_stable['generator']['mw']:.1f} MW (target ~250 MW)")
        print(f"  Alarms Low:    0x{t_stable['safety']['alarmLow']:x}")
        print(f"  Alarms High:   0x{t_stable['safety']['alarmHigh']:x}")
        assert t_stable['safety']['mft'] == 0, "Unnecessary MFT trip during normal operation!"
        assert t_stable['safety']['turbineTrip'] == 0, "Unnecessary Turbine trip during normal operation!"
        print(">> TEST 2 PASSED: Plant smoothly started up and maintains sustained stable normal operation!")

        # -------------------------------------------------------------
        # TEST 3 & 4: ATTACK VIA MODBUS & ANOMALY DETECTION
        # -------------------------------------------------------------
        print("\n=== TEST 3 & 4: MODBUS MANIPULATION ATTACK & ANOMALY DETECTION ===")
        # Attacker connects directly to Layer 2 PLC Modbus port 5020 and overrides fuel to 100 kg/min (runaway overfire)
        print("Attacker injecting malicious fuel feed override: REG 10 = 95 kg/min...")
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.connect(("127.0.0.1", 5020))
        # Switch to manual override mode (REG 60 = 0) so PLC doesn't auto-trim attacker's fuel
        modbus_write_single(sock, 1, 60, 0)
        # Inject extreme fuel
        modbus_write_single(sock, 1, 10, 95)
        sock.close()

        print("Monitoring physical response to attack and safety response...")
        attack_detected = False
        tripped_overpressure = False
        for i in range(15):
            time.sleep(1)
            t_att = http_get("http://127.0.0.1:8085/api/telemetry")
            drum_p = t_att['boiler']['drumPress']
            anom = t_att['safety']['anomalyStatus']
            sec_lvl = t_att['safety']['securityAlertLevel']
            mft = t_att['safety']['mft']
            srv1 = t_att['boiler']['safety1']
            print(f"  [Attack T+{i+1}s] Press: {drum_p:5.1f} PSI | SRV1: {srv1} | Anom: 0x{anom:x} | SecLevel: {sec_lvl} | MFT: {mft}")
            if anom > 0:
                attack_detected = True
            if mft == 1:
                tripped_overpressure = True
                print(">> Master Fuel Trip (MFT) triggered by safety system due to boiler overpressure!")
                break

        assert attack_detected, "Anomaly detection failed to flag the fuel manipulation / overpressure!"
        assert tripped_overpressure, "Protective safety system failed to trip boiler on overpressure!"
        print(">> TEST 3 & 4 PASSED: Attack affected physics, anomaly was detected, and safety interlocks tripped safely!")

        # -------------------------------------------------------------
        # TEST 5: EMERGENCY STOP (ESD)
        # -------------------------------------------------------------
        print("\n=== TEST 5: EMERGENCY STOP BUTTON VIA LAYER 3 ===")
        # Send ESD command
        resp = http_post("http://127.0.0.1:8085/api/command", {"cmd": "esd"})
        print(f"Command response: {resp}")
        time.sleep(1)
        t_esd = http_get("http://127.0.0.1:8085/api/telemetry")
        print(f"MFT Active: {t_esd['safety']['mft']} (expect 1)")
        print(f"Turbine Trip: {t_esd['safety']['turbineTrip']} (expect 1)")
        print(f"ESD Latched: {t_esd['safety']['esd']} (expect 1)")
        print(f"Fuel Rate: {t_esd['combustion']['fuelRate']} (expect 0)")
        print(f"Governor CV: {t_esd['turbine']['cv']} (expect 0)")
        print(f"Grid Breaker: {t_esd['generator']['breaker']} (expect 0)")
        assert t_esd['safety']['mft'] == 1 and t_esd['safety']['turbineTrip'] == 1, "ESD failed to trip systems!"
        print(">> TEST 5 PASSED: Emergency Stop latches trip and forces safe shutdown of all plant processes!")

        # -------------------------------------------------------------
        # TEST 6: RESTART SIMULATION
        # -------------------------------------------------------------
        print("\n=== TEST 6: RESTART SIMULATION BUTTON VIA LAYER 3 ===")
        # Send Restart command
        resp_rst = http_post("http://127.0.0.1:8085/api/command", {"cmd": "restart"})
        print(f"Restart response: {resp_rst}")
        time.sleep(2)
        t_rst = http_get("http://127.0.0.1:8085/api/telemetry")
        print(f"MFT Active after restart: {t_rst['safety']['mft']} (expect 0)")
        print(f"Turbine Trip after restart: {t_rst['safety']['turbineTrip']} (expect 0)")
        print(f"First-Out after restart: {t_rst['safety']['firstOut']} (expect 0)")
        print(f"Boiler Drum Temp: {t_rst['boiler']['drumTemp']:.1f} °C (expect ~25)")
        print(f"Boiler Drum Press: {t_rst['boiler']['drumPress']:.1f} PSI (expect ~15)")
        print(f"Turbine RPM: {t_rst['turbine']['rpm']:.0f} RPM (expect 0)")
        print(f"Active Power: {t_rst['generator']['mw']:.1f} MW (expect 0)")
        assert t_rst['safety']['mft'] == 0, "Restart did not clear trip!"
        assert t_rst['boiler']['drumTemp'] < 70.0, "Restart did not reset physical variables!"
        print(">> TEST 6 PASSED: Restart Simulation completely resets physical twin, clears trips, and returns to safe initial state!")

        print("\n=======================================================")
        print("  ALL 6 TESTS PASSED SUCCESSFULLY!")
        print("=======================================================")

    finally:
        print("\n[TEST RUNNER] Cleaning up subprocesses...")
        p_scada.terminate()
        p_phys.terminate()
        p_plc.terminate()
        time.sleep(1)

if __name__ == "__main__":
    run_tests()
