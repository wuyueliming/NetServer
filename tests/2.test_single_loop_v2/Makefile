CXX = g++
CXXFLAGS = -std=c++17 -Wall -g -I../../src/server -I../../src/server/base

SRCS_DIR = ../../src/server

SRCS = $(SRCS_DIR)/Reactor.cc \
       $(SRCS_DIR)/Acceptor.cc \
       $(SRCS_DIR)/TcpServer.cc \
       $(SRCS_DIR)/LoopThread.cc \
       $(SRCS_DIR)/ThreadPool.cc \
       $(SRCS_DIR)/TcpConnection.cc \
       $(SRCS_DIR)/TimeWheel.cc \
       $(SRCS_DIR)/Channel.cc \
       $(SRCS_DIR)/SignalHandler.cc

all: server client

server: $(SRCS) server.cpp
	$(CXX) $(CXXFLAGS) -o $@ server.cpp $(SRCS) -lpthread

client: client.cpp
	$(CXX) $(CXXFLAGS) -o $@ client.cpp -lpthread

clean:
	rm -f server client

.PHONY: all clean
