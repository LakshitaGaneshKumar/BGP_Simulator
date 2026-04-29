CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -g -Iinclude
TARGET = bgp_sim

all:
	$(CXX) $(CXXFLAGS) src/*.cpp -o $(TARGET)

clean:
	rm -f $(TARGET)

test: test_graph test_cycle test_propagation
	./test_graph
	./test_cycle
	./test_propagation

test_graph:
	$(CXX) $(CXXFLAGS) src/graph_loader.cpp src/cycle_detection.cpp tests/test_graph.cpp -o test_graph

test_cycle:
	$(CXX) $(CXXFLAGS) src/graph_loader.cpp src/cycle_detection.cpp tests/test_cycle.cpp -o test_cycle

test_propagation:
	$(CXX) $(CXXFLAGS) tests/test_propagation.cpp -o test_propagation