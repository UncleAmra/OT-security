#include "../include/HttpScadaServer.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <cstring>
#include <iomanip>

HttpScadaServer::HttpScadaServer(ScadaPoller &poller, const std::string &webDir, 
                                 const std::string &bindIp, int port)
    : m_poller(poller),
      m_webDir(webDir),
      m_bindIp(bindIp),
      m_port(port),
      m_running(false),
      m_serverSock(INVALID_SOCKET) {}

HttpScadaServer::~HttpScadaServer() {
    stop();
}

bool HttpScadaServer::start() {
    if (m_running) return true;

    m_serverSock = socket(AF_INET, SOCK_STREAM, 0);
    if (!IS_VALID_SOCKET(m_serverSock)) {
        std::cerr << "[HTTP-SERVER] Socket creation failed.\n";
        return false;
    }

    int opt = 1;
#ifdef _WIN32
    setsockopt(m_serverSock, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));
#else
    setsockopt(m_serverSock, SOL_SOCKET, SO_REUSEADDR, (const void*)&opt, sizeof(opt));
#endif

    struct sockaddr_in serverAddr;
    std::memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = (m_bindIp == "0.0.0.0") ? INADDR_ANY : inet_addr(m_bindIp.c_str());

    bool bound = false;
    int initialPort = m_port;
    for (int p = initialPort; p <= initialPort + 10; ++p) {
        serverAddr.sin_port = htons(p);
        if (bind(m_serverSock, reinterpret_cast<struct sockaddr*>(&serverAddr), sizeof(serverAddr)) == 0) {
            m_port = p;
            bound = true;
            break;
        }
    }

    if (!bound) {
        std::cerr << "[HTTP-SERVER] Bind error on port " << initialPort << " (and fallback ports up to " << initialPort + 10 << ")\n";
        CLOSE_SOCKET(m_serverSock);
        m_serverSock = INVALID_SOCKET;
        return false;
    }

    if (listen(m_serverSock, 16) < 0) {
        std::cerr << "[HTTP-SERVER] Listen error on port " << m_port << std::endl;
        CLOSE_SOCKET(m_serverSock);
        m_serverSock = INVALID_SOCKET;
        return false;
    }

    m_running = true;
    m_acceptThread = std::thread(&HttpScadaServer::acceptLoop, this);

    std::cout << "\033[1;36m[HTTP-SERVER] SCADA Web HMI Server active at http://" 
              << (m_bindIp == "0.0.0.0" ? "localhost" : m_bindIp) << ":" << m_port << "\033[0m\n";
    return true;
}

void HttpScadaServer::stop() {
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

void HttpScadaServer::acceptLoop() {
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
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
            continue;
        }

        std::thread([this, clientSock]() {
            handleClient(clientSock);
        }).detach();
    }
}

static std::string getContentType(const std::string &path) {
    if (path.rfind(".html") != std::string::npos) return "text/html; charset=utf-8";
    if (path.rfind(".css") != std::string::npos)  return "text/css; charset=utf-8";
    if (path.rfind(".js") != std::string::npos)   return "application/javascript; charset=utf-8";
    if (path.rfind(".json") != std::string::npos) return "application/json";
    if (path.rfind(".svg") != std::string::npos)  return "image/svg+xml";
    if (path.rfind(".png") != std::string::npos)  return "image/png";
    return "text/plain";
}

