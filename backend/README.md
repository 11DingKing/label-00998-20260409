# Backend - TCP通信服务器

基于C++ Socket实现的TCP服务器/客户端通信程序。

## 功能特性

- 多客户端并发连接支持
- 消息协议（解决TCP粘包/拆包）
- 心跳检测机制
- 断线自动重连
- 超时控制
- 优雅关闭连接

## 本地运行

```bash
# 编译
make all

# 启动服务器
./server [端口号]

# 启动客户端
./client [服务器IP] [端口号]
```

## Docker运行

```bash
docker build -t tcp-server .
docker run -p 8888:8888 tcp-server
```

## 配置参数

| 参数 | 默认值 | 说明 |
|------|--------|------|
| DEFAULT_PORT | 8888 | 默认端口 |
| MAX_CLIENTS | 100 | 最大客户端数 |
| HEARTBEAT_INTERVAL | 15s | 心跳间隔 |
