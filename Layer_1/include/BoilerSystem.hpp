#ifndef BOILER_SYSTEM_HPP
#define BOILER_SYSTEM_HPP

#include <string>

/**
 * ==============================================================================
 *  BOILER & STEAM DRUM SYSTEM (THERMAL & MASS BALANCE DIGITAL TWIN)
 * ==============================================================================
 * Models:
 *   - Water/steam drum thermodynamic equilibrium and saturation pressure curve.
 *   - Three-element drum level with realistic "shrink & swell" bubble expansion.
 *   - Dual ASME Section I safety relief valves with pop-open & reseat hysteresis.
 *   - Superheater section with spray attemperator (desuperheater).
 *   - Bottom blowdown water removal.
 * ==============================================================================
 */

class BoilerSystem {
public:
    BoilerSystem();

    void reset();

    /**
     * @brief Step forward in time by dt seconds.
     * @param heatInputRate Total thermal heat energy from combustion (kJ/s or kW)
     * @param feedwaterFlowMass kg/s of feedwater pumped into drum
     * @param steamMassFlow kg/s of steam leaving to superheater/turbine
     * @param sprayValvePos 0-100% spray attemperator cooling valve
     * @param blowdownCmd 0=Closed, 1=Open
     * @param dt Time delta in seconds
     */
    void step(double heatInputRate, double feedwaterFlowMass, double steamMassFlow, 
              double sprayValvePos, int blowdownCmd, double dt);

    // Operator & Safety Valve Controls
    void setSafetyValve1(bool open) { m_safetyValve1 = open; }
    void setSafetyValve2(bool open) { m_safetyValve2 = open; }

    // Telemetry getters
    double getDrumTemperature() const { return m_drumTemperature; }
    double getDrumPressure() const { return m_drumPressure; }
    double getDrumLevel() const { return m_indicatedDrumLevel; }
    double getTrueLiquidLevel() const { return m_trueLiquidLevel; }
    double getSuperheaterTemperature() const { return m_superheaterTemperature; }
    bool   getSafetyValve1State() const { return m_safetyValve1; }
    bool   getSafetyValve2State() const { return m_safetyValve2; }
    double getAttemperatorSpray() const { return m_sprayValvePos; }
    bool   getBlowdownState() const { return m_blowdownOpen; }

private:
    // Thermodynamic state
    double m_drumTemperature;          // °C
    double m_drumPressure;             // PSI
    double m_trueLiquidLevel;          // % (0 - 100%)
    double m_indicatedDrumLevel;       // % (includes swell/shrink bubble transient)
    double m_superheaterTemperature;   // °C

    // Valves
    bool   m_safetyValve1;             // Mechanical spring pop valve 1 (Trip @ 180 PSI)
    bool   m_safetyValve2;             // Mechanical spring pop valve 2 (Trip @ 195 PSI)
    double m_sprayValvePos;            // 0 - 100%
    bool   m_blowdownOpen;             // Bottom drain

    // Derivative tracking for shrink/swell
    double m_prevSteamFlow;
    double m_swellBubbleVolume;

    static constexpr double AMBIENT_TEMP = 25.0;           // °C
    static constexpr double ATMOSPHERIC_PRESSURE = 14.7;   // PSI
    static constexpr double NORMAL_DRUM_LEVEL = 50.0;      // %
};

#endif // BOILER_SYSTEM_HPP
