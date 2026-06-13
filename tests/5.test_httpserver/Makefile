CXX = g++
CXXFLAGS = -std=c++17 -Wall -g -I../../src/server -I../../src/server/base -I../../src/protocol/HTTP

SRCS_DIR = ../../src/server

SRCS = $(SRCS_DIR)/EventLoop.cc \
       $(SRCS_DIR)/Acceptor.cc \
       $(SRCS_DIR)/TcpServer.cc \
       $(SRCS_DIR)/LoopThread.cc \
       $(SRCS_DIR)/LoopThreadPool.cc \
       $(SRCS_DIR)/TcpConnection.cc \
       $(SRCS_DIR)/TimeWheel.cc \
       $(SRCS_DIR)/Channel.cc \
       $(SRCS_DIR)/SignalHandler.cc

all: server client1 client2 client3 client4 client5 client6

server: $(SRCS) server.cpp
	$(CXX) $(CXXFLAGS) -o $@ server.cpp $(SRCS)

client1:client1.cpp
	$(CXX) $(CXXFLAGS) -o $@ $^
client2:client2.cpp
	$(CXX) $(CXXFLAGS) -o $@ $^
client3:client3.cpp
	$(CXX) $(CXXFLAGS) -o $@ $^
client4:client4.cpp
	$(CXX) $(CXXFLAGS) -o $@ $^
client5:client5.cpp
	$(CXX) $(CXXFLAGS) -o $@ $^
client6:client6.cpp
	$(CXX) $(CXXFLAGS) -o $@ $^

clean:
	rm -f server client1 client2 client3 client4 client5 client6

.PHONY: all clean