void HttpScadaServer::handleClient(socket_handle_t clientSock) {
    NetUtils::setSocketTimeout(clientSock, 3000);

    char buffer[4096];
    int bytesRead = recv(clientSock, buffer, sizeof(buffer) - 1, 0);
    if (bytesRead <= 0) {
        CLOSE_SOCKET(clientSock);
        return;
    }
    buffer[bytesRead] = '\0';
    std::string request(buffer);

    std::istringstream reqStream(request);
    std::string method, uri, httpVersion;
    reqStream >> method >> uri >> httpVersion;

    // Handle API endpoints
    if (method == "GET" && uri == "/api/telemetry") {
        std::string json = generateTelemetryJson();
        std::string response = "HTTP/1.1 200 OK\r\n"
                               "Content-Type: application/json\r\n"
                               "Access-Control-Allow-Origin: *\r\n"
                               "Content-Length: " + std::to_string(json.size()) + "\r\n"
                               "Connection: close\r\n\r\n" + json;
        NetUtils::sendAll(clientSock, reinterpret_cast<const uint8_t*>(response.c_str()), static_cast<int>(response.size()));
    }
    else if (method == "POST" && uri == "/api/command") {
        // Find body in request
        size_t bodyPos = request.find("\r\n\r\n");
        std::string body = (bodyPos != std::string::npos) ? request.substr(bodyPos + 4) : "";

        std::string msg;
        bool ok = handleCommand(body, msg);

        std::string jsonResp = "{\"success\": " + std::string(ok ? "true" : "false") + ", \"message\": \"" + msg + "\"}";
        std::string response = "HTTP/1.1 200 OK\r\n"
                               "Content-Type: application/json\r\n"
                               "Access-Control-Allow-Origin: *\r\n"
                               "Content-Length: " + std::to_string(jsonResp.size()) + "\r\n"
                               "Connection: close\r\n\r\n" + jsonResp;
        NetUtils::sendAll(clientSock, reinterpret_cast<const uint8_t*>(response.c_str()), static_cast<int>(response.size()));
    }
    else if (method == "GET") {
        // Serve Static File
        std::string filePath = uri;
        if (filePath == "/" || filePath.empty()) {
            filePath = "/index.html";
        }

        std::string fullPath = m_webDir + filePath;
        std::ifstream file(fullPath, std::ios::binary);

        if (file.is_open()) {
            std::ostringstream ss;
            ss << file.rdbuf();
            std::string content = ss.str();
            std::string contentType = getContentType(filePath);

            std::string response = "HTTP/1.1 200 OK\r\n"
                                   "Content-Type: " + contentType + "\r\n"
                                   "Content-Length: " + std::to_string(content.size()) + "\r\n"
                                   "Connection: close\r\n\r\n" + content;
            NetUtils::sendAll(clientSock, reinterpret_cast<const uint8_t*>(response.c_str()), static_cast<int>(response.size()));
        } else {
            std::string notFound = "HTTP/1.1 404 Not Found\r\nContent-Length: 9\r\nConnection: close\r\n\r\nNot Found";
            NetUtils::sendAll(clientSock, reinterpret_cast<const uint8_t*>(notFound.c_str()), static_cast<int>(notFound.size()));
        }
    } else if (method == "OPTIONS") {
        std::string cors = "HTTP/1.1 204 No Content\r\n"
                           "Access-Control-Allow-Origin: *\r\n"
                           "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n"
                           "Access-Control-Allow-Headers: Content-Type\r\n"
                           "Connection: close\r\n\r\n";
        NetUtils::sendAll(clientSock, reinterpret_cast<const uint8_t*>(cors.c_str()), static_cast<int>(cors.size()));
    }

    CLOSE_SOCKET(clientSock);
}

