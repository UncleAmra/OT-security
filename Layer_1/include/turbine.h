/**
 * @file turbine.h
 * @brief Turbine simulation properties and rotational dynamics.
 */

#ifndef TURBINE_H
#define TURBINE_H

#include "config.h"

typedef struct {
    double rpm;                     /* Turbine speed (0-3600 RPM) */
    double steam_inflow_rate;       /* Steam Inflow Rate (%) */
    double vibration_level;         /* Vibration Level (mm/s) */
    int main_steam_valve_state;     /* Main Steam Valve State (0=Closed, 1=Open) */
} Turbine;

/**
 * @brief Initialize Turbine state.
 */
void init_turbine(Turbine *turbine);

/**
 * @brief Update Turbine physical simulation.
 * @param turbine Pointer to Turbine state
 * @param boiler_pressure Current boiler pressure (PSI)
 * @param dt Time step (seconds)
 */
void update_turbine_physics(Turbine *turbine, double boiler_pressure, double dt);

#endif /* TURBINE_H */
