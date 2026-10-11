#include "../include/CondenserFeedwaterSystem.hpp"
#include <algorithm>
#include <cmath>

CondenserFeedwaterSystem::CondenserFeedwaterSystem() {
    reset();
}

void CondenserFeedwaterSystem::reset() {
    m_hotwellTemp = 28.0;
    m_vacuumInHg = 28.5; // Nominal operating vacuum (28.5 inHg)
    m_circWaterFlow = 70.0;
    m_circWaterInletTemp = 18.0;
    m_feedwaterMassFlow = 0.0;
    m_deaeratorPressure = 1.2;
    m_deaeratorLevel = 60.0;
    m_cepRunning = true;
}

void CondenserFeedwaterSystem::step(double steamExhaustMassFlow, double circWaterFlow, 
                                     double bfpASpeed, double bfpBSpeed, bool cepRunning, double dt) {
    m_circWaterFlow = std::clamp(circWaterFlow, 0.0, 100.0);
    bfpASpeed = std::clamp(bfpASpeed, 0.0, 100.0);
    bfpBSpeed = std::clamp(bfpBSpeed, 0.0, 100.0);
    m_cepRunning = cepRunning;

    // 1. Hotwell Temperature Dynamics
    // Steam condensing into hotwell transfers latent heat
    double heatAdded = steamExhaustMassFlow * 0.42; 
    // Cooling water circulating through tube bundle absorbs heat
    double heatRemoved = (m_circWaterFlow / 100.0) * 0.12 * (m_hotwellTemp - m_circWaterInletTemp);
    double ambientLoss = 0.005 * (m_hotwellTemp - 25.0);

    m_hotwellTemp += (heatAdded - heatRemoved - ambientLoss) * dt;
    m_hotwellTemp = std::clamp(m_hotwellTemp, 20.0, 105.0);

    // 2. Condenser Vacuum Dynamics (inHg)
    // Steam condensation creates deep vacuum (approaching 28.5 - 29.5 inHg)
    double targetVacuum = 0.0;
    if (m_circWaterFlow > 10.0) {
        // Higher hotwell temperature worsens vacuum
        targetVacuum = 29.5 - (m_hotwellTemp - 20.0) * 0.22;
        targetVacuum = std::clamp(targetVacuum, 0.0, 29.8);
    } else {
        // Loss of circulating water causes loss of vacuum back to atmospheric (0 inHg)
        targetVacuum = 0.0;
    }

    m_vacuumInHg += (targetVacuum - m_vacuumInHg) * 0.15 * dt;
    m_vacuumInHg = std::clamp(m_vacuumInHg, 0.0, 30.0);

    // 3. Boiler Feedwater Pumps (BFP A and B)
    // Redundant high-pressure feed pumps delivering kg/s to boiler
    double bfpMaxFlow = 90.0; // kg/s per pump
    double flowPumpA = (bfpASpeed / 100.0) * bfpMaxFlow;
    double flowPumpB = (bfpBSpeed / 100.0) * bfpMaxFlow;

    m_feedwaterMassFlow = flowPumpA + flowPumpB;

    // 4. Deaerator Mass and Pressure
    // Mass in from condensate pump (CEP), mass out to BFP
    double cepFlow = m_cepRunning ? (m_circWaterFlow > 10.0 ? steamExhaustMassFlow : 0.0) : 0.0;
    double netDeaeratorFlow = cepFlow - m_feedwaterMassFlow;
    m_deaeratorLevel += (netDeaeratorFlow * 0.12) * dt;
    m_deaeratorLevel = std::clamp(m_deaeratorLevel, 0.0, 100.0);

    // Deaerator pressure heated by turbine extraction steam
    m_deaeratorPressure = 1.0 + (m_hotwellTemp / 100.0) * 3.5;
}
