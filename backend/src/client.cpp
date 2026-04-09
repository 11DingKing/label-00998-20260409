#include <iostream>
#include <string>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <thread>
#include <atomic>
#include <chrono>
#include "../include/logger.h"
#include "../include/common.h"
#include "../include/socket_utils.h"
#include "../include/message_protocol.h"

class TCPClient {
private:
    int clientSocket;
    std::string serverIP;
    int serverPort;
    std::atomic<bool> isConnected;
    std::atomic<bool> isRunning;
    std::thread heartbeatThread;
    std::chrono::steady_clock::time_point lastHeartbeatResponse;

    ErrorCode createSocket() {
        LOG_INFO("步骤1: 创建socket套接字...");
        clientSocket = socket(AF_INET, SOCK_STREAM, 0);
        if (clientSocket < 0) {
            LOG_ERROR("创建socket失败: " + std::string(strerror(errno)));
            return ErrorCode::SOCKET_CREATE_FAILED;
        }
        LOG_INFO("Socket创建成功，fd: " + std::to_string(clientSocket));
        return ErrorCode::SUCCESS;
    }

    ErrorCode connectToServer(int maxRetries = MAX_RECONNECT_ATTEMPTS) {
        LOG_INFO("步骤2: 连接服务器 " + serverIP + ":" + std::to_string(serverPort) + "...");
        struct sockaddr_in serverAddr;
        memset(&serverAddr, 0, sizeof(serverAddr));
        serverAddr.sin_family = AF_INET;
        serverAddr.sin_port = htons(serverPort);
        if (inet_pton(AF_INET, serverIP.c_str(), &serverAddr.sin_addr) <= 0) {
            LOG_ERROR("无效的IP地址: " + serverIP);
            return ErrorCode::CONNECT_FAILED;
        }
        for (int attempt = 1; attempt <= maxRetries; attempt++) {
            LOG_INFO("连接尝试 " + std::to_string(attempt) + "/" + std::to_string(maxRetries));
            if (connect(clientSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) == 0) {
                SocketUtils::setTimeout(clientSocket, RECV_TIMEOUT, SEND_TIMEOUT);
                SocketUtils::enableKeepAlive(clientSocket);
                LOG_INFO("成功连接到服务器");
                isConnected = true;
                lastHeartbeatResponse = std::chrono::steady_clock::now();
                return ErrorCode::SUCCESS;
            }
            LOG_WARNING("连接失败: " + std::string(strerror(errno)));
            if (attempt < maxRetries) {
                std::this_thread::sleep_for(std::chrono::seconds(RECONNECT_INTERVAL));
                close(clientSocket);
                if (createSocket() != ErrorCode::SUCCESS) return ErrorCode::SOCKET_CREATE_FAILED;
            }
        }
        return ErrorCode::CONNECT_FAILED;
    }

    void heartbeatSender() {
        while (isRunning && isConnected) {
            std::this_thread::sleep_for(std::chrono::seconds(HEARTBEAT_INTERVAL));
            if (!isConnected) break;
            auto packet = MessageProtocol::createHeartbeatRequest();
            SocketUtils::sendAll(clientSocket, packet.data(), packet.size());
            auto now = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
                now - lastHeartbeatResponse).count();
            if (elapsed > HEARTBEAT_TIMEOUT) isConnected = false;
        }
    }

    bool tryReconnect() {
        LOG_INFO("尝试重新连接...");
        if (clientSocket >= 0) { close(clientSocket); clientSocket = -1; }
        if (createSocket() != ErrorCode::SUCCESS) return false;
        return connectToServer() == ErrorCode::SUCCESS;
    }

    void startCommunication() {
        LOG_INFO("步骤3: 开始通信...");
        LOG_INFO("输入 'quit' 退出");
        char headerBuffer[sizeof(MessageHeader)];
        char dataBuffer[BUFFER_SIZE];
        std::string message;
        while (isRunning && isConnected) {
            std::cout << "请输入消息: ";
            std::getline(std::cin, message);
            if (message == EXIT_COMMAND || message == EXIT_COMMAND_UPPER) {
                auto packet = MessageProtocol::createDisconnect();
                SocketUtils::sendAll(clientSocket, packet.data(), packet.size());
                break;
            }
            if (message.empty()) continue;
            auto packet = MessageProtocol::pack(message, MessageType::DATA);
            if (SocketUtils::sendAll(clientSocket, packet.data(), packet.size()) < 0) {
                if (tryReconnect()) LOG_INFO("重连成功");
                else break;
                continue;
            }
            ssize_t headerRecv = SocketUtils::recvAll(clientSocket, headerBuffer, sizeof(MessageHeader));
            if (headerRecv <= 0) {
                if (tryReconnect()) continue;
                break;
            }
            MessageHeader header;
            if (!MessageProtocol::parseHeader(headerBuffer, header)) continue;
            if (static_cast<MessageType>(header.type) == MessageType::HEARTBEAT_RSP) {
                lastHeartbeatResponse = std::chrono::steady_clock::now();
                continue;
            }
            if (header.length > 0 && header.length < BUFFER_SIZE) {
                ssize_t dataRecv = SocketUtils::recvAll(clientSocket, dataBuffer, header.length);
                if (dataRecv > 0) {
                    dataBuffer[header.length] = '\0';
                    LOG_INFO("服务器响应: " + std::string(dataBuffer));
                }
            }
        }
    }

    void closeConnection() {
        LOG_INFO("步骤4: 关闭连接...");
        isConnected = false;
        if (heartbeatThread.joinable()) heartbeatThread.join();
        if (clientSocket >= 0) { SocketUtils::gracefulClose(clientSocket); clientSocket = -1; }
    }

public:
    TCPClient(const std::string& ip, int port)
        : clientSocket(-1), serverIP(ip), serverPort(port), isConnected(false), isRunning(false) {}
    ~TCPClient() { stop(); }

    ErrorCode start() {
        LOG_INFO("========== TCP客户端启动 ==========");
        isRunning = true;
        ErrorCode err = createSocket();
        if (err != ErrorCode::SUCCESS) return err;
        err = connectToServer();
        if (err != ErrorCode::SUCCESS) { close(clientSocket); return err; }
        heartbeatThread = std::thread(&TCPClient::heartbeatSender, this);
        startCommunication();
        closeConnection();
        LOG_INFO("========== TCP客户端已停止 ==========");
        return ErrorCode::SUCCESS;
    }

    void stop() { isRunning = false; isConnected = false; }
};

int main(int argc, char* argv[]) {
    std::string serverIP = "127.0.0.1";
    int serverPort = DEFAULT_PORT;
    if (argc >= 2) {
        serverIP = argv[1];
        struct sockaddr_in sa;
        if (inet_pton(AF_INET, serverIP.c_str(), &(sa.sin_addr)) <= 0) {
            LOG_ERROR("无效的IP地址"); return -1;
        }
    }
    if (argc >= 3) {
        try {
            serverPort = std::stoi(argv[2]);
            if (serverPort < 1024 || serverPort > 65535) { LOG_ERROR("端口号无效"); return -1; }
        } catch (...) { LOG_ERROR("无效的端口号"); return -1; }
    }
    TCPClient client(serverIP, serverPort);
    return static_cast<int>(client.start());
}
