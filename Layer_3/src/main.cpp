/**
 * ==============================================================================
 *  LAYER 3: SCADA / HMI OPERATOR WORKSTATION (C++ & EMBEDDED WEB SERVER)
 * ==============================================================================
 * Connects to Layer 2 PLC (VM 2) over Modbus/TCP, polls telemetry at 4 Hz,
 * and hosts the rich Web SCADA interface on port 8080 for operators on VM 3.
 * ==============================================================================
 */

#include <iostream>
#include <string>
#include <chrono>
#include <thread>
#include <csignal>
#include <ctime>

#include "../../common/SocketUtils.hpp"
#include "../include/ScadaPoller.hpp"
#include "../include/HttpScadaServer.hpp"

static volatile bool g_scadaRunning = true;

void signalHandler(int signum) {
    (void)signum;
    g_scadaRunning = false;
}

std::string getTimestampStr() {
    auto now = std::chrono::system_clock::now();
    std::time_t now_c = std::chrono::system_clock::to_time_t(now);
    struct tm *lt = std::localtime(&now_c);
    char buf[64];
    if (lt && std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", lt)) {
        return std::string(buf);
    }
    return "0000-00-00 00:00:00";
}

int main(int argc, char *argv[]) {
    std::string plcIp = "127.0.0.1";
    int plcPort = 5020;
    int httpPort = 8085;
    std::string webDir = "web";

    if (argc >= 2) plcIp = argv[1];
    if (argc >= 3) plcPort = std::stoi(argv[2]);
    if (argc >= 4) httpPort = std::stoi(argv[3]);
    if (argc >= 5) webDir = argv[4];

    std::signal(SIGINT, signalHandler);

    if (!NetUtils::initializeNetwork()) {
        std::cerr << "[SCADA-ERROR] Failed to initialize socket subsystem.\n";
        return 1;
    }

    std::cout << "\033[1;35m================================================================================\n"
              << "       PURDUE LAYER 3: SCADA / HMI OPERATOR WORKSTATION DAEMON (C++)            \n"
              << "================================================================================\033[0m\n"
              << "[INIT] Target Layer 2 PLC Controller: " << plcIp << ":" << plcPort << "\n"
              << "[INIT] Preferred SCADA Web Server Port: " << httpPort << "\n"
              << "[INIT] Web Static Root Directory:     " << webDir << "\n\n";

    ScadaPoller poller(plcIp, plcPort);
    if (!poller.start()) {
        std::cerr << "[FATAL] Failed to launch SCADA Poller.\n";
        NetUtils::shutdownNetwork();
        return 1;
    }

    HttpScadaServer httpServer(poller, webDir, "0.0.0.0", httpPort);
    if (!httpServer.start()) {
        std::cerr << "[FATAL] Failed to launch SCADA HTTP Server.\n";
        poller.stop();
        NetUtils::shutdownNetwork();
        return 1;
    }

    std::cout << "\033[1;32m[SCADA READY] Operator Dashboard is active!\n"
              << "              Open your browser and navigate to: http://localhost:" << httpServer.getPort() << "\n"
              << "              Or from another VM: http://<VM3_IP>:" << httpServer.getPort() << "\033[0m\n\n";

    while (g_scadaRunning) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    std::cout << "\n[SHUTDOWN] Stopping SCADA HMI workstation...\n";
    httpServer.stop();
    poller.stop();
    NetUtils::shutdownNetwork();
    std::cout << "[SHUTDOWN] Exited cleanly.\n";
    return 0;
}