std::string HttpScadaServer::generateTelemetryJson() {
    auto r = m_poller.getCachedRegisters();
    bool connected = m_poller.isConnected();

    if (r.size() < PlantRegisters::TOTAL_REGISTERS) {
        return "{\"connected\": false}";
    }

    uint16_t firstOut = r[PlantRegisters::REG_FIRST_OUT_TRIP_CODE];
    std::string firstOutName = "NONE";
    if (firstOut == PlantRegisters::FirstOutCode::MANUAL_ESD_OPERATOR)   firstOutName = "OPERATOR MANUAL ESD";
    else if (firstOut == PlantRegisters::FirstOutCode::BOILER_OVERPRESSURE)  firstOutName = "BOILER OVERPRESSURE";
    else if (firstOut == PlantRegisters::FirstOutCode::DRUM_LEVEL_LOW_LOW)   firstOutName = "DRUM LEVEL LOW-LOW";
    else if (firstOut == PlantRegisters::FirstOutCode::DRUM_LEVEL_HIGH_HIGH) firstOutName = "DRUM LEVEL HIGH-HIGH";
    else if (firstOut == PlantRegisters::FirstOutCode::FLAME_FAILURE)        firstOutName = "FLAME FAILURE";
    else if (firstOut == PlantRegisters::FirstOutCode::TURBINE_OVERSPEED)    firstOutName = "TURBINE OVERSPEED";
    else if (firstOut == PlantRegisters::FirstOutCode::HIGH_BEARING_VIBRATION) firstOutName = "BEARING HIGH VIBRATION";
    else if (firstOut == PlantRegisters::FirstOutCode::LOSS_OF_LUBE_OIL)     firstOutName = "LOSS OF LUBE OIL";
    else if (firstOut == PlantRegisters::FirstOutCode::CONDENSER_VACUUM_LOSS)firstOutName = "VACUUM LOSS";

    std::ostringstream j;
    j << "{"
      << "\"connected\": " << (connected ? "true" : "false") << ","
      << "\"heartbeat\": " << r[PlantRegisters::REG_WATCHDOG_HEARTBEAT] << ","
      << "\"plcMode\": " << r[PlantRegisters::REG_PLC_MODE] << ","

      // Boiler
      << "\"boiler\": {"
      << "\"drumTemp\": " << r[PlantRegisters::REG_BOILER_DRUM_TEMP] << ","
      << "\"drumPress\": " << r[PlantRegisters::REG_BOILER_DRUM_PRESS] << ","
      << "\"drumLevel\": " << r[PlantRegisters::REG_BOILER_DRUM_LEVEL] << ","
      << "\"superheatTemp\": " << r[PlantRegisters::REG_SUPERHEATER_TEMP] << ","
      << "\"steamFlow\": " << r[PlantRegisters::REG_STEAM_MASS_FLOW] << ","
      << "\"feedFlow\": " << r[PlantRegisters::REG_FEEDWATER_MASS_FLOW] << ","
      << "\"safety1\": " << r[PlantRegisters::REG_SAFETY_VALVE_1] << ","
      << "\"safety2\": " << r[PlantRegisters::REG_SAFETY_VALVE_2] << ","
      << "\"attemperator\": " << r[PlantRegisters::REG_ATTEMPERATOR_SPRAY] << ","
      << "\"blowdown\": " << r[PlantRegisters::REG_BLOWDOWN_VALVE]
      << "},"

      // Combustion
      << "\"combustion\": {"
      << "\"fuelRate\": " << r[PlantRegisters::REG_FUEL_FEED_RATE] << ","
      << "\"fdFan\": " << r[PlantRegisters::REG_FD_FAN_SPEED] << ","
      << "\"idFan\": " << r[PlantRegisters::REG_ID_FAN_SPEED] << ","
      << "\"furnacePress\": " << ((static_cast<int>(r[PlantRegisters::REG_FURNACE_PRESSURE]) - 500) / 10.0) << ","
      << "\"flueGasO2\": " << (r[PlantRegisters::REG_FLUE_GAS_O2] / 100.0) << ","
      << "\"burnerState\": " << r[PlantRegisters::REG_BURNER_FIRING_STATE] << ","
      << "\"flame\": " << r[PlantRegisters::REG_FLAME_DETECTOR] << ","
      << "\"efficiency\": " << (r[PlantRegisters::REG_COMBUSTION_EFFICIENCY] / 10.0)
      << "},"

      // Turbine
      << "\"turbine\": {"
      << "\"rpm\": " << r[PlantRegisters::REG_TURBINE_SPEED_RPM] << ","
      << "\"msv\": " << r[PlantRegisters::REG_MAIN_STEAM_STOP_VALVE] << ","
      << "\"cv\": " << r[PlantRegisters::REG_GOVERNOR_CONTROL_VALVE] << ","
      << "\"brg1Vib\": " << (r[PlantRegisters::REG_TURBINE_VIBRATION_BRG1] / 10.0) << ","
      << "\"brg2Vib\": " << (r[PlantRegisters::REG_TURBINE_VIBRATION_BRG2] / 10.0) << ","
      << "\"lubeOilPress\": " << (r[PlantRegisters::REG_LUBE_OIL_PRESSURE] / 10.0) << ","
      << "\"lubeOilTemp\": " << r[PlantRegisters::REG_LUBE_OIL_TEMP] << ","
      << "\"chestTemp\": " << r[PlantRegisters::REG_TURBINE_CHEST_TEMP] << ","
      << "\"tripped\": " << r[PlantRegisters::REG_TURBINE_TRIP_SOLENOID]
      << "},"

      // Generator
      << "\"generator\": {"
      << "\"mw\": " << r[PlantRegisters::REG_GEN_ACTIVE_POWER_MW] << ","
      << "\"mvar\": " << ((static_cast<int>(r[PlantRegisters::REG_GEN_REACTIVE_POWER_MVAR]) - 1000) / 10.0) << ","
      << "\"freq\": " << (r[PlantRegisters::REG_GEN_FREQUENCY_HZ] / 100.0) << ","
      << "\"volt\": " << (r[PlantRegisters::REG_GEN_TERMINAL_VOLT_KV] / 10.0) << ","
      << "\"breaker\": " << r[PlantRegisters::REG_GRID_BREAKER_52G] << ","
      << "\"phaseDelta\": " << (r[PlantRegisters::REG_SYNCHROSCOPE_PHASE_DELTA] / 10.0) << ","
      << "\"powerFactor\": " << (r[PlantRegisters::REG_POWER_FACTOR] / 100.0) << ","
      << "\"statorTemp\": " << r[PlantRegisters::REG_STATOR_TEMP] << ","
      << "\"syncPermit\": " << r[PlantRegisters::REG_GRID_SYNC_PERMIT]
      << "},"

      // Condenser & Feedwater
      << "\"condenser\": {"
      << "\"hotwellTemp\": " << r[PlantRegisters::REG_COND_HOTWELL_TEMP] << ","
      << "\"vacuum\": " << (r[PlantRegisters::REG_COND_VACUUM_INHG] / 10.0) << ","
      << "\"circFlow\": " << r[PlantRegisters::REG_CIRC_WATER_FLOW] << ","
      << "\"bfpA\": " << r[PlantRegisters::REG_BFP_A_SPEED] << ","
      << "\"bfpB\": " << r[PlantRegisters::REG_BFP_B_SPEED] << ","
      << "\"deaeratorLevel\": " << r[PlantRegisters::REG_DEAERATOR_LEVEL] << ","
      << "\"cepState\": " << r[PlantRegisters::REG_CONDENSATE_PUMP_STATE]
      << "},"

      // Safety & Alarms
      << "\"safety\": {"
      << "\"mft\": " << r[PlantRegisters::REG_MASTER_FUEL_TRIP] << ","
      << "\"turbineTrip\": " << r[PlantRegisters::REG_TURBINE_TRIP] << ","
      << "\"genTrip\": " << r[PlantRegisters::REG_GENERATOR_TRIP] << ","
      << "\"firstOut\": " << firstOut << ","
      << "\"firstOutName\": \"" << firstOutName << "\","
      << "\"alarmLow\": " << r[PlantRegisters::REG_ALARM_WORD_LOW] << ","
      << "\"alarmHigh\": " << r[PlantRegisters::REG_ALARM_WORD_HIGH]
      << "},"

      // Operator Targets
      << "\"targets\": {"
      << "\"targetMW\": " << r[PlantRegisters::REG_OPERATOR_TARGET_MW] << ","
      << "\"targetLevel\": " << r[PlantRegisters::REG_OPERATOR_TARGET_DRUM_LVL]
      << "}"
      << "}";

    return j.str();
}

