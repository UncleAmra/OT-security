#ifndef PLC_SCAN_ENGINE_HPP
#define PLC_SCAN_ENGINE_HPP

#include <vector>
#include <cstdint>
#include "PlcDataBank.hpp"
#include "PidController.hpp"
#include "PlcSafetyInterlocks.hpp"
#include "../../common/PlantRegisters.hpp"

/**
 * ==============================================================================
 *  INDUSTRIAL PLC SCAN CYCLE ENGINE
 * ==============================================================================
 * Executes deterministic industrial cyclic control loops:
 *   1. Scan Inputs (Field telemetry from Layer 1, Operator setpoints from Layer 3)
 *   2. Evaluate Real-Time Regulatory PID Control Loops
 *   3. Evaluate Safety Interlocks (SIS / 2oo3)
 *   4. Write Outputs (Actuator commands, trip relays, alarm bitmasks)
 * ==============================================================================
 */

class PlcScanEngine {
public:
    PlcScanEngine(PlcDataBank &dataBank);

    void reset();

    /**
     * @brief Run one single deterministic scan cycle.
     * @param dt Time delta since previous scan in seconds
     */
    void executeScan(double dt);

    const PlcSafetyInterlocks& getSafetySystem() const { return m_safety; }
    PlcSafetyInterlocks&       getSafetySystem()       { return m_safety; }

private:
    PlcDataBank&        m_dataBank;
    PlcSafetyInterlocks m_safety;

    // Regulatory Control Loops
    PidController m_drumLevelPid;      // 3-element drum level control -> BFP speed
    PidController m_turbineSpeedPid;   // DEH speed governor -> Control Valve (CV)
    PidController m_turbineLoadPid;    // Grid-connected MW load governor -> CV
    PidController m_draftPressurePid;  // Furnace draft -> ID fan speed

    uint16_t m_scanCount;
};

#endif // PLC_SCAN_ENGINE_HPP
