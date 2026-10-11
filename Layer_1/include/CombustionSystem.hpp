#ifndef COMBUSTION_SYSTEM_HPP
#define COMBUSTION_SYSTEM_HPP

/**
 * ==============================================================================
 *  COMBUSTION, FURNACE DRAFT & BURNER MANAGEMENT SYSTEM (BMS)
 * ==============================================================================
 * Models:
 *   - Burner fuel feed dynamics & stoichiometric air combustion.
 *   - Forced Draft (FD) Fan & Induced Draft (ID) Fan air balance.
 *   - Balanced draft furnace pressure regulation (target: -5 to -15 mm H2O).
 *   - Flue gas oxygen (O2 %) & combustion thermal efficiency.
 *   - Optical flame scanner safeguard with flame failure trip.
 * ==============================================================================
 */

class CombustionSystem {
public:
    CombustionSystem();

    void reset();

    /**
     * @brief Step combustion process.
     * @param fuelFeedRate kg/min fuel flow [0 - 100]
     * @param fdFanSpeed % Forced draft fan speed [0 - 100]
     * @param idFanSpeed % Induced draft fan speed [0 - 100]
     * @param masterFuelTrip True if MFT is active (forces fuel valve shut)
     * @param dt Time delta in seconds
     */
    void step(double fuelFeedRate, double fdFanSpeed, double idFanSpeed, bool masterFuelTrip, double dt);

    // Getters
    double getEffectiveFuelRate() const { return m_effectiveFuelRate; }
    double getFurnacePressure() const { return m_furnacePressure; }
    double getFlueGasO2() const { return m_flueGasO2; }
    double getAirFuelRatio() const { return m_airFuelRatio; }
    int    getBurnerState() const { return m_burnerState; }
    bool   hasFlame() const { return m_flameDetected; }
    double getCombustionEfficiency() const { return m_efficiency; }
    double getThermalHeatOutput() const { return m_heatOutputKW; }

private:
    double m_effectiveFuelRate;     // kg/min
    double m_furnacePressure;       // mm H2O (-25.0 to +15.0)
    double m_flueGasO2;             // % excess oxygen
    double m_airFuelRatio;          // stoichiometric ratio (e.g. 1.15)
    int    m_burnerState;           // 0=Off, 1=Igniting, 2=Firing
    bool   m_flameDetected;         // Optical detector
    double m_efficiency;            // % (70 - 92%)
    double m_heatOutputKW;          // Thermal energy output
};

#endif // COMBUSTION_SYSTEM_HPP
