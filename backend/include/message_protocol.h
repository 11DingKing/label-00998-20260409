#ifndef MESSAGE_PROTOCOL_H
#define MESSAGE_PROTOCOL_H

#include <string>
#include <vector>
#include <cstring>
#include <arpa/inet.h>
#include "common.h"

class MessageProtocol {
public:
    static std::vector<char> pack(const std::string& data, MessageType type = MessageType::DATA) {
        MessageHeader header;
        header.magic = htonl(MESSAGE_MAGIC);
        header.length = htonl(static_cast<uint32_t>(data.length()));
        header.type = htons(static_cast<uint16_t>(type));
        header.reserved = 0;

        std::vector<char> packet(sizeof(MessageHeader) + data.length());
        memcpy(packet.data(), &header, sizeof(MessageHeader));
        memcpy(packet.data() + sizeof(MessageHeader), data.c_str(), data.length());
        return packet;
    }

    static bool parseHeader(const char* buffer, MessageHeader& header) {
        memcpy(&header, buffer, sizeof(MessageHeader));
        header.magic = ntohl(header.magic);
        header.length = ntohl(header.length);
        header.type = ntohs(header.type);
        return header.magic == MESSAGE_MAGIC;
    }

    static size_t getHeaderSize() { return sizeof(MessageHeader); }

    static std::vector<char> createHeartbeatRequest() {
        return pack(HEARTBEAT_REQUEST, MessageType::HEARTBEAT_REQ);
    }

    static std::vector<char> createHeartbeatResponse() {
        return pack(HEARTBEAT_RESPONSE, MessageType::HEARTBEAT_RSP);
    }

    static std::vector<char> createDisconnect() {
        return pack("", MessageType::DISCONNECT);
    }
};

#endif // MESSAGE_PROTOCOL_H