bool HttpScadaServer::handleCommand(const std::string &body, std::string &outMessage) {
    // Basic JSON string key-value parser without requiring external libraries
    if (body.find("\"cmd\":\"esd\"") != std::string::npos || body.find("\"cmd\": \"esd\"") != std::string::npos) {
        bool ok = m_poller.triggerEmergencyShutdown();
        outMessage = ok ? "Emergency Shutdown (ESD) Executed" : "Failed to send ESD";
        return ok;
    }

    if (body.find("target_mw") != std::string::npos) {
        size_t valPos = body.find("\"val\":");
        if (valPos != std::string::npos) {
            int val = std::stoi(body.substr(valPos + 6));
            bool ok = m_poller.setTargetMegawatts(static_cast<uint16_t>(val));
            outMessage = "Target MW set to " + std::to_string(val);
            return ok;
        }
    }

    if (body.find("target_level") != std::string::npos) {
        size_t valPos = body.find("\"val\":");
        if (valPos != std::string::npos) {
            int val = std::stoi(body.substr(valPos + 6));
            bool ok = m_poller.setTargetDrumLevel(static_cast<uint16_t>(val));
            outMessage = "Target Drum Level set to " + std::to_string(val);
            return ok;
        }
    }

    if (body.find("breaker") != std::string::npos) {
        size_t valPos = body.find("\"val\":");
        if (valPos != std::string::npos) {
            int val = std::stoi(body.substr(valPos + 6));
            bool ok = m_poller.setBreakerState(val != 0);
            outMessage = (val != 0) ? "52G Breaker Close Commanded" : "52G Breaker Open Commanded";
            return ok;
        }
    }

    if (body.find("safety_valve") != std::string::npos) {
        size_t valPos = body.find("\"val\":");
        if (valPos != std::string::npos) {
            int val = std::stoi(body.substr(valPos + 6));
            bool ok = m_poller.toggleSafetyValve(1, val != 0);
            outMessage = (val != 0) ? "Safety Valve 1 Popped (Venting)" : "Safety Valve 1 Reseated";
            return ok;
        }
    }

    if (body.find("fuel_rate") != std::string::npos) {
        size_t valPos = body.find("\"val\":");
        if (valPos != std::string::npos) {
            int val = std::stoi(body.substr(valPos + 6));
            bool ok = m_poller.setFuelRate(static_cast<uint16_t>(val));
            outMessage = "Fuel rate adjusted to " + std::to_string(val) + " kg/min";
            return ok;
        }
    }

    if (body.find("mode") != std::string::npos) {
        size_t valPos = body.find("\"val\":");
        if (valPos != std::string::npos) {
            int val = std::stoi(body.substr(valPos + 6));
            bool ok = m_poller.setPlcMode(static_cast<uint16_t>(val));
            outMessage = "PLC Mode set to " + std::string(val == 1 ? "AUTO" : "MANUAL");
            return ok;
        }
    }

    outMessage = "Unknown command";
    return false;
}
