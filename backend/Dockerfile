# 多阶段构建 - 支持 ARM64 和 AMD64
FROM --platform=$BUILDPLATFORM alpine:3.19 AS builder

# 安装编译工具
RUN apk add --no-cache g++ make musl-dev

WORKDIR /app

# 复制源代码
COPY include/ ./include/
COPY src/ ./src/
COPY Makefile ./

# 编译
RUN make all

# 运行阶段 - 使用轻量级镜像
FROM alpine:3.19

RUN apk add --no-cache libstdc++ libgcc

WORKDIR /app

# 从构建阶段复制编译好的二进制文件
COPY --from=builder /app/server ./
COPY --from=builder /app/client ./

# 暴露端口
EXPOSE 8888

# 默认启动服务器
CMD ["./server", "8888"]
