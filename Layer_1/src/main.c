/**
 * @file main.c
 * @brief Main execution loop connecting to the PLC, driving the simulation ticks, and reporting.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <signal.h>
#include "config.h"
#include "boiler.h"
#include "turbine.h"
#include "generator.h"
#include "condenser.h"
#include "modbus_client.h"

static volatile int keep_running = 1;

/**
 * @brief Handle Ctrl+C for clean shutdown.
 */
void handle_sigint(int sig) {
    (void)sig;
    keep_running = 0;
}

/**
 * @brief Utility to get formatted local time string.
 */
void get_time_string(char *buf, size_t max_size) {
    time_t raw_time;
    struct tm *time_info;
    time(&raw_time);
    time_info = localtime(&raw_time);
    if (time_info) {
        strftime(buf, max_size, "%Y-%m-%d %H:%M:%S", time_info);
    } else {
        strncpy(buf, "N/A", max_size);
    }
}

int main(int argc, char *argv[]) {
    char plc_ip[256] = DEFAULT_PLC_IP;
    int plc_port = DEFAULT_PLC_PORT;

    // Parse command line arguments
    if (argc >= 2) {
        strncpy(plc_ip, argv[1], sizeof(plc_ip) - 1);
    }
    if (argc >= 3) {
        plc_port = atoi(argv[2]);
    }

    signal(SIGINT, handle_sigint);

    if (!init_sockets()) {
        return EXIT_FAILURE;
    }

    // Initialize all plant components
    Boiler boiler;
    Turbine turbine;
    Generator generator;
    Condenser condenser;

    init_boiler(&boiler);
    init_turbine(&turbine);
    init_generator(&generator);
    init_condenser(&condenser);

    printf("\033[1;36m========================================================================\n");
    printf("        OT POWER PLANT MODULAR PHYSICAL LEVEL DIGITAL TWIN (LAYER 1)     \n");
    printf("========================================================================\033[0m\n");
    printf("[INFO] Tick Rate: %d ms\n", SIM_TICK_MS);
    printf("[INFO] Target: PLC at %s:%d\n\n", plc_ip, plc_port);

    socket_t sock = INVALID_SOCKET;
    double dt = (double)SIM_TICK_MS / 1000.0;
    uint16_t registers[16];
    memset(registers, 0, sizeof(registers));

    // Seeds random number generator for turbine micro-vibrations
    srand((unsigned int)time(NULL));

    while (keep_running) {
        // Handle PLC connection
        if (!is_valid_socket(sock)) {
            sock = connect_to_plc(plc_ip, plc_port);
            if (!is_valid_socket(sock)) {
                fprintf(stderr, "\033[1;33m[RETRY] PLC Offline. Retrying in 3 seconds...\n\033[0m");
#ifdef _WIN32
                Sleep(3000);
#else
                sleep(3);
#endif
                continue;
            }
        }

        // 1. Read holding registers from the PLC to receive operator commands
        // We read all 16 registers to capture states in one transaction
        int status = read_holding_registers(sock, DEFAULT_UNIT_ID, 0, 16, registers);
        if (status < 0) {
            fprintf(stderr, "\033[1;31m[COMMUNICATION ERROR] Read failed from PLC. Reconnecting...\n\033[0m");
            close_socket(sock);
            sock = INVALID_SOCKET;
            continue;
        }

        // 2. Extract inputs from PLC registers and map them to physical states
        boiler.valve_state = (int)registers[REG_BOILER_VALVE];
        boiler.fuel_feed_rate = (double)registers[REG_FUEL_FEED];
        
        turbine.main_steam_valve_state = (int)registers[REG_MAIN_STEAM_VALVE];
        
        generator.breaker_state = (int)registers[REG_GEN_BREAKER];
        
        condenser.cooling_water_flow = (double)registers[REG_COND_COOLING_FLOW];

        // 3. Update the physical components sequentially based on plant material/flow structure
        update_boiler_physics(&boiler, dt);
        update_turbine_physics(&turbine, boiler.pressure, dt);
        update_generator_physics(&generator, turbine.rpm, dt);
        update_condenser_physics(&condenser, turbine.steam_inflow_rate, dt);

        // 4. Update the registers array with computed telemetry data
        registers[REG_BOILER_TEMP]  = (uint16_t)boiler.temperature;
        registers[REG_BOILER_PRESS] = (uint16_t)boiler.pressure;
        registers[REG_BOILER_LEVEL] = (uint16_t)boiler.water_level;
        
        registers[REG_TURBINE_RPM]  = (uint16_t)turbine.rpm;
        registers[REG_STEAM_INFLOW] = (uint16_t)turbine.steam_inflow_rate;
        registers[REG_VIBRATION]    = (uint16_t)(turbine.vibration_level * 10.0); // scaled by 10 for Modbus
        
        registers[REG_GEN_POWER]    = (uint16_t)generator.power_output_mw;
        registers[REG_GEN_FREQ]     = (uint16_t)(generator.frequency_hz * 100.0); // scaled by 100
        registers[REG_GEN_VOLT]     = (uint16_t)(generator.voltage_kv * 10.0);    // scaled by 10
        
        registers[REG_COND_TEMP]    = (uint16_t)condenser.hotwell_temp;
        registers[REG_COND_VACUUM]  = (uint16_t)(condenser.vacuum_pressure * 10.0); // scaled by 10

        // 5. Write computed telemetry registers back to the PLC
        status = write_holding_registers(sock, DEFAULT_UNIT_ID, 0, 16, registers);
        if (status < 0) {
            fprintf(stderr, "\033[1;31m[COMMUNICATION ERROR] Write failed to PLC. Reconnecting...\n\033[0m");
            close_socket(sock);
            sock = INVALID_SOCKET;
            continue;
        }

        // 6. Print clean, structured telemetry screen to the console
        char time_str[32];
        get_time_string(time_str, sizeof(time_str));

        printf("\033[H\033[J"); // Clear console screen (ANSI escape code)
        printf("\033[1;36m========================================================================\n");
        printf(" [%s] POWER PLANT TELEMETRY LOOP\n", time_str);
        printf("========================================================================\033[0m\n");
        
        printf(" \033[1;33mBOILER\033[0m    | Temp: %5.1f °C | Press: %5.1f PSI  | Water: %5.1f %%   | Valve: %s | Fuel: %4.1f kg/min\n",
               boiler.temperature, boiler.pressure, boiler.water_level,
               boiler.valve_state == 1 ? "\033[1;32mOPEN \033[0m" : "\033[1;31mCLOSE\033[0m",
               boiler.fuel_feed_rate);
               
        printf(" \033[1;34mTURBINE\033[0m   | RPM:  %5.0f rpm | Steam: %5.1f %%    | Vibe:  %5.2f mm/s | MSV:   %s\n",
               turbine.rpm, turbine.steam_inflow_rate, turbine.vibration_level,
               turbine.main_steam_valve_state == 1 ? "\033[1;32mOPEN \033[0m" : "\033[1;31mCLOSE\033[0m");
               
        printf(" \033[1;32mGENERATOR\033[0m | Power:%5.1f MW  | Freq:  %5.2f Hz   | Volt:  %5.1f kV   | Brkr:  %s\n",
               generator.power_output_mw, generator.frequency_hz, generator.voltage_kv,
               generator.breaker_state == 1 ? "\033[1;32mCLOSED\033[0m" : "\033[1;31mOPEN  \033[0m");
               
        printf(" \033[1;35mCONDENSER\033[0m | Temp: %5.1f °C | Vacuum:%5.1f inHg | Flow:  %5.1f %%   |\n",
               condenser.hotwell_temp, condenser.vacuum_pressure, condenser.cooling_water_flow);
               
        printf("\033[1;36m========================================================================\033[0m\n");

        // Alarms check
        int critical_incident = 0;
        if (boiler.pressure > 180.0) {
            printf("  \033[1;31m[ALARM] Boiler Pressure critical: %.1f PSI!\033[0m\n", boiler.pressure);
            critical_incident = 1;
        }
        if (boiler.water_level < 20.0) {
            printf("  \033[1;31m[ALARM] Boiler Water Level critical: %.1f %%!\033[0m\n", boiler.water_level);
            critical_incident = 1;
        }
        if (turbine.vibration_level > 4.0) {
            printf("  \033[1;31m[ALARM] Turbine Vibration critical: %.2f mm/s!\033[0m\n", turbine.vibration_level);
            critical_incident = 1;
        }
        if (turbine.rpm > 3750.0) {
            printf("  \033[1;31m[ALARM] Turbine Overspeed critical: %.0f RPM!\033[0m\n", turbine.rpm);
            critical_incident = 1;
        }
        if (condenser.hotwell_temp > 90.0) {
            printf("  \033[1;31m[ALARM] Condenser Temp critical: %.1f °C!\033[0m\n", condenser.hotwell_temp);
            critical_incident = 1;
        }

        if (critical_incident) {
            printf("  \033[1;5;31m[CRITICAL INCIDENT RESPONSE NODE INITIATED]\033[0m\n");
        } else {
            printf("  \033[1;32m[STATUS] All plant components operating within normal safety limits.\033[0m\n");
        }

        // Sleep
#ifdef _WIN32
        Sleep(SIM_TICK_MS);
#else
        usleep(SIM_TICK_MS * 1000);
#endif
    }

    printf("\n[SHUTDOWN] Exiting simulator loop...\n");
    if (is_valid_socket(sock)) {
        close_socket(sock);
    }
    cleanup_sockets();
    printf("[SHUTDOWN] Cleanup finished. Bye!\n");

    return EXIT_SUCCESS;
}
