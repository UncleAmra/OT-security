/**
 * @file condenser.c
 * @brief Condenser cooling calculations.
 */

#include "condenser.h"

void init_condenser(Condenser *condenser) {
    if (!condenser) return;
    condenser->hotwell_temp = AMBIENT_TEMP;
    condenser->vacuum_pressure = 0.0; // Starts at atmospheric pressure (0 inHg vacuum)
    condenser->cooling_water_flow = 0.0;
}

void update_condenser_physics(Condenser *condenser, double steam_inflow_rate, double dt) {
    if (!condenser) return;

    // 1. Hotwell Temperature Dynamics
    // Steam exhaust from the turbine adds heat
    double heat_added = steam_inflow_rate * 0.35; // °C/s equivalence
    
    // Cooling water inlet temperature is assumed to be 20.0 °C
    double cooling_inlet_temp = 20.0;
    
    // Heat removal is proportional to cooling water flow and temperature differential
    double heat_removed = (condenser->cooling_water_flow / 100.0) * 0.08 * (condenser->hotwell_temp - cooling_inlet_temp);
    
    // Natural ambient heat loss
    double ambient_loss = 0.01 * (condenser->hotwell_temp - AMBIENT_TEMP);

    double dT = heat_added - heat_removed - ambient_loss;
    condenser->hotwell_temp += dT * dt;
    
    if (condenser->hotwell_temp < AMBIENT_TEMP) {
        condenser->hotwell_temp = AMBIENT_TEMP;
    }

    // 2. Vacuum Pressure Dynamics (inHg)
    // In a condenser, lower hotwell temperature creates a better vacuum (closer to 30 inHg)
    // If cooling water flow is 0, steam accumulates, destroying the vacuum (approaching 0 inHg)
    double target_vacuum = 0.0;
    if (condenser->cooling_water_flow > 5.0) {
        // Ideal vacuum is 30 inHg. High temperature limits the maximum achievable vacuum.
        target_vacuum = 30.0 - (condenser->hotwell_temp - 20.0) * 0.32;
    } else {
        // Without cooling flow, condensation stops and pressure builds back to atmospheric (0 inHg vacuum)
        target_vacuum = 0.0;
    }

    // Apply vacuum dynamics/lag
    double dV = (target_vacuum - condenser->vacuum_pressure) * 0.12;
    condenser->vacuum_pressure += dV * dt;

    // Boundary constraints (0.0 is atmospheric pressure, 30.0 is perfect vacuum)
    if (condenser->vacuum_pressure < 0.0) {
        condenser->vacuum_pressure = 0.0;
    } else if (condenser->vacuum_pressure > 30.0) {
        condenser->vacuum_pressure = 30.0;
    }
}
