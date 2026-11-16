#include "../include/TurbineSystem.hpp"
#include <algorithm>
#include <cmath>
#include <cstdlib>

TurbineSystem::TurbineSystem() {
    reset();
}

void TurbineSystem::reset() {
    m_speedRPM = 0.0;
    m_steamMassFlow = 0.0;
    m_vibrationBrg1 = 0.0;
    m_vibrationBrg2 = 0.0;
    m_rotorExpansion = 0.0;
    m_lubeOilPressure = 0.0;
    m_lubeOilTemp = 35.0;
    m_chestTemperature = 25.0;
    m_isTripped = false;
    m_mechanicalPowerMW = 0.0;
}

void TurbineSystem::step(double boilerPressure, double condenserPressure, bool msvOpen, 
                         double cvPosition, bool gridConnected, double electricalTorqueLoad, 
                         bool tripSolenoid, double dt) {
    // 1. Emergency Trip Solenoid
    if (tripSolenoid) {
        m_isTripped = true;
    }

    cvPosition = std::clamp(cvPosition, 0.0, 100.0);

    // 2. Steam Inflow Calculation
    if (!m_isTripped && msvOpen && cvPosition > 0.5 && boilerPressure > 20.0) {
        // Differential pressure driving steam across turbine stages
        double pressureRatio = (boilerPressure - 14.7) / 140.0;
        pressureRatio = std::clamp(pressureRatio, 0.0, 1.5);

        // Condenser vacuum effect (deeper vacuum draws steam more effectively)
        double vacFactor = 0.7 + 0.3 * (condenserPressure / 28.5);
        vacFactor = std::clamp(vacFactor, 0.2, 1.1);

        // Mass flow proportional to throttle opening and pressure ratio
        double maxCapacity = 80.0; // kg/s design nominal flow
        double throttle = cvPosition / 100.0;
        m_steamMassFlow = maxCapacity * throttle * pressureRatio * vacFactor;
        m_steamMassFlow = std::clamp(m_steamMassFlow, 0.0, 120.0);
    } else {
        m_steamMassFlow = 0.0;
    }

    // 3. Shaft Dynamics (Inertia & Rotational Acceleration)
    if (m_isTripped || !msvOpen || cvPosition <= 0.5 || m_steamMassFlow < 0.1) {
        // Coast-down deceleration under friction and windage
        double coastDown = 25.0 + 0.015 * m_speedRPM;
        m_speedRPM -= coastDown * dt;
        if (m_speedRPM < 0.0) m_speedRPM = 0.0;
        m_mechanicalPowerMW = 0.0;
    } else {
        if (gridConnected && m_speedRPM > 3400.0) {
            // Locked to grid frequency (60 Hz -> 3600 RPM)
            double speedError = 3600.0 - m_speedRPM;
            m_speedRPM += speedError * 0.9 * dt;

            // Mechanical power delivered to generator shaft (~250 MW at nominal 50 kg/s)
            m_mechanicalPowerMW = (m_steamMassFlow / 50.0) * 250.0;
            m_mechanicalPowerMW = std::clamp(m_mechanicalPowerMW, 0.0, 500.0);
        } else {
            // Free-spinning speed roll mode
            // Equilibrium speed is proportional to steam inflow
            double targetRPM = (m_steamMassFlow / 20.0) * 3600.0;
            double dRPM = (targetRPM - m_speedRPM) * 0.5;
            dRPM = std::clamp(dRPM, -200.0, 150.0);

            m_speedRPM += dRPM * dt;
            m_mechanicalPowerMW = 0.0;
        }
    }

    m_speedRPM = std::clamp(m_speedRPM, 0.0, 4400.0);

    // 4. Multi-Bearing Vibration Modeling (Bearings 1 and 2)
    if (m_speedRPM > 50.0) {
        // Baseline quadratic vibration
        double normSpeed = m_speedRPM / 3600.0;
        double baseVibe = normSpeed * normSpeed * 1.2;

        // Critical resonance speed around 1800 RPM (First bending critical)
        double resonance = 0.0;
        if (m_speedRPM > 1650.0 && m_speedRPM < 1950.0) {
            double dist = std::abs(m_speedRPM - 1800.0);
            resonance = (150.0 - dist) * 0.025; // Peaks up to ~3.75 mm/s
        }

        // Overspeed runaway vibration (> 3600 RPM)
        double overspeedVibe = 0.0;
        if (m_speedRPM > 3600.0) {
            overspeedVibe = std::pow((m_speedRPM - 3600.0) / 100.0, 1.8) * 0.8;
        }

        // Small micro-sensor noise
        double noise1 = (static_cast<double>(std::rand() % 100 - 50) / 1000.0);
        double noise2 = (static_cast<double>(std::rand() % 100 - 50) / 1000.0);

        m_vibrationBrg1 = std::max(0.0, baseVibe + resonance + overspeedVibe + noise1);
        m_vibrationBrg2 = std::max(0.0, baseVibe * 1.1 + resonance * 0.9 + overspeedVibe + noise2);
    } else {
        m_vibrationBrg1 = 0.0;
        m_vibrationBrg2 = 0.0;
    }

    // 5. Lube Oil Pressure and Rotor Thermal Expansion
    if (m_speedRPM > 200.0) {
        // Shaft-driven main lube oil pump
        m_lubeOilPressure = 2.8 + (m_speedRPM / 3600.0) * 0.4;
        m_lubeOilTemp = 42.0 + (m_speedRPM / 3600.0) * 8.0;
    } else {
        // AC auxiliary lube oil pump
        m_lubeOilPressure = 1.9;
        m_lubeOilTemp = 35.0;
    }

    // Steam chest temperature warms up with steam flow
    if (m_steamMassFlow > 5.0) {
        double targetChest = 480.0 * (cvPosition / 100.0);
        m_chestTemperature += (targetChest - m_chestTemperature) * 0.05 * dt;
    } else {
        m_chestTemperature += (25.0 - m_chestTemperature) * 0.01 * dt;
    }

    // Rotor differential expansion due to metal heating
    m_rotorExpansion = (m_chestTemperature / 500.0) * 4.2; // mm
}
