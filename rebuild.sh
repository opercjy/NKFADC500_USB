#!/bin/bash

# ==============================================================================
# KFADC500 DAQ Safe Rebuild Script
# Description: Prevents accidental 'rm -rf *' by automating the clean build process.
# ==============================================================================

echo -e "\n\033[1;33m[SYSTEM:INFO] Initiating Safe Clean Build Sequence...\033[0m"

# 1. 루트 디렉토리인지 확인 (src 폴더 존재 여부로 간단히 체크)
if [ ! -d "src" ]; then
    echo -e "\033[1;31m[SYSTEM:ERROR] Please run this script from the project root directory.\033[0m"
    exit 1
fi

# 2. 기존 build 폴더를 통째로 안전하게 삭제 후 재생성 (rm -rf * 타이핑 방지)
echo -e "\033[1;36m[SYSTEM:INFO] Cleaning old build directory...\033[0m"
rm -rf build
mkdir build

# 3. build 폴더로 이동하여 CMake 및 Make 병렬 컴파일 수행
cd build
echo -e "\033[1;36m[SYSTEM:INFO] Running CMake...\033[0m"
cmake ..

echo -e "\033[1;36m[SYSTEM:INFO] Compiling sources (make -j4)...\033[0m"
make -j4

# 4. 컴파일 성공 여부 확인
if [ $? -eq 0 ]; then
    cd ..
    echo -e "\n\033[1;32m============================================================\033[0m"
    echo -e "\033[1;32m [SUCCESS] Build completed safely! \033[0m"
    echo -e "\033[1;32m============================================================\033[0m"
    echo -e " Executables are ready in the \033[1;36m./bin/\033[0m directory."
    echo -e " Run DAQ: \033[1;33m./bin/kfadc500_daq -f config/kfadc500.config\033[0m\n"
else
    cd ..
    echo -e "\n\033[1;31m============================================================\033[0m"
    echo -e "\033[1;31m [FAILED] Compilation error occurred. \033[0m"
    echo -e "\033[1;31m============================================================\033[0m\n"
    exit 1
fi