#ifndef SOCKET_UTILS_HPP
#define SOCKET_UTILS_HPP

#include <iostream>
#include <string>
#include <vector>
#include <cstring>
#include <cstdint>
#include <chrono>
#include <thread>

#ifdef _WIN32
    #ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
    #endif
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "ws2_32.lib")
    typedef SOCKET socket_handle_t;
    #define CLOSE_SOCKET(s) closesocket(s)
    #define IS_VALID_SOCKET(s) ((s) != INVALID_SOCKET)
#else
    #include <sys/types.h>
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    #include <fcntl.h>
    #include <netdb.h>
    typedef int socket_handle_t;
    #define CLOSE_SOCKET(s) close(s)
    #define IS_VALID_SOCKET(s) ((s) >= 0)
    #define INVALID_SOCKET (-1)
    #define SOCKET_ERROR (-1)
#endif

namespace NetUtils {

    /**
     * @brief Initialize OS socket subsystem (WSAStartup on Windows).
     */
    inline bool initializeNetwork() {
#ifdef _WIN32
        WSADATA wsaData;
        int res = WSAStartup(MAKEWORD(2, 2), &wsaData);
        if (res != 0) {
            std::cerr << "[NET-ERROR] WSAStartup failed: " << res << std::endl;
            return false;
        }
#endif
        return true;
    }

    /**
     * @brief Cleanup OS socket subsystem (WSACleanup on Windows).
     */
    inline void shutdownNetwork() {
#ifdef _WIN32
        WSACleanup();
#endif
    }

    /**
     * @brief Set socket send and receive timeouts in milliseconds.
     */
    inline bool setSocketTimeout(socket_handle_t sock, int timeout_ms) {
#ifdef _WIN32
        DWORD tv = timeout_ms;
        if (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv)) != 0) return false;
        if (setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (const char*)&tv, sizeof(tv)) != 0) return false;
#else
        struct timeval tv;
        tv.tv_sec = timeout_ms / 1000;
        tv.tv_usec = (timeout_ms % 1000) * 1000;
        if (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const void*)&tv, sizeof(tv)) != 0) return false;
        if (setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (const void*)&tv, sizeof(tv)) != 0) return false;
#endif
        return true;
    }

    /**
     * @brief Read exact number of bytes from socket until full buffer received.
     */
    inline int receiveExact(socket_handle_t sock, uint8_t *buffer, int length) {
        int total = 0;
        while (total < length) {
            int received = recv(sock, reinterpret_cast<char*>(buffer + total), length - total, 0);
            if (received <= 0) {
                return -1; // Connection closed or error
            }
            total += received;
        }
        return total;
    }

    /**
     * @brief Send full byte buffer.
     */
    inline int sendAll(socket_handle_t sock, const uint8_t *buffer, int length) {
        int total = 0;
        while (total < length) {
            int sent = send(sock, reinterpret_cast<const char*>(buffer + total), length - total, 0);
            if (sent <= 0) {
                return -1;
            }
            total += sent;
        }
        return total;
    }

    /**
     * @brief Lightweight RAII TCP Client Socket wrapper.
     */
    class TcpClient {
    public:
        TcpClient() : m_sock(INVALID_SOCKET), m_connected(false) {}
        
        ~TcpClient() {
            disconnect();
        }

        bool connectToServer(const std::string &host, int port, int timeout_ms = 3000) {
            disconnect();

            m_sock = socket(AF_INET, SOCK_STREAM, 0);
            if (!IS_VALID_SOCKET(m_sock)) {
                return false;
            }

            setSocketTimeout(m_sock, timeout_ms);

            struct sockaddr_in server_addr;
            std::memset(&server_addr, 0, sizeof(server_addr));
            server_addr.sin_family = AF_INET;
            server_addr.sin_port = htons(port);

#ifdef _WIN32
            server_addr.sin_addr.s_addr = inet_addr(host.c_str());
            if (server_addr.sin_addr.s_addr == INADDR_NONE) {
                struct addrinfo hints, *res = nullptr;
                std::memset(&hints, 0, sizeof(hints));
                hints.ai_family = AF_INET;
                std::string port_str = std::to_string(port);
                if (getaddrinfo(host.c_str(), port_str.c_str(), &hints, &res) == 0 && res != nullptr) {
                    std::memcpy(&server_addr, res->ai_addr, sizeof(server_addr));
                    freeaddrinfo(res);
                } else {
                    CLOSE_SOCKET(m_sock);
                    m_sock = INVALID_SOCKET;
                    return false;
                }
            }
#else
            if (inet_pton(AF_INET, host.c_str(), &server_addr.sin_addr) <= 0) {
                CLOSE_SOCKET(m_sock);
                m_sock = INVALID_SOCKET;
                return false;
            }
#endif

            if (connect(m_sock, reinterpret_cast<struct sockaddr*>(&server_addr), sizeof(server_addr)) < 0) {
                CLOSE_SOCKET(m_sock);
                m_sock = INVALID_SOCKET;
                return false;
            }

            m_connected = true;
            return true;
        }

        void disconnect() {
            if (IS_VALID_SOCKET(m_sock)) {
                CLOSE_SOCKET(m_sock);
                m_sock = INVALID_SOCKET;
            }
            m_connected = false;
        }

        bool isConnected() const {
            return m_connected && IS_VALID_SOCKET(m_sock);
        }

        socket_handle_t getHandle() const {
            return m_sock;
        }

        bool sendData(const uint8_t *data, int length) {
            if (!isConnected()) return false;
            return (sendAll(m_sock, data, length) == length);
        }

        bool receiveData(uint8_t *buffer, int length) {
            if (!isConnected()) return false;
            return (receiveExact(m_sock, buffer, length) == length);
        }

    private:
        socket_handle_t m_sock;
        bool m_connected;
    };

}

#endif // SOCKET_UTILS_HPP
