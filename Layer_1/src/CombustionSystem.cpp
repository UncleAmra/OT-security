#include "../include/CombustionSystem.hpp"
#include <algorithm>
#include <cmath>

CombustionSystem::CombustionSystem() {
    reset();
}

void CombustionSystem::reset() {
    m_effectiveFuelRate = 0.0;
    m_furnacePressure = -10.0; // Nominal -10 mm H2O draft
    m_flueGasO2 = 21.0;        // Ambient air O2 %
    m_airFuelRatio = 1.0;
    m_burnerState = 0;         // Off
    m_flameDetected = false;
    m_efficiency = 88.0;
    m_heatOutputKW = 0.0;
}

void CombustionSystem::step(double fuelFeedRate, double fdFanSpeed, double idFanSpeed, 
                            bool masterFuelTrip, double dt) {
    fdFanSpeed = std::clamp(fdFanSpeed, 0.0, 100.0);
    idFanSpeed = std::clamp(idFanSpeed, 0.0, 100.0);

    // 1. Master Fuel Trip (MFT) Safety Interlock
    if (masterFuelTrip) {
        m_effectiveFuelRate = 0.0;
        m_burnerState = 0;
        m_flameDetected = false;
        m_heatOutputKW = 0.0;
    } else {
        // Normal fuel throttle with safety clamps
        m_effectiveFuelRate = std::clamp(fuelFeedRate, 0.0, 100.0);

        if (m_effectiveFuelRate > 2.0 && fdFanSpeed > 10.0) {
            m_burnerState = 2; // Active Firing
            m_flameDetected = true;
        } else {
            m_burnerState = 0; // Off
            m_flameDetected = false;
        }
    }

    // 2. Draft Air Balance and Furnace Pressure
    // Positive pressure pushed by FD fan, negative pulled by ID fan
    // Target draft is slightly negative: -10 mm H2O to prevent flue gas leaking into boiler house
    double airFlowIn = fdFanSpeed * 1.0;
    double flueGasOut = idFanSpeed * 1.15; // ID fan typically slightly larger capacity

    double draftDelta = airFlowIn - flueGasOut;
    // Base draft is -10 mm H2O; imbalance moves pressure
    double targetDraft = -10.0 + (draftDelta * 0.35);

    m_furnacePressure += (targetDraft - m_furnacePressure) * 0.4 * dt;
    m_furnacePressure = std::clamp(m_furnacePressure, -35.0, 20.0);

    // 3. Flue Gas Oxygen (O2 %) & Air-to-Fuel Ratio
    if (m_burnerState == 2 && m_effectiveFuelRate > 1.0) {
        // Stoichiometric air demand: ~1.0 unit air per unit fuel
        double airSupply = fdFanSpeed;
        m_airFuelRatio = (m_effectiveFuelRate > 0.0) ? (airSupply / m_effectiveFuelRate) : 1.0;

        // Optimum excess O2 in flue gas is ~2.5 - 3.5%
        double targetO2 = (m_airFuelRatio - 1.0) * 8.0 + 3.0;
        targetO2 = std::clamp(targetO2, 0.5, 12.0);
        m_flueGasO2 += (targetO2 - m_flueGasO2) * 0.2 * dt;

        // Combustion Efficiency curve
        // Drops if not enough air (unburnt carbon) or too much excess air (heats up stack gas)
        m_efficiency = 90.0 - std::abs(m_airFuelRatio - 1.15) * 15.0;
        m_efficiency = std::clamp(m_efficiency, 65.0, 92.0);

        // Heat Output in kW
        m_heatOutputKW = m_effectiveFuelRate * (m_efficiency / 100.0) * 65.0;
    } else {
        // Boiler purged with fresh air
        m_flueGasO2 += (20.9 - m_flueGasO2) * 0.1 * dt;
        m_airFuelRatio = 1.0;
        m_efficiency = 0.0;
        m_heatOutputKW = 0.0;
    }
}
