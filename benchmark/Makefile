CXX = g++
CXXFLAGS = -std=c++17 -Wall -O2 -I../src/server -I../src/server/base -I../src/protocol/HTTP -MMD -MP

SRCS_DIR = ../src/server
SRCS = $(SRCS_DIR)/Reactor.cc \
       $(SRCS_DIR)/Acceptor.cc \
       $(SRCS_DIR)/TcpServer.cc \
       $(SRCS_DIR)/TcpConnection.cc \
       $(SRCS_DIR)/LoopThread.cc \
       $(SRCS_DIR)/ThreadPool.cc \
       $(SRCS_DIR)/TimeWheel.cc \
       $(SRCS_DIR)/Channel.cc \
       $(SRCS_DIR)/SignalHandler.cc \
       $(SRCS_DIR)/Connector.cc \
       $(SRCS_DIR)/TcpClient.cc

# 编译服务器对象文件
SRCS_OBJS = $(SRCS:.cc=.o)

%.o: %.cc
	$(CXX) $(CXXFLAGS) -c -o $@ $<

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<

all: bench_server bench_server_opt

bench_server.cpp bench_server_opt.cpp:
	# 这些文件不作为 .o 编译，而是直接用于链接
	# .d 文件自动追踪头文件依赖

bench_server: $(SRCS_OBJS) bench_server.o
	$(CXX) $(CXXFLAGS) -o $@ bench_server.o $(SRCS_OBJS) -lpthread

bench_server_opt: $(SRCS_OBJS) bench_server_opt.o
	$(CXX) $(CXXFLAGS) -o $@ bench_server_opt.o $(SRCS_OBJS) -lpthread

# 头文件依赖追踪
-include $(SRCS:.cc=.d) bench_server.d bench_server_opt.d

clean:
	rm -f bench_server bench_server_opt $(SRCS_OBJS) *.d

.PHONY: all clean
