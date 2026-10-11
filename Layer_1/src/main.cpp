/**
 * ==============================================================================
 *  LAYER 1: MODULAR POWER PLANT DIGITAL TWIN ENGINE (PHYSICS TWIN)
 * ==============================================================================
 * Connects to Layer 2 PLC Controller over Modbus/TCP, reads actuator outputs,
 * steps thermodynamic & electromechanical state equations, and pushes full
 * plant telemetry registers.
 * ==============================================================================
 */

#include <iostream>
#include <iomanip>
#include <string>
#include <vector>
#include <chrono>
#include <thread>
#include <csignal>
#include <ctime>

#include "../../common/PlantRegisters.hpp"
#include "../../common/SocketUtils.hpp"
#include "../../common/ModbusProtocol.hpp"
#include "../include/DigitalTwinPlant.hpp"

static volatile bool g_running = true;

void signalHandler(int signum) {
    (void)signum;
    g_running = false;
}

std::string getCurrentTimestamp() {
    auto now = std::chrono::system_clock::now();
    std::time_t now_c = std::chrono::system_clock::to_time_t(now);
    struct tm *lt = std::localtime(&now_c);
    char buf[64];
    if (lt && std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", lt)) {
        return std::string(buf);
    }
    return "0000-00-00 00:00:00";
}

int main(int argc, char *argv[]) {
    std::string plcIp = "127.0.0.1";
    int plcPort = 5020;
    uint8_t unitId = 1;
    int tickRateMs = 500; // 500 ms simulation tick rate (2 Hz)

    if (argc >= 2) plcIp = argv[1];
    if (argc >= 3) plcPort = std::stoi(argv[2]);
    if (argc >= 4) tickRateMs = std::stoi(argv[3]);

    std::signal(SIGINT, signalHandler);

    if (!NetUtils::initializeNetwork()) {
        std::cerr << "[ERROR] Failed to initialize socket library.\n";
        return 1;
    }

    std::cout << "\033[1;36m================================================================================\n"
              << "       PURDUE LAYER 1: ADVANCED POWER PLANT DIGITAL TWIN ENGINE (C++)           \n"
              << "================================================================================\033[0m\n"
              << "[INIT] Target PLC Node: " << plcIp << ":" << plcPort << " (Unit ID: " << (int)unitId << ")\n"
              << "[INIT] Physics Tick Rate: " << tickRateMs << " ms\n"
              << "[INIT] Total Modbus Register Footprint: " << PlantRegisters::TOTAL_REGISTERS << " Holding Registers\n\n";

    DigitalTwinPlant twinPlant;
    NetUtils::TcpClient tcpClient;
    Modbus::ModbusClient modbusClient;

    std::vector<uint16_t> plcRegisters(PlantRegisters::TOTAL_REGISTERS, 0);
    std::vector<uint16_t> telemetryRegisters(PlantRegisters::TOTAL_REGISTERS, 0);

    // Default starting plant parameters
    plcRegisters[PlantRegisters::REG_FD_FAN_SPEED] = 30;
    plcRegisters[PlantRegisters::REG_ID_FAN_SPEED] = 32;
    plcRegisters[PlantRegisters::REG_FUEL_FEED_RATE] = 25;
    plcRegisters[PlantRegisters::REG_MAIN_STEAM_STOP_VALVE] = 1;
    plcRegisters[PlantRegisters::REG_GOVERNOR_CONTROL_VALVE] = 15;
    plcRegisters[PlantRegisters::REG_CIRC_WATER_FLOW] = 70;
    plcRegisters[PlantRegisters::REG_BFP_A_SPEED] = 35;
    plcRegisters[PlantRegisters::REG_CONDENSATE_PUMP_STATE] = 1;
    plcRegisters[PlantRegisters::REG_EXCITER_FIELD_CURRENT] = 1000; // 100.0 Amps

    double dt = static_cast<double>(tickRateMs) / 1000.0;

    while (g_running) {
        // Ensure connection to PLC
        if (!tcpClient.isConnected()) {
            std::cout << "\033[1;33m[CONNECTING] Connecting to Layer 2 PLC at " << plcIp << ":" << plcPort << "...\033[0m\n";
            if (!tcpClient.connectToServer(plcIp, plcPort, 3000)) {
                std::cerr << "\033[1;31m[RETRY] PLC Offline. Retrying in 2 seconds...\033[0m\n";
                std::this_thread::sleep_for(std::chrono::seconds(2));
                continue;
            }
            std::cout << "\033[1;32m[CONNECTED] Successfully connected to Layer 2 PLC Controller.\033[0m\n";
        }

        // Step 1: Read all 64 holding registers from the PLC to receive actuator & operator inputs
        if (!modbusClient.readHoldingRegisters(tcpClient, unitId, 0, PlantRegisters::TOTAL_REGISTERS, plcRegisters)) {
            std::cerr << "\033[1;31m[COMM ERROR] Failed reading registers from PLC. Resetting connection...\033[0m\n";
            tcpClient.disconnect();
            continue;
        }

        // Step 2: Step the coupled digital twin physics equations
        twinPlant.step(plcRegisters, telemetryRegisters, dt);

        // Step 3: Write calculated sensor telemetry back to the PLC
        if (!modbusClient.writeMultipleRegisters(tcpClient, unitId, 0, telemetryRegisters)) {
            std::cerr << "\033[1;31m[COMM ERROR] Failed writing telemetry registers to PLC. Resetting connection...\033[0m\n";
            tcpClient.disconnect();
            continue;
        }

        // Step 4: High-Tech ANSI Console Visualization Dashboard
        std::cout << "\033[H\033[J"; // Clear screen
        std::cout << "\033[1;36m================================================================================\n"
                  << " [" << getCurrentTimestamp() << "] POWER PLANT DIGITAL TWIN (LAYER 1) - TELEMETRY LOOP\n"
                  << "================================================================================\033[0m\n";

        // Boiler & Steam Drum Row
        std::cout << "\033[1;33m[BOILER & DRUM]\033[0m\n"
                  << "  Drum Temp:    " << std::setw(5) << twinPlant.getBoiler().getDrumTemperature() << " °C  | "
                  << "Drum Press:   " << std::setw(5) << twinPlant.getBoiler().getDrumPressure() << " PSI  | "
                  << "Drum Level:   " << std::setw(5) << twinPlant.getBoiler().getDrumLevel() << " %\n"
                  << "  Superheater:  " << std::setw(5) << twinPlant.getBoiler().getSuperheaterTemperature() << " °C  | "
                  << "Steam Flow:   " << std::setw(5) << twinPlant.getTurbine().getSteamMassFlow() << " kg/s | "
                  << "Feed Flow:    " << std::setw(5) << twinPlant.getCondenser().getFeedwaterMassFlow() << " kg/s\n"
                  << "  Safety Valve 1: " << (twinPlant.getBoiler().getSafetyValve1State() ? "\033[1;31mOPEN (RELIEF)\033[0m" : "\033[1;32mCLOSED\033[0m       ")
                  << " | Safety Valve 2: " << (twinPlant.getBoiler().getSafetyValve2State() ? "\033[1;31mOPEN (RELIEF)\033[0m\n" : "\033[1;32mCLOSED\033[0m\n");

        // Combustion Row
        std::cout << "\033[1;31m[COMBUSTION & DRAFT]\033[0m\n"
                  << "  Fuel Rate:    " << std::setw(5) << twinPlant.getCombustion().getEffectiveFuelRate() << " kg/m| "
                  << "Furnace Draft:" << std::setw(5) << twinPlant.getCombustion().getFurnacePressure() << " mmH2O| "
                  << "Flue Gas O2:  " << std::setw(5) << twinPlant.getCombustion().getFlueGasO2() << " %\n"
                  << "  Burner Status:" << (twinPlant.getCombustion().hasFlame() ? " \033[1;32mFIRING (FLAME ON)\033[0m" : " \033[1;31mEXTINGUISHED\033[0m    ")
                  << " | Efficiency:   " << std::setw(5) << twinPlant.getCombustion().getCombustionEfficiency() << " %   | "
                  << "Thermal Output:" << std::setw(5) << twinPlant.getCombustion().getThermalHeatOutput() << " kW\n";

        // Steam Turbine Row
        std::cout << "\033[1;34m[STEAM TURBINE DEH]\033[0m\n"
                  << "  Rotor Speed:  " << std::setw(5) << std::fixed << std::setprecision(0) << twinPlant.getTurbine().getSpeedRPM() << " RPM | "
                  << "Bearing 1 Vib:" << std::setw(5) << std::setprecision(2) << twinPlant.getTurbine().getVibrationBearing1() << " mm/s| "
                  << "Bearing 2 Vib:" << std::setw(5) << twinPlant.getTurbine().getVibrationBearing2() << " mm/s\n"
                  << "  Lube Oil Press:" << std::setw(4) << twinPlant.getTurbine().getLubeOilPressure() << " Bar | "
                  << "Lube Oil Temp:" << std::setw(5) << twinPlant.getTurbine().getLubeOilTemp() << " °C  | "
                  << "ETS Trip State:" << (twinPlant.getTurbine().isTripped() ? "\033[1;31mTRIPPED\033[0m\n" : "\033[1;32mHEALTHY\033[0m\n");

        // Generator & Electrical Row
        std::cout << "\033[1;32m[GENERATOR & SWITCHYARD]\033[0m\n"
                  << "  Active Power: " << std::setw(5) << std::setprecision(1) << twinPlant.getGenerator().getActivePowerMW() << " MW  | "
                  << "Frequency:    " << std::setw(5) << std::setprecision(2) << twinPlant.getGenerator().getFrequencyHz() << " Hz | "
                  << "Terminal Volt:" << std::setw(5) << std::setprecision(1) << twinPlant.getGenerator().getTerminalVoltageKV() << " kV\n"
                  << "  52G Breaker:  " << (twinPlant.getGenerator().getBreakerState() ? "\033[1;32mCLOSED (GRID ON)\033[0m" : "\033[1;33mOPEN (ISOLATED)\033[0m ")
                  << " | Power Factor: " << std::setw(5) << std::setprecision(2) << twinPlant.getGenerator().getPowerFactor() << "     | "
                  << "Sync Permit:  " << (twinPlant.getGenerator().isSyncPermitted() ? "\033[1;32mPERMITTED\033[0m\n" : "\033[1;31mLOCKED\033[0m\n");

        // Condenser Row
        std::cout << "\033[1;35m[CONDENSER & FEEDWATER]\033[0m\n"
                  << "  Vacuum Level: " << std::setw(5) << std::setprecision(1) << twinPlant.getCondenser().getVacuumInHg() << " inHg| "
                  << "Hotwell Temp: " << std::setw(5) << twinPlant.getCondenser().getHotwellTemp() << " °C  | "
                  << "Circ Water:   " << std::setw(5) << twinPlant.getCondenser().getCircWaterFlow() << " %\n"
                  << "  Deaerator Lvl:" << std::setw(5) << twinPlant.getCondenser().getDeaeratorLevel() << " %   | "
                  << "Deaerator Press:" << std::setw(4) << twinPlant.getCondenser().getDeaeratorPressure() << " Bar | "
                  << "CEP Pump:     " << (twinPlant.getCondenser().isCepRunning() ? "\033[1;32mRUNNING\033[0m\n" : "\033[1;31mSTOPPED\033[0m\n");

        std::cout << "\033[1;36m================================================================================\033[0m\n";

        // Safety Warnings & Alarms Check
        if (twinPlant.getBoiler().getDrumPressure() > 185.0) {
            std::cout << "  \033[1;31m[CRITICAL ALARM] BOILER OVERPRESSURE DETECTED: " << twinPlant.getBoiler().getDrumPressure() << " PSI!\033[0m\n";
        }
        if (twinPlant.getBoiler().getDrumLevel() < 20.0 || twinPlant.getBoiler().getDrumLevel() > 80.0) {
            std::cout << "  \033[1;31m[CRITICAL ALARM] DRUM WATER LEVEL OUT OF SAFE TOLERANCE: " << twinPlant.getBoiler().getDrumLevel() << " %!\033[0m\n";
        }
        if (twinPlant.getTurbine().getSpeedRPM() > 3750.0) {
            std::cout << "  \033[1;31m[CRITICAL ALARM] TURBINE OVERSPEED DANGER: " << twinPlant.getTurbine().getSpeedRPM() << " RPM!\033[0m\n";
        }
        if (twinPlant.getTurbine().getVibrationBearing1() > 4.0 || twinPlant.getTurbine().getVibrationBearing2() > 4.0) {
            std::cout << "  \033[1;31m[CRITICAL ALARM] HIGH BEARING DISPLACEMENT VIBRATION!\033[0m\n";
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(tickRateMs));
    }

    std::cout << "\n[SHUTDOWN] Exiting Layer 1 Physics Twin cleanly...\n";
    tcpClient.disconnect();
    NetUtils::shutdownNetwork();
    return 0;
}
