cmake_minimum_required(VERSION 3.14)

project(chat LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_COMPILER clang++)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_CXX_FLAGS_DEBUG "-g -O0 -fsanitize=address")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -Wall -Werror -Wextra")

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets)
find_package(PkgConfig REQUIRED)

pkg_check_modules(PORTAUDIO_CPP REQUIRED portaudiocpp)

set(ROOT_PATH ${CMAKE_SOURCE_DIR}/../..)

# Добавляем ВСЕ необходимые include директории
include_directories(${ROOT_PATH})                    # Чтобы найти src/
include_directories(${ROOT_PATH}/src)                # Чтобы найти client/
include_directories(${ROOT_PATH}/src/general)        # Для general/parse.hpp
include_directories(${ROOT_PATH}/src/lib)

add_executable(chat
  main.cpp
  welcomewindow.cpp
  welcomewindow.hpp
  mainwindow.cpp
  mainwindow.hpp
  ${ROOT_PATH}/src/client/client.hpp
  ${ROOT_PATH}/src/client/client.cpp
  ${ROOT_PATH}/src/client/vstream.cpp
  ${ROOT_PATH}/src/general/parse.hpp
  ${ROOT_PATH}/src/lib/epoll.cpp
  ${ROOT_PATH}/src/lib/epoll.hpp
  ${ROOT_PATH}/src/lib/socket.cpp
  ${ROOT_PATH}/src/lib/socket.hpp
)

target_link_libraries(chat PRIVATE Qt${QT_VERSION_MAJOR}::Widgets  ${PORTAUDIO_CPP_LIBRARIES})
target_compile_options(chat PRIVATE ${PORTAUDIO_CPP_CFLAGS_OTHER})

include(GNUInstallDirs)
install(TARGETS chat
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)