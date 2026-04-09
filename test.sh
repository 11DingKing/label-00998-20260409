#!/bin/bash
# TCP服务器质检测试脚本

set -e

echo "=========================================="
echo "       TCP服务器质检测试"
echo "=========================================="
echo ""

# 颜色定义
GREEN='\033[0;32m'
RED='\033[0;31m'
NC='\033[0m'

pass() { echo -e "${GREEN}✓ $1${NC}"; }
fail() { echo -e "${RED}✗ $1${NC}"; exit 1; }

# 1. 启动服务
echo "1. 启动Docker服务..."
docker-compose up --build -d > /dev/null 2>&1
sleep 2
pass "Docker服务启动完成"

# 2. 检查容器状态
echo ""
echo "2. 检查容器状态..."
if docker-compose ps | grep -q "Up"; then
    pass "容器运行正常"
else
    fail "容器未运行"
fi

# 3. 检查端口监听
echo ""
echo "3. 检查端口监听..."
if nc -zv 127.0.0.1 8888 2>&1 | grep -q "succeeded\|open"; then
    pass "端口8888监听正常"
else
    fail "端口8888未监听"
fi

# 4. 检查服务器日志
echo ""
echo "4. 检查服务器启动日志..."
if docker-compose logs backend 2>&1 | grep -q "服务器已就绪"; then
    pass "服务器启动成功"
else
    fail "服务器启动失败"
fi

# 5. TCP连接测试
echo ""
echo "5. TCP连接测试..."
if echo "" | nc -w 2 127.0.0.1 8888 > /dev/null 2>&1; then
    pass "TCP连接正常"
else
    fail "TCP连接失败"
fi

# 6. 显示服务器日志
echo ""
echo "6. 服务器运行日志（最近10行）:"
echo "----------------------------------------"
docker-compose logs --tail=10 backend 2>&1 | grep -v "WARN"
echo "----------------------------------------"

echo ""
echo "=========================================="
echo -e "${GREEN}       所有测试通过！${NC}"
echo "=========================================="
echo ""
echo "手动测试命令："
echo "  docker exec -it tcp-server ./client 127.0.0.1 8888"
echo ""
echo "停止服务："
echo "  docker-compose down"
