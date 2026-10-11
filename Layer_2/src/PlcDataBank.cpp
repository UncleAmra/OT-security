#include "../include/PlcDataBank.hpp"
#include <algorithm>

PlcDataBank::PlcDataBank() {
    resetToDefaults();
}

void PlcDataBank::resetToDefaults() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_registers.assign(PlantRegisters::TOTAL_REGISTERS, 0);

    // Initial safe default setpoints
    m_registers[PlantRegisters::REG_BOILER_DRUM_LEVEL]        = 50;   // 50% target
    m_registers[PlantRegisters::REG_OPERATOR_TARGET_DRUM_LVL] = 50;
    m_registers[PlantRegisters::REG_OPERATOR_TARGET_MW]       = 250;  // 250 MW default demand
    m_registers[PlantRegisters::REG_FD_FAN_SPEED]             = 40;   // 40%
    m_registers[PlantRegisters::REG_ID_FAN_SPEED]             = 42;   // 42%
    m_registers[PlantRegisters::REG_FUEL_FEED_RATE]           = 30;   // 30 kg/min
    m_registers[PlantRegisters::REG_MAIN_STEAM_STOP_VALVE]    = 1;    // MSV open
    m_registers[PlantRegisters::REG_GOVERNOR_CONTROL_VALVE]   = 20;   // CV 20%
    m_registers[PlantRegisters::REG_CIRC_WATER_FLOW]          = 70;   // 70% cooling flow
    m_registers[PlantRegisters::REG_COND_VACUUM_INHG]         = 285;  // 28.5 inHg nominal vacuum
    m_registers[PlantRegisters::REG_BFP_A_SPEED]              = 35;   // 35% feed pump A
    m_registers[PlantRegisters::REG_CONDENSATE_PUMP_STATE]    = 1;    // CEP active
    m_registers[PlantRegisters::REG_BURNER_FIRING_STATE]      = 2;    // 2 = Firing
    m_registers[PlantRegisters::REG_FLAME_DETECTOR]           = 1;    // 1 = Flame present
    m_registers[PlantRegisters::REG_PLC_MODE]                 = 1;    // 1 = Auto/Cascade mode
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

void PlcDataBank::applySnapshot(const std::vector<uint16_t> &inRegisters) {
    std::lock_guard<std::mutex> lock(m_mutex);
    size_t copyCount = std::min(m_registers.size(), inRegisters.size());
    for (size_t i = 0; i < copyCount; ++i) {
        m_registers[i] = inRegisters[i];
    }
}
