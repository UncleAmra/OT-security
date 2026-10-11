#include "../include/ModbusTcpServer.hpp"
#include <iostream>
#include <cstring>

ModbusTcpServer::ModbusTcpServer(PlcDataBank &dataBank, const std::string &bindIp, int port)
    : m_dataBank(dataBank),
      m_bindIp(bindIp),
      m_port(port),
      m_running(false),
      m_serverSock(INVALID_SOCKET) {}

ModbusTcpServer::~ModbusTcpServer() {
    stop();
}

bool ModbusTcpServer::start() {
    if (m_running) return true;

    m_serverSock = socket(AF_INET, SOCK_STREAM, 0);
    if (!IS_VALID_SOCKET(m_serverSock)) {
        std::cerr << "[PLC-SERVER] Failed to create server socket.\n";
        return false;
    }

    // Set SO_REUSEADDR
    int opt = 1;
#ifdef _WIN32
    setsockopt(m_serverSock, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));
#else
    setsockopt(m_serverSock, SOL_SOCKET, SO_REUSEADDR, (const void*)&opt, sizeof(opt));
#endif

    struct sockaddr_in serverAddr;
    std::memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(m_port);
    serverAddr.sin_addr.s_addr = (m_bindIp == "0.0.0.0") ? INADDR_ANY : inet_addr(m_bindIp.c_str());

    if (bind(m_serverSock, reinterpret_cast<struct sockaddr*>(&serverAddr), sizeof(serverAddr)) < 0) {
        std::cerr << "[PLC-SERVER] Bind error on port " << m_port << std::endl;
        CLOSE_SOCKET(m_serverSock);
        m_serverSock = INVALID_SOCKET;
        return false;
    }

    if (listen(m_serverSock, 16) < 0) {
        std::cerr << "[PLC-SERVER] Listen error on port " << m_port << std::endl;
        CLOSE_SOCKET(m_serverSock);
        m_serverSock = INVALID_SOCKET;
        return false;
    }

    m_running = true;
    m_acceptThread = std::thread(&ModbusTcpServer::acceptLoop, this);

    std::cout << "\033[1;32m[PLC-SERVER] Modbus/TCP Industrial Server listening on " 
              << m_bindIp << ":" << m_port << "\033[0m\n";
    return true;
}

void ModbusTcpServer::stop() {
    if (!m_running) return;
    m_running = false;

    if (IS_VALID_SOCKET(m_serverSock)) {
        CLOSE_SOCKET(m_serverSock);
        m_serverSock = INVALID_SOCKET;
    }

    if (m_acceptThread.joinable()) {
        m_acceptThread.join();
    }
}

void ModbusTcpServer::acceptLoop() {
    while (m_running) {
        struct sockaddr_in clientAddr;
#ifdef _WIN32
        int clientLen = sizeof(clientAddr);
#else
        socklen_t clientLen = sizeof(clientAddr);
#endif
        socket_handle_t clientSock = accept(m_serverSock, reinterpret_cast<struct sockaddr*>(&clientAddr), &clientLen);

        if (!IS_VALID_SOCKET(clientSock)) {
            if (!m_running) break;
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            continue;
        }

        char clientIp[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &(clientAddr.sin_addr), clientIp, INET_ADDRSTRLEN);
        int clientPort = ntohs(clientAddr.sin_port);

        // Spawn detached client handling thread
        std::thread([this, clientSock, clientIpStr = std::string(clientIp), clientPort]() {
            handleClient(clientSock);
        }).detach();
    }
}

