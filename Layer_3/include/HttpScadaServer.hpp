#ifndef HTTP_SCADA_SERVER_HPP
#define HTTP_SCADA_SERVER_HPP

#include <string>
#include <thread>
#include <atomic>
#include "ScadaPoller.hpp"
#include "../../common/SocketUtils.hpp"

/*
 * HTTP server for the HMI. Serves the files in web/ plus
 * GET /api/telemetry (JSON) and POST /api/command (operator actions).
 */

class HttpScadaServer {
public:
    HttpScadaServer(ScadaPoller &poller, const std::string &webDir = "web", 
                    const std::string &bindIp = "0.0.0.0", int port = 8080);
    ~HttpScadaServer();

    bool start();
    void stop();

    bool isRunning() const { return m_running; }
    int  getPort() const { return m_port; }

private:
    void acceptLoop();
    void handleClient(socket_handle_t clientSock);

    std::string generateTelemetryJson();
    bool handleCommand(const std::string &body, std::string &outMessage);

    ScadaPoller&          m_poller;
    std::string           m_webDir;
    std::string           m_bindIp;
    int                   m_port;
    std::atomic<bool>     m_running;
    socket_handle_t       m_serverSock;
    std::thread           m_acceptThread;
};

#endif // HTTP_SCADA_SERVER_HPP
