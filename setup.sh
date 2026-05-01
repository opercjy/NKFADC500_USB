#!/bin/bash
# ====================================================================
# NKFADC500_USB Environment Setup Script
# Usage: source setup.sh
# ====================================================================

# 1. 프로젝트 최상단 디렉토리 자동 인식
export NKFADC500_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" >/dev/null 2>&1 && pwd)"

# 2. Notice Korea 하드웨어 라이브러리 (NKHOME) 및 USB 설정
export NKHOME=/usr/local/notice
export LIBUSB_INC=/usr/include/libusb-1.0
# [참고] RHEL/CentOS/Rocky 계열은 /usr/lib64, Ubuntu/Debian 계열은 /usr/lib/x86_64-linux-gnu 사용
export LIBUSB_LIB=/usr/lib64

# 3. CERN ROOT 환경 변수 로드 (thisroot.sh)
# 이미 ROOTSYS가 잡혀있지 않은 경우에만 실행하도록 처리하여 중복 로드 방지
if [ -z "$ROOTSYS" ]; then
  echo "[INFO] ROOT environment not found. Searching for thisroot.sh..."

  # 주로 사용되는 ROOT 설치 경로들을 순차적으로 탐색하여 로드
  if [ -f "/usr/local/bin/thisroot.sh" ]; then
    source /usr/local/bin/thisroot.sh
  elif [ -f "/home/notice/root/bin/thisroot.sh" ]; then
    source /home/notice/root/bin/thisroot.sh
  elif [ -f "/opt/root/bin/thisroot.sh" ]; then
    source /opt/root/bin/thisroot.sh
  else
    echo "[WARNING] thisroot.sh not found! Please check your ROOT installation path."
  fi
fi

# 4. PATH 설정 (C++ 응용 프로그램 및 Notice bin 동시 등록)
if [ -z "${PATH}" ]; then
  export PATH=$NKFADC500_ROOT/bin:$NKHOME/bin
else
  export PATH=$NKFADC500_ROOT/bin:$NKHOME/bin:$PATH
fi

# 5. 공유 라이브러리(LD_LIBRARY_PATH) 설정
# (USB 통신용 동적 라이브러리 및 Notice 하드웨어 라이브러리 동시 등록)
if [ -z "${LD_LIBRARY_PATH}" ]; then
  export LD_LIBRARY_PATH=$NKFADC500_ROOT/lib:$NKHOME/lib:/usr/local/lib
else
  export LD_LIBRARY_PATH=$NKFADC500_ROOT/lib:$NKHOME/lib:/usr/local/lib:$LD_LIBRARY_PATH
fi

# 6. ROOT C++ 헤더 인식 경로 (오프라인 매크로 AnalyzeSPE.C 등에서 활용)
# NKVME_FADC400의 'objects/include'와 달리, NKFADC500_USB는 최상단 'include/' 디렉토리를 사용합니다.
if [ -z "${ROOT_INCLUDE_PATH}" ]; then
  export ROOT_INCLUDE_PATH=$NKFADC500_ROOT/include:$NKFADC500_ROOT/vendor/kfadc500_usb
else
  export ROOT_INCLUDE_PATH=$NKFADC500_ROOT/include:$NKFADC500_ROOT/vendor/kfadc500_usb:$ROOT_INCLUDE_PATH
fi

# 7. Python 모듈 경로 설정 (GUI 및 CLI 도구 연동)
# NKFADC500_USB는 Python 기반의 GUI와 ZMQ 통신을 사용하므로 PYTHONPATH 설정이 필수적입니다.
if [ -z "${PYTHONPATH}" ]; then
  export PYTHONPATH=$NKFADC500_ROOT/python
else
  export PYTHONPATH=$NKFADC500_ROOT/python:$PYTHONPATH
fi

echo "=========================================================="
echo " ✅ NKFADC500_USB & Notice Hardware Environment Loaded!"
echo "    - 📂 Project Root : $NKFADC500_ROOT"
echo "    - ⚙️ NKHOME        : $NKHOME"
echo "    - 📚 ROOTSYS       : $ROOTSYS"
echo "    - 🛠️ PATH updated (Added Project & Notice bin)"
echo "    - 🔗 LD_LIBRARY_PATH updated (USB & Notice lib)"
echo "    - 🧠 ROOT_INCLUDE_PATH updated (include & vendor headers)"
echo "    - 🐍 PYTHONPATH updated (Python GUI/CLI framework)"
echo "=========================================================="
