#ifndef COMMON_H
#define COMMON_H

#include <string>
#include <cstdint>

// ==================== 网络配置 ====================
const int DEFAULT_PORT = 8888;
const int BUFFER_SIZE = 4096;
const int MAX_BACKLOG = 10;
const int MAX_CLIENTS = 100;

// ==================== 超时配置 ====================
const int CONNECT_TIMEOUT = 10;
const int RECV_TIMEOUT = 30;
const int SEND_TIMEOUT = 10;
const int HEARTBEAT_INTERVAL = 15;
const int HEARTBEAT_TIMEOUT = 45;

// ==================== 重连配置 ====================
const int MAX_RECONNECT_ATTEMPTS = 5;
const int RECONNECT_INTERVAL = 3;

// ==================== 命令定义 ====================
const std::string EXIT_COMMAND = "quit";
const std::string EXIT_COMMAND_UPPER = "QUIT";
const std::string HEARTBEAT_REQUEST = "HEARTBEAT_PING";
const std::string HEARTBEAT_RESPONSE = "HEARTBEAT_PONG";

// ==================== 消息协议 ====================
#pragma pack(push, 1)
struct MessageHeader {
    uint32_t magic;
    uint32_t length;
    uint16_t type;
    uint16_t reserved;
};
#pragma pack(pop)

const uint32_t MESSAGE_MAGIC = 0x5443504D;

enum class MessageType : uint16_t {
    DATA = 0x0001,
    HEARTBEAT_REQ = 0x0002,
    HEARTBEAT_RSP = 0x0003,
    DISCONNECT = 0x0004,
    ACK = 0x0005
};

// ==================== 错误码定义 ====================
enum class ErrorCode : int {
    SUCCESS = 0,
    SOCKET_CREATE_FAILED = -1,
    BIND_FAILED = -2,
    LISTEN_FAILED = -3,
    ACCEPT_FAILED = -4,
    CONNECT_FAILED = -5,
    SEND_FAILED = -6,
    RECV_FAILED = -7,
    TIMEOUT = -8,
    CONNECTION_CLOSED = -9,
    INVALID_MESSAGE = -10,
    MAX_CLIENTS_REACHED = -11
};

inline std::string errorCodeToString(ErrorCode code) {
    switch (code) {
        case ErrorCode::SUCCESS: return "成功";
        case ErrorCode::SOCKET_CREATE_FAILED: return "创建Socket失败";
        case ErrorCode::BIND_FAILED: return "绑定端口失败";
        case ErrorCode::LISTEN_FAILED: return "监听失败";
        case ErrorCode::ACCEPT_FAILED: return "接受连接失败";
        case ErrorCode::CONNECT_FAILED: return "连接失败";
        case ErrorCode::SEND_FAILED: return "发送失败";
        case ErrorCode::RECV_FAILED: return "接收失败";
        case ErrorCode::TIMEOUT: return "超时";
        case ErrorCode::CONNECTION_CLOSED: return "连接已关闭";
        case ErrorCode::INVALID_MESSAGE: return "无效消息";
        case ErrorCode::MAX_CLIENTS_REACHED: return "达到最大客户端数";
        default: return "未知错误";
    }
}

#endif // COMMON_H
