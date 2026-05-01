cmake_minimum_required(VERSION 3.15)
project(KFADC500_DAQ VERSION 1.0.0 LANGUAGES C CXX)

# -------------------------------------------------------------------
# 1. C/C++ 표준 및 컴파일러 플래그 설정
# -------------------------------------------------------------------
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 99)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -Wall -Wextra -Wno-unused-parameter")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O3 -Wall -Wno-pointer-sign")

# ==============================================================================
# 💡 [UX/UI 패치] 최종 빌드 산출물을 bin/ 과 lib/ 디렉토리로 자동 분리
# ==============================================================================
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/lib)
file(MAKE_DIRECTORY ${CMAKE_SOURCE_DIR}/data)

# -------------------------------------------------------------------
# 2. 외부 의존성 탐색 (libusb-1.0, ZeroMQ, Threads, ROOT)
# -------------------------------------------------------------------
find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBUSB REQUIRED libusb-1.0)
pkg_check_modules(ZMQ REQUIRED libzmq)
find_package(Threads REQUIRED)

# CERN ROOT 프레임워크 탐색
find_package(ROOT REQUIRED COMPONENTS Core Tree RIO)
# 💡 [위치 수정] ROOT 환경 설정을 글로벌로 끌어올림
include(${ROOT_USE_FILE}) 

# -------------------------------------------------------------------
# 3. 벤더 하드웨어 정적 라이브러리 구축 
# -------------------------------------------------------------------
add_library(notice_hw STATIC
    vendor/kfadc500_usb/NoticeKFADC500USB.c
)

target_include_directories(notice_hw PUBLIC 
    ${CMAKE_CURRENT_SOURCE_DIR}/vendor/nkusb/nkusb
    ${CMAKE_CURRENT_SOURCE_DIR}/vendor/usb3com/usb3com
    ${CMAKE_CURRENT_SOURCE_DIR}/vendor/kfadc500_usb
    ${LIBUSB_INCLUDE_DIRS}
)

# -------------------------------------------------------------------
# 4. [Target 1] 고속 Online DAQ 실행 파일 
# -------------------------------------------------------------------
add_executable(kfadc500_daq
    src/main.cc
    src/ReadDataWorker.cc
    src/ZmqPublisher.cc
    src/ConfigParser.cc
    src/RootProducer.cc   # 💡 [추가] 실시간 온라인 적분을 위해 포함!
)

target_include_directories(kfadc500_daq PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${ZMQ_INCLUDE_DIRS}
    ${ROOT_INCLUDE_DIRS}  # 💡 [추가] ROOT 헤더 인식
)

target_link_libraries(kfadc500_daq PRIVATE
    notice_hw
    ${LIBUSB_LIBRARIES}
    ${ZMQ_LIBRARIES}
    ${ROOT_LIBRARIES}     # 💡 [추가] ROOT 라이브러리 링크
    Threads::Threads
)

# -------------------------------------------------------------------
# 5. [Target 2] Offline ROOT Production 변환기 실행 파일
# -------------------------------------------------------------------
add_executable(kfadc500_prod
    src/production_main.cc
    src/RootProducer.cc
)

target_include_directories(kfadc500_prod PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${ROOT_INCLUDE_DIRS}
)

target_link_libraries(kfadc500_prod PRIVATE
    ${ROOT_LIBRARIES}
)

# -------------------------------------------------------------------
# 6. 설치 규칙 및 ZMQ GUI 런처 자동 생성
# -------------------------------------------------------------------
install(TARGETS kfadc500_daq kfadc500_prod DESTINATION bin)

message(STATUS "[PHASE 4] Setting up Python ZMQ GUI Dashboard Environment...")
set(MONITOR_SCRIPT "${CMAKE_SOURCE_DIR}/bin/kfadc500_mon")
file(WRITE ${MONITOR_SCRIPT} "#!/bin/bash\n")
file(APPEND ${MONITOR_SCRIPT} "# KFADC500 Unified Monitoring Dashboard Launcher\n\n")
file(APPEND ${MONITOR_SCRIPT} "export PYTHONPATH=\$PYTHONPATH:${CMAKE_SOURCE_DIR}/python/gui\n")
file(APPEND ${MONITOR_SCRIPT} "python3 ${CMAKE_SOURCE_DIR}/python/gui/main.py \"\$@\"\n")
execute_process(COMMAND chmod +x ${MONITOR_SCRIPT})
message(STATUS "[PHASE 4] GUI Launcher created: ${MONITOR_SCRIPT}")