#ifndef DIGITAL_TWIN_PLANT_HPP
#define DIGITAL_TWIN_PLANT_HPP

#include <vector>
#include <cstdint>
#include "BoilerSystem.hpp"
#include "CombustionSystem.hpp"
#include "TurbineSystem.hpp"
#include "GeneratorSystem.hpp"
#include "CondenserFeedwaterSystem.hpp"
#include "../../common/PlantRegisters.hpp"

/**
 * ==============================================================================
 *  DIGITAL TWIN POWER PLANT (COMPLETE COUPLED CYCLE ENGINE)
 * ==============================================================================
 * Glues all thermodynamic and electrical subsystems together into a unified
 * digital twin. Bridges the physical model state with the Modbus register array.
 * ==============================================================================
 */

class DigitalTwinPlant {
public:
    DigitalTwinPlant();

    void resetAll();

    /**
     * @brief Update the plant simulation by one time delta dt.
     * @param plcHoldingRegisters The current 64 holding registers read from the PLC
     * @param outTelemetryRegisters The updated 64 holding registers to write to the PLC
     * @param dt Time step in seconds
     */
    void step(const std::vector<uint16_t> &plcHoldingRegisters, 
              std::vector<uint16_t> &outTelemetryRegisters, 
              double dt);

    // Subsystem accessors
    BoilerSystem&             getBoiler()     { return m_boiler; }
    CombustionSystem&         getCombustion() { return m_combustion; }
    TurbineSystem&            getTurbine()    { return m_turbine; }
    GeneratorSystem&          getGenerator()  { return m_generator; }
    CondenserFeedwaterSystem& getCondenser()  { return m_condenser; }

private:
    BoilerSystem             m_boiler;
    CombustionSystem         m_combustion;
    TurbineSystem            m_turbine;
    GeneratorSystem          m_generator;
    CondenserFeedwaterSystem m_condenser;

    uint16_t m_heartbeat;
};

#endif // DIGITAL_TWIN_PLANT_HPP
