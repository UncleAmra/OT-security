#include "../include/PlcScanEngine.hpp"
#include <iostream>
#include <cmath>

PlcScanEngine::PlcScanEngine(PlcDataBank &dataBank)
    : m_dataBank(dataBank),
      m_drumLevelPid(1.8, 0.08, 0.4, 0.0, 100.0),      // Kp, Ki, Kd, min, max
      m_turbineSpeedPid(0.25, 0.04, 0.02, 0.0, 100.0),
      m_turbineLoadPid(0.35, 0.05, 0.01, 0.0, 100.0),
      m_draftPressurePid(1.2, 0.1, 0.05, 0.0, 100.0),
      m_scanCount(0) {
    reset();
}

void PlcScanEngine::reset() {
    m_drumLevelPid.reset();
    m_turbineSpeedPid.reset();
    m_turbineLoadPid.reset();
    m_draftPressurePid.reset();
    m_safety.reset();
    m_scanCount = 0;
}

void PlcScanEngine::executeScan(double dt) {
    m_scanCount++;

    // PHASE 1: INPUT SCAN
    std::vector<uint16_t> registers;
    m_dataBank.getFullSnapshot(registers);

    if (registers.size() < PlantRegisters::TOTAL_REGISTERS) return;

    uint16_t plcMode = registers[PlantRegisters::REG_PLC_MODE]; // 0=Manual, 1=Auto

    // PHASE 2: REGULATORY CONTROL LOOPS (AUTOMATIC REGULATION)
    if (plcMode == 1 && !m_safety.isMasterFuelTripped() && !m_safety.isTurbineTripped()) {
        // Loop 1: Drum Level 3-Element Control
        double levelPv = static_cast<double>(registers[PlantRegisters::REG_BOILER_DRUM_LEVEL]);
        double levelSp = static_cast<double>(registers[PlantRegisters::REG_OPERATOR_TARGET_DRUM_LVL]);
        if (levelSp < 10.0 || levelSp > 90.0) levelSp = 50.0;

        // Feedforward element: steam flow vs feedwater flow feedforward
        double steamFlow = static_cast<double>(registers[PlantRegisters::REG_STEAM_MASS_FLOW]);
        double feedFlow  = static_cast<double>(registers[PlantRegisters::REG_FEEDWATER_MASS_FLOW]);
        double flowError = (steamFlow - feedFlow) * 0.15;

        double bfpOutput = m_drumLevelPid.calculate(levelSp, levelPv, dt) + flowError;
        bfpOutput = std::clamp(bfpOutput, 0.0, 100.0);
        registers[PlantRegisters::REG_BFP_A_SPEED] = static_cast<uint16_t>(std::round(bfpOutput));

        // Loop 2: Steam Turbine DEH Governor
        bool breakerClosed = (registers[PlantRegisters::REG_GRID_BREAKER_52G] != 0);
        if (!breakerClosed) {
            // Speed Control Mode (Run up to 3600 RPM)
            double targetSpeed = 3600.0;
            double currentSpeed = static_cast<double>(registers[PlantRegisters::REG_TURBINE_SPEED_RPM]);
            double cvOut = m_turbineSpeedPid.calculate(targetSpeed, currentSpeed, dt);
            registers[PlantRegisters::REG_GOVERNOR_CONTROL_VALVE] = static_cast<uint16_t>(std::round(cvOut));
        } else {
            // Megawatt Load Control Mode (Grid tied)
            double targetMW = static_cast<double>(registers[PlantRegisters::REG_OPERATOR_TARGET_MW]);
            if (targetMW <= 0.0) targetMW = 250.0;
            double currentMW = static_cast<double>(registers[PlantRegisters::REG_GEN_ACTIVE_POWER_MW]);
            double cvOut = m_turbineLoadPid.calculate(targetMW, currentMW, dt);
            registers[PlantRegisters::REG_GOVERNOR_CONTROL_VALVE] = static_cast<uint16_t>(std::round(cvOut));
        }

        // Loop 3: Furnace Draft Control
        // Balance ID fan to maintain negative draft (-10 mm H2O)
        double draftPv = (static_cast<double>(registers[PlantRegisters::REG_FURNACE_PRESSURE]) - 500.0) / 10.0;
        double draftSp = -10.0;
        double idFanOut = m_draftPressurePid.calculate(draftSp, draftPv, dt);
        double fdFan = static_cast<double>(registers[PlantRegisters::REG_FD_FAN_SPEED]);
        idFanOut = std::clamp(fdFan * 1.05 + (draftPv - draftSp) * 0.8, 10.0, 100.0);
        registers[PlantRegisters::REG_ID_FAN_SPEED] = static_cast<uint16_t>(std::round(idFanOut));
    }

    // PHASE 3: SAFETY INTERLOCKS EVALUATION
    m_safety.evaluate(registers);

    // Update scan heartbeat
    registers[PlantRegisters::REG_WATCHDOG_HEARTBEAT] = m_scanCount;

    // PHASE 4: OUTPUT FLUSH TO DATA BANK
    m_dataBank.applySnapshot(registers);
}
