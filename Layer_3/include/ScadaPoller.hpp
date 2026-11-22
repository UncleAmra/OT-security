#ifndef SCADA_POLLER_HPP
#define SCADA_POLLER_HPP

#include <string>
#include <vector>
#include <mutex>
#include <atomic>
#include <thread>
#include <cstdint>
#include "../../common/PlantRegisters.hpp"
#include "../../common/SocketUtils.hpp"
#include "../../common/ModbusProtocol.hpp"

/*
 * Reads all PLC registers every 250 ms in a background thread and keeps
 * the latest copy. Also sends operator commands as single-register writes.
 */

class ScadaPoller {
public:
    ScadaPoller(const std::string &plcIp = "127.0.0.1", int plcPort = 5020, uint8_t unitId = 1);
    ~ScadaPoller();

    bool start();
    void stop();

    bool isConnected() const { return m_connected; }

    // Telemetry cache
    std::vector<uint16_t> getCachedRegisters() const;

    // Operator commands
    bool sendWriteRegister(uint16_t address, uint16_t value);
    bool triggerEmergencyShutdown();
    bool restartSimulation();
    bool setTargetMegawatts(uint16_t targetMW);
    bool setTargetDrumLevel(uint16_t targetLevel);
    bool setBreakerState(bool closed);
    bool toggleSafetyValve(int valveNum, bool open);
    bool setFuelRate(uint16_t fuelRate);
    bool setPlcMode(uint16_t mode); // 0=Manual, 1=Auto

private:
    void pollLoop();

    std::string           m_plcIp;
    int                   m_plcPort;
    uint8_t               m_unitId;
    std::atomic<bool>     m_running;
    std::atomic<bool>     m_connected;

    mutable std::mutex    m_cacheMutex;
    std::vector<uint16_t> m_registers;

    std::thread           m_pollThread;
    std::mutex            m_ioMutex;   // one Modbus request/response on the socket at a time
    NetUtils::TcpClient   m_tcpClient;
    Modbus::ModbusClient  m_modbus;
};

#endif // SCADA_POLLER_HPP
