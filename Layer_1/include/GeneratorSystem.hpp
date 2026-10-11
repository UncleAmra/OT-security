#ifndef GENERATOR_SYSTEM_HPP
#define GENERATOR_SYSTEM_HPP

/**
 * ==============================================================================
 *  SYNCHRONOUS GENERATOR, AVR & ELECTRICAL SWITCHYARD SYSTEM
 * ==============================================================================
 * Models:
 *   - Automatic Voltage Regulator (AVR) & field excitation current.
 *   - Terminal voltage generation (22.0 kV nominal).
 *   - Grid Synchronization Synchroscope (ΔV, Δf, Δθ phase angle difference).
 *   - 52G Main Generator Breaker closing interlock.
 *   - Active power (MW) & reactive power (MVAR) generation.
 *   - Generator stator thermal heating & electrical loss calculations.
 * ==============================================================================
 */

class GeneratorSystem {
public:
    GeneratorSystem();

    void reset();

    /**
     * @brief Step electrical state.
     * @param turbineRPM Shaft rotational speed
     * @param mechanicalPowerMW Mechanical shaft power from turbine
     * @param breakerCmd 0=Open, 1=Close request
     * @param targetExciterCurrent Desired excitation current (Amps)
     * @param dt Time delta in seconds
     */
    void step(double turbineRPM, double mechanicalPowerMW, bool breakerCmd, 
              double targetExciterCurrent, double dt);

    // Getters
    double getActivePowerMW() const { return m_activePowerMW; }
    double getReactivePowerMVAR() const { return m_reactivePowerMVAR; }
    double getFrequencyHz() const { return m_frequencyHz; }
    double getTerminalVoltageKV() const { return m_terminalVoltageKV; }
    bool   getBreakerState() const { return m_breakerClosed; }
    double getSynchroscopePhaseDelta() const { return m_phaseDeltaDeg; }
    double getExciterFieldCurrent() const { return m_fieldCurrentAmps; }
    double getPowerFactor() const { return m_powerFactor; }
    double getStatorTemperature() const { return m_statorTemp; }
    bool   isSyncPermitted() const { return m_syncPermitted; }

private:
    double m_activePowerMW;          // MW
    double m_reactivePowerMVAR;      // MVAR
    double m_frequencyHz;            // Hz (60.00 nominal)
    double m_terminalVoltageKV;      // kV (22.0 nominal)
    bool   m_breakerClosed;          // 52G Breaker
    double m_phaseDeltaDeg;          // Phase angle error to grid
    double m_fieldCurrentAmps;       // Excitation current
    double m_powerFactor;            // cos phi
    double m_statorTemp;             // °C
    bool   m_syncPermitted;          // Synchroscope check
};

#endif // GENERATOR_SYSTEM_HPP
