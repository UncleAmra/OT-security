/**
 * @file config.h
 * @brief Global configuration, Modbus register mappings, and simulation tick rate.
 */

#ifndef CONFIG_H
#define CONFIG_H

#include <stdint.h>

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "ws2_32.lib")
    typedef SOCKET socket_t;
    #define close_socket(s) closesocket(s)
    #define is_valid_socket(s) ((s) != INVALID_SOCKET)
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    typedef int socket_t;
    #define close_socket(s) close(s)
    #define is_valid_socket(s) ((s) >= 0)
    #define INVALID_SOCKET (-1)
    #define SOCKET_ERROR (-1)
#endif

/* PLC Modbus TCP Connection Details */
#define DEFAULT_PLC_IP "127.0.0.1"
#define DEFAULT_PLC_PORT 5020
#define DEFAULT_UNIT_ID 1

/* Modbus Registers Mapping */
#define REG_BOILER_TEMP       0   /* Write: Boiler Temperature (°C) */
#define REG_BOILER_PRESS      1   /* Write: Boiler Pressure (PSI) */
#define REG_BOILER_LEVEL      2   /* Write: Boiler Water Level (%) */
#define REG_BOILER_VALVE      3   /* Read: Boiler Relief Valve State (0=Closed, 1=Open) */
#define REG_FUEL_FEED         4   /* Read: Fuel Feed Rate (kg/min) */

#define REG_TURBINE_RPM       5   /* Write: Turbine RPM (0-3600) */
#define REG_STEAM_INFLOW      6   /* Write: Steam Inflow Rate (%) */
#define REG_VIBRATION         7   /* Write: Turbine Vibration Level (mm/s * 10) */
#define REG_MAIN_STEAM_VALVE  8   /* Read: Main Steam Valve State (0=Closed, 1=Open) */

#define REG_GEN_POWER         9   /* Write: Generator Power Output (MW) */
#define REG_GEN_FREQ          10  /* Write: Grid Frequency (Hz * 100) */
#define REG_GEN_VOLT          11  /* Write: Generator Voltage (kV * 10) */
#define REG_GEN_BREAKER       12  /* Read: Generator Breaker State (0=Open, 1=Closed) */

#define REG_COND_TEMP         13  /* Write: Hotwell Temperature (°C) */
#define REG_COND_VACUUM       14  /* Write: Condenser Vacuum Pressure (inHg * 10) */
#define REG_COND_COOLING_FLOW 15  /* Read: Cooling Water Flow (%) */

/* Total number of registers to read (starting at REG_BOILER_VALVE / index 3, up to 15) */
#define MODBUS_START_READ_ADDR 3
#define MODBUS_READ_COUNT      13

/* Simulation Configuration */
#define SIM_TICK_MS 1000
#define AMBIENT_TEMP 25.0
#define ATMOSPHERIC_PRESSURE 14.7

#endif /* CONFIG_H */
