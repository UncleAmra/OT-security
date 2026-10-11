#include "../include/PlcSafetyInterlocks.hpp"
#include <iostream>

PlcSafetyInterlocks::PlcSafetyInterlocks() {
    reset();
}

void PlcSafetyInterlocks::reset() {
    m_mftActive = false;
    m_turbineTripActive = false;
    m_generatorTripActive = false;
    m_firstOutCode = PlantRegisters::FirstOutCode::NONE;
    m_alarmLow = 0;
    m_alarmHigh = 0;
    m_startupGraceCounter = 0;
}

void PlcSafetyInterlocks::recordFirstOut(uint16_t tripCode) {
    if (m_firstOutCode == PlantRegisters::FirstOutCode::NONE) {
        m_firstOutCode = tripCode;
    }
}

void PlcSafetyInterlocks::triggerManualEsd() {
    recordFirstOut(PlantRegisters::FirstOutCode::MANUAL_ESD_OPERATOR);
    m_mftActive = true;
    m_turbineTripActive = true;
    m_generatorTripActive = true;
    m_alarmHigh |= PlantRegisters::AlarmHighBits::ESD_TRIGGERED;
}

void PlcSafetyInterlocks::resetTrips() {
    reset();
}

void PlcSafetyInterlocks::evaluate(std::vector<uint16_t> &reg) {
    if (reg.size() < PlantRegisters::TOTAL_REGISTERS) return;

    m_alarmLow = 0;
    m_alarmHigh = 0;

    // Check Operator Emergency Shutdown command
    if (reg[PlantRegisters::REG_EMERGENCY_SHUTDOWN_CMD] != 0) {
        triggerManualEsd();
    }

    // 1. BOILER INTERLOCKS
    uint16_t drumPress = reg[PlantRegisters::REG_BOILER_DRUM_PRESS];
    uint16_t drumLevel = reg[PlantRegisters::REG_BOILER_DRUM_LEVEL];
    uint16_t superheatTemp = reg[PlantRegisters::REG_SUPERHEATER_TEMP];

    // High Pressure Warning & Hi-Hi Trip
    if (drumPress > 175) {
        m_alarmLow |= PlantRegisters::AlarmLowBits::BOILER_PRESS_HIGH;
    }
    if (drumPress >= 190) {
        m_alarmLow |= PlantRegisters::AlarmLowBits::BOILER_PRESS_HI_HI;
        recordFirstOut(PlantRegisters::FirstOutCode::BOILER_OVERPRESSURE);
        m_mftActive = true;
        // Pop safety relief valves automatically
        reg[PlantRegisters::REG_SAFETY_VALVE_1] = 1;
        reg[PlantRegisters::REG_SAFETY_VALVE_2] = 1;
    }

    // Drum Level Low-Low Trip (Dry drum causes catastrophic tube rupture)
    if (drumLevel < 35) {
        m_alarmLow |= PlantRegisters::AlarmLowBits::DRUM_LEVEL_LOW;
    }
    if (drumLevel <= 18) {
        m_alarmLow |= PlantRegisters::AlarmLowBits::DRUM_LEVEL_LO_LO;
        recordFirstOut(PlantRegisters::FirstOutCode::DRUM_LEVEL_LOW_LOW);
        m_mftActive = true;
    }

    // Drum Level High-High Trip (Water induction into turbine blades)
    if (drumLevel > 65) {
        m_alarmLow |= PlantRegisters::AlarmLowBits::DRUM_LEVEL_HIGH;
    }
    if (drumLevel >= 82) {
        m_alarmLow |= PlantRegisters::AlarmLowBits::DRUM_LEVEL_HI_HI;
        recordFirstOut(PlantRegisters::FirstOutCode::DRUM_LEVEL_HIGH_HIGH);
        m_turbineTripActive = true; // Trip turbine immediately to protect blades
    }

    // Superheater Live Steam High Temp Warning
    if (superheatTemp > 560) {
        m_alarmLow |= PlantRegisters::AlarmLowBits::SUPERHEATER_TEMP_HIGH;
    }

    // 2. COMBUSTION & DRAFT INTERLOCKS
    m_startupGraceCounter++;
    int flameDetected = reg[PlantRegisters::REG_FLAME_DETECTOR];
    uint16_t fuelRate = reg[PlantRegisters::REG_FUEL_FEED_RATE];
    uint16_t burnerState = reg[PlantRegisters::REG_BURNER_FIRING_STATE];

    // Only trip on flame failure if burner was in active firing state (state 2)
    // and after startup grace period (50 scans / 5 seconds)
    if (m_startupGraceCounter > 50 && burnerState == 2 && fuelRate > 5 && flameDetected == 0) {
        m_alarmLow |= PlantRegisters::AlarmLowBits::FLAME_LOST;
        recordFirstOut(PlantRegisters::FirstOutCode::FLAME_FAILURE);
        m_mftActive = true;
    }

    // 3. TURBINE INTERLOCKS
    uint16_t turbineRPM = reg[PlantRegisters::REG_TURBINE_SPEED_RPM];
    uint16_t brg1Vib = reg[PlantRegisters::REG_TURBINE_VIBRATION_BRG1]; // scaled * 10
    uint16_t brg2Vib = reg[PlantRegisters::REG_TURBINE_VIBRATION_BRG2];
    uint16_t lubeOilPress = reg[PlantRegisters::REG_LUBE_OIL_PRESSURE]; // scaled * 10

    // Overspeed Protection
    if (turbineRPM >= 3708) { // 103%
        m_alarmLow |= PlantRegisters::AlarmLowBits::TURBINE_OVERSPEED_WARN;
    }
    if (turbineRPM >= 3960) { // 110% Emergency Trip
        m_alarmLow |= PlantRegisters::AlarmLowBits::TURBINE_OVERSPEED_TRIP;
        recordFirstOut(PlantRegisters::FirstOutCode::TURBINE_OVERSPEED);
        m_turbineTripActive = true;
    }

    // High Bearing Displacement Vibration (Trip @ 4.5 mm/s = 45 scaled)
    if (brg1Vib > 30 || brg2Vib > 30) {
        m_alarmLow |= PlantRegisters::AlarmLowBits::TURBINE_VIBRATION_WARN;
    }
    if (brg1Vib >= 45 || brg2Vib >= 45) {
        m_alarmLow |= PlantRegisters::AlarmLowBits::TURBINE_VIBRATION_TRIP;
        recordFirstOut(PlantRegisters::FirstOutCode::HIGH_BEARING_VIBRATION);
        m_turbineTripActive = true;
    }

    // Lube Oil Pressure Failure (Trip below 1.5 Bar = 15 scaled)
    if (turbineRPM > 200 && lubeOilPress < 15) {
        m_alarmLow |= PlantRegisters::AlarmLowBits::LUBE_OIL_PRESS_LOW;
        recordFirstOut(PlantRegisters::FirstOutCode::LOSS_OF_LUBE_OIL);
        m_turbineTripActive = true;
    }

    // 4. CONDENSER INTERLOCKS
    uint16_t vacuum = reg[PlantRegisters::REG_COND_VACUUM_INHG]; // scaled * 10
    if (vacuum < 180) { // Less than 18.0 inHg vacuum
        m_alarmLow |= PlantRegisters::AlarmLowBits::CONDENSER_VACUUM_LOW;
    }
    if (turbineRPM > 1000 && vacuum < 80) { // Loss of vacuum backpressure trip
        recordFirstOut(PlantRegisters::FirstOutCode::CONDENSER_VACUUM_LOSS);
        m_turbineTripActive = true;
    }

    // 5. CASCADE INTERLOCK PROPAGATION
    // If Turbine trips, generator grid breaker 52G must immediately trip open
    if (m_turbineTripActive) {
        m_generatorTripActive = true;
        reg[PlantRegisters::REG_MAIN_STEAM_STOP_VALVE] = 0;   // Shut MSV
        reg[PlantRegisters::REG_GOVERNOR_CONTROL_VALVE] = 0;  // Shut CV
    }

    // If Master Fuel Trip is active, shut fuel valve
    if (m_mftActive) {
        reg[PlantRegisters::REG_FUEL_FEED_RATE] = 0;
        m_alarmHigh |= PlantRegisters::AlarmHighBits::MASTER_FUEL_TRIP_ACTIVE;
    }

    if (m_turbineTripActive) {
        m_alarmHigh |= PlantRegisters::AlarmHighBits::TURBINE_TRIP_ACTIVE;
    }

    if (m_generatorTripActive) {
        reg[PlantRegisters::REG_GRID_BREAKER_52G] = 0; // Trip breaker
        m_alarmHigh |= PlantRegisters::AlarmHighBits::GENERATOR_TRIP_ACTIVE;
    }

    // Update registers with computed safety states
    reg[PlantRegisters::REG_MASTER_FUEL_TRIP]    = m_mftActive ? 1 : 0;
    reg[PlantRegisters::REG_TURBINE_TRIP]        = m_turbineTripActive ? 1 : 0;
    reg[PlantRegisters::REG_GENERATOR_TRIP]      = m_generatorTripActive ? 1 : 0;
    reg[PlantRegisters::REG_FIRST_OUT_TRIP_CODE] = m_firstOutCode;
    reg[PlantRegisters::REG_ALARM_WORD_LOW]      = m_alarmLow;
    reg[PlantRegisters::REG_ALARM_WORD_HIGH]     = m_alarmHigh;
}
