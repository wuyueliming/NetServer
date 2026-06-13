CXX = g++
CXXFLAGS = -std=c++17 -Wall -O2 -I../src/server -I../src/server/base -I../src/protocol/HTTP

SRCS_DIR = ../src/server

SRCS = $(SRCS_DIR)/EventLoop.cc \
       $(SRCS_DIR)/Acceptor.cc \
       $(SRCS_DIR)/TcpServer.cc \
       $(SRCS_DIR)/TcpConnection.cc \
       $(SRCS_DIR)/LoopThread.cc \
       $(SRCS_DIR)/LoopThreadPool.cc \
       $(SRCS_DIR)/TimeWheel.cc \
       $(SRCS_DIR)/Channel.cc \
       $(SRCS_DIR)/SignalHandler.cc

all: bench_server

bench_server: $(SRCS) bench_server.cpp
	$(CXX) $(CXXFLAGS) -o $@ bench_server.cpp $(SRCS) -lpthread

clean:
	rm -f bench_server

.PHONY: all clean
