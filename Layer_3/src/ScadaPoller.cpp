#include "../include/ScadaPoller.hpp"
#include <iostream>
#include <chrono>

ScadaPoller::ScadaPoller(const std::string &plcIp, int plcPort, uint8_t unitId)
    : m_plcIp(plcIp),
      m_plcPort(plcPort),
      m_unitId(unitId),
      m_running(false),
      m_connected(false),
      m_registers(PlantRegisters::TOTAL_REGISTERS, 0) {}

ScadaPoller::~ScadaPoller() {
    stop();

    
}

bool ScadaPoller::start() {
    if (m_running) return true;
    m_running = true;
    m_pollThread = std::thread(&ScadaPoller::pollLoop, this);
    return true;
}

void ScadaPoller::stop() {
    if (!m_running) return;
    m_running = false;
    if (m_pollThread.joinable()) {
        m_pollThread.join();
    }
    m_tcpClient.disconnect();
    m_connected = false;
}

std::vector<uint16_t> ScadaPoller::getCachedRegisters() const {
    std::lock_guard<std::mutex> lock(m_cacheMutex);
    return m_registers;
}

void ScadaPoller::pollLoop() {
    std::vector<uint16_t> buffer(PlantRegisters::TOTAL_REGISTERS, 0);

    while (m_running) {
        int sleepMs = 250; // 4 Hz polling rate
        {
            // Hold the I/O lock for the whole request/response so an operator
            // command from the HTTP thread can't interleave with this read.
            std::lock_guard<std::mutex> io(m_ioMutex);

            if (!m_tcpClient.isConnected()) {
                m_connected = false;
                if (m_tcpClient.connectToServer(m_plcIp, m_plcPort, 2000)) {
                    m_connected = true;
                    std::cout << "\033[1;32m[SCADA-POLLER] Connected to Layer 2 PLC at "
                              << m_plcIp << ":" << m_plcPort << "\033[0m\n";
                } else {
                    sleepMs = 1000;
                }
            }

            if (m_tcpClient.isConnected()) {
                if (m_modbus.readHoldingRegisters(m_tcpClient, m_unitId, 0, PlantRegisters::TOTAL_REGISTERS, buffer)) {
                    std::lock_guard<std::mutex> lock(m_cacheMutex);
                    m_registers = buffer;
                } else {
                    m_connected = false;
                    m_tcpClient.disconnect();
                    sleepMs = 500;
                }
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(sleepMs));
    }
}

bool ScadaPoller::sendWriteRegister(uint16_t address, uint16_t value) {
    std::lock_guard<std::mutex> io(m_ioMutex);
    if (!m_tcpClient.isConnected()) return false;
    if (!m_modbus.writeSingleRegister(m_tcpClient, m_unitId, address, value)) {
        m_connected = false;
        m_tcpClient.disconnect();
        return false;
    }
    return true;
}

bool ScadaPoller::triggerEmergencyShutdown() {
    return sendWriteRegister(PlantRegisters::REG_EMERGENCY_SHUTDOWN_CMD, 1);
}

bool ScadaPoller::restartSimulation() {
    return sendWriteRegister(PlantRegisters::REG_RESET_SIMULATION_CMD, 1);
}

bool ScadaPoller::setTargetMegawatts(uint16_t targetMW) {
    return sendWriteRegister(PlantRegisters::REG_OPERATOR_TARGET_MW, targetMW);
}

bool ScadaPoller::setTargetDrumLevel(uint16_t targetLevel) {
    return sendWriteRegister(PlantRegisters::REG_OPERATOR_TARGET_DRUM_LVL, targetLevel);
}

bool ScadaPoller::setBreakerState(bool closed) {
    return sendWriteRegister(PlantRegisters::REG_GRID_BREAKER_52G, closed ? 1 : 0);
}

bool ScadaPoller::toggleSafetyValve(int valveNum, bool open) {
    uint16_t addr = (valveNum == 1) ? PlantRegisters::REG_SAFETY_VALVE_1 : PlantRegisters::REG_SAFETY_VALVE_2;
    return sendWriteRegister(addr, open ? 1 : 0);
}

bool ScadaPoller::setFuelRate(uint16_t fuelRate) {
    return sendWriteRegister(PlantRegisters::REG_FUEL_FEED_RATE, fuelRate);
}

bool ScadaPoller::setPlcMode(uint16_t mode) {
    return sendWriteRegister(PlantRegisters::REG_PLC_MODE, mode);
}
