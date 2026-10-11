#ifndef MODBUS_PROTOCOL_HPP
#define MODBUS_PROTOCOL_HPP

#include <cstdint>
#include <vector>
#include <cstring>
#include <iostream>
#include "SocketUtils.hpp"

/**
 * ==============================================================================
 *  LIGHTWEIGHT ROBUST MODBUS/TCP PROTOCOL ENGINE (C++17/C++20)
 * ==============================================================================
 * Encapsulates MBAP (Modbus Application Protocol) header and PDU serialization
 * for both Client (Master) and Server (Slave) modes.
 * Zero external library dependencies.
 * ==============================================================================
 */

namespace Modbus {

    // Standard Modbus Function Codes
    constexpr uint8_t FC_READ_COILS                 = 0x01;
    constexpr uint8_t FC_READ_DISCRETE_INPUTS       = 0x02;
    constexpr uint8_t FC_READ_HOLDING_REGISTERS     = 0x03;
    constexpr uint8_t FC_READ_INPUT_REGISTERS       = 0x04;
    constexpr uint8_t FC_WRITE_SINGLE_COIL          = 0x05;
    constexpr uint8_t FC_WRITE_SINGLE_REGISTER      = 0x06;
    constexpr uint8_t FC_WRITE_MULTIPLE_COILS       = 0x0F; // 15
    constexpr uint8_t FC_WRITE_MULTIPLE_REGISTERS   = 0x10; // 16

    // Exception Codes
    constexpr uint8_t EXCEPTION_ILLEGAL_FUNCTION    = 0x01;
    constexpr uint8_t EXCEPTION_ILLEGAL_DATA_ADDR   = 0x02;
    constexpr uint8_t EXCEPTION_ILLEGAL_DATA_VAL    = 0x03;
    constexpr uint8_t EXCEPTION_SERVER_DEVICE_BUSY  = 0x06;

    #pragma pack(push, 1)
    struct MbapHeader {
        uint16_t transactionId; // Big-endian
        uint16_t protocolId;    // 0x0000 for Modbus TCP
        uint16_t length;        // Number of following bytes (unitId + PDU)
        uint8_t  unitId;        // Unit identifier (1 - 247)
    };
    #pragma pack(pop)

    inline uint16_t toBigEndian(uint16_t val) {
        return (val >> 8) | (val << 8);
    }

    inline uint16_t fromBigEndian(uint16_t val) {
        return (val >> 8) | (val << 8);
    }

    /**
     * @brief Modbus/TCP Client Helper for reading & writing holding registers.
     */
    class ModbusClient {
    public:
        ModbusClient() : m_transactionId(0x1000) {}

        /**
         * @brief Read Holding Registers (Function Code 03)
         */
        bool readHoldingRegisters(NetUtils::TcpClient &client, uint8_t unitId, uint16_t startAddr, 
                                  uint16_t count, std::vector<uint16_t> &outRegisters) {
            if (!client.isConnected() || count == 0 || count > 125) return false;

            m_transactionId++;
            uint16_t currentTransId = m_transactionId;

            uint8_t req[12];
            req[0] = static_cast<uint8_t>((currentTransId >> 8) & 0xFF);
            req[1] = static_cast<uint8_t>(currentTransId & 0xFF);
            req[2] = 0x00; // Protocol ID high
            req[3] = 0x00; // Protocol ID low
            req[4] = 0x00; // Length high (6 bytes follow)
            req[5] = 0x06; // Length low
            req[6] = unitId;
            req[7] = FC_READ_HOLDING_REGISTERS;
            req[8] = static_cast<uint8_t>((startAddr >> 8) & 0xFF);
            req[9] = static_cast<uint8_t>(startAddr & 0xFF);
            req[10] = static_cast<uint8_t>((count >> 8) & 0xFF);
            req[11] = static_cast<uint8_t>(count & 0xFF);

            if (!client.sendData(req, sizeof(req))) {
                return false;
            }

            // Read MBAP Header (7 bytes: TransID[2], ProtoID[2], Length[2], UnitID[1])
            uint8_t mbap[7];
            if (!client.receiveData(mbap, 7)) {
                return false;
            }

            uint16_t len = (static_cast<uint16_t>(mbap[4]) << 8) | mbap[5];
            if (len < 2 || len > 260) return false;

            uint16_t pduLen = len - 1; // Remaining bytes for PDU after Unit ID
            std::vector<uint8_t> pdu(pduLen);
            if (!client.receiveData(pdu.data(), pduLen)) {
                return false;
            }

            // Check for exception response (FC | 0x80)
            if (pdu[0] == (FC_READ_HOLDING_REGISTERS | 0x80)) {
                std::cerr << "[MODBUS] Read exception received: 0x" << std::hex << static_cast<int>(pdu[1]) << std::dec << std::endl;
                return false;
            }

            if (pdu[0] != FC_READ_HOLDING_REGISTERS) return false;

            uint8_t byteCount = pdu[1];
            if (byteCount != count * 2) return false;

            outRegisters.resize(count);
            for (size_t i = 0; i < count; ++i) {
                outRegisters[i] = (static_cast<uint16_t>(pdu[2 + (i * 2)]) << 8) | pdu[3 + (i * 2)];
            }

            return true;
        }

