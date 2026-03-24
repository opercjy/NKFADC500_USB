cmake_minimum_required(VERSION 3.15)
project(KFADC500_DAQ VERSION 1.0.0 LANGUAGES C CXX)

# 1. 최신 C++ 표준 및 컴파일러 플래그 설정
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 99)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -Wall -Wextra -Wno-unused-parameter")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O3 -Wall")

# 2. 외부 의존성 라이브러리 탐색 (libusb-1.0, ZeroMQ, Threads)
find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBUSB REQUIRED libusb-1.0)
pkg_check_modules(ZMQ REQUIRED libzmq)
find_package(Threads REQUIRED)

# -------------------------------------------------------------------
# 3. 💡 벤더 하드웨어 라이브러리 구축 (usb3com.c 추가 완료!)
# -------------------------------------------------------------------
add_library(notice_hw STATIC
    vendor/NoticeKFADC500USB.c
    vendor/nkusb.c
    vendor/usb3com.c   # <-- USB 3.0 코어 라이브러리 추가!
)
target_include_directories(notice_hw PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/vendor)

# 4. 차세대 DAQ 메인 실행 파일 구축
add_executable(kfadc500_daq
    src/main.cc
    src/ReadDataWorker.cc
    src/ZmqPublisher.cc
)

target_include_directories(kfadc500_daq PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/vendor
    ${LIBUSB_INCLUDE_DIRS}
    ${ZMQ_INCLUDE_DIRS}
)

# 라이브러리 링크 (벤더 lib + libusb + ZMQ + pthread)
target_link_libraries(kfadc500_daq PRIVATE
    notice_hw
    ${LIBUSB_LIBRARIES}
    ${ZMQ_LIBRARIES}
    Threads::Threads
)

install(TARGETS kfadc500_daq DESTINATION bin)