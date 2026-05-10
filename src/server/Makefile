TARGET = server

CXX = clang++
CXXFLAGS = -Wall -Werror -Wextra -std=c++20 -I .. -O3

LDFLAGS = -L../lib
LDLIBS = -lsocket -lepoll

SRC = $(wildcard *.cpp)
OBJ = $(patsubst %.cpp, %.o, $(SRC))

all: build_libs $(TARGET)

build_libs:
	$(MAKE) -C ../lib/

$(TARGET): $(OBJ)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS) $(LDLIBS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -rf *.o $(TARGET)
	$(MAKE) -C ../lib/ clean

.PHONY: all clean build_libs