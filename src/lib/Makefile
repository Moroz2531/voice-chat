TARGET_SOCKET = libsocket.a
TARGET_EPOLL = libepoll.a

CPP = clang++
CPPFLAGS = -Wall -Werror -Wextra -std=c++20 -g3
AR = ar
ARFLAGS = rcs

all: $(TARGET_SOCKET) $(TARGET_EPOLL)

$(TARGET_SOCKET):
	$(CPP) $(CPPFLAGS) -c socket.cpp -o socket.o
	$(AR) $(ARFLAGS) $@ socket.o
	rm -f socket.o

$(TARGET_EPOLL):
	$(CPP) $(CPPFLAGS) -c epoll.cpp -o epoll.o
	$(AR) $(ARFLAGS) $@ epoll.o
	rm -f epoll.o

clean:
	rm -rf *.o $(TARGET_SOCKET) $(TARGET_EPOLL)

.PHONY: all clean