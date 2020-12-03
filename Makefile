CXX=g++
CXXFLAGS=-std=c++14 -Wall -pedantic -pthread -lboost_system
CXX_INCLUDE_DIRS=/usr/local/include
CXX_INCLUDE_PARAMS=$(addprefix -I , $(CXX_INCLUDE_DIRS))
CXX_LIB_DIRS=/usr/local/lib
CXX_LIB_PARAMS=$(addprefix -L , $(CXX_LIB_DIRS))

part1: 
	$(CXX) echo_server.cpp -o http_server $(CXX_INCLUDE_PARAMS) $(CXX_LIB_PARAMS) $(CXXFLAGS)
	g++ console.cpp -o console.cgi -std=c++11 -Wall -pedantic -pthread -lboost_system
part2: main.cpp
	g++ main.cpp -o cgi_server.exe -lws2_32 -lwsock32 -std=c++14

clean:
	rm -f http_server
	rm -f console.cgi