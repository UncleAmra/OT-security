/**
 * ==============================================================================
 *  LAYER 2: INDUSTRIAL PLC CONTROLLER & LOGIC RUNTIME ENGINE (C++)
 * ==============================================================================
 * Operates as the central industrial controller in the Purdue model:
 *   - Hosts multi-client Modbus/TCP server on Port 5020 (or 502)
 *   - Runs cyclic scan loop: Input Scan -> PID Loops -> Safety Interlocks -> Output
 *   - Enforces 2oo3 voting trip logic, Master Fuel Trip (MFT), and ESD
 * ==============================================================================
 */

#include <iostream>
#include <iomanip>
#include <string>
#include <chrono>
#include <thread>
#include <csignal>
#include <ctime>

#include "../../common/PlantRegisters.hpp"
#include "../../common/SocketUtils.hpp"
#include "../include/PlcDataBank.hpp"
#include "../include/PlcScanEngine.hpp"
#include "../include/ModbusTcpServer.hpp"

static volatile bool g_plcRunning = true;

void signalHandler(int signum) {
    (void)signum;
    g_plcRunning = false;
}

std::string getTimestamp() {
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
    std::string bindIp = "0.0.0.0";
    int port = 5020;
    int scanCycleMs = 100; // 100 ms industrial scan cycle (10 Hz)

    if (argc >= 2) bindIp = argv[1];
    if (argc >= 3) port = std::stoi(argv[2]);
    if (argc >= 4) scanCycleMs = std::stoi(argv[3]);

    std::signal(SIGINT, signalHandler);

    if (!NetUtils::initializeNetwork()) {
        std::cerr << "[PLC-ERROR] Failed initializing network library.\n";
        return 1;
    }

    std::cout << "\033[1;32m================================================================================\n"
              << "       PURDUE LAYER 2: INDUSTRIAL PLC CONTROLLER RUNTIME DAEMON (C++)           \n"
              << "================================================================================\033[0m\n"
              << "[INIT] Modbus Server Binding: " << bindIp << ":" << port << "\n"
              << "[INIT] Target PLC Scan Cycle: " << scanCycleMs << " ms\n"
              << "[INIT] Memory: 64 Modbus Holding Registers (40001 - 40064)\n\n";

    PlcDataBank dataBank;
    PlcScanEngine scanEngine(dataBank);
    ModbusTcpServer modbusServer(dataBank, bindIp, port);

    if (!modbusServer.start()) {
        std::cerr << "[FATAL] Failed to launch Modbus TCP Server daemon.\n";
        NetUtils::shutdownNetwork();
        return 1;
    }

    double dt = static_cast<double>(scanCycleMs) / 1000.0;
    int renderCounter = 0;

    while (g_plcRunning) {
        auto startTime = std::chrono::steady_clock::now();

        // Execute one deterministic PLC scan cycle
        scanEngine.executeScan(dt);

        // Update dashboard every ~500ms (every 5 scans)
        renderCounter++;
        if (renderCounter >= 5) {
            renderCounter = 0;

            std::vector<uint16_t> reg;
            dataBank.getFullSnapshot(reg);

            std::cout << "\033[H\033[J"; // Clear screen
            std::cout << "\033[1;32m================================================================================\n"
                      << " [" << getTimestamp() << "] INDUSTRIAL PLC RUNTIME CONTROLLER (LAYER 2)\n"
                      << "================================================================================\033[0m\n";

            // Controller Status Row
            uint16_t mode = reg[PlantRegisters::REG_PLC_MODE];
            std::cout << " \033[1;36m[CONTROLLER STATUS]\033[0m Mode: " 
                      << (mode == 1 ? "\033[1;32mAUTO / CASCADE REGULATION\033[0m" : "\033[1;33mMANUAL / DIRECT OVERRIDE\033[0m")
                      << " | Heartbeat: " << reg[PlantRegisters::REG_WATCHDOG_HEARTBEAT]
                      << " | Modbus Port: " << port << "\n\n";

            // Closed-Loop Regulatory Controllers
            std::cout << " \033[1;33m[REGULATORY PID LOOPS]\033[0m\n"
                      << "  Drum Level PV: " << std::setw(3) << reg[PlantRegisters::REG_BOILER_DRUM_LEVEL] << " %  "
                      << "--> Setpoint: " << std::setw(3) << reg[PlantRegisters::REG_OPERATOR_TARGET_DRUM_LVL] << " %  "
                      << "--> BFP A Output: " << std::setw(3) << reg[PlantRegisters::REG_BFP_A_SPEED] << " %\n"
                      << "  Turbine Speed: " << std::setw(4) << reg[PlantRegisters::REG_TURBINE_SPEED_RPM] << " RPM "
                      << "--> MW Target: " << std::setw(3) << reg[PlantRegisters::REG_OPERATOR_TARGET_MW] << " MW "
                      << "--> Governor CV: " << std::setw(3) << reg[PlantRegisters::REG_GOVERNOR_CONTROL_VALVE] << " %\n"
                      << "  Furnace Draft: " << std::setw(3) << (static_cast<int>(reg[PlantRegisters::REG_FURNACE_PRESSURE]) - 500) / 10.0 << " mmH2O"
                      << "--> ID Fan Output: " << std::setw(3) << reg[PlantRegisters::REG_ID_FAN_SPEED] << " %\n\n";

            // Safety Instrumented System (SIS) Status
            std::cout << " \033[1;31m[SAFETY INSTRUMENTED SYSTEM (SIS / 2oo3)]\033[0m\n"
                      << "  Master Fuel Trip (MFT): " << (reg[PlantRegisters::REG_MASTER_FUEL_TRIP] ? "\033[1;5;31m[TRIPPED / FUEL SHUT]\033[0m" : "\033[1;32m[HEALTHY]\033[0m") << "\n"
                      << "  Turbine Emergency Trip: " << (reg[PlantRegisters::REG_TURBINE_TRIP] ? "\033[1;5;31m[TRIPPED / VALVES SHUT]\033[0m" : "\033[1;32m[HEALTHY]\033[0m") << "\n"
                      << "  Generator 52G Breaker:  " << (reg[PlantRegisters::REG_GRID_BREAKER_52G] ? "\033[1;32m[CLOSED / GRID TIED]\033[0m" : "\033[1;33m[OPEN / ISOLATED]\033[0m") << "\n";

            // First Out Trip Root Cause
            uint16_t firstOut = reg[PlantRegisters::REG_FIRST_OUT_TRIP_CODE];
            std::string firstOutText = "NONE (ALL SYSTEMS NORMAL)";
            if (firstOut == PlantRegisters::FirstOutCode::MANUAL_ESD_OPERATOR)   firstOutText = "OPERATOR MANUAL ESD TRIP";
            else if (firstOut == PlantRegisters::FirstOutCode::BOILER_OVERPRESSURE)  firstOutText = "BOILER OVERPRESSURE (P >= 190 PSI)";
            else if (firstOut == PlantRegisters::FirstOutCode::DRUM_LEVEL_LOW_LOW)   firstOutText = "DRUM WATER LEVEL LOW-LOW (L <= 18%)";
            else if (firstOut == PlantRegisters::FirstOutCode::DRUM_LEVEL_HIGH_HIGH) firstOutText = "DRUM WATER LEVEL HIGH-HIGH (L >= 82%)";
            else if (firstOut == PlantRegisters::FirstOutCode::FLAME_FAILURE)        firstOutText = "FURNACE FLAME FAILURE";
            else if (firstOut == PlantRegisters::FirstOutCode::TURBINE_OVERSPEED)    firstOutText = "TURBINE OVERSPEED (RPM >= 3960)";
            else if (firstOut == PlantRegisters::FirstOutCode::HIGH_BEARING_VIBRATION) firstOutText = "TURBINE BEARING VIBRATION HIGH-HIGH (>= 4.5 mm/s)";
            else if (firstOut == PlantRegisters::FirstOutCode::LOSS_OF_LUBE_OIL)     firstOutText = "LOSS OF BEARING LUBE OIL PRESSURE";
            else if (firstOut == PlantRegisters::FirstOutCode::CONDENSER_VACUUM_LOSS)firstOutText = "CONDENSER LOSS OF VACUUM BACKPRESSURE";

            std::cout << "  First-Out Cause Code:   \033[1;33m" << firstOut << " (" << firstOutText << ")\033[0m\n\n";

            // Alarm summary
            uint16_t alarmLow = reg[PlantRegisters::REG_ALARM_WORD_LOW];
            uint16_t alarmHigh = reg[PlantRegisters::REG_ALARM_WORD_HIGH];
            std::cout << " \033[1;35m[ACTIVE ALARMS]\033[0m Alarm Low: 0x" << std::hex << alarmLow 
                      << " | Alarm High: 0x" << alarmHigh << std::dec << "\n";

            if (alarmLow != 0 || alarmHigh != 0) {
                std::cout << "  \033[1;31mACTIVE WARNINGS:\033[0m";
                if (alarmLow & PlantRegisters::AlarmLowBits::BOILER_PRESS_HIGH)      std::cout << " [Boiler Press High]";
                if (alarmLow & PlantRegisters::AlarmLowBits::DRUM_LEVEL_LOW)         std::cout << " [Drum Level Low]";
                if (alarmLow & PlantRegisters::AlarmLowBits::DRUM_LEVEL_HIGH)        std::cout << " [Drum Level High]";
                if (alarmLow & PlantRegisters::AlarmLowBits::TURBINE_VIBRATION_WARN) std::cout << " [Turbine Vib High]";
                if (alarmLow & PlantRegisters::AlarmLowBits::TURBINE_OVERSPEED_WARN) std::cout << " [Turbine Overspeed]";
                if (alarmLow & PlantRegisters::AlarmLowBits::CONDENSER_VACUUM_LOW)   std::cout << " [Condenser Vacuum Low]";
                std::cout << "\n";
            } else {
                std::cout << "  \033[1;32mNo active plant alarms.\033[0m\n";
            }

            std::cout << "\033[1;32m================================================================================\033[0m\n";
        }

        auto elapsed = std::chrono::steady_clock::now() - startTime;
        auto sleepTime = std::chrono::milliseconds(scanCycleMs) - elapsed;
        if (sleepTime > std::chrono::milliseconds(0)) {
            std::this_thread::sleep_for(sleepTime);
        }
    }

    std::cout << "\n[PLC-SHUTDOWN] Stopping PLC Controller daemon...\n";
    modbusServer.stop();
    NetUtils::shutdownNetwork();
    std::cout << "[PLC-SHUTDOWN] Exited cleanly.\n";
    return 0;
}
