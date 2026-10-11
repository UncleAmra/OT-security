#include "../include/BoilerSystem.hpp"
#include <cmath>
#include <algorithm>

BoilerSystem::BoilerSystem() {
    reset();
}

void BoilerSystem::reset() {
    m_drumTemperature = AMBIENT_TEMP;
    m_drumPressure = ATMOSPHERIC_PRESSURE;
    m_trueLiquidLevel = NORMAL_DRUM_LEVEL;
    m_indicatedDrumLevel = NORMAL_DRUM_LEVEL;
    m_superheaterTemperature = AMBIENT_TEMP;
    m_safetyValve1 = false;
    m_safetyValve2 = false;
    m_sprayValvePos = 0.0;
    m_blowdownOpen = false;
    m_prevSteamFlow = 0.0;
    m_swellBubbleVolume = 0.0;
}

void BoilerSystem::step(double heatInputRate, double feedwaterFlowMass, double steamMassFlow, 
                        double sprayValvePos, int blowdownCmd, double dt) {
    m_sprayValvePos = std::clamp(sprayValvePos, 0.0, 100.0);
    m_blowdownOpen = (blowdownCmd != 0);

    // 1. ASME Safety Relief Valves (Hysteresis Pop / Reseat Logic)
    // Valve 1 pops at 180 PSI, reseats at 170 PSI
    if (m_drumPressure >= 180.0) {
        m_safetyValve1 = true;
    } else if (m_drumPressure <= 170.0) {
        m_safetyValve1 = false;
    }

    // Valve 2 pops at 195 PSI, reseats at 185 PSI
    if (m_drumPressure >= 195.0) {
        m_safetyValve2 = true;
    } else if (m_drumPressure <= 185.0) {
        m_safetyValve2 = false;
    }

    // 2. Thermodynamic Heat & Temperature Equations
    // Combustion thermal heat input (scaled)
    double heatInput = heatInputRate * 0.12; 

    // Ambient convective loss
    double heatLoss = 0.004 * (m_drumTemperature - AMBIENT_TEMP);

    // Latent heat loss due to steam generation
    double steamCooling = 0.0;
    if (m_drumTemperature > 100.0) {
        steamCooling = steamMassFlow * 0.45;
    }

    // Safety valve venting cooling
    double reliefCooling = 0.0;
    if (m_safetyValve1) reliefCooling += 15.0;
    if (m_safetyValve2) reliefCooling += 25.0;

    // Net temperature change
    double dT = heatInput - heatLoss - steamCooling - reliefCooling;
    m_drumTemperature += dT * dt;
    if (m_drumTemperature < AMBIENT_TEMP) {
        m_drumTemperature = AMBIENT_TEMP;
    }

    // 3. Drum Pressure Dynamics (Rankine Steam Saturation Curve)
    if (m_safetyValve1 || m_safetyValve2) {
        // High rate relief pressure drop
        double reliefFlowRate = (m_safetyValve1 ? 4.5 : 0.0) + (m_safetyValve2 ? 8.0 : 0.0);
        double dP = -reliefFlowRate * (m_drumPressure - ATMOSPHERIC_PRESSURE) * 0.2;
        m_drumPressure += dP * dt;
    } else {
        // Saturated steam pressure builds above 100°C
        double targetPressure = ATMOSPHERIC_PRESSURE;
        if (m_drumTemperature > 100.0) {
            targetPressure += std::pow((m_drumTemperature - 100.0) * 0.14, 1.42) * 8.5;
        }

        // Pressure lag
        double dP = (targetPressure - m_drumPressure) * 0.18;
        m_drumPressure += dP * dt;
    }

    if (m_drumPressure < ATMOSPHERIC_PRESSURE) {
        m_drumPressure = ATMOSPHERIC_PRESSURE;
    }

    // 4. True Liquid Water Mass Balance
    // Mass Balance: dMass/dt = FeedwaterFlow - SteamFlow - BlowdownFlow
    double blowdownFlow = m_blowdownOpen ? 3.0 : 0.0;
    double netMassFlow = feedwaterFlowMass - steamMassFlow - blowdownFlow;

    // 1 kg/s net imbalance results in ~0.25% level change per second
    m_trueLiquidLevel += (netMassFlow * 0.25) * dt;
    m_trueLiquidLevel = std::clamp(m_trueLiquidLevel, 0.0, 100.0);

    // 5. Shrink & Swell Phenomenon (Two-Phase Steam Bubble Volume)
    // Rapid increase in steam flow -> pressure drops -> steam bubbles expand (SWELL)
    // Rapid increase in cold feedwater -> collapses bubbles (SHRINK)
    double steamFlowDelta = steamMassFlow - m_prevSteamFlow;
    m_prevSteamFlow = steamMassFlow;

    // Bubble volume increases with steam outflow surges
    m_swellBubbleVolume += (steamFlowDelta * 2.5);
    // Bubble volume relaxes back to steady-state with a 4-second time constant
    m_swellBubbleVolume -= (m_swellBubbleVolume * 0.25) * dt;
    m_swellBubbleVolume = std::clamp(m_swellBubbleVolume, -15.0, 20.0);

    // Indicated level seen by DP (Differential Pressure) transmitters
    m_indicatedDrumLevel = m_trueLiquidLevel + m_swellBubbleVolume;
    m_indicatedDrumLevel = std::clamp(m_indicatedDrumLevel, 0.0, 100.0);

    // 6. Superheater & Desuperheater (Attemperator) Temperature
    if (m_drumTemperature > 100.0) {
        // Superheated steam absorbs additional furnace radiant heat up to 540°C
        double targetSuperheat = m_drumTemperature + 180.0;
        // Spray valve injects cold feedwater directly into steam header
        double sprayCooling = (m_sprayValvePos / 100.0) * 120.0;
        targetSuperheat -= sprayCooling;

        m_superheaterTemperature += (targetSuperheat - m_superheaterTemperature) * 0.15 * dt;
    } else {
        m_superheaterTemperature = m_drumTemperature;
    }
}
