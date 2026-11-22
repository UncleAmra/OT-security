#ifndef MODBUS_TCP_SERVER_HPP
#define MODBUS_TCP_SERVER_HPP

#include <string>
#include <thread>
#include <atomic>
#include <vector>
#include "PlcDataBank.hpp"
#include "../../common/SocketUtils.hpp"
#include "../../common/ModbusProtocol.hpp"

/*
 * Modbus/TCP server for the PLC. Accepts several clients at once
 * (Layer 1, Layer 3, and later Layer 4) and supports FC03, FC06 and FC16.
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
