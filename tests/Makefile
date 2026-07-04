CXX ?= c++
# Core is pure C++17 (no Pico headers); doctest.h lives here, core in the parent.
CXXFLAGS ?= -std=c++17 -Wall -Wextra -O0 -g -I . -I ..
SRC = test_main.cpp test_pantograph.cpp
EXISTING = $(wildcard $(SRC))
run: $(EXISTING)
	$(CXX) $(CXXFLAGS) $(EXISTING) -o run
	./run
clean:
	rm -f run
.PHONY: run clean
