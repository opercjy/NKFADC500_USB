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

# CERN ROOT 프레임워크 탐색 (오프라인 프로덕션 용도)
# 주의: 빌드 전 터미널에서 `source /path/to/root/bin/thisroot.sh` 가 실행되어 있어야 합니다.
find_package(ROOT REQUIRED COMPONENTS Core Tree RIO)

# -------------------------------------------------------------------
# 3. 벤더 하드웨어 정적 라이브러리 구축 (ROOT 래퍼 제외, 순수 C 코어만)
# -------------------------------------------------------------------
add_library(notice_hw STATIC
    # 💡 [수정] 중복 정의를 막기 위해 nkusb.c와 usb3com.c를 제거하고, 
    # 모든 함수가 포함된 메인 코어 하나만 컴파일합니다.
    vendor/kfadc500_usb/NoticeKFADC500USB.c
)

# 헤더(Header) 파일들을 찾기 위한 Include 경로는 그대로 유지합니다.
target_include_directories(notice_hw PUBLIC 
    ${CMAKE_CURRENT_SOURCE_DIR}/vendor/nkusb/nkusb
    ${CMAKE_CURRENT_SOURCE_DIR}/vendor/usb3com/usb3com
    ${CMAKE_CURRENT_SOURCE_DIR}/vendor/kfadc500_usb
    ${LIBUSB_INCLUDE_DIRS}
)
# -------------------------------------------------------------------
# 4. [Target 1] 고속 Online DAQ 실행 파일 (ROOT 의존성 없음)
# -------------------------------------------------------------------
add_executable(kfadc500_daq
    src/main.cc
    src/ReadDataWorker.cc
    src/ZmqPublisher.cc
    src/ConfigParser.cc   # 💡 [핵심 패치] 파서 소스 코드를 빌드 대상에 추가!
)

target_include_directories(kfadc500_daq PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${ZMQ_INCLUDE_DIRS}
)

target_link_libraries(kfadc500_daq PRIVATE
    notice_hw
    ${LIBUSB_LIBRARIES}
    ${ZMQ_LIBRARIES}
    Threads::Threads
)

# -------------------------------------------------------------------
# 5. [Target 2] Offline ROOT Production 변환기 실행 파일
# -------------------------------------------------------------------
include(${ROOT_USE_FILE}) # ROOT 컴파일 플래그 자동 적용

add_executable(kfadc500_prod
    src/production_main.cc
    src/RootProducer.cc
)

target_include_directories(kfadc500_prod PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${ROOT_INCLUDE_DIRS}
)

# ROOT 라이브러리 직접 링크
target_link_libraries(kfadc500_prod PRIVATE
    ${ROOT_LIBRARIES}
)

# -------------------------------------------------------------------
# 6. 설치 규칙
# -------------------------------------------------------------------
install(TARGETS kfadc500_daq kfadc500_prod DESTINATION bin)