#include "../include/PlcDataBank.hpp"
#include <algorithm>

PlcDataBank::PlcDataBank() {
    resetToDefaults();
}

void PlcDataBank::resetToDefaults() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_registers.assign(PlantRegisters::TOTAL_REGISTERS, 0);

    // Initial safe cold startup state
    m_registers[PlantRegisters::REG_BOILER_DRUM_TEMP]         = 25;   // 25 °C ambient
    m_registers[PlantRegisters::REG_BOILER_DRUM_PRESS]        = 15;   // ~14.7 PSI atmospheric
    m_registers[PlantRegisters::REG_BOILER_DRUM_LEVEL]        = 50;   // 50% target
    m_registers[PlantRegisters::REG_OPERATOR_TARGET_DRUM_LVL] = 50;
    m_registers[PlantRegisters::REG_OPERATOR_TARGET_MW]       = 250;  // 250 MW default demand
    m_registers[PlantRegisters::REG_FD_FAN_SPEED]             = 20;   // 20% purge speed
    m_registers[PlantRegisters::REG_ID_FAN_SPEED]             = 22;   // 22% draft balance
    m_registers[PlantRegisters::REG_FUEL_FEED_RATE]           = 0;    // 0 kg/min (Cold startup: combustion off)
    m_registers[PlantRegisters::REG_MAIN_STEAM_STOP_VALVE]    = 0;    // MSV closed at cold startup
    m_registers[PlantRegisters::REG_GOVERNOR_CONTROL_VALVE]   = 0;    // CV closed at cold startup
    m_registers[PlantRegisters::REG_CIRC_WATER_FLOW]          = 70;   // 70% cooling flow ready
    m_registers[PlantRegisters::REG_COND_VACUUM_INHG]         = 285;  // 28.5 inHg nominal vacuum
    m_registers[PlantRegisters::REG_BFP_A_SPEED]              = 0;    // 0% at cold startup
    m_registers[PlantRegisters::REG_CONDENSATE_PUMP_STATE]    = 1;    // CEP active
    m_registers[PlantRegisters::REG_BURNER_FIRING_STATE]      = 0;    // 0 = Off
    m_registers[PlantRegisters::REG_FLAME_DETECTOR]           = 0;    // 0 = Dark / no flame
    m_registers[PlantRegisters::REG_TURBINE_SPEED_RPM]        = 0;    // 0 RPM
    m_registers[PlantRegisters::REG_GRID_BREAKER_52G]         = 0;    // 0 = Open / Isolated
    m_registers[PlantRegisters::REG_GEN_ACTIVE_POWER_MW]      = 0;    // 0 MW
    m_registers[PlantRegisters::REG_EXCITER_FIELD_CURRENT]    = 1000; // 100.0 Amps nominal
    m_registers[PlantRegisters::REG_PLC_MODE]                 = 1;    // 1 = Auto mode
    m_registers[PlantRegisters::REG_ANOMALY_STATUS]           = 0;    // No anomalies
    m_registers[PlantRegisters::REG_SECURITY_ALERT_LEVEL]     = 0;    // Normal
    m_registers[PlantRegisters::REG_RESET_SIMULATION_CMD]     = 0;    // Idle
}

uint16_t PlcDataBank::getHoldingRegister(uint16_t address) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (address < m_registers.size()) {
        return m_registers[address];
    }
    return 0;
}

void PlcDataBank::setHoldingRegister(uint16_t address, uint16_t value) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (address < m_registers.size()) {
        m_registers[address] = value;
    }
}

std::vector<uint16_t> PlcDataBank::getHoldingRegisters(uint16_t startAddr, uint16_t count) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (startAddr >= m_registers.size()) {
        return {};
    }
    uint16_t endAddr = std::min<uint16_t>(startAddr + count, static_cast<uint16_t>(m_registers.size()));
    return std::vector<uint16_t>(m_registers.begin() + startAddr, m_registers.begin() + endAddr);
}

void PlcDataBank::setHoldingRegisters(uint16_t startAddr, const std::vector<uint16_t> &values) {
    std::lock_guard<std::mutex> lock(m_mutex);
    for (size_t i = 0; i < values.size(); ++i) {
        size_t idx = startAddr + i;
        if (idx < m_registers.size()) {
            m_registers[idx] = values[i];
        }
    }
}

void PlcDataBank::getFullSnapshot(std::vector<uint16_t> &outRegisters) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    outRegisters = m_registers;
}

void PlcDataBank::applyChanges(const std::vector<uint16_t> &before, const std::vector<uint16_t> &after) {
    std::lock_guard<std::mutex> lock(m_mutex);
    size_t n = std::min({m_registers.size(), before.size(), after.size()});
    for (size_t i = 0; i < n; ++i) {
        if (after[i] != before[i]) {
            m_registers[i] = after[i];
        }
    }
}

void PlcDataBank::applySnapshot(const std::vector<uint16_t> &inRegisters) {
    std::lock_guard<std::mutex> lock(m_mutex);
    size_t copyCount = std::min(m_registers.size(), inRegisters.size());
    for (size_t i = 0; i < copyCount; ++i) {
        m_registers[i] = inRegisters[i];
    }
}
