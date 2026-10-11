#include "../include/DigitalTwinPlant.hpp"
#include <iostream>
#include <cmath>

DigitalTwinPlant::DigitalTwinPlant() : m_heartbeat(0) {
    resetAll();
}

void DigitalTwinPlant::resetAll() {
    m_boiler.reset();
    m_combustion.reset();
    m_turbine.reset();
    m_generator.reset();
    m_condenser.reset();
    m_heartbeat = 0;
}

void DigitalTwinPlant::step(const std::vector<uint16_t> &plcHoldingRegisters, 
                            std::vector<uint16_t> &outTelemetryRegisters, 
                            double dt) {
    if (plcHoldingRegisters.size() < PlantRegisters::TOTAL_REGISTERS) {
        return;
    }

    outTelemetryRegisters = plcHoldingRegisters; // Start from current values

    m_heartbeat++;

    // 1. EXTRACT ACTUATOR COMMANDS & CONTROLS FROM PLC REGISTERS
    // Boiler Controls
    bool   safetyValveCmd1 = (plcHoldingRegisters[PlantRegisters::REG_SAFETY_VALVE_1] != 0);
    bool   safetyValveCmd2 = (plcHoldingRegisters[PlantRegisters::REG_SAFETY_VALVE_2] != 0);
    double sprayValvePos   = static_cast<double>(plcHoldingRegisters[PlantRegisters::REG_ATTEMPERATOR_SPRAY]);
    int    blowdownCmd     = static_cast<int>(plcHoldingRegisters[PlantRegisters::REG_BLOWDOWN_VALVE]);

    // Combustion Controls
    double fuelFeedRate    = static_cast<double>(plcHoldingRegisters[PlantRegisters::REG_FUEL_FEED_RATE]);
    double fdFanSpeed      = static_cast<double>(plcHoldingRegisters[PlantRegisters::REG_FD_FAN_SPEED]);
    double idFanSpeed      = static_cast<double>(plcHoldingRegisters[PlantRegisters::REG_ID_FAN_SPEED]);
    bool   mftActive       = (plcHoldingRegisters[PlantRegisters::REG_MASTER_FUEL_TRIP] != 0);

    // Turbine Controls
    bool   msvOpen         = (plcHoldingRegisters[PlantRegisters::REG_MAIN_STEAM_STOP_VALVE] != 0);
    double cvPosition      = static_cast<double>(plcHoldingRegisters[PlantRegisters::REG_GOVERNOR_CONTROL_VALVE]);
    bool   turbineTripCmd  = (plcHoldingRegisters[PlantRegisters::REG_TURBINE_TRIP] != 0);

    // Generator Controls
    bool   breakerCmd      = (plcHoldingRegisters[PlantRegisters::REG_GRID_BREAKER_52G] != 0);
    double exciterCurrent  = static_cast<double>(plcHoldingRegisters[PlantRegisters::REG_EXCITER_FIELD_CURRENT]) / 10.0;
    if (exciterCurrent <= 0.1) exciterCurrent = 100.0; // Default 100A excitation nominal

    // Condenser & Feedwater Controls
    double circWaterFlow   = static_cast<double>(plcHoldingRegisters[PlantRegisters::REG_CIRC_WATER_FLOW]);
    double bfpASpeed       = static_cast<double>(plcHoldingRegisters[PlantRegisters::REG_BFP_A_SPEED]);
    double bfpBSpeed       = static_cast<double>(plcHoldingRegisters[PlantRegisters::REG_BFP_B_SPEED]);
    bool   cepRunning      = (plcHoldingRegisters[PlantRegisters::REG_CONDENSATE_PUMP_STATE] != 0);

    // Manual override for safety relief valves if requested
    if (safetyValveCmd1) m_boiler.setSafetyValve1(true);
    if (safetyValveCmd2) m_boiler.setSafetyValve2(true);

    // 2. COUPLED THERMODYNAMIC / ELECTROMECHANICAL CYCLE STEPPING
    // Step A: Combustion
    m_combustion.step(fuelFeedRate, fdFanSpeed, idFanSpeed, mftActive, dt);

    // Step B: Condenser & Feedwater (produces feedwater flow to drum)
    double previousSteamFlow = m_turbine.getSteamMassFlow();
    m_condenser.step(previousSteamFlow, circWaterFlow, bfpASpeed, bfpBSpeed, cepRunning, dt);

    // Step C: Boiler (receives combustion heat and feedwater flow)
    m_boiler.step(m_combustion.getThermalHeatOutput(), 
                  m_condenser.getFeedwaterMassFlow(), 
                  previousSteamFlow, 
                  sprayValvePos, 
                  blowdownCmd, 
                  dt);

    // Step D: Turbine (driven by boiler pressure, exhausts to condenser)
    m_turbine.step(m_boiler.getDrumPressure(), 
                   m_condenser.getVacuumInHg(), 
                   msvOpen, 
                   cvPosition, 
                   m_generator.getBreakerState(), 
                   m_generator.getActivePowerMW(), 
                   turbineTripCmd, 
                   dt);

    // Step E: Generator (driven by turbine RPM and shaft mechanical MW)
    m_generator.step(m_turbine.getSpeedRPM(), 
                     m_turbine.getMechanicalPowerMW(), 
                     breakerCmd, 
                     exciterCurrent, 
                     dt);

    // 3. ENCODE PHYSICAL TELEMETRY INTO OUTGOING MODBUS REGISTERS
    // Boiler
    outTelemetryRegisters[PlantRegisters::REG_BOILER_DRUM_TEMP]    = static_cast<uint16_t>(m_boiler.getDrumTemperature());
    outTelemetryRegisters[PlantRegisters::REG_BOILER_DRUM_PRESS]   = static_cast<uint16_t>(m_boiler.getDrumPressure());
    outTelemetryRegisters[PlantRegisters::REG_BOILER_DRUM_LEVEL]   = static_cast<uint16_t>(m_boiler.getDrumLevel());
    outTelemetryRegisters[PlantRegisters::REG_SUPERHEATER_TEMP]    = static_cast<uint16_t>(m_boiler.getSuperheaterTemperature());
    outTelemetryRegisters[PlantRegisters::REG_STEAM_MASS_FLOW]     = static_cast<uint16_t>(m_turbine.getSteamMassFlow());
    outTelemetryRegisters[PlantRegisters::REG_FEEDWATER_MASS_FLOW] = static_cast<uint16_t>(m_condenser.getFeedwaterMassFlow());
    outTelemetryRegisters[PlantRegisters::REG_SAFETY_VALVE_1]      = m_boiler.getSafetyValve1State() ? 1 : 0;
    outTelemetryRegisters[PlantRegisters::REG_SAFETY_VALVE_2]      = m_boiler.getSafetyValve2State() ? 1 : 0;

    // Combustion
    outTelemetryRegisters[PlantRegisters::REG_FURNACE_PRESSURE]      = static_cast<uint16_t>(std::round(m_combustion.getFurnacePressure() * 10.0 + 500.0)); // Offset by 500
    outTelemetryRegisters[PlantRegisters::REG_FLUE_GAS_O2]           = static_cast<uint16_t>(std::round(m_combustion.getFlueGasO2() * 100.0));
    outTelemetryRegisters[PlantRegisters::REG_AIR_FUEL_RATIO]        = static_cast<uint16_t>(std::round(m_combustion.getAirFuelRatio() * 10.0));
    outTelemetryRegisters[PlantRegisters::REG_BURNER_FIRING_STATE]   = static_cast<uint16_t>(m_combustion.getBurnerState());
    outTelemetryRegisters[PlantRegisters::REG_FLAME_DETECTOR]        = m_combustion.hasFlame() ? 1 : 0;
    outTelemetryRegisters[PlantRegisters::REG_COMBUSTION_EFFICIENCY] = static_cast<uint16_t>(std::round(m_combustion.getCombustionEfficiency() * 10.0));

    // Turbine
    outTelemetryRegisters[PlantRegisters::REG_TURBINE_SPEED_RPM]       = static_cast<uint16_t>(m_turbine.getSpeedRPM());
    outTelemetryRegisters[PlantRegisters::REG_TURBINE_VIBRATION_BRG1]  = static_cast<uint16_t>(std::round(m_turbine.getVibrationBearing1() * 10.0));
    outTelemetryRegisters[PlantRegisters::REG_TURBINE_VIBRATION_BRG2]  = static_cast<uint16_t>(std::round(m_turbine.getVibrationBearing2() * 10.0));
    outTelemetryRegisters[PlantRegisters::REG_TURBINE_ROTOR_EXPANSION] = static_cast<uint16_t>(std::round(m_turbine.getRotorExpansion() * 100.0));
    outTelemetryRegisters[PlantRegisters::REG_LUBE_OIL_PRESSURE]       = static_cast<uint16_t>(std::round(m_turbine.getLubeOilPressure() * 10.0));
    outTelemetryRegisters[PlantRegisters::REG_LUBE_OIL_TEMP]           = static_cast<uint16_t>(std::round(m_turbine.getLubeOilTemp()));
    outTelemetryRegisters[PlantRegisters::REG_TURBINE_CHEST_TEMP]      = static_cast<uint16_t>(std::round(m_turbine.getChestTemperature()));
    outTelemetryRegisters[PlantRegisters::REG_TURBINE_TRIP_SOLENOID]   = m_turbine.isTripped() ? 1 : 0;

    // Generator
    outTelemetryRegisters[PlantRegisters::REG_GEN_ACTIVE_POWER_MW]      = static_cast<uint16_t>(std::round(m_generator.getActivePowerMW()));
    outTelemetryRegisters[PlantRegisters::REG_GEN_REACTIVE_POWER_MVAR]  = static_cast<uint16_t>(std::round(m_generator.getReactivePowerMVAR() * 10.0 + 1000.0)); // Offset 1000
    outTelemetryRegisters[PlantRegisters::REG_GEN_FREQUENCY_HZ]         = static_cast<uint16_t>(std::round(m_generator.getFrequencyHz() * 100.0));
    outTelemetryRegisters[PlantRegisters::REG_GEN_TERMINAL_VOLT_KV]     = static_cast<uint16_t>(std::round(m_generator.getTerminalVoltageKV() * 10.0));
    outTelemetryRegisters[PlantRegisters::REG_GRID_BREAKER_52G]         = m_generator.getBreakerState() ? 1 : 0;
    outTelemetryRegisters[PlantRegisters::REG_SYNCHROSCOPE_PHASE_DELTA] = static_cast<uint16_t>(std::round(m_generator.getSynchroscopePhaseDelta() * 10.0));
    outTelemetryRegisters[PlantRegisters::REG_EXCITER_FIELD_CURRENT]    = static_cast<uint16_t>(std::round(m_generator.getExciterFieldCurrent() * 10.0));
    outTelemetryRegisters[PlantRegisters::REG_POWER_FACTOR]             = static_cast<uint16_t>(std::round(m_generator.getPowerFactor() * 100.0));
    outTelemetryRegisters[PlantRegisters::REG_STATOR_TEMP]              = static_cast<uint16_t>(std::round(m_generator.getStatorTemperature()));
    outTelemetryRegisters[PlantRegisters::REG_GRID_SYNC_PERMIT]         = m_generator.isSyncPermitted() ? 1 : 0;

    // Condenser & Feedwater
    outTelemetryRegisters[PlantRegisters::REG_COND_HOTWELL_TEMP]     = static_cast<uint16_t>(std::round(m_condenser.getHotwellTemp()));
    outTelemetryRegisters[PlantRegisters::REG_COND_VACUUM_INHG]      = static_cast<uint16_t>(std::round(m_condenser.getVacuumInHg() * 10.0));
    outTelemetryRegisters[PlantRegisters::REG_CIRC_WATER_FLOW]       = static_cast<uint16_t>(std::round(m_condenser.getCircWaterFlow()));
    outTelemetryRegisters[PlantRegisters::REG_CIRC_WATER_INLET_TEMP] = static_cast<uint16_t>(std::round(m_condenser.getCircWaterInletTemp()));
    outTelemetryRegisters[PlantRegisters::REG_DEAERATOR_PRESSURE]    = static_cast<uint16_t>(std::round(m_condenser.getDeaeratorPressure() * 10.0));
    outTelemetryRegisters[PlantRegisters::REG_DEAERATOR_LEVEL]       = static_cast<uint16_t>(std::round(m_condenser.getDeaeratorLevel()));

    // System Heartbeat
    outTelemetryRegisters[PlantRegisters::REG_WATCHDOG_HEARTBEAT]    = m_heartbeat;
}
