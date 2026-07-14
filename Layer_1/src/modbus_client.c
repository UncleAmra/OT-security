/**
 * @file modbus_client.c
 * @brief Socket and Modbus TCP network functions.
 */

#include "modbus_client.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef _WIN32
    #include <sys/time.h>
#endif

int init_sockets(void) {
#ifdef _WIN32
    WSADATA wsaData;
    int res = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (res != 0) {
        fprintf(stderr, "\033[1;31m[ERROR] WSAStartup failed: %d\n\033[0m", res);
        return 0;
    }
#endif
    return 1;
}

void cleanup_sockets(void) {
#ifdef _WIN32
    WSACleanup();
#endif
}

socket_t connect_to_plc(const char *ip, int port) {
    socket_t sock = socket(AF_INET, SOCK_STREAM, 0);
    if (!is_valid_socket(sock)) {
        perror("\033[1;31m[ERROR] Socket creation failed\033[0m");
        return INVALID_SOCKET;
    }

    struct sockaddr_in plc_addr;
    memset(&plc_addr, 0, sizeof(plc_addr));
    plc_addr.sin_family = AF_INET;
    plc_addr.sin_port = htons(port);

#ifdef _WIN32
    plc_addr.sin_addr.s_addr = inet_addr(ip);
    if (plc_addr.sin_addr.s_addr == INADDR_NONE) {
        struct addrinfo hints, *res;
        memset(&hints, 0, sizeof(hints));
        hints.ai_family = AF_INET;
        char port_str[16];
        snprintf(port_str, sizeof(port_str), "%d", port);
        if (getaddrinfo(ip, port_str, &hints, &res) == 0) {
            memcpy(&plc_addr, res->ai_addr, sizeof(struct sockaddr_in));
            freeaddrinfo(res);
        } else {
            close_socket(sock);
            return INVALID_SOCKET;
        }
    }
#else
    if (inet_pton(AF_INET, ip, &plc_addr.sin_addr) <= 0) {
        fprintf(stderr, "\033[1;31m[ERROR] Invalid address format: %s\n\033[0m", ip);
        close_socket(sock);
        return INVALID_SOCKET;
    }
#endif

    // Set connection timeouts (3 seconds)
#ifdef _WIN32
    DWORD timeout = 3000;
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (const char*)&timeout, sizeof(timeout));
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout));
#else
    struct timeval timeout;
    timeout.tv_sec = 3;
    timeout.tv_usec = 0;
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (const void*)&timeout, sizeof(timeout));
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const void*)&timeout, sizeof(timeout));
#endif

    printf("[CONNECTING] Connecting to PLC at %s:%d...\n", ip, port);
    if (connect(sock, (struct sockaddr *)&plc_addr, sizeof(plc_addr)) < 0) {
        close_socket(sock);
        return INVALID_SOCKET;
    }

    printf("\033[1;32m[CONNECTED] Connected to PLC.\n\033[0m");
    return sock;
}

/**
 * @brief Helper to read exact number of bytes from socket.
 */
static int recv_all(socket_t sock, uint8_t *buf, int len) {
    int total = 0;
    while (total < len) {
        int n = recv(sock, (char *)(buf + total), len - total, 0);
        if (n <= 0) {
            return -1;
        }
        total += n;
    }
    return total;
}

int read_holding_register(socket_t sock, uint8_t unit_id, uint16_t reg_addr, uint16_t *out_val) {
    return read_holding_registers(sock, unit_id, reg_addr, 1, out_val);
}

