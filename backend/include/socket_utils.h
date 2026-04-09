#ifndef SOCKET_UTILS_H
#define SOCKET_UTILS_H

#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <cerrno>
#include <cstring>
#include <string>
#include "common.h"
#include "logger.h"

class SocketUtils {
public:
    static bool setTimeout(int socket, int recvTimeout, int sendTimeout) {
        struct timeval tv;
        tv.tv_sec = recvTimeout;
        tv.tv_usec = 0;
        if (setsockopt(socket, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) {
            LOG_WARNING("设置接收超时失败: " + std::string(strerror(errno)));
            return false;
        }
        tv.tv_sec = sendTimeout;
        if (setsockopt(socket, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv)) < 0) {
            LOG_WARNING("设置发送超时失败: " + std::string(strerror(errno)));
            return false;
        }
        return true;
    }

    static bool enableKeepAlive(int socket, int idleTime = 60,
                                [[maybe_unused]] int interval = 10,
                                [[maybe_unused]] int count = 3) {
        int keepAlive = 1;
        if (setsockopt(socket, SOL_SOCKET, SO_KEEPALIVE, &keepAlive, sizeof(keepAlive)) < 0) {
            LOG_WARNING("启用Keep-Alive失败: " + std::string(strerror(errno)));
            return false;
        }
#ifdef __APPLE__
        setsockopt(socket, IPPROTO_TCP, TCP_KEEPALIVE, &idleTime, sizeof(idleTime));
#else
        setsockopt(socket, IPPROTO_TCP, TCP_KEEPIDLE, &idleTime, sizeof(idleTime));
        setsockopt(socket, IPPROTO_TCP, TCP_KEEPINTVL, &interval, sizeof(interval));
        setsockopt(socket, IPPROTO_TCP, TCP_KEEPCNT, &count, sizeof(count));
#endif
        return true;
    }

    static bool setReuseAddr(int socket) {
        int opt = 1;
        return setsockopt(socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) >= 0;
    }

    static bool setNonBlocking(int socket, bool nonBlocking = true) {
        int flags = fcntl(socket, F_GETFL, 0);
        if (flags < 0) return false;
        if (nonBlocking) flags |= O_NONBLOCK;
        else flags &= ~O_NONBLOCK;
        return fcntl(socket, F_SETFL, flags) >= 0;
    }

    static bool disableNagle(int socket) {
        int flag = 1;
        return setsockopt(socket, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag)) >= 0;
    }

    static void gracefulClose(int socket) {
        if (socket < 0) return;
        shutdown(socket, SHUT_WR);
        char buffer[1024];
        struct timeval tv = {1, 0};
        setsockopt(socket, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
        while (recv(socket, buffer, sizeof(buffer), 0) > 0) {}
        close(socket);
    }

    static bool getPeerInfo(int socket, std::string& ip, int& port) {
        struct sockaddr_in addr;
        socklen_t addrLen = sizeof(addr);
        if (getpeername(socket, (struct sockaddr*)&addr, &addrLen) < 0) return false;
        char ipStr[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &addr.sin_addr, ipStr, INET_ADDRSTRLEN);
        ip = ipStr;
        port = ntohs(addr.sin_port);
        return true;
    }

    static ssize_t sendAll(int socket, const char* data, size_t length) {
        size_t totalSent = 0;
        while (totalSent < length) {
            ssize_t sent = send(socket, data + totalSent, length - totalSent, 0);
            if (sent < 0) {
                if (errno == EINTR) continue;
                return -1;
            }
            if (sent == 0) return totalSent;
            totalSent += sent;
        }
        return totalSent;
    }

    static ssize_t recvAll(int socket, char* buffer, size_t length) {
        size_t totalRecv = 0;
        while (totalRecv < length) {
            ssize_t received = recv(socket, buffer + totalRecv, length - totalRecv, 0);
            if (received < 0) {
                if (errno == EINTR) continue;
                if (errno == EAGAIN || errno == EWOULDBLOCK) return totalRecv;
                return -1;
            }
            if (received == 0) return 0;
            totalRecv += received;
        }
        return totalRecv;
    }
};

#endif // SOCKET_UTILS_H
