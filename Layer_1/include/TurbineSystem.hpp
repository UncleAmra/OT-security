#ifndef TURBINE_SYSTEM_HPP
#define TURBINE_SYSTEM_HPP

/**
 * ==============================================================================
 *  STEAM TURBINE & DIGITAL ELECTRO-HYDRAULIC (DEH) GOVERNOR SYSTEM
 * ==============================================================================
 * Models:
 *   - Main Stop Valve (MSV) & modulated Governor Control Valves (CV).
 *   - Rotor inertia & speed dynamics (J * dω/dt = Torque_steam - Torque_drag - Torque_load).
 *   - Critical rotational resonance vibration around 1800 RPM.
 *   - High-fidelity multi-bearing vibration probes (Bearings 1 & 2).
 *   - Overspeed Protection Controller (OPC @ 3708 RPM) & Emergency Trip (@ 3960 RPM).
 *   - Lube oil supply pressure & rotor differential expansion.
 * ==============================================================================
 */

class TurbineSystem {
public:
    TurbineSystem();

    void reset();

    /**
     * @brief Step forward in time.
     * @param boilerPressure PSI steam driving turbine
     * @param condenserPressure InHg vacuum opposing exhaust
     * @param msvOpen Main Stop Valve (0=Closed, 1=Open)
     * @param cvPosition Control Valve throttle (0 - 100%)
     * @param gridConnected True if generator 52G breaker is closed
     * @param electricalTorqueLoad Opposing electrical torque when grid connected
     * @param tripSolenoid True if Emergency Trip System ETS is tripped
     * @param dt Time delta in seconds
     */
    void step(double boilerPressure, double condenserPressure, bool msvOpen, 
              double cvPosition, bool gridConnected, double electricalTorqueLoad, 
              bool tripSolenoid, double dt);

    // Telemetry Getters
    double getSpeedRPM() const { return m_speedRPM; }
    double getSteamMassFlow() const { return m_steamMassFlow; }
    double getVibrationBearing1() const { return m_vibrationBrg1; }
    double getVibrationBearing2() const { return m_vibrationBrg2; }
    double getRotorExpansion() const { return m_rotorExpansion; }
    double getLubeOilPressure() const { return m_lubeOilPressure; }
    double getLubeOilTemp() const { return m_lubeOilTemp; }
    double getChestTemperature() const { return m_chestTemperature; }
    bool   isTripped() const { return m_isTripped; }
    double getMechanicalPowerMW() const { return m_mechanicalPowerMW; }

private:
    double m_speedRPM;               // RPM (0 - 4000)
    double m_steamMassFlow;          // kg/s
    double m_vibrationBrg1;          // mm/s
    double m_vibrationBrg2;          // mm/s
    double m_rotorExpansion;         // mm
    double m_lubeOilPressure;        // Bar
    double m_lubeOilTemp;            // °C
    double m_chestTemperature;       // °C
    bool   m_isTripped;              // Emergency Trip status
    double m_mechanicalPowerMW;      // Shaft mechanical output
};

#endif // TURBINE_SYSTEM_HPP
