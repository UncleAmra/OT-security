/**
 * @file modbus_client.h
 * @brief Custom lightweight Modbus/TCP client definitions.
 */

#ifndef MODBUS_CLIENT_H
#define MODBUS_CLIENT_H

#include "config.h"

/**
 * @brief Initialize socket subsystems (Winsock setup for Windows).
 * @return 1 on success, 0 on failure.
 */
int init_sockets(void);

/**
 * @brief Clean up socket subsystems (Winsock cleanup for Windows).
 */
void cleanup_sockets(void);

/**
 * @brief Establish a connection to the PLC Modbus/TCP server.
 * @param ip IP Address of the PLC
 * @param port Port of the PLC
 * @return A socket identifier, or INVALID_SOCKET on failure.
 */
socket_t connect_to_plc(const char *ip, int port);

/**
 * @brief Read a single Modbus holding register (Function Code 03).
 * @param sock Active network socket
 * @param unit_id Unit Identifier
 * @param reg_addr Starting register address
 * @param out_val Pointer to store the read register value
 * @return 0 on success, negative error code on failure.
 */
int read_holding_register(socket_t sock, uint8_t unit_id, uint16_t reg_addr, uint16_t *out_val);

/**
 * @brief Read multiple contiguous Modbus holding registers (Function Code 03).
 * @param sock Active network socket
 * @param unit_id Unit Identifier
 * @param start_addr Starting register address
 * @param count Number of registers to read
 * @param out_vals Array to store read values (must be at least count in size)
 * @return 0 on success, negative error code on failure.
 */
int read_holding_registers(socket_t sock, uint8_t unit_id, uint16_t start_addr, uint16_t count, uint16_t *out_vals);

/**
 * @brief Write multiple contiguous Modbus holding registers (Function Code 16 / 0x10).
 * @param sock Active network socket
 * @param unit_id Unit Identifier
 * @param start_addr Starting register address
 * @param count Number of registers to write
 * @param values Array containing values to write
 * @return 0 on success, negative error code on failure.
 */
int write_holding_registers(socket_t sock, uint8_t unit_id, uint16_t start_addr, uint16_t count, const uint16_t *values);

#endif /* MODBUS_CLIENT_H */
