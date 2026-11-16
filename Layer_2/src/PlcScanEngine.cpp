#include "../include/PlcScanEngine.hpp"
#include <iostream>
#include <cmath>

PlcScanEngine::PlcScanEngine(PlcDataBank &dataBank)
    : m_dataBank(dataBank),
      m_drumLevelPid(1.2, 0.05, 0.1, 0.0, 100.0),       // Kp, Ki, Kd, min, max
      m_turbineSpeedPid(0.08, 0.015, 0.005, 0.0, 60.0), // CV limit 60% during speed roll
      m_turbineLoadPid(0.25, 0.03, 0.01, 0.0, 100.0),
      m_draftPressurePid(1.0, 0.05, 0.02, 10.0, 100.0),
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

    // Check for Simulation Reset command
    if (registers[PlantRegisters::REG_RESET_SIMULATION_CMD] != 0) {
        reset();
        m_dataBank.resetToDefaults();
        m_dataBank.getFullSnapshot(registers);
        registers[PlantRegisters::REG_RESET_SIMULATION_CMD] = 1; // Signal Layer 1 to reset physics
        m_dataBank.applySnapshot(registers);
        return;
    }

    uint16_t plcMode = registers[PlantRegisters::REG_PLC_MODE]; // 0=Manual, 1=Auto

    // PHASE 2: REGULATORY CONTROL & CONTROLLED STARTUP SEQUENCE (AUTO MODE)
    if (plcMode == 1 && !m_safety.isMasterFuelTripped() && !m_safety.isTurbineTripped()) {
        double drumTemp   = static_cast<double>(registers[PlantRegisters::REG_BOILER_DRUM_TEMP]);
        double drumPress  = static_cast<double>(registers[PlantRegisters::REG_BOILER_DRUM_PRESS]);
        double drumLevel  = static_cast<double>(registers[PlantRegisters::REG_BOILER_DRUM_LEVEL]);
        double currentRPM = static_cast<double>(registers[PlantRegisters::REG_TURBINE_SPEED_RPM]);
        bool breakerClosed = (registers[PlantRegisters::REG_GRID_BREAKER_52G] != 0);

        // --- SUB-LOOP A: COMBUSTION & BOILER PRESSURE REGULATION ---
        if (drumPress < 60.0 || drumTemp < 120.0) {
            // Warmup Stage: Safe startup firing rate
            registers[PlantRegisters::REG_FUEL_FEED_RATE] = 25; // 25 kg/min safe light-off
            registers[PlantRegisters::REG_FD_FAN_SPEED]   = 28;
            registers[PlantRegisters::REG_BURNER_FIRING_STATE] = 2; // Firing
            registers[PlantRegisters::REG_FLAME_DETECTOR] = 1;
            registers[PlantRegisters::REG_MAIN_STEAM_STOP_VALVE] = 0; // MSV closed until pressure builds
            registers[PlantRegisters::REG_GOVERNOR_CONTROL_VALVE] = 0; // CV closed during warmup
        } else {
            // Pressure Built Stage: modulate fuel to maintain ~150 - 155 PSI
            registers[PlantRegisters::REG_BURNER_FIRING_STATE] = 2;
            registers[PlantRegisters::REG_FLAME_DETECTOR] = 1;
            registers[PlantRegisters::REG_MAIN_STEAM_STOP_VALVE] = 1; // Open MSV
            registers[PlantRegisters::REG_FD_FAN_SPEED] = 35;

            // Fuel trim to hold pressure ~152 PSI
            double targetPress = 152.0;
            double pressError = targetPress - drumPress;
            double fuel = 28.0 + pressError * 0.4;
            registers[PlantRegisters::REG_FUEL_FEED_RATE] = static_cast<uint16_t>(std::clamp(fuel, 15.0, 45.0));
        }

        // --- SUB-LOOP B: TURBINE GOVERNOR & GENERATOR SYNCHRONIZATION ---
        if (drumPress >= 60.0) {
            if (!breakerClosed) {
                // Speed Roll Stage: accelerate smoothly to 3600 RPM
                // Max CV clamped to 28% during free roll to eliminate overshoot
                double targetSpeed = 3600.0;
                double cvOut = m_turbineSpeedPid.calculate(targetSpeed, currentRPM, dt);
                cvOut = std::clamp(cvOut, 0.0, 28.0);
                registers[PlantRegisters::REG_GOVERNOR_CONTROL_VALVE] = static_cast<uint16_t>(std::round(cvOut));

                // Auto-synchronize when near rated speed and voltage is ready
                double genVolt = static_cast<double>(registers[PlantRegisters::REG_GEN_TERMINAL_VOLT_KV]) / 10.0;
                if (currentRPM >= 3400.0 && (genVolt >= 16.0 || registers[PlantRegisters::REG_GRID_SYNC_PERMIT] != 0)) {
                    registers[PlantRegisters::REG_GRID_BREAKER_52G] = 1; // Close 52G breaker
                }
            } else {
                // Grid Tied Megawatt Regulation Stage
                double targetMW = static_cast<double>(registers[PlantRegisters::REG_OPERATOR_TARGET_MW]);
                if (targetMW <= 0.0) targetMW = 250.0;
                double currentMW = static_cast<double>(registers[PlantRegisters::REG_GEN_ACTIVE_POWER_MW]);
                double cvOut = m_turbineLoadPid.calculate(targetMW, currentMW, dt);
                registers[PlantRegisters::REG_GOVERNOR_CONTROL_VALVE] = static_cast<uint16_t>(std::round(cvOut));
            }
        }

        // --- SUB-LOOP C: DRUM LEVEL 3-ELEMENT FEEDWATER CONTROL ---
        double levelSp = static_cast<double>(registers[PlantRegisters::REG_OPERATOR_TARGET_DRUM_LVL]);
        if (levelSp < 20.0 || levelSp > 80.0) levelSp = 50.0;

        double steamFlow = static_cast<double>(registers[PlantRegisters::REG_STEAM_MASS_FLOW]);
        // Feedforward from steam flow (each pump delivers 60 kg/s at 100%)
        double feedforwardPump = (steamFlow / 60.0) * 100.0;
        double pidOutput = m_drumLevelPid.calculate(levelSp, drumLevel, dt);
        double bfpOutput = feedforwardPump * 0.6 + pidOutput * 0.4;
        if (drumLevel < 48.0) bfpOutput += (48.0 - drumLevel) * 1.5; // Proportional boost if dipping
        bfpOutput = std::clamp(bfpOutput, 0.0, 100.0);
        registers[PlantRegisters::REG_BFP_A_SPEED] = static_cast<uint16_t>(std::round(bfpOutput));

        // --- SUB-LOOP D: FURNACE DRAFT CONTROL ---
        double draftPv = (static_cast<double>(registers[PlantRegisters::REG_FURNACE_PRESSURE]) - 500.0) / 10.0;
        double draftSp = -10.0;
        double fdFan = static_cast<double>(registers[PlantRegisters::REG_FD_FAN_SPEED]);
        double idFanOut = fdFan * 1.08 + (draftPv - draftSp) * 0.6;
        idFanOut = std::clamp(idFanOut, 10.0, 100.0);
        registers[PlantRegisters::REG_ID_FAN_SPEED] = static_cast<uint16_t>(std::round(idFanOut));
    }

    // PHASE 3: SAFETY INTERLOCKS EVALUATION
    m_safety.evaluate(registers);

    // Update scan heartbeat
    registers[PlantRegisters::REG_WATCHDOG_HEARTBEAT] = m_scanCount;

    // PHASE 4: OUTPUT FLUSH TO DATA BANK
    m_dataBank.applySnapshot(registers);
}
