#include <iostream>
#include <string>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <signal.h>
#include <thread>
#include <vector>
#include <mutex>
#include <atomic>
#include <map>
#include <chrono>
#include <sys/select.h>
#include "../include/logger.h"
#include "../include/common.h"
#include "../include/socket_utils.h"
#include "../include/message_protocol.h"

struct ClientInfo {
    int socket;
    std::string ip;
    int port;
    std::chrono::steady_clock::time_point lastHeartbeat;
    bool isActive;
    ClientInfo() : socket(-1), port(0), isActive(false) {}
    ClientInfo(int s, const std::string& i, int p)
        : socket(s), ip(i), port(p),
          lastHeartbeat(std::chrono::steady_clock::now()), isActive(true) {}
};

class TCPServer {
private:
    int serverSocket;
    int port;
    std::atomic<bool> isRunning;
    std::map<int, ClientInfo> clients;
    std::mutex clientsMutex;
    std::vector<std::thread> clientThreads;
    std::thread heartbeatThread;

    ErrorCode createSocket() {
        LOG_INFO("步骤1: 创建socket套接字...");
        serverSocket = socket(AF_INET, SOCK_STREAM, 0);
        if (serverSocket < 0) {
            LOG_ERROR("创建socket失败: " + std::string(strerror(errno)));
            return ErrorCode::SOCKET_CREATE_FAILED;
        }
        SocketUtils::setReuseAddr(serverSocket);
        SocketUtils::enableKeepAlive(serverSocket);
        LOG_INFO("Socket创建成功，fd: " + std::to_string(serverSocket));
        return ErrorCode::SUCCESS;
    }

    ErrorCode bindPort() {
        LOG_INFO("步骤2: 绑定端口号 " + std::to_string(port) + "...");
        struct sockaddr_in serverAddr;
        memset(&serverAddr, 0, sizeof(serverAddr));
        serverAddr.sin_family = AF_INET;
        serverAddr.sin_addr.s_addr = INADDR_ANY;
        serverAddr.sin_port = htons(port);
        if (bind(serverSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
            LOG_ERROR("绑定端口失败: " + std::string(strerror(errno)));
            return ErrorCode::BIND_FAILED;
        }
        LOG_INFO("端口绑定成功: " + std::to_string(port));
        return ErrorCode::SUCCESS;
    }

    ErrorCode startListening() {
        LOG_INFO("步骤3: 开启监听属性...");
        if (listen(serverSocket, MAX_BACKLOG) < 0) {
            LOG_ERROR("开启监听失败: " + std::string(strerror(errno)));
            return ErrorCode::LISTEN_FAILED;
        }
        LOG_INFO("监听已开启，最大队列: " + std::to_string(MAX_BACKLOG));
        return ErrorCode::SUCCESS;
    }

    void acceptClients() {
        LOG_INFO("步骤4: 等待客户端连接...");
        while (isRunning) {
            fd_set readfds;
            FD_ZERO(&readfds);
            FD_SET(serverSocket, &readfds);
            struct timeval tv = {1, 0};
            int ret = select(serverSocket + 1, &readfds, NULL, NULL, &tv);
            if (ret < 0) { if (errno == EINTR) continue; break; }
            if (ret == 0) continue;
            {
                std::lock_guard<std::mutex> lock(clientsMutex);
                if (clients.size() >= MAX_CLIENTS) continue;
            }
            struct sockaddr_in clientAddr;
            socklen_t clientAddrLen = sizeof(clientAddr);
            int clientSocket = accept(serverSocket, (struct sockaddr*)&clientAddr, &clientAddrLen);
            if (clientSocket < 0) { if (errno == EINTR) continue; continue; }
            char clientIP[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &clientAddr.sin_addr, clientIP, INET_ADDRSTRLEN);
            int clientPort = ntohs(clientAddr.sin_port);
            SocketUtils::setTimeout(clientSocket, RECV_TIMEOUT, SEND_TIMEOUT);
            SocketUtils::enableKeepAlive(clientSocket);
            {
                std::lock_guard<std::mutex> lock(clientsMutex);
                clients[clientSocket] = ClientInfo(clientSocket, clientIP, clientPort);
            }
            LOG_INFO("新客户端连接: " + std::string(clientIP) + ":" + std::to_string(clientPort));
            clientThreads.emplace_back(&TCPServer::handleClient, this, clientSocket);
        }
    }

    void handleClient(int clientSocket) {
        std::string clientInfo;
        {
            std::lock_guard<std::mutex> lock(clientsMutex);
            auto it = clients.find(clientSocket);
            if (it != clients.end()) clientInfo = it->second.ip + ":" + std::to_string(it->second.port);
        }
        LOG_INFO("步骤5: 开始与客户端 " + clientInfo + " 通信...");
        char headerBuffer[sizeof(MessageHeader)];
        char dataBuffer[BUFFER_SIZE];
        while (isRunning) {
            {
                std::lock_guard<std::mutex> lock(clientsMutex);
                auto it = clients.find(clientSocket);
                if (it == clients.end() || !it->second.isActive) break;
            }
            ssize_t headerRecv = SocketUtils::recvAll(clientSocket, headerBuffer, sizeof(MessageHeader));
            if (headerRecv <= 0) {
                if (headerRecv == 0) LOG_INFO("客户端 " + clientInfo + " 断开连接");
                break;
            }
            MessageHeader header;
            if (!MessageProtocol::parseHeader(headerBuffer, header)) continue;
            if (header.length > 0 && header.length < BUFFER_SIZE) {
                ssize_t dataRecv = SocketUtils::recvAll(clientSocket, dataBuffer, header.length);
                if (dataRecv <= 0) break;
                dataBuffer[header.length] = '\0';
            }
            {
                std::lock_guard<std::mutex> lock(clientsMutex);
                auto it = clients.find(clientSocket);
                if (it != clients.end()) it->second.lastHeartbeat = std::chrono::steady_clock::now();
            }
            MessageType msgType = static_cast<MessageType>(header.type);
            switch (msgType) {
                case MessageType::HEARTBEAT_REQ: {
                    auto response = MessageProtocol::createHeartbeatResponse();
                    SocketUtils::sendAll(clientSocket, response.data(), response.size());
                    break;
                }
                case MessageType::DISCONNECT: goto cleanup;
                case MessageType::DATA: {
                    std::string message(dataBuffer, header.length);
                    LOG_INFO("收到消息[" + clientInfo + "]: " + message);
                    if (message == EXIT_COMMAND || message == EXIT_COMMAND_UPPER) goto cleanup;
                    std::string response = "服务器已收到: " + message;
                    auto packet = MessageProtocol::pack(response, MessageType::DATA);
                    if (SocketUtils::sendAll(clientSocket, packet.data(), packet.size()) < 0) goto cleanup;
                    break;
                }
                default: break;
            }
        }
    cleanup:
        closeClient(clientSocket);
    }

    void heartbeatChecker() {
        while (isRunning) {
            std::this_thread::sleep_for(std::chrono::seconds(HEARTBEAT_INTERVAL));
            auto now = std::chrono::steady_clock::now();
            std::vector<int> timeoutClients;
            {
                std::lock_guard<std::mutex> lock(clientsMutex);
                for (auto& pair : clients) {
                    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
                        now - pair.second.lastHeartbeat).count();
                    if (elapsed > HEARTBEAT_TIMEOUT) timeoutClients.push_back(pair.first);
                }
            }
            for (int socket : timeoutClients) closeClient(socket);
        }
    }