        /**
         * @brief Write Single Holding Register (Function Code 06)
         */
        bool writeSingleRegister(NetUtils::TcpClient &client, uint8_t unitId, uint16_t regAddr, uint16_t value) {
            if (!client.isConnected()) return false;

            m_transactionId++;
            uint16_t currentTransId = m_transactionId;

            uint8_t req[12];
            req[0] = static_cast<uint8_t>((currentTransId >> 8) & 0xFF);
            req[1] = static_cast<uint8_t>(currentTransId & 0xFF);
            req[2] = 0x00;
            req[3] = 0x00;
            req[4] = 0x00;
            req[5] = 0x06;
            req[6] = unitId;
            req[7] = FC_WRITE_SINGLE_REGISTER;
            req[8] = static_cast<uint8_t>((regAddr >> 8) & 0xFF);
            req[9] = static_cast<uint8_t>(regAddr & 0xFF);
            req[10] = static_cast<uint8_t>((value >> 8) & 0xFF);
            req[11] = static_cast<uint8_t>(value & 0xFF);

            if (!client.sendData(req, sizeof(req))) return false;

            uint8_t mbap[7];
            if (!client.receiveData(mbap, 7)) return false;

            uint16_t len = (static_cast<uint16_t>(mbap[4]) << 8) | mbap[5];
            if (len < 2 || len > 260) return false;

            uint16_t pduLen = len - 1;
            std::vector<uint8_t> pdu(pduLen);
            if (!client.receiveData(pdu.data(), pduLen)) return false;

            return (pdu[0] == FC_WRITE_SINGLE_REGISTER);
        }

        /**
         * @brief Write Multiple Holding Registers (Function Code 16 / 0x10)
         */
        bool writeMultipleRegisters(NetUtils::TcpClient &client, uint8_t unitId, uint16_t startAddr, 
                                    const std::vector<uint16_t> &values) {
            if (!client.isConnected() || values.empty() || values.size() > 120) return false;

            uint16_t count = static_cast<uint16_t>(values.size());
            uint16_t byteCount = count * 2;
            uint16_t followingLen = 1 + 1 + 2 + 2 + 1 + byteCount; // unitId + FC + addr + count + byteCount + bytes

            m_transactionId++;
            uint16_t currentTransId = m_transactionId;

            std::vector<uint8_t> req(6 + followingLen);
            req[0] = static_cast<uint8_t>((currentTransId >> 8) & 0xFF);
            req[1] = static_cast<uint8_t>(currentTransId & 0xFF);
            req[2] = 0x00;
            req[3] = 0x00;
            req[4] = static_cast<uint8_t>((followingLen >> 8) & 0xFF);
            req[5] = static_cast<uint8_t>(followingLen & 0xFF);
            req[6] = unitId;
            req[7] = FC_WRITE_MULTIPLE_REGISTERS;
            req[8] = static_cast<uint8_t>((startAddr >> 8) & 0xFF);
            req[9] = static_cast<uint8_t>(startAddr & 0xFF);
            req[10] = static_cast<uint8_t>((count >> 8) & 0xFF);
            req[11] = static_cast<uint8_t>(count & 0xFF);
            req[12] = static_cast<uint8_t>(byteCount);

            for (size_t i = 0; i < count; ++i) {
                req[13 + (i * 2)] = static_cast<uint8_t>((values[i] >> 8) & 0xFF);
                req[14 + (i * 2)] = static_cast<uint8_t>(values[i] & 0xFF);
            }

            if (!client.sendData(req.data(), static_cast<int>(req.size()))) return false;

            uint8_t mbap[7];
            if (!client.receiveData(mbap, 7)) return false;

            uint16_t len = (static_cast<uint16_t>(mbap[4]) << 8) | mbap[5];
            if (len < 2 || len > 260) return false;

            uint16_t pduLen = len - 1;
            std::vector<uint8_t> pdu(pduLen);
            if (!client.receiveData(pdu.data(), pduLen)) return false;

            return (pdu[0] == FC_WRITE_MULTIPLE_REGISTERS);
        }

    private:
        uint16_t m_transactionId;
    };

}

#endif // MODBUS_PROTOCOL_HPP
