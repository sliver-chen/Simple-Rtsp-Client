project(Simple-Rtsp-Client)

set(CMAKE_C_STANDARD 99)
set(CMAKE_CXX_STANDARD 11)

set(SRC RtspPlayer.cpp sdp.c test.cpp)

add_executable(Simple-Rtsp-Client ${SRC})
