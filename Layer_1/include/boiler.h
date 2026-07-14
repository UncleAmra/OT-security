/**
 * @file boiler.h
 * @brief Boiler simulation properties and thermodynamics.
 */

#ifndef BOILER_H
#define BOILER_H

#include "config.h"

typedef struct {
    double temperature;       /* Boiler Temperature (°C) */
    double pressure;          /* Boiler Internal Pressure (PSI) */
    double water_level;       /* Water Level (%) */
    int valve_state;          /* Relief Valve State (0=Closed, 1=Open) */
    double fuel_feed_rate;    /* Fuel Feed Rate (kg/min) */
} Boiler;

/**
 * @brief Initialize Boiler state.
 */
void init_boiler(Boiler *boiler);

/**
 * @brief Update Boiler physical simulation.
 * @param boiler Pointer to Boiler state
 * @param dt Time step (seconds)
 */
void update_boiler_physics(Boiler *boiler, double dt);

#endif /* BOILER_H */
