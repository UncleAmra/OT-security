#ifndef TURBINE_SYSTEM_HPP
#define TURBINE_SYSTEM_HPP

/*
 * Steam turbine model: stop and control valves, rotor speed from a torque
 * balance, bearing vibration (with a resonance near 1800 RPM), lube oil and
 * rotor expansion.
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