void ModbusTcpServer::handleClient(socket_handle_t clientSock) {
    NetUtils::setSocketTimeout(clientSock, 5000);

    while (m_running) {
        // 1. Read MBAP Header (7 bytes: TransID[2], ProtoID[2], Length[2], UnitID[1])
        uint8_t mbap[7];
        if (NetUtils::receiveExact(clientSock, mbap, 7) != 7) {
            break; // Client disconnected
        }

        uint16_t transId = (static_cast<uint16_t>(mbap[0]) << 8) | mbap[1];
        uint16_t protoId = (static_cast<uint16_t>(mbap[2]) << 8) | mbap[3];
        uint16_t pduLen  = (static_cast<uint16_t>(mbap[4]) << 8) | mbap[5];
        uint8_t  unitId  = mbap[6];

        if (protoId != 0x0000 || pduLen < 2 || pduLen > 260) {
            break;
        }

        // PDU contains: Function Code (1 byte) + Function data (pduLen - 1 bytes)
        std::vector<uint8_t> pdu(pduLen - 1);
        if (NetUtils::receiveExact(clientSock, pdu.data(), pduLen - 1) != static_cast<int>(pduLen - 1)) {
            break;
        }

        uint8_t functionCode = pdu[0];

        // 2. Process Modbus Function Codes
        if (functionCode == Modbus::FC_READ_HOLDING_REGISTERS) { // FC 03
            if (pdu.size() < 5) break;

            uint16_t startAddr = (static_cast<uint16_t>(pdu[1]) << 8) | pdu[2];
            uint16_t count     = (static_cast<uint16_t>(pdu[3]) << 8) | pdu[4];

            if (count == 0 || count > 125 || (startAddr + count) > PlantRegisters::TOTAL_REGISTERS) {
                // Send Exception 0x02 Illegal Address
                uint8_t exc[9] = {
                    mbap[0], mbap[1], 0, 0, 0, 3, unitId,
                    static_cast<uint8_t>(functionCode | 0x80),
                    Modbus::EXCEPTION_ILLEGAL_DATA_ADDR
                };
                NetUtils::sendAll(clientSock, exc, sizeof(exc));
                continue;
            }

            auto values = m_dataBank.getHoldingRegisters(startAddr, count);
            uint8_t byteCount = static_cast<uint8_t>(values.size() * 2);
            uint16_t respLen = 1 + 1 + 1 + byteCount; // unitId + FC + byteCount + data

            std::vector<uint8_t> resp(6 + respLen);
            resp[0] = mbap[0]; resp[1] = mbap[1]; // TransID
            resp[2] = 0;       resp[3] = 0;       // ProtoID
            resp[4] = static_cast<uint8_t>((respLen >> 8) & 0xFF);
            resp[5] = static_cast<uint8_t>(respLen & 0xFF);
            resp[6] = unitId;
            resp[7] = functionCode;
            resp[8] = byteCount;

            for (size_t i = 0; i < values.size(); ++i) {
                resp[9 + (i * 2)]     = static_cast<uint8_t>((values[i] >> 8) & 0xFF);
                resp[10 + (i * 2)]    = static_cast<uint8_t>(values[i] & 0xFF);
            }

            NetUtils::sendAll(clientSock, resp.data(), static_cast<int>(resp.size()));
        }
        else if (functionCode == Modbus::FC_WRITE_SINGLE_REGISTER) { // FC 06
            if (pdu.size() < 5) break;

            uint16_t regAddr = (static_cast<uint16_t>(pdu[1]) << 8) | pdu[2];
            uint16_t value   = (static_cast<uint16_t>(pdu[3]) << 8) | pdu[4];

            if (regAddr >= PlantRegisters::TOTAL_REGISTERS) {
                uint8_t exc[9] = {
                    mbap[0], mbap[1], 0, 0, 0, 3, unitId,
                    static_cast<uint8_t>(functionCode | 0x80),
                    Modbus::EXCEPTION_ILLEGAL_DATA_ADDR
                };
                NetUtils::sendAll(clientSock, exc, sizeof(exc));
                continue;
            }

            m_dataBank.setHoldingRegister(regAddr, value);

            // Echo request as response
            uint8_t resp[12];
            resp[0] = mbap[0]; resp[1] = mbap[1];
            resp[2] = 0;       resp[3] = 0;
            resp[4] = 0;       resp[5] = 6;
            resp[6] = unitId;
            resp[7] = functionCode;
            resp[8] = pdu[1];  resp[9] = pdu[2];
            resp[10] = pdu[3]; resp[11] = pdu[4];

            NetUtils::sendAll(clientSock, resp, sizeof(resp));
        }
        else if (functionCode == Modbus::FC_WRITE_MULTIPLE_REGISTERS) { // FC 16 (0x10)
            if (pdu.size() < 6) break;

            uint16_t startAddr = (static_cast<uint16_t>(pdu[1]) << 8) | pdu[2];
            uint16_t count     = (static_cast<uint16_t>(pdu[3]) << 8) | pdu[4];
            uint8_t  byteCount = pdu[5];

            if (count == 0 || (count * 2 != byteCount) || (startAddr + count) > PlantRegisters::TOTAL_REGISTERS) {
                uint8_t exc[9] = {
                    mbap[0], mbap[1], 0, 0, 0, 3, unitId,
                    static_cast<uint8_t>(functionCode | 0x80),
                    Modbus::EXCEPTION_ILLEGAL_DATA_ADDR
                };
                NetUtils::sendAll(clientSock, exc, sizeof(exc));
                continue;
            }

            std::vector<uint16_t> vals(count);
            for (size_t i = 0; i < count; ++i) {
                vals[i] = (static_cast<uint16_t>(pdu[6 + (i * 2)]) << 8) | pdu[7 + (i * 2)];
            }
            m_dataBank.setHoldingRegisters(startAddr, vals);

            // Standard FC 16 response: StartAddr (2) + Count (2)
            uint8_t resp[12];
            resp[0] = mbap[0]; resp[1] = mbap[1];
            resp[2] = 0;       resp[3] = 0;
            resp[4] = 0;       resp[5] = 6;
            resp[6] = unitId;
            resp[7] = functionCode;
            resp[8] = pdu[1];  resp[9] = pdu[2];
            resp[10] = pdu[3]; resp[11] = pdu[4];

            NetUtils::sendAll(clientSock, resp, sizeof(resp));
        }
        else {
            // Unsupported Function Code Exception
            uint8_t exc[9] = {
                mbap[0], mbap[1], 0, 0, 0, 3, unitId,
                static_cast<uint8_t>(functionCode | 0x80),
                Modbus::EXCEPTION_ILLEGAL_FUNCTION
            };
            NetUtils::sendAll(clientSock, exc, sizeof(exc));
        }
    }

    CLOSE_SOCKET(clientSock);
}
