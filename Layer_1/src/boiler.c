/**
 * @file boiler.c
 * @brief Boiler physics calculations.
 */

#include "boiler.h"
#include <math.h>

void init_boiler(Boiler *boiler) {
    if (!boiler) return;
    boiler->temperature = AMBIENT_TEMP;
    boiler->pressure = ATMOSPHERIC_PRESSURE;
    boiler->water_level = 100.0;
    boiler->valve_state = 0;
    boiler->fuel_feed_rate = 0.0;
}

void update_boiler_physics(Boiler *boiler, double dt) {
    if (!boiler) return;

    // 1. Temperature Physics
    // Burner heating rate is proportional to fuel feed rate (e.g. max feed of 100 kg/min -> ~15 °C/s increase)
    double heating = boiler->fuel_feed_rate * 0.15;
    
    // Convective heat loss to environment
    double heat_loss = 0.005 * (boiler->temperature - AMBIENT_TEMP);
    
    // Steam release cooling (latent heat of vaporization)
    double steam_cooling = 0.0;
    if (boiler->valve_state == 1) {
        steam_cooling = 12.0 * (boiler->temperature - AMBIENT_TEMP) * 0.1;
        if (steam_cooling < 12.0) {
            steam_cooling = 12.0; // Baseline venting cooling
        }
    }

    double dT = heating - heat_loss - steam_cooling;
    boiler->temperature += dT * dt;
    if (boiler->temperature < AMBIENT_TEMP) {
        boiler->temperature = AMBIENT_TEMP;
    }

    // 2. Water Level & Steam Generation
    double steam_gen = 0.0;
    if (boiler->temperature > 100.0) {
        // Steam generation rate increases with temperature above boiling point
        steam_gen = (boiler->temperature - 100.0) * 0.008;
    }
    
    // Water boiling off reduces water level
    boiler->water_level -= steam_gen * dt;

    // Feedwater pump automatically tries to maintain water level at 100%
    if (boiler->water_level < 100.0) {
        double pump_rate = 0.4; // 0.4% per second
        boiler->water_level += pump_rate * dt;
    }
    
    if (boiler->water_level > 100.0) {
        boiler->water_level = 100.0;
    } else if (boiler->water_level < 0.0) {
        boiler->water_level = 0.0;
    }

    // 3. Boiler Pressure Physics
    if (boiler->valve_state == 1) {
        // Relief valve open: pressure vents rapidly
        double dP = -3.0 * (boiler->pressure - ATMOSPHERIC_PRESSURE);
        boiler->pressure += dP * dt;
    } else {
        // Relief valve closed: pressure builds up proportional to temperature above 100°C
        double target_pressure = ATMOSPHERIC_PRESSURE;
        if (boiler->temperature > 100.0) {
            // Dry/saturated steam pressure curve approximation:
            target_pressure += (boiler->temperature - 100.0) * 1.35;
        }
        
        // Pressure dynamics with inertia
        double dP = (target_pressure - boiler->pressure) * 0.15;
        boiler->pressure += dP * dt;
    }

    if (boiler->pressure < ATMOSPHERIC_PRESSURE) {
        boiler->pressure = ATMOSPHERIC_PRESSURE;
    }
}
