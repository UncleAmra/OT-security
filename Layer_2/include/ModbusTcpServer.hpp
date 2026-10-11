#ifndef MODBUS_TCP_SERVER_HPP
#define MODBUS_TCP_SERVER_HPP

#include <string>
#include <thread>
#include <atomic>
#include <vector>
#include "PlcDataBank.hpp"
#include "../../common/SocketUtils.hpp"
#include "../../common/ModbusProtocol.hpp"

/**
 * ==============================================================================
 *  MULTI-CLIENT INDUSTRIAL MODBUS/TCP SERVER (DAEMON)
 * ==============================================================================
 * Listens on Port 5020 (or Port 502) and serves multiple industrial clients
 * simultaneously (Layer 1 Physics Twin, Layer 3 HMI, Layer 4 Security Agents).
 * ==============================================================================
 */

class ModbusTcpServer {
public:
    ModbusTcpServer(PlcDataBank &dataBank, const std::string &bindIp = "0.0.0.0", int port = 5020);
    ~ModbusTcpServer();

    bool start();
    void stop();

    bool isRunning() const { return m_running; }
    int  getPort() const { return m_port; }

private:
    void acceptLoop();
    void handleClient(socket_handle_t clientSock);

    PlcDataBank&          m_dataBank;
    std::string           m_bindIp;
    int                   m_port;
    std::atomic<bool>     m_running;
    socket_handle_t       m_serverSock;
    std::thread           m_acceptThread;
};

#endif // MODBUS_TCP_SERVER_HPP
