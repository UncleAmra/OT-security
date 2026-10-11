#ifndef PLANT_REGISTERS_HPP
#define PLANT_REGISTERS_HPP

#include <cstdint>
#include <string>

/**
 * ==============================================================================
 *  OT POWER PLANT DIGITAL TWIN & PLC ARCHITECTURE - MASTER MODBUS REGISTER MAP
 * ==============================================================================
 * Standardized holding register allocation (Holding Registers 40001 - 40064,
 * Modbus zero-based addresses 0000 - 0063).
 *
 * Applicable across:
 *   - Layer 1: Physics Engine (Digital Twin)
 *   - Layer 2: Industrial PLC Controller Engine
 *   - Layer 3: SCADA / HMI Operator Workstation
 *   - Layer 4: AI Security Sniffer & Threat Detection Node
 * ==============================================================================
 */

namespace PlantRegisters {

    // Total holding registers allocated
    constexpr uint16_t TOTAL_REGISTERS = 64;

    // -------------------------------------------------------------------------
    // 1. BOILER & STEAM DRUM SUBSYSTEM (Registers 0 - 9)
    // -------------------------------------------------------------------------
    constexpr uint16_t REG_BOILER_DRUM_TEMP         = 0;   // Telemetry: Drum Saturation Temp (°C)
    constexpr uint16_t REG_BOILER_DRUM_PRESS        = 1;   // Telemetry: Drum Internal Pressure (PSI)
    constexpr uint16_t REG_BOILER_DRUM_LEVEL        = 2;   // Telemetry: Drum Liquid Water Level (%) [Normal: 50%]
    constexpr uint16_t REG_SUPERHEATER_TEMP         = 3;   // Telemetry: Superheated Live Steam Temp (°C) [Nominal: 540°C]
    constexpr uint16_t REG_STEAM_MASS_FLOW          = 4;   // Telemetry: Main Steam Mass Flow Rate (kg/s)
    constexpr uint16_t REG_FEEDWATER_MASS_FLOW      = 5;   // Telemetry: Feedwater Inflow Rate (kg/s)
    constexpr uint16_t REG_SAFETY_VALVE_1           = 6;   // Control/Status: Safety Relief Valve 1 (0=Closed, 1=Open)
    constexpr uint16_t REG_SAFETY_VALVE_2           = 7;   // Control/Status: Safety Relief Valve 2 (0=Closed, 1=Open)
    constexpr uint16_t REG_ATTEMPERATOR_SPRAY       = 8;   // Control/Status: Desuperheater Spray Valve (0 - 100%)
    constexpr uint16_t REG_BLOWDOWN_VALVE           = 9;   // Control/Status: Bottom Blowdown Drain Valve (0=Closed, 1=Open)

    // -------------------------------------------------------------------------
    // 2. COMBUSTION & DRAFT AIR SUBSYSTEM (Registers 10 - 19)
    // -------------------------------------------------------------------------
    constexpr uint16_t REG_FUEL_FEED_RATE           = 10;  // Control: Fuel Supply Flow Rate (kg/min) [0 - 100]
    constexpr uint16_t REG_FD_FAN_SPEED             = 11;  // Control: Forced Draft Fan Speed (%)
    constexpr uint16_t REG_ID_FAN_SPEED             = 12;  // Control: Induced Draft Fan Speed (%)
    constexpr uint16_t REG_FURNACE_PRESSURE         = 13;  // Telemetry: Furnace Draft (mm H2O * 10, signed offset 500)
    constexpr uint16_t REG_FLUE_GAS_O2              = 14;  // Telemetry: Excess Oxygen in Flue Gas (% * 100)
    constexpr uint16_t REG_AIR_FUEL_RATIO           = 15;  // Telemetry: Stoichiometric Ratio (% * 10)
    constexpr uint16_t REG_BURNER_FIRING_STATE      = 16;  // Status: Burner Status (0=Off, 1=Igniting, 2=Firing)
    constexpr uint16_t REG_FLAME_DETECTOR           = 17;  // Telemetry: Optical Flame Sensor (0=Dark, 1=Flame Present)
    constexpr uint16_t REG_SOOTBLOWER_STATE         = 18;  // Control: Sootblower Cycle (0=Idle, 1=Blowing)
    constexpr uint16_t REG_COMBUSTION_EFFICIENCY    = 19;  // Telemetry: Combustion Thermal Efficiency (% * 10)

