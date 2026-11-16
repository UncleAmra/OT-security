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
    // Combustion thermal heat input (scaled for responsive & believable testbed startup)
    // At nominal firing (1800 kW), heatGain is ~7.2 °C/s, warming cold boiler to 100°C in ~11s
    double heatInput = heatInputRate * 0.004; 

    // Ambient convective loss
    double heatLoss = 0.008 * (m_drumTemperature - AMBIENT_TEMP);

    // Latent heat loss due to steam generation
    double steamCooling = 0.0;
    if (m_drumTemperature > 100.0) {
        steamCooling = steamMassFlow * 0.06;
    }

    // Safety valve venting cooling
    double reliefCooling = 0.0;
    if (m_safetyValve1) reliefCooling += 6.0;
    if (m_safetyValve2) reliefCooling += 10.0;

    // Net temperature change
    double dT = heatInput - heatLoss - steamCooling - reliefCooling;
    m_drumTemperature += dT * dt;
    if (m_drumTemperature < AMBIENT_TEMP) {
        m_drumTemperature = AMBIENT_TEMP;
    }

    // 3. Drum Pressure Dynamics (Rankine Steam Saturation Curve)
    // Saturated steam pressure builds smoothly above 100°C
    double targetPressure = ATMOSPHERIC_PRESSURE;
    if (m_drumTemperature > 100.0) {
        // Nominal operating saturation: 180°C corresponds to ~155 PSI
        // 192°C approaches high-pressure warning (175 PSI)
        // 200°C triggers overpressure trip (190 PSI)
        targetPressure = ATMOSPHERIC_PRESSURE + ((m_drumTemperature - 100.0) / 80.0) * 140.0;
    }

    // Pressure relief when safety valves are open
    if (m_safetyValve1) targetPressure -= 25.0;
    if (m_safetyValve2) targetPressure -= 35.0;
    if (targetPressure < ATMOSPHERIC_PRESSURE) {
        targetPressure = ATMOSPHERIC_PRESSURE;
    }

    // Smooth physical pressure lag
    double dP = (targetPressure - m_drumPressure) * 0.5;
    m_drumPressure += dP * dt;

    if (m_drumPressure < ATMOSPHERIC_PRESSURE) {
        m_drumPressure = ATMOSPHERIC_PRESSURE;
    }

    // 4. True Liquid Water Mass Balance
    // Mass Balance: dMass/dt = FeedwaterFlow - SteamFlow - BlowdownFlow
    double blowdownFlow = m_blowdownOpen ? 3.0 : 0.0;
    double netMassFlow = feedwaterFlowMass - steamMassFlow - blowdownFlow;

    // 10 kg/s imbalance results in ~0.4% level change per second
    m_trueLiquidLevel += (netMassFlow * 0.04) * dt;
    m_trueLiquidLevel = std::clamp(m_trueLiquidLevel, 0.0, 100.0);

    // 5. Shrink & Swell Phenomenon (Two-Phase Steam Bubble Volume)
    double steamFlowDelta = steamMassFlow - m_prevSteamFlow;
    m_prevSteamFlow = steamMassFlow;

    m_swellBubbleVolume += (steamFlowDelta * 0.3);
    m_swellBubbleVolume -= (m_swellBubbleVolume * 0.2) * dt;
    m_swellBubbleVolume = std::clamp(m_swellBubbleVolume, -5.0, 5.0);

    // Indicated level seen by DP (Differential Pressure) transmitters
    m_indicatedDrumLevel = m_trueLiquidLevel + m_swellBubbleVolume;
    m_indicatedDrumLevel = std::clamp(m_indicatedDrumLevel, 0.0, 100.0);

    // 6. Superheater & Desuperheater (Attemperator) Temperature
    if (m_drumTemperature > 100.0) {
        // Superheated steam absorbs additional radiant heat up to ~450 - 540°C
        double sprayCooling = (m_sprayValvePos / 100.0) * 80.0;
        double targetSuperheat = m_drumTemperature + 280.0 - sprayCooling;

        m_superheaterTemperature += (targetSuperheat - m_superheaterTemperature) * 0.15 * dt;
    } else {
        m_superheaterTemperature = m_drumTemperature;
    }
}
