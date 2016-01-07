cmake_minimum_required (VERSION 2.8)
project (desktop_stream)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -O3 -Wall")

find_package(Threads REQUIRED)

#Only tested with versions 1.55 and 1.56
find_package(Boost 1.54.0 COMPONENTS system regex filesystem coroutine context thread REQUIRED)
include_directories(${Boost_INCLUDE_DIR})

if(APPLE)
  set(OPENSSL_ROOT_DIR "/usr/local/opt/openssl")
endif()

set(process_source_files ./tiny-process-library/process.cpp)

if(WIN32)
  list(APPEND process_source_files ./tiny-process-library/process_win.cpp)
  #If compiled using MSYS2, use sh to run commands
  if(MSYS)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DMSYS_PROCESS_USE_SH")
  endif()
else()
  list(APPEND process_source_files ./tiny-process-library/process_unix.cpp)
endif()

find_package(OpenSSL REQUIRED)
include_directories(${OPENSSL_INCLUDE_DIR})

find_package(Threads REQUIRED)

include_directories(.)
include_directories(./tiny-process-library)
include_directories(./Simple-Web-Server)
include_directories(./Simple-WebSocket-Server)

add_executable(desktop_stream main.cpp ${process_source_files})
target_link_libraries(desktop_stream ${Boost_LIBRARIES})
target_link_libraries(desktop_stream ${OPENSSL_CRYPTO_LIBRARY})
target_link_libraries(desktop_stream ${CMAKE_THREAD_LIBS_INIT})

if(MSYS)
    target_link_libraries(desktop_stream ws2_32 wsock32)
endif()