int read_holding_registers(socket_t sock, uint8_t unit_id, uint16_t start_addr, uint16_t count, uint16_t *out_vals) {
    static uint16_t trans_id = 0x3000;
    trans_id++;

    // Modbus TCP request ADU (12 bytes)
    uint8_t req[12] = {
        (uint8_t)((trans_id >> 8) & 0xFF), (uint8_t)(trans_id & 0xFF), // Transaction ID
        0x00, 0x00,                                                    // Protocol ID
        0x00, 0x06,                                                    // Length (6 bytes follow)
        unit_id,                                                       // Unit ID
        0x03,                                                          // Function Code: Read Holding Registers
        (uint8_t)((start_addr >> 8) & 0xFF), (uint8_t)(start_addr & 0xFF),
        (uint8_t)((count >> 8) & 0xFF), (uint8_t)(count & 0xFF)
    };

    if (send(sock, (const char *)req, sizeof(req), 0) < 0) {
        return -1;
    }

    // Read MBAP Header (7 bytes)
    uint8_t rsp_header[7];
    if (recv_all(sock, rsp_header, 7) < 0) {
        return -1;
    }

    uint16_t resp_trans_id = (rsp_header[0] << 8) | rsp_header[1];
    if (resp_trans_id != trans_id) {
        fprintf(stderr, "[WARNING] Modbus Transaction ID mismatch: sent 0x%04X, got 0x%04X\n", trans_id, resp_trans_id);
    }

    uint16_t pdu_len = (rsp_header[4] << 8) | rsp_header[5];
    if (pdu_len < 3 || pdu_len > 256) {
        return -1;
    }

    // Read PDU response (Unit ID + Function + Byte Count + Data)
    uint8_t rsp_pdu[256];
    if (recv_all(sock, rsp_pdu, pdu_len) < 0) {
        return -1;
    }

    // Check if exception response
    if (rsp_pdu[1] == (0x03 | 0x80)) {
        fprintf(stderr, "\033[1;31m[MODBUS EXCEPTION] Read registers failed, exception code: 0x%02X\n\033[0m", rsp_pdu[2]);
        return -2;
    }
    if (rsp_pdu[1] != 0x03) {
        return -1;
    }

    uint8_t expected_bytes = count * 2;
    if (rsp_pdu[2] != expected_bytes) {
        return -1;
    }

    // Decode register values (Big Endian to Host)
    for (int i = 0; i < count; i++) {
        out_vals[i] = (rsp_pdu[3 + (i * 2)] << 8) | rsp_pdu[4 + (i * 2)];
    }

    return 0;
}

int write_holding_registers(socket_t sock, uint8_t unit_id, uint16_t start_addr, uint16_t count, const uint16_t *values) {
    static uint16_t trans_id = 0x4000;
    trans_id++;

    int pdu_len = 1 + 2 + 2 + 1 + (count * 2); // Function Code (1) + Start Addr (2) + Quant (2) + Byte Count (1) + Data (count*2)
    int following_len = 1 + pdu_len;           // Unit ID + PDU

    uint8_t req[256];
    req[0] = (uint8_t)((trans_id >> 8) & 0xFF);
    req[1] = (uint8_t)(trans_id & 0xFF);
    req[2] = 0x00;
    req[3] = 0x00;
    req[4] = (uint8_t)((following_len >> 8) & 0xFF);
    req[5] = (uint8_t)(following_len & 0xFF);
    req[6] = unit_id;
    req[7] = 0x10; // Function Code: Write Multiple Registers
    req[8] = (uint8_t)((start_addr >> 8) & 0xFF);
    req[9] = (uint8_t)(start_addr & 0xFF);
    req[10] = (uint8_t)((count >> 8) & 0xFF);
    req[11] = (uint8_t)(count & 0xFF);
    req[12] = (uint8_t)(count * 2);

    for (int i = 0; i < count; i++) {
        req[13 + (i * 2)] = (uint8_t)((values[i] >> 8) & 0xFF);
        req[14 + (i * 2)] = (uint8_t)(values[i] & 0xFF);
    }

    int req_size = 13 + (count * 2);
    if (send(sock, (const char *)req, req_size, 0) < 0) {
        return -1;
    }

    // Read MBAP Header (7 bytes)
    uint8_t rsp_header[7];
    if (recv_all(sock, rsp_header, 7) < 0) {
        return -1;
    }

    uint16_t resp_trans_id = (rsp_header[0] << 8) | rsp_header[1];
    if (resp_trans_id != trans_id) {
        fprintf(stderr, "[WARNING] Modbus Transaction ID mismatch: sent 0x%04X, got 0x%04X\n", trans_id, resp_trans_id);
    }

    uint16_t resp_pdu_len = (rsp_header[4] << 8) | rsp_header[5];
    if (resp_pdu_len < 3 || resp_pdu_len > 256) {
        return -1;
    }

    // Read PDU response (6 bytes: Unit ID + Func + StartAddr + Count)
    uint8_t rsp_pdu[256];
    if (recv_all(sock, rsp_pdu, resp_pdu_len) < 0) {
        return -1;
    }

    if (rsp_pdu[1] == (0x10 | 0x80)) {
        fprintf(stderr, "\033[1;31m[MODBUS EXCEPTION] Write registers failed, exception code: 0x%02X\n\033[0m", rsp_pdu[2]);
        return -2;
    }
    if (rsp_pdu[1] != 0x10) {
        return -1;
    }

    return 0;
}
