CXX = g++
CXXFLAGS = -std=c++20 -Wall -pthread

all: server

server: server.cpp p1_helper.cpp
	$(CXX) $(CXXFLAGS) -o server server.cpp p1_helper.cpp

clean:
	rm -f server