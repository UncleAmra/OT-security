/**
 * @file generator.c
 * @brief Generator physics calculations.
 */

#include "generator.h"

void init_generator(Generator *generator) {
    if (!generator) return;
    generator->power_output_mw = 0.0;
    generator->frequency_hz = 0.0;
    generator->voltage_kv = 0.0;
    generator->breaker_state = 0;
}

void update_generator_physics(Generator *generator, double turbine_rpm, double dt) {
    if (!generator) return;
    (void)dt; // dt unused in direct algebraic mapping

    if (turbine_rpm > 50.0) {
        // Terminal voltage is proportional to shaft speed (magnetic excitation)
        generator->voltage_kv = (turbine_rpm / 3600.0) * 22.0;
        if (generator->voltage_kv > 25.0) {
            generator->voltage_kv = 25.0; // Voltage limit
        }

        // AC Frequency is determined by shaft speed and grid synchronization
        if (generator->breaker_state == 1) {
            // Breaker Closed (Grid Tied):
            // The generator is synchronized to the massive grid inertia.
            // Grid frequency is locked around 60 Hz with minor deviations based on generator torque
            generator->frequency_hz = 60.0 + (turbine_rpm - 3600.0) * 0.001;
            
            // Power Output:
            // Generator generates power proportional to turbine mechanical power (speed/torque)
            if (turbine_rpm > 1200.0) {
                // Generates up to 500 MW when running at nominal 3600 RPM
                double load_factor = (turbine_rpm - 1200.0) / 2400.0;
                generator->power_output_mw = load_factor * 500.0;
            } else {
                generator->power_output_mw = 0.0;
            }
        } else {
            // Breaker Open (No Load / Synchronizing phase):
            // Generator runs in open-circuit mode. No power output to grid.
            generator->power_output_mw = 0.0;
            
            // Frequency is purely a function of mechanical shaft rotation speed (Freq = RPM / 60)
            generator->frequency_hz = turbine_rpm / 60.0;
        }
    } else {
        // Generator is stationary
        generator->power_output_mw = 0.0;
        generator->frequency_hz = 0.0;
        generator->voltage_kv = 0.0;
    }

    // Safety checks
    if (generator->power_output_mw < 0.0) generator->power_output_mw = 0.0;
    if (generator->frequency_hz < 0.0) generator->frequency_hz = 0.0;
    if (generator->voltage_kv < 0.0) generator->voltage_kv = 0.0;
}
