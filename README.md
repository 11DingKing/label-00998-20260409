# TCP网络通信项目

## How to Run

```bash
# 使用 Docker Compose 启动（后台运行，必须加 -d）
docker-compose up --build -d

# 注意：不加 -d 会前台运行并显示日志，看起来像"卡住"
# 实际是服务器在等待客户端连接，按 Ctrl+C 可退出

# 查看服务状态
docker-compose ps

# 查看日志
docker-compose logs -f

# 停止服务
docker-compose down
```

## Services

| 服务 | 端口 | 说明 |
|------|------|------|
| backend | 8888 | TCP通信服务器 |

## 测试账号

本项目为TCP Socket通信，无需账号认证。

## 题目内容

使用C++编程语言，应用Socket实现TCP网络通信。

### 服务器端流程
1. 创建socket套接字
2. 给socket绑定端口号
3. 开启监听属性
4. 等待客户端连接
5. 开始通讯
6. 关闭连接

### 客户端流程
1. 创建socket套接字
2. 连接服务端
3. 开始通讯
4. 关闭连接

### 完善功能
- 多客户端并发支持（多线程）
- 消息协议（解决粘包/拆包问题）
- 心跳检测机制
- 断线自动重连
- 超时控制
- 优雅关闭连接

---

## 质检测试指南

### 1. 验证服务启动

```bash
# 启动服务
docker-compose up --build -d

# 检查容器状态（应显示 Up）
docker-compose ps

# 预期输出：
# NAME         IMAGE         COMMAND           SERVICE   STATUS         PORTS
# tcp-server   998-backend   "./server 8888"   backend   Up             0.0.0.0:8888->8888/tcp
```

### 2. 验证端口监听

```bash
# 检查端口是否监听
nc -zv 127.0.0.1 8888

# 预期输出：
# Connection to 127.0.0.1 port 8888 [tcp/*] succeeded!
```

### 3. 验证服务器日志

```bash
# 查看服务器启动日志
docker-compose logs backend

# 预期输出包含：
# ========== TCP服务器启动 ==========
# 步骤1: 创建socket套接字...
# Socket创建成功
# 步骤2: 绑定端口号 8888...
# 端口绑定成功: 8888
# 步骤3: 开启监听属性...
# 监听已开启
# 服务器已就绪，等待客户端连接...
# 步骤4: 等待客户端连接...
```

### 4. TCP连接测试（使用 nc/netcat）

```bash
# 方法1：使用 nc 测试 TCP 连接
echo "test" | nc 127.0.0.1 8888

# 方法2：交互式测试（需要手动输入）
nc 127.0.0.1 8888
# 然后输入任意内容，按 Ctrl+C 退出
```

### 5. 使用容器内客户端测试

```bash
# 进入容器使用内置客户端测试
docker exec -it tcp-server ./client 127.0.0.1 8888

# 交互操作：
# 1. 输入消息，如：hello
# 2. 查看服务器响应
# 3. 输入 quit 退出
```

### 6. 多客户端并发测试

```bash
# 打开多个终端，同时运行：
docker exec -it tcp-server ./client 127.0.0.1 8888

# 或使用脚本模拟多连接：
for i in {1..5}; do
  (echo "client$i" | nc 127.0.0.1 8888) &
done
wait

# 查看服务器日志确认多客户端连接
docker-compose logs --tail=20 backend
```

### 7. 一键测试脚本

```bash
# 创建并运行测试脚本
cat << 'EOF' > test.sh
#!/bin/bash
echo "=== TCP服务器测试 ==="

echo "1. 检查容器状态..."
docker-compose ps | grep -q "Up" && echo "✓ 容器运行正常" || echo "✗ 容器未运行"

echo "2. 检查端口监听..."
nc -zv 127.0.0.1 8888 2>&1 | grep -q "succeeded" && echo "✓ 端口8888监听正常" || echo "✗ 端口未监听"

echo "3. 检查服务器日志..."
docker-compose logs backend 2>&1 | grep -q "服务器已就绪" && echo "✓ 服务器启动成功" || echo "✗ 服务器启动失败"

echo "=== 测试完成 ==="
EOF
chmod +x test.sh
./test.sh
```

### 8. 清理环境

```bash
# 停止并清理
docker-compose down

# 清理镜像（可选）
docker rmi 998-backend
```

---

## 项目结构

```
.
├── backend/                 # 后端服务
│   ├── include/            # 头文件
│   ├── src/                # 源代码
│   ├── Dockerfile          # Docker构建文件
│   ├── Makefile            # 编译配置
│   └── README.md           # 后端说明
├── docker-compose.yml      # Docker编排
├── .gitignore              # Git忽略配置
└── README.md               # 项目说明
```

## 技术栈

- C++11
- POSIX Socket API
- 多线程 (pthread)
- Docker (Alpine Linux)
