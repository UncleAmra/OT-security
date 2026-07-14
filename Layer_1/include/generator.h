/**
 * @file generator.h
 * @brief Generator simulation properties and electrical dynamics.
 */

#ifndef GENERATOR_H
#define GENERATOR_H

#include "config.h"

typedef struct {
    double power_output_mw;         /* Grid power output (0-500 MW) */
    double frequency_hz;            /* AC grid frequency (0-60 Hz) */
    double voltage_kv;              /* Output terminal voltage (0-22 kV) */
    int breaker_state;              /* Grid sync breaker state (0=Open, 1=Closed) */
} Generator;

/**
 * @brief Initialize Generator state.
 */
void init_generator(Generator *generator);

/**
 * @brief Update Generator physical simulation.
 * @param generator Pointer to Generator state
 * @param turbine_rpm Current turbine RPM
 * @param dt Time step (seconds)
 */
void update_generator_physics(Generator *generator, double turbine_rpm, double dt);

#endif /* GENERATOR_H */
