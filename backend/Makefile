# Makefile for TCP Communication Backend

CXX = g++
CXXFLAGS = -std=c++11 -Wall -Wextra -O2 -pthread
INCLUDES = -I./include

SRC_DIR = src
SERVER_TARGET = server
CLIENT_TARGET = client

all: $(SERVER_TARGET) $(CLIENT_TARGET)

$(SERVER_TARGET): $(SRC_DIR)/server.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -o $(SERVER_TARGET) $(SRC_DIR)/server.cpp

$(CLIENT_TARGET): $(SRC_DIR)/client.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -o $(CLIENT_TARGET) $(SRC_DIR)/client.cpp

clean:
	rm -f $(SERVER_TARGET) $(CLIENT_TARGET)

.PHONY: all clean
