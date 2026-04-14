#!/bin/bash
# ============================================================
# Echo Benchmark 一键运行脚本
# ============================================================

set -e

BENCH="./cmake-build-debug/benchmark/echo_bench"
SERVER_IP="127.0.0.1"
SERVER_PORT=8888
SERVER_THREADS=16

# 测试矩阵（常规测试）
declare -a MESSAGE_SIZES=(100 1024 65536)
CONNECTIONS=800
DURATION=15

# C10K 测试配置
C10K_CONNECTIONS=10000
C10K_MESSAGE_SIZE=100
C10K_DURATION=30

# 日志文件
LOG_DIR="$(dirname "$0")/logs"
mkdir -p "$LOG_DIR"
SERVER_LOG="$LOG_DIR/server_$(date +%Y%m%d_%H%M%S).log"

# 颜色
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
RED='\033[0;31m'
NC='\033[0m'

echo -e "${GREEN}============================================${NC}"
echo -e "${GREEN}  HyperMuduo Echo Benchmark Suite${NC}"
echo -e "${GREEN}============================================${NC}"

# 检查编译产物
if [ ! -f "$BENCH" ]; then
    echo -e "${RED}Error: $BENCH not found. Run 'cmake --build cmake-build-debug' first.${NC}"
    exit 1
fi

# 检查系统配置（C10K 需要）
check_system_for_c10k() {
    local current_limit=$(ulimit -n)
    local recommended_limit=65536

    if [ "$current_limit" -lt "$recommended_limit" ]; then
        echo -e "${YELLOW}  [Warning] File descriptor limit: $current_limit (recommended: $recommended_limit for C10K)${NC}"
        echo -e "${YELLOW}  [Warning] Consider running: ulimit -n $recommended_limit${NC}"
    else
        echo -e "${GREEN}  [OK] File descriptor limit: $current_limit${NC}"
    fi
}

# 启动服务器（后台，日志写入文件）
echo -e "\n${YELLOW}[1/4] Starting Echo Server on port $SERVER_PORT ($SERVER_THREADS threads)${NC}"
echo -e "${CYAN}  Server log: $SERVER_LOG${NC}"
$BENCH server $SERVER_PORT $SERVER_THREADS > $SERVER_LOG 2>&1 &
SERVER_PID=$!
sleep 2

# 检查服务器是否启动成功
if ! kill -0 $SERVER_PID 2>/dev/null; then
    echo -e "${RED}Error: Server failed to start. Check $SERVER_LOG${NC}"
    exit 1
fi
echo -e "${GREEN}  Server started (PID: $SERVER_PID)${NC}"

# 运行常规测试（800 连接，3 种消息大小）
echo -e "\n${YELLOW}[2/4] Running Standard Benchmark Tests (800 connections)...${NC}"

TEST_NUM=0
for MSG_SIZE in "${MESSAGE_SIZES[@]}"; do
    TEST_NUM=$((TEST_NUM + 1))
    echo -e "\n${GREEN}============================================${NC}"
    echo -e "${GREEN}  Test #$TEST_NUM: ${CONNECTIONS} connections, ${MSG_SIZE} bytes, ${DURATION}s${NC}"
    echo -e "${GREEN}============================================${NC}\n"

    $BENCH client $SERVER_IP $SERVER_PORT $CONNECTIONS $MSG_SIZE $DURATION

    sleep 1
done

# 运行 C10K 测试
echo -e "\n${YELLOW}[3/4] Running C10K Test (10,000 connections)...${NC}"
ulimit -n 65536
check_system_for_c10k

echo -e "\n${GREEN}============================================${NC}"
echo -e "${GREEN}  C10K Test: ${C10K_CONNECTIONS} connections, ${C10K_MESSAGE_SIZE} bytes, ${C10K_DURATION}s${NC}"
echo -e "${GREEN}============================================${NC}\n"

$BENCH client $SERVER_IP $SERVER_PORT $C10K_CONNECTIONS $C10K_MESSAGE_SIZE $C10K_DURATION

# 停止服务器
echo -e "\n${YELLOW}[4/4] Stopping server...${NC}"
kill $SERVER_PID 2>/dev/null || true
wait $SERVER_PID 2>/dev/null || true
echo -e "${GREEN}  Server stopped${NC}"

echo -e "\n${GREEN}============================================${NC}"
echo -e "${GREEN}  Benchmark Complete${NC}"
echo -e "${GREEN}============================================${NC}"
echo -e "${CYAN}  Server log: $SERVER_LOG${NC}"
echo -e "${CYAN}  Logs: $LOG_DIR/*.log${NC}"
echo -e "${GREEN}============================================${NC}"