    // -------------------------------------------------------------------------
    // 3. STEAM TURBINE & DEH GOVERNOR SUBSYSTEM (Registers 20 - 29)
    // -------------------------------------------------------------------------
    constexpr uint16_t REG_TURBINE_SPEED_RPM        = 20;  // Telemetry: Shaft Rotational Speed (RPM) [Nominal: 3600]
    constexpr uint16_t REG_MAIN_STEAM_STOP_VALVE    = 21;  // Control/Status: Main Stop Valve MSV (0=Closed, 1=Open)
    constexpr uint16_t REG_GOVERNOR_CONTROL_VALVE   = 22;  // Control/Status: Control Valve CV Throttle (0 - 100%)
    constexpr uint16_t REG_TURBINE_VIBRATION_BRG1   = 23;  // Telemetry: Bearing 1 Vibration (mm/s * 10)
    constexpr uint16_t REG_TURBINE_VIBRATION_BRG2   = 24;  // Telemetry: Bearing 2 Vibration (mm/s * 10)
    constexpr uint16_t REG_TURBINE_ROTOR_EXPANSION  = 25;  // Telemetry: Rotor Differential Expansion (mm * 100)
    constexpr uint16_t REG_LUBE_OIL_PRESSURE        = 26;  // Telemetry: Turbine Lube Oil Supply (Bar * 10) [Nominal: 2.8 Bar]
    constexpr uint16_t REG_LUBE_OIL_TEMP            = 27;  // Telemetry: Turbine Lube Oil Temperature (°C) [Nominal: 45°C]
    constexpr uint16_t REG_TURBINE_TRIP_SOLENOID    = 28;  // Status: Emergency Trip Solenoid (0=Reset, 1=Tripped)
    constexpr uint16_t REG_TURBINE_CHEST_TEMP       = 29;  // Telemetry: HP Turbine Steam Chest Metal Temp (°C)

    // -------------------------------------------------------------------------
    // 4. SYNCHRONOUS GENERATOR & ELECTRICAL SWITCHYARD (Registers 30 - 39)
    // -------------------------------------------------------------------------
    constexpr uint16_t REG_GEN_ACTIVE_POWER_MW      = 30;  // Telemetry: Generator Real Power Output (MW) [0 - 500]
    constexpr uint16_t REG_GEN_REACTIVE_POWER_MVAR  = 31;  // Telemetry: Generator Reactive Power (MVAR * 10)
    constexpr uint16_t REG_GEN_FREQUENCY_HZ         = 32;  // Telemetry: AC Electrical Frequency (Hz * 100) [60.00 Hz]
    constexpr uint16_t REG_GEN_TERMINAL_VOLT_KV     = 33;  // Telemetry: Terminal Phase Voltage (kV * 10) [22.0 kV]
    constexpr uint16_t REG_GRID_BREAKER_52G         = 34;  // Control/Status: 52G Main Generator Breaker (0=Open, 1=Closed)
    constexpr uint16_t REG_SYNCHROSCOPE_PHASE_DELTA = 35;  // Telemetry: Phase Angle Difference to Grid (Deg * 10)
    constexpr uint16_t REG_EXCITER_FIELD_CURRENT    = 36;  // Telemetry/Control: Rotor Field Excitation (Amps * 10)
    constexpr uint16_t REG_POWER_FACTOR             = 37;  // Telemetry: Electrical Power Factor (cos φ * 100) [e.g. 92 = 0.92]
    constexpr uint16_t REG_STATOR_TEMP              = 38;  // Telemetry: Generator Stator Winding Temp (°C)
    constexpr uint16_t REG_GRID_SYNC_PERMIT         = 39;  // Status: Synchroscope Permissive Interlock (0=Block, 1=Permitted)

    // -------------------------------------------------------------------------
    // 5. CONDENSER & FEEDWATER LOOP SUBSYSTEM (Registers 40 - 49)
    // -------------------------------------------------------------------------
    constexpr uint16_t REG_COND_HOTWELL_TEMP        = 40;  // Telemetry: Condensate Hotwell Water Temp (°C)
    constexpr uint16_t REG_COND_VACUUM_INHG         = 41;  // Telemetry: Surface Condenser Vacuum (inHg * 10) [Nominal: 28.5]
    constexpr uint16_t REG_CIRC_WATER_FLOW          = 42;  // Control/Telemetry: Circulating Water Flow (%)
    constexpr uint16_t REG_CIRC_WATER_INLET_TEMP    = 43;  // Telemetry: Cooling Tower Inlet Water Temp (°C)
    constexpr uint16_t REG_BFP_A_SPEED              = 44;  // Control: Boiler Feed Pump A Speed (%) [Redundant Pump]
    constexpr uint16_t REG_BFP_B_SPEED              = 45;  // Control: Boiler Feed Pump B Speed (%) [Redundant Pump]
    constexpr uint16_t REG_DEAERATOR_PRESSURE       = 46;  // Telemetry: Deaerator Steam Pressure (Bar * 10)
    constexpr uint16_t REG_DEAERATOR_LEVEL          = 47;  // Telemetry: Deaerator Storage Tank Level (%)
    constexpr uint16_t REG_CONDENSATE_PUMP_STATE    = 48;  // Control/Status: Condensate Extraction Pump (0=Off, 1=Running)
    constexpr uint16_t REG_COOLING_TOWER_FAN        = 49;  // Control: Cooling Tower Induced Draft Fan Speed (%)

    // -------------------------------------------------------------------------
    // 6. SAFETY INSTRUMENTED SYSTEM (SIS) & OPERATOR COMMANDS (Registers 50 - 63)
    // -------------------------------------------------------------------------
    constexpr uint16_t REG_MASTER_FUEL_TRIP         = 50;  // SIS Status: Master Fuel Trip MFT (0=Healthy, 1=TRIPPED)
    constexpr uint16_t REG_TURBINE_TRIP             = 51;  // SIS Status: Turbine Emergency Trip (0=Healthy, 1=TRIPPED)
    constexpr uint16_t REG_GENERATOR_TRIP           = 52;  // SIS Status: Generator Lockout 86G Trip (0=Healthy, 1=TRIPPED)
    constexpr uint16_t REG_EMERGENCY_SHUTDOWN_CMD   = 53;  // Operator Command: Emergency Plant Shutdown ESD (0=Normal, 1=TRIP)
    constexpr uint16_t REG_ALARM_WORD_HIGH          = 54;  // Telemetry: Alarm Bits 16 - 31 (Bitmask)
    constexpr uint16_t REG_ALARM_WORD_LOW           = 55;  // Telemetry: Alarm Bits 0 - 15 (Bitmask)
    constexpr uint16_t REG_FIRST_OUT_TRIP_CODE      = 56;  // Telemetry: First-Out Root Cause Trip Cause Code (see FirstOutCodes)
    constexpr uint16_t REG_OPERATOR_TARGET_MW       = 57;  // Operator Command: Target Megawatt Demand (MW) [0 - 500]
    constexpr uint16_t REG_OPERATOR_TARGET_DRUM_LVL = 58;  // Operator Command: Target Drum Level (% Setpoint) [default 50]
    constexpr uint16_t REG_WATCHDOG_HEARTBEAT       = 59;  // Telemetry: Incrementing Scan Heartbeat Counter
    constexpr uint16_t REG_PLC_MODE                 = 60;  // Status: PLC Mode (0=Manual, 1=Auto/Cascade, 2=Maintenance)
    constexpr uint16_t REG_RESERVED_61              = 61;  // Reserved
    constexpr uint16_t REG_RESERVED_62              = 62;  // Reserved
    constexpr uint16_t REG_RESERVED_63              = 63;  // Reserved

    // -------------------------------------------------------------------------
    // FIRST-OUT ROOT CAUSE CODES (Stored in REG_FIRST_OUT_TRIP_CODE)
    // -------------------------------------------------------------------------
    namespace FirstOutCode {
        constexpr uint16_t NONE                     = 0;
        constexpr uint16_t MANUAL_ESD_OPERATOR      = 1;
        constexpr uint16_t BOILER_OVERPRESSURE      = 2;
        constexpr uint16_t DRUM_LEVEL_LOW_LOW       = 3;
        constexpr uint16_t DRUM_LEVEL_HIGH_HIGH     = 4;
        constexpr uint16_t FLAME_FAILURE            = 5;
        constexpr uint16_t TURBINE_OVERSPEED        = 6;
        constexpr uint16_t HIGH_BEARING_VIBRATION   = 7;
        constexpr uint16_t LOSS_OF_LUBE_OIL         = 8;
        constexpr uint16_t CONDENSER_VACUUM_LOSS    = 9;
        constexpr uint16_t GENERATOR_REVERSE_POWER  = 10;
        constexpr uint16_t LOSS_OF_EXCITATION       = 11;
        constexpr uint16_t ELECTRICAL_FAULT_87G     = 12;
    }

    // -------------------------------------------------------------------------
    // ALARM BITMASK DEFINITIONS (Stored in REG_ALARM_WORD_LOW & REG_ALARM_WORD_HIGH)
    // -------------------------------------------------------------------------
    namespace AlarmLowBits {
        constexpr uint16_t BOILER_PRESS_HIGH        = (1 << 0);
        constexpr uint16_t BOILER_PRESS_HI_HI       = (1 << 1);
        constexpr uint16_t DRUM_LEVEL_LOW           = (1 << 2);
        constexpr uint16_t DRUM_LEVEL_LO_LO         = (1 << 3);
        constexpr uint16_t DRUM_LEVEL_HIGH          = (1 << 4);
        constexpr uint16_t DRUM_LEVEL_HI_HI         = (1 << 5);
        constexpr uint16_t SUPERHEATER_TEMP_HIGH    = (1 << 6);
        constexpr uint16_t FURNACE_PRESS_ABNORMAL   = (1 << 7);
        constexpr uint16_t FLAME_LOST               = (1 << 8);
        constexpr uint16_t TURBINE_VIBRATION_WARN   = (1 << 9);
        constexpr uint16_t TURBINE_VIBRATION_TRIP   = (1 << 10);
        constexpr uint16_t TURBINE_OVERSPEED_WARN   = (1 << 11);
        constexpr uint16_t TURBINE_OVERSPEED_TRIP   = (1 << 12);
        constexpr uint16_t LUBE_OIL_PRESS_LOW       = (1 << 13);
        constexpr uint16_t CONDENSER_VACUUM_LOW     = (1 << 14);
        constexpr uint16_t HOTWELL_TEMP_HIGH        = (1 << 15);
    }

    namespace AlarmHighBits {
        constexpr uint16_t MASTER_FUEL_TRIP_ACTIVE  = (1 << 0);
        constexpr uint16_t TURBINE_TRIP_ACTIVE      = (1 << 1);
        constexpr uint16_t GENERATOR_TRIP_ACTIVE    = (1 << 2);
        constexpr uint16_t SYNCHRONIZATION_FAIL     = (1 << 3);
        constexpr uint16_t REVERSE_POWER_TRIP       = (1 << 4);
        constexpr uint16_t STATOR_TEMP_HIGH         = (1 << 5);
        constexpr uint16_t BFP_A_TRIP               = (1 << 6);
        constexpr uint16_t BFP_B_TRIP               = (1 << 7);
        constexpr uint16_t DEAERATOR_LEVEL_LOW      = (1 << 8);
        constexpr uint16_t ESD_TRIGGERED            = (1 << 9);
    }
}

#endif // PLANT_REGISTERS_HPP
