#include "../include/GeneratorSystem.hpp"
#include <algorithm>
#include <cmath>

GeneratorSystem::GeneratorSystem() {
    reset();
}

void GeneratorSystem::reset() {
    m_activePowerMW = 0.0;
    m_reactivePowerMVAR = 0.0;
    m_frequencyHz = 0.0;
    m_terminalVoltageKV = 0.0;
    m_breakerClosed = false;
    m_phaseDeltaDeg = 180.0;
    m_fieldCurrentAmps = 0.0;
    m_powerFactor = 0.95;
    m_statorTemp = 30.0;
    m_syncPermitted = false;
}

void GeneratorSystem::step(double turbineRPM, double mechanicalPowerMW, bool breakerCmd, 
                            double targetExciterCurrent, double dt) {
    // 1. Excitation System & Terminal Voltage
    // Field current regulated by AVR (Automatic Voltage Regulator)
    m_fieldCurrentAmps = std::clamp(targetExciterCurrent, 0.0, 150.0);

    if (turbineRPM > 100.0) {
        // Faraday's Law: E = k * omega * Phi_field
        double baseVoltage = (turbineRPM / 3600.0) * (m_fieldCurrentAmps / 100.0) * 22.0;
        m_terminalVoltageKV += (baseVoltage - m_terminalVoltageKV) * 0.5 * dt;
        m_terminalVoltageKV = std::clamp(m_terminalVoltageKV, 0.0, 26.0);
    } else {
        m_terminalVoltageKV = 0.0;
    }

    // 2. Frequency & Synchroscope Check
    constexpr double GRID_FREQ_HZ = 60.00;
    constexpr double GRID_VOLTAGE_KV = 22.00;

    if (!m_breakerClosed) {
        // Off-line (Open Circuit): Frequency directly tied to shaft speed (f = RPM / 60)
        m_frequencyHz = turbineRPM / 60.0;

        // Phase angle rotates relative to grid
        double deltaFreq = m_frequencyHz - GRID_FREQ_HZ;
        m_phaseDeltaDeg += deltaFreq * 360.0 * dt;
        while (m_phaseDeltaDeg >= 360.0) m_phaseDeltaDeg -= 360.0;
        while (m_phaseDeltaDeg < 0.0)    m_phaseDeltaDeg += 360.0;

        // Synchroscope Permissive Check:
        // Voltage within 2%, Freq within 0.1 Hz, Phase within 5 degrees of top (0 or 360)
        bool voltOk = std::abs(m_terminalVoltageKV - GRID_VOLTAGE_KV) < 0.5;
        bool freqOk = std::abs(m_frequencyHz - GRID_FREQ_HZ) < 0.15;
        bool phaseOk = (m_phaseDeltaDeg < 7.0 || m_phaseDeltaDeg > 353.0);

        m_syncPermitted = (voltOk && freqOk && phaseOk);

        // Breaker Closing Interlock (Auto-Synchronizer arms and closes)
        if (breakerCmd && (m_syncPermitted || (voltOk && freqOk))) {
            m_breakerClosed = true;
            m_phaseDeltaDeg = 0.0;
        }

        m_activePowerMW = 0.0;
        m_reactivePowerMVAR = 0.0;
    } else {
        // On-line (Grid Tied):
        // Breaker opening command
        if (!breakerCmd) {
            m_breakerClosed = false;
        }

        // Locked to grid frequency with tiny damping oscillation
        m_frequencyHz = GRID_FREQ_HZ + (turbineRPM - 3600.0) * 0.0005;
        m_phaseDeltaDeg = 0.0;
        m_syncPermitted = true;

        // Real Power Output (MW) is proportional to mechanical shaft torque delivered
        m_activePowerMW = mechanicalPowerMW;
        m_activePowerMW = std::clamp(m_activePowerMW, 0.0, 550.0);

        // Reactive Power (MVAR) depends on exciter field current vs grid voltage
        m_reactivePowerMVAR = (m_terminalVoltageKV - GRID_VOLTAGE_KV) * 45.0;
        m_reactivePowerMVAR = std::clamp(m_reactivePowerMVAR, -80.0, 150.0);

        // Power factor cos phi
        double apparentPower = std::sqrt(m_activePowerMW * m_activePowerMW + m_reactivePowerMVAR * m_reactivePowerMVAR);
        if (apparentPower > 1.0) {
            m_powerFactor = m_activePowerMW / apparentPower;
        } else {
            m_powerFactor = 1.0;
        }
    }

    // 3. Stator Winding Thermal Model
    // I^2 * R copper heating + iron core losses
    double heating = (m_activePowerMW / 500.0) * (m_activePowerMW / 500.0) * 45.0;
    double cooling = 0.02 * (m_statorTemp - 30.0); // Hydrogen / water stator cooling
    m_statorTemp += (heating - cooling) * dt;
    m_statorTemp = std::clamp(m_statorTemp, 25.0, 130.0);
}
