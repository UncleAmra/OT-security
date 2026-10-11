#ifndef CONDENSER_FEEDWATER_SYSTEM_HPP
#define CONDENSER_FEEDWATER_SYSTEM_HPP

/**
 * ==============================================================================
 *  SURFACE CONDENSER, FEEDWATER & DEAERATOR SYSTEM
 * ==============================================================================
 * Models:
 *   - Surface condenser hotwell heat balance and vacuum dynamics (inHg).
 *   - Circulating water pumps (CWP) & cooling tower loop.
 *   - Redundant Boiler Feed Pumps (BFP A and B) with speed modulation.
 *   - Deaerator tank mass and energy storage balance.
 *   - Condensate Extraction Pump (CEP).
 * ==============================================================================
 */

class CondenserFeedwaterSystem {
public:
    CondenserFeedwaterSystem();

    void reset();

    /**
     * @brief Step condenser & feedwater cycles.
     * @param steamExhaustMassFlow kg/s steam entering condenser from turbine
     * @param circWaterFlow % Circulating cooling water flow (0 - 100%)
     * @param bfpASpeed % Speed of Boiler Feed Pump A (0 - 100%)
     * @param bfpBSpeed % Speed of Boiler Feed Pump B (0 - 100%)
     * @param cepRunning Condensate extraction pump state
     * @param dt Time delta in seconds
     */
    void step(double steamExhaustMassFlow, double circWaterFlow, 
              double bfpASpeed, double bfpBSpeed, bool cepRunning, double dt);

    // Getters
    double getHotwellTemp() const { return m_hotwellTemp; }
    double getVacuumInHg() const { return m_vacuumInHg; }
    double getCircWaterFlow() const { return m_circWaterFlow; }
    double getCircWaterInletTemp() const { return m_circWaterInletTemp; }
    double getFeedwaterMassFlow() const { return m_feedwaterMassFlow; }
    double getDeaeratorPressure() const { return m_deaeratorPressure; }
    double getDeaeratorLevel() const { return m_deaeratorLevel; }
    bool   isCepRunning() const { return m_cepRunning; }

private:
    double m_hotwellTemp;            // °C
    double m_vacuumInHg;             // inHg (0 - 30.0)
    double m_circWaterFlow;          // %
    double m_circWaterInletTemp;     // °C
    double m_feedwaterMassFlow;      // kg/s pumped to boiler
    double m_deaeratorPressure;      // Bar
    double m_deaeratorLevel;         // %
    bool   m_cepRunning;             // CEP pump
};

#endif // CONDENSER_FEEDWATER_SYSTEM_HPP
