/**
 * @file condenser.h
 * @brief Condenser simulation properties and cooling water loop.
 */

#ifndef CONDENSER_H
#define CONDENSER_H

#include "config.h"

typedef struct {
    double hotwell_temp;            /* Hotwell condensate temperature (°C) */
    double vacuum_pressure;         /* Condenser vacuum pressure (inHg, 0-30) */
    double cooling_water_flow;      /* Cooling water flow rate (%) */
} Condenser;

/**
 * @brief Initialize Condenser state.
 */
void init_condenser(Condenser *condenser);

/**
 * @brief Update Condenser physical simulation.
 * @param condenser Pointer to Condenser state
 * @param steam_inflow_rate Current turbine steam inflow rate (%)
 * @param dt Time step (seconds)
 */
void update_condenser_physics(Condenser *condenser, double steam_inflow_rate, double dt);

#endif /* CONDENSER_H */
