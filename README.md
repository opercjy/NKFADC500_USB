
# KFADC500 High-Performance DAQ Framework (CLI Checkpoint)

![C++17](https://img.shields.io/badge/C++-17-blue.svg) ![CMake](https://img.shields.io/badge/CMake-3.15+-green.svg) ![Python](https://img.shields.io/badge/Python-3.8+-yellow.svg) ![ZeroMQ](https://img.shields.io/badge/ZeroMQ-IPC-red.svg) ![ROOT](https://img.shields.io/badge/CERN_ROOT-6.x-white.svg) ![Notice Certified](https://img.shields.io/badge/Notice_Hardware-Verified_Maintenance-orange.svg)

**License & Notice:** 본 프레임워크는 고에너지 물리(HEP) 실험 및 학술 연구 목적으로 개발 및 유지보수되고 있습니다. `vendor/` 디렉토리 내에 포함된 로우레벨 하드웨어 제어 C 드라이버 소스 코드의 모든 지적 재산권(Intellectual Property)은 **Notice Korea**에 귀속됩니다. 
*(본 문서는 대한민국 연구진을 위해 한국어로 작성되었습니다. 해외 연구자분들은 번역 플랫폼을 적극 활용해 주시기 바랍니다.)*

**KFADC500 High-Performance DAQ**는 Notice Korea의 USB 3.0 기반 고속 파형 디지타이저(KFADC500, 500MS/s, 4CH)를 위한 엔터프라이즈급 무결성 데이터 수집 프레임워크입니다. 

기존 제조사(Vendor)가 제공하는 기본 C API의 한계(메모리 누수, 패킷 찢어짐, 데드락, 유저 편의성 부재)를 극복하기 위해, **순수 C++17과 libusb-1.0을 결합하여 바닥부터 재설계(From-Scratch)** 되었습니다. 고에너지 물리 실험 및 극한의 노이즈가 발생하는 실제 빔 타임(Beam-time) 환경에서 시스템 다운 없이 최대 5Gbps 대역폭을 100% 소화하도록 설계되었습니다. 현재 버전은 완벽한 Command Line Interface(CLI), ZMQ 기반 Python 실시간 모니터링, 그리고 초고속 오프라인 프로덕션 파이프라인이 모두 구축된 안정화 체크포인트 릴리즈입니다.

---

## Core Architecture & Features (The "Immortal" DAQ)

기존 랩실용 PoC 수준의 벤더 코드를 완전히 격리하고, 아래의 핵심 아키텍처를 도입했습니다.

1. **Zero-Malloc Object Pool (메모리 폭발 원천 봉쇄)**
   - 초당 수백 MB가 쏟아지는 환경에서 런타임 `new` / `malloc` 호출은 치명적인 CPU 오버헤드와 OOM을 유발합니다. 시작 시 1MB 단위의 대용량 데이터 블록 1,000개를 Pool에 미리 할당하여 동적 메모리 할당을 0회로 강제합니다.
2. **Mega-Bulk Fetch & Residual Stitching (찢어진 패킷 완벽 복원)**
   - USB 3.0 대역폭 극대화를 위해 `libusb_bulk_transfer`를 1MB 단위로 호출합니다. 통신 지연으로 인해 패킷 경계가 잘려서 들어오는 현상(Torn Packet)을 완벽히 이어 붙이는 알고리즘을 탑재했습니다.
3. **Graceful Shutdown & Interruption Handling (안전 종료 로직)**
   - DAQ 수집 중이거나 수십 GB의 ROOT 프로덕션 도중 언제든 `Ctrl+C`를 누르면, 즉각 하드웨어 래치를 잠그고 진행 중인 마지막 메모리 블록까지 디스크에 안전하게 기록한 뒤 우아하게 종료됩니다. (좀비 파일 생성 및 데이터 유실 원천 차단)
4. **Hybrid ZMQ Pipeline (Language Decoupling)**
   - 무거운 C++ 코어는 오직 하드웨어 제어와 디스크 쓰기에만 집중하고, ZMQ PUB 소켓으로 바이너리 스트림을 송출합니다. 시각화는 가벼운 Python + PyQtGraph 프론트엔드가 담당하여 DAQ 시스템의 오버헤드를 0으로 유지합니다.
5. **Data & Binary Isolation (안전한 파일 시스템 구조)**
   - 빌드 시 실행 파일(`bin/`), 데이터 저장소(`data/`), 파이썬 환경(`python/`)을 완벽히 분리하여 소스 코드 영역을 청결하게 유지합니다.

---

## Directory Structure

제조사의 레거시 드라이버 코드는 `vendor/` 디렉토리에 철저히 캡슐화되어 메인 DAQ 파이프라인의 안전성을 침해하지 않습니다.

```text
KFADC500_DAQ/
 ├── bin/                   # 컴파일된 최종 C++ 실행 파일 (kfadc500_daq, kfadc500_prod)
 ├── data/                  # 수집된 Raw Data(.dat) 및 ROOT 파일(.root) 전용 공간
 ├── config/                # 직관적인 INI 포맷의 장비 설정 파일
 ├── python/                # Python 기반 ZMQ 실시간 온라인 모니터링 생태계
 │   ├── requirements.txt   # Python 의존성 파일
 │   └── online_monitor.py  # 파스텔 라이트 테마 실시간 파형/전하량 뷰어
 ├── rules/                 # Sudo 권한 없이 USB 접근을 허용하는 Udev 스크립트
 ├── vendor/                # Notice Korea 제공 로우레벨 래퍼 (안전 격리됨)
 ├── src/ & include/        # 메인 시스템 C++ 소스 코드 및 헤더
 ├── AnalyzeSPE.C           # MINUIT2 기반 단일 광전자(SPE) 정밀 피팅 매크로
 └── rebuild.sh             # 안전한 클린 빌드 자동화 스크립트 (rm -rf * 휴먼 에러 방지)
```

---

## Prerequisites & Build Instructions

### Dependencies (Rocky Linux 9 기준)
- `GCC/G++ 11.0+` (C++17 Support), `CMake 3.15+`
- `libusb-1.0-devel`, `zeromq-devel`
- `CERN ROOT 6.x`
- `Python 3.8+` (온라인 모니터링용)

```bash
# OS 패키지 설치
sudo dnf install epel-release
sudo dnf install gcc-c++ cmake make pkgconf-pkg-config libusb1-devel zeromq-devel

# Python 가상환경 패키지 설치
pip install -r python/requirements.txt
```

### Build & Udev Rules Setup
장비에 일반 유저(Non-root) 권한으로 접근하기 위해 Udev 룰을 먼저 세팅한 뒤, 안전 빌드 스크립트를 실행합니다.
```bash
cd rules && ./setup_env.sh
cd ..

# 직접 build 폴더를 제어하지 마시고, 제공되는 스크립트를 사용하십시오.
chmod +x rebuild.sh
./rebuild.sh
```

---

## Usage Guide

모든 명령어는 프로젝트 최상단 디렉토리에서 실행합니다.

### 1. Online DAQ (`kfadc500_daq`)
초고속 데이터 획득을 수행합니다. 설정 테이블과 실시간 자원(Queue/Pool) 모니터링이 제공됩니다. (종료 시 `Ctrl+C`)
```bash
./bin/kfadc500_daq -f config/kfadc500.config -o data/run_001.dat
```

### 2. Real-Time Online Monitoring (`online_monitor.py`)
DAQ가 실행 중일 때 새로운 터미널 창을 열고 실행합니다. ZMQ로 데이터를 수신하여 눈이 편안한 오프화이트(Off-White) 테마 기반의 60FPS 실시간 파형 및 전하량 히스토그램을 렌더링합니다. (창 닫기 혹은 `Ctrl+C`로 안전 종료)
```bash
python python/online_monitor.py
```

### 3. Offline ROOT Production (`kfadc500_prod`)
수집된 Raw 바이너리(`.dat`) 파일을 물리 분석을 위한 ROOT `TTree`로 초고속 변환합니다. 변환 종료 시 처리 속도(MB/s)가 리포팅됩니다.
```bash
# [MODE 1] 초고속 배치 모드 (-w 옵션 추가 시 파형 배열까지 통째로 저장)
./bin/kfadc500_prod data/run_001.dat -w

# [MODE 2] 인터랙티브 디스플레이 모드 (파일 저장 없이 ROOT TCanvas로 파형 탐색)
./bin/kfadc500_prod -d data/run_001.dat
```

### 4. Physics Analysis: SPE Calibration (`AnalyzeSPE.C`)
ROOT `MINUIT2` 최소화 엔진을 탑재하여 PMT의 단일 광전자(SPE) 스펙트럼을 정밀하게 피팅합니다.
```bash
root -l -q 'AnalyzeSPE.C("data/run_001.root", 0)'  
```

---

## Development Roadmap

- [x] **Phase 1: Core CLI & Raw Binary DAQ**
  - Zero-Malloc Object Pool 기반의 1MB Mega-Bulk 수집 코어 완성.
  - ANSI C++ 기반의 터미널 UX 및 `Ctrl+C` Graceful Shutdown 방어 로직 적용 완료.

- [x] **Phase 2: Offline ROOT Production & Physics Analysis**
  - 트리거 래치 노이즈 마스킹 및 TTree 초고속(수십 MB/s) 일괄 변환기 구현.
  - 디렉토리 격리(`bin`, `data`) 및 `rebuild.sh` 안전 빌드 파이프라인 구축.
  - MINUIT2 엔진 기반 단일 광전자(SPE) 정밀 분석 매크로 완성.

- [x] **Phase 3: Real-Time Online Monitor (Python / NumPy / PyQtGraph)**
  - C++ 백엔드(ZMQ PUB)와 Python 프론트엔드(ZMQ SUB)를 완벽히 분리한 실시간 뷰어 구축.
  - `numpy.frombuffer()`를 활용한 Zero-Copy 초고속 바이너리 디코딩.
  - 장시간 현장 연구의 피로도를 낮추는 오프화이트/파스텔 다크 테마 및 0점 베이스라인 렌더링 적용.

- [ ] **Phase 4: Advanced Monitoring GUI Dashboard**
  - PyQt/PySide 기반의 완전한 통합 관제 대시보드 구축.
  - 빔 타임(Beam-time) 중 채널별 실시간 트리거 레이트, 페데스탈 요동 추이 등을 종합적으로 시각화하는 엔터프라이즈급 GUI 프론트엔드 완성 예정.

---

## Acknowledgments
본 연구 및 시스템 개발은 대한민국 정부의 연구비 지원을 받아 수행되고 있습니다. 기초과학 발전을 위한 헌신적인 성원과 지원에 깊은 감사를 드립니다.
