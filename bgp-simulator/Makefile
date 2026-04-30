CXX = g++
CXXFLAGS = -std=c++23 -Wall -Wextra -g -Iinclude

all:
	$(CXX) $(CXXFLAGS) src/*.cpp -o bgp_sim

test:
	$(CXX) $(CXXFLAGS) src/graph_loader.cpp src/cycle_detection.cpp tests/test_graph.cpp -o test_graph
	$(CXX) $(CXXFLAGS) src/graph_loader.cpp src/cycle_detection.cpp tests/test_cycle.cpp -o test_cycle
	$(CXX) $(CXXFLAGS) src/propagation_rank.cpp src/simulator.cpp src/csv_writer.cpp src/rov_loader.cpp src/graph_loader.cpp src/cycle_detection.cpp tests/test_propagation.cpp -o test_propagation
	$(CXX) $(CXXFLAGS) src/propagation_rank.cpp src/simulator.cpp src/csv_writer.cpp src/rov_loader.cpp src/graph_loader.cpp src/cycle_detection.cpp tests/test_rov.cpp -o test_rov
	./test_graph
	./test_cycle
	./test_propagation
	./test_rov

clean:
	rm -f bgp_sim test_graph test_cycle test_propagation test_rov *.csv