#!/bin/bash

# ==============================================================================
# KFADC500 DAQ Safe Rebuild Script
# Description: Prevents accidental 'rm -rf *' by automating the clean build process.
# ==============================================================================

echo -e "\n\033[1;33m[SYSTEM:INFO] Initiating Safe Clean Build Sequence...\033[0m"

if [ ! -d "src" ]; then
    echo -e "\033[1;31m[SYSTEM:ERROR] Please run this script from the project root directory.\033[0m"
    exit 1
fi

echo -e "\033[1;36m[SYSTEM:INFO] Cleaning old build directory...\033[0m"
rm -rf build
mkdir build

cd build
echo -e "\033[1;36m[SYSTEM:INFO] Running CMake...\033[0m"
cmake ..

echo -e "\033[1;36m[SYSTEM:INFO] Compiling sources (make -j4)...\033[0m"
make -j4

if [ $? -eq 0 ]; then
    cd ..
    echo -e "\n\033[1;32m============================================================\033[0m"
    echo -e "\033[1;32m [SUCCESS] Build completed safely! \033[0m"
    echo -e "\033[1;32m============================================================\033[0m"
    echo -e " Executables are ready in the \033[1;36m./bin/\033[0m directory.\n"
    
    # 💡 [안내 메시지 갱신] CLI와 GUI 대시보드 실행 방법을 모두 안내합니다.
    echo -e " \033[1;35m[ 1. CLI Mode (Terminal) ]\033[0m"
    echo -e " Run DAQ: \033[1;33m./bin/kfadc500_daq -f config/kfadc500.config -o data/run_001.dat\033[0m\n"
    
    echo -e " \033[1;35m[ 2. GUI Mode (Unified Dashboard) ]\033[0m"
    echo -e " Run Monitor: \033[1;33m./bin/kfadc500_mon\033[0m\n"
else
    cd ..
    echo -e "\n\033[1;31m============================================================\033[0m"
    echo -e "\033[1;31m [FAILED] Compilation error occurred. \033[0m"
    echo -e "\033[1;31m============================================================\033[0m\n"
    exit 1
fi