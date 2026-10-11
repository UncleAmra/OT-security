#ifndef PLC_SAFETY_INTERLOCKS_HPP
#define PLC_SAFETY_INTERLOCKS_HPP

#include <cstdint>
#include <vector>
#include "../../common/PlantRegisters.hpp"

/**
 * ==============================================================================
 *  SAFETY INSTRUMENTED SYSTEM (SIS) & EMERGENCY TRIP LOGIC (2oo3)
 * ==============================================================================
 * Industrial emergency shutdown system enforcing:
 *   - Master Fuel Trip (MFT)
 *   - Turbine Emergency Trip System (ETS)
 *   - Generator Lockout (86G)
 *   - First-Out root cause identification
 *   - Alarm bitmask synthesis
 * ==============================================================================
 */

class PlcSafetyInterlocks {
public:
    PlcSafetyInterlocks();

    void reset();

    /**
     * @brief Evaluate plant safety state and apply interlocks.
     * @param registers Reference to working holding register array
     */
    void evaluate(std::vector<uint16_t> &registers);

    bool isMasterFuelTripped() const { return m_mftActive; }
    bool isTurbineTripped() const { return m_turbineTripActive; }
    bool isGeneratorTripped() const { return m_generatorTripActive; }
    uint16_t getFirstOutTripCode() const { return m_firstOutCode; }

    void triggerManualEsd();
    void resetTrips();

private:
    bool     m_mftActive;
    bool     m_turbineTripActive;
    bool     m_generatorTripActive;
    uint16_t m_firstOutCode;
    uint16_t m_alarmLow;
    uint16_t m_alarmHigh;
    uint32_t m_startupGraceCounter;

    void recordFirstOut(uint16_t tripCode);
};

#endif // PLC_SAFETY_INTERLOCKS_HPP