    void closeClient(int clientSocket) {
        std::lock_guard<std::mutex> lock(clientsMutex);
        auto it = clients.find(clientSocket);
        if (it != clients.end()) {
            it->second.isActive = false;
            LOG_INFO("关闭客户端连接: " + it->second.ip + ":" + std::to_string(it->second.port));
            SocketUtils::gracefulClose(clientSocket);
            clients.erase(it);
        }
    }

    void closeAllConnections() {
        LOG_INFO("步骤6: 关闭所有连接...");
        {
            std::lock_guard<std::mutex> lock(clientsMutex);
            for (auto& pair : clients) {
                pair.second.isActive = false;
                SocketUtils::gracefulClose(pair.first);
            }
            clients.clear();
        }
        for (auto& t : clientThreads) if (t.joinable()) t.join();
        clientThreads.clear();
        if (serverSocket >= 0) { close(serverSocket); serverSocket = -1; }
    }

public:
    TCPServer(int portNum = DEFAULT_PORT) : serverSocket(-1), port(portNum), isRunning(false) {}
    ~TCPServer() { stop(); }

    ErrorCode start() {
        LOG_INFO("========== TCP服务器启动 ==========");
        ErrorCode err = createSocket();
        if (err != ErrorCode::SUCCESS) return err;
        err = bindPort();
        if (err != ErrorCode::SUCCESS) { close(serverSocket); return err; }
        err = startListening();
        if (err != ErrorCode::SUCCESS) { close(serverSocket); return err; }
        isRunning = true;
        heartbeatThread = std::thread(&TCPServer::heartbeatChecker, this);
        LOG_INFO("服务器已就绪，等待客户端连接...");
        acceptClients();
        closeAllConnections();
        if (heartbeatThread.joinable()) heartbeatThread.join();
        LOG_INFO("========== TCP服务器已停止 ==========");
        return ErrorCode::SUCCESS;
    }

    void stop() { if (isRunning) isRunning = false; }
};

TCPServer* g_server = nullptr;
void signalHandler(int signal) {
    if (signal == SIGINT || signal == SIGTERM) {
        LOG_INFO("\n收到退出信号，正在关闭服务器...");
        if (g_server) g_server->stop();
    }
}

int main(int argc, char* argv[]) {
    int port = DEFAULT_PORT;
    if (argc > 1) {
        try {
            port = std::stoi(argv[1]);
            if (port < 1024 || port > 65535) { LOG_ERROR("端口号无效"); return -1; }
        } catch (...) { LOG_ERROR("无效的端口号"); return -1; }
    }
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);
    TCPServer server(port);
    g_server = &server;
    ErrorCode result = server.start();
    g_server = nullptr;
    return static_cast<int>(result);
}
