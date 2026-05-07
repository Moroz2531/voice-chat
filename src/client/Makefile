TARGET = client

CXX = clang++
CXXFLAGS = -O3 -Wall -Werror -Wextra -std=c++20 -I ..

LDFLAGS = -L../lib -L/usr/local/lib
LDLIBS = -lsocket -lepoll -lportaudio -lasound -lm -lpthread -lportaudiocpp

SRC = $(wildcard *.cpp)
OBJ = $(patsubst %.cpp, %.o, $(SRC))

all: build_libs $(TARGET)

build_libs:
	$(MAKE) -C ../lib/

$(TARGET): $(OBJ)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS) $(LDLIBS)
	strip $(TARGET)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -rf *.o $(TARGET)
	$(MAKE) -C ../lib/ clean

.PHONY: all clean build_libs