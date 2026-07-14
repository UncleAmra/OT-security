/**
 * @file turbine.c
 * @brief Turbine physics calculations.
 */

#include "turbine.h"
#include <stdlib.h>
#include <math.h>

void init_turbine(Turbine *turbine) {
    if (!turbine) return;
    turbine->rpm = 0.0;
    turbine->steam_inflow_rate = 0.0;
    turbine->vibration_level = 0.0;
    turbine->main_steam_valve_state = 0;
}

void update_turbine_physics(Turbine *turbine, double boiler_pressure, double dt) {
    if (!turbine) return;

    // 1. Calculate Steam Inflow Rate
    if (turbine->main_steam_valve_state == 1) {
        // Steam inflow is proportional to boiler pressure above atmospheric
        double press_diff = boiler_pressure - ATMOSPHERIC_PRESSURE;
        if (press_diff < 0.0) press_diff = 0.0;
        
        // 100 PSI over atmosphere translates to 100% steam inflow rate
        turbine->steam_inflow_rate = (press_diff / 100.0) * 100.0;
        if (turbine->steam_inflow_rate > 100.0) {
            turbine->steam_inflow_rate = 100.0;
        }
    } else {
        // Valve is closed: no steam flows into the turbine
        turbine->steam_inflow_rate = 0.0;
    }

    // 2. RPM Dynamics (steam torque vs. friction and grid load)
    if (turbine->steam_inflow_rate > 0.0) {
        // Target RPM is proportional to steam inflow rate (max 3600 RPM at 100% steam)
        double target_rpm = (turbine->steam_inflow_rate / 100.0) * 3600.0;
        
        // Dynamic spin-up with rotational inertia
        double spin_up_coeff = 0.08; 
        turbine->rpm += (target_rpm - turbine->rpm) * spin_up_coeff * dt;
    } else {
        // Coast down due to mechanical drag/friction
        double friction_coeff = 0.02; // slow coast down
        turbine->rpm -= (turbine->rpm * friction_coeff) * dt;
    }

    // Ensure bounds
    if (turbine->rpm < 0.0) {
        turbine->rpm = 0.0;
    } else if (turbine->rpm > 4000.0) { // Capped at absolute structural failure limit
        turbine->rpm = 4000.0;
    }

    // 3. Vibration Level Calculations
    if (turbine->rpm > 10.0) {
        // Base vibration climbs non-linearly with rotational speed
        double normalized_speed = turbine->rpm / 3600.0;
        double base_vibration = normalized_speed * normalized_speed * 3.5;
        
        // Resonance band around 1800 RPM (first critical speed)
        double resonance = 0.0;
        if (turbine->rpm > 1700.0 && turbine->rpm < 1900.0) {
            // High vibration peak at resonance
            double diff = fabs(turbine->rpm - 1800.0);
            resonance = (100.0 - diff) * 0.02; 
        }

        // Overspeed vibration warning (extreme speeds cause structural shaking)
        double overspeed_vibe = 0.0;
        if (turbine->rpm > 3600.0) {
            overspeed_vibe = (turbine->rpm - 3600.0) * 0.08;
        }

        // Small white-noise fluctuation (micro-vibration)
        double noise = ((double)(rand() % 200 - 100) / 1000.0); // -0.1 to 0.1 mm/s

        turbine->vibration_level = base_vibration + resonance + overspeed_vibe + noise;
    } else {
        turbine->vibration_level = 0.0;
    }

    if (turbine->vibration_level < 0.0) {
        turbine->vibration_level = 0.0;
    }
}
