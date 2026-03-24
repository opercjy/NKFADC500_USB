
# KFADC500 High-Performance DAQ Framework (CLI Checkpoint)

![C++17](https://img.shields.io/badge/C++-17-blue.svg) ![CMake](https://img.shields.io/badge/CMake-3.15+-green.svg) ![libusb](https://img.shields.io/badge/libusb-1.0-orange.svg) ![ZeroMQ](https://img.shields.io/badge/ZeroMQ-IPC-red.svg) ![ROOT](https://img.shields.io/badge/CERN_ROOT-6.x-white.svg) ![Notice Certified](https://img.shields.io/badge/Notice_Hardware-Verified_Maintenance-orange.svg)

**Notice:** 본 문서는 대한민국 연구진 및 사용자를 위해 한국어로 작성되었습니다. 영어나 기타 언어를 사용하시는 연구자분들은 웹 브라우저나 번역 플랫폼의 번역 기능(Translation tools)을 적극 활용해 주시기 바랍니다.

**KFADC500 High-Performance DAQ**는 Notice Korea의 USB 3.0 기반 고속 파형 디지타이저(KFADC500, 500MS/s, 4CH)를 위한 엔터프라이즈급 무결성 데이터 수집 프레임워크입니다. 

기존 제조사(Vendor)가 제공하는 기본 C API의 한계(메모리 누수, 패킷 찢어짐, 데드락, 유저 편의성 부재)를 극복하기 위해, **순수 C++17과 libusb-1.0을 결합하여 바닥부터 재설계(From-Scratch)** 되었습니다. 고에너지 물리(HEP) 실험 및 극한의 노이즈가 발생하는 실제 빔 타임(Beam-time) 환경에서 시스템 다운 없이 최대 5Gbps 대역폭을 100% 소화하도록 설계되었습니다. 현재 버전은 완벽한 Command Line Interface(CLI) 및 오프라인 프로덕션 파이프라인이 구축된 안정화 체크포인트 릴리즈입니다.

---

## Core Architecture & Features (The "Immortal" DAQ)

기존 랩실용 PoC 수준의 벤더 코드를 완전히 격리하고, 아래의 핵심 아키텍처를 도입했습니다.

1. **Zero-Malloc Object Pool (메모리 폭발 원천 봉쇄)**
   - 초당 수백 MB가 쏟아지는 환경에서 런타임 `new` / `malloc` 호출은 치명적인 CPU 오버헤드와 OOM을 유발합니다. 프로그램 시작 시 1MB 단위의 대용량 데이터 블록 1,000개를 Pool에 미리 할당하여 수집-전송-반납의 전 과정에서 동적 메모리 할당을 0회로 강제합니다.
2. **Mega-Bulk Fetch & Residual Stitching (찢어진 패킷 완벽 복원)**
   - USB 3.0 대역폭 극대화를 위해 `libusb_bulk_transfer`를 1MB 단위로 묶어서 호출합니다. 통신 지연으로 인해 패킷 경계가 잘려서 들어오는 현상(Torn Packet)을 완벽히 이어 붙이는 알고리즘을 탑재하여 이벤트 정렬(Alignment) 붕괴를 영구 차단합니다.
3. **Advanced Terminal UX/UI (연구자 친화적 인터페이스)**
   - 실행 시 DAQ 주요 파라미터(Trigger LUT, Polarity, Threshold, Delay, DAC Offset)를 직관적인 테이블로 출력합니다.
   - ANSI 이스케이프 코드(`\r`, `\033[K`)를 활용한 단일 라인 실시간 모니터링 시스템을 구축하여 터미널 도배를 방지합니다. (수집 속도, 남은 메모리 Pool 크기 실시간 확인).
   - 종료 시 획득한 총 이벤트 수와 평균 트리거 레이트(Hz)를 자동 산출하여 보고합니다.
4. **Hardware Latch Noise Masking (초기 노이즈 자동 차단)**
   - 고속 FADC 특유의 초기 버퍼 스위칭 스파이크를 소프트웨어적으로 완벽히 격리(처음 20 Bin Skip, 22~80 Bin Pedestal 계산)하여 전하합(Charge) 연산의 무결성을 보장합니다.
5. **Data & Binary Isolation (안전한 파일 시스템 구조)**
   - 빌드 시 실행 파일(`bin/`), 라이브러리(`lib/`), 데이터 저장소(`data/`)를 자동으로 분리 및 생성하여 소스 코드 영역을 청결하게 유지합니다.

---

## Directory Structure

제조사의 레거시 드라이버 코드는 `vendor/` 디렉토리에 철저히 캡슐화되어 메인 DAQ 파이프라인의 안전성을 침해하지 않습니다.

```text
KFADC500_DAQ/
 ├── bin/                   # 컴파일된 최종 실행 파일 (kfadc500_daq, kfadc500_prod)
 ├── lib/                   # 컴파일된 정적 라이브러리
 ├── data/                  # 수집된 Raw Data(.dat) 및 ROOT 파일(.root) 전용 공간
 ├── CMakeLists.txt         # 최상위 빌드 스크립트
 ├── config/
 │   └── kfadc500.config    # 직관적인 INI 포맷의 장비 설정 파일
 ├── rules/
 │   └── setup_env.sh       # Sudo 권한 없이 USB 접근을 허용하는 Udev 설정 스크립트
 ├── vendor/                # Notice Korea 제공 로우레벨 래퍼 (안전 격리됨)
 ├── include/               # C++17 고속 DAQ 코어 및 오프라인 ROOT 프로듀서 헤더
 ├── src/                   # 메인 시스템 소스 코드
 └── AnalyzeSPE.C           # MINUIT2 기반 단일 광전자(SPE) 정밀 피팅 매크로
```

---

## Prerequisites & Build Instructions

### Dependencies (Rocky Linux 9 기준)
- `GCC/G++ 11.0+` (C++17 Support)
- `CMake 3.15+`
- `libusb-1.0-devel`, `zeromq-devel`
- `CERN ROOT 6.x` (오프라인 데이터 프로덕션 및 물리 분석용)

```bash
sudo dnf install epel-release
sudo dnf install gcc-c++ cmake make pkgconf-pkg-config libusb1-devel zeromq-devel
```

### Build & Udev Rules Setup
장비에 일반 유저(Non-root) 권한으로 접근하기 위해 Udev 룰을 먼저 세팅합니다.
```bash
cd rules && ./setup_env.sh
cd ..
mkdir build && cd build
cmake ..
make -j4
```
빌드가 완료되면 프로젝트 최상단에 `bin/`, `lib/`, `data/` 디렉토리가 자동 생성됩니다.

---

## Usage Guide

모든 명령어는 프로젝트 최상단 디렉토리에서 실행합니다. 데이터 출력 경로는 기본적으로 `data/` 디렉토리를 향하도록 설정되어 있습니다.

### 1. Online DAQ (`kfadc500_daq`)
초고속 데이터 획득을 수행합니다. 설정 테이블과 실시간 자원(Queue/Pool) 모니터링이 제공됩니다.

```bash
# 기본 실행 (무한 수집, Ctrl+C로 우아하게 종료)
./bin/kfadc500_daq -f config/kfadc500.config -o data/run_001.dat

# 이벤트 개수 제한 수집 (-n: 100,000개 도달 시 자동 종료)
./bin/kfadc500_daq -f config/kfadc500.config -o data/run_001.dat -n 100000
```
*(옵션: `-f` Config 파일, `-o` 출력 파일, `-n` 목표 이벤트 수, `-t` 목표 수집 시간)*

### 2. Offline ROOT Production (`kfadc500_prod`)
수집된 Raw 바이너리(`.dat`) 파일을 물리 분석을 위한 ROOT `TTree`로 초고속 변환합니다. **배치 모드**와 **대화형 디스플레이 모드**는 완벽하게 독립적으로 동작(Mutually Exclusive)합니다.

```bash
# [MODE 1] 초고속 배치 모드 (자동으로 data/run_001.root 생성)
./bin/kfadc500_prod data/run_001.dat

# [MODE 2] 인터랙티브 디스플레이 모드 (파형 탐색 전용, 파일 저장 안 함)
./bin/kfadc500_prod -d data/run_001.dat
```
*(디스플레이 모드 단축키: `Enter/n`: 다음 이벤트, `p`: 이전, `j <id>`: 점프, `q`: 즉시 종료)*

### 3. Physics Analysis: SPE Calibration (`AnalyzeSPE.C`)
ROOT의 강력한 `MINUIT2` 최소화 엔진을 탑재하여 PMT의 **단일 광전자(Single Photoelectron, SPE)** 스펙트럼을 정밀하게 피팅하고, 채널별 2D 파형 산점도(Persistence Plot)를 렌더링합니다.

```bash
# 0번 채널 분석 실행 (기본 경로는 data/kfadc500_data.root 로 잡혀 있음)
root -l -q 'AnalyzeSPE.C("data/run_001.root", 0)'  
```

---

## Development Roadmap

- [x] **Phase 1: Core CLI & Raw Binary DAQ**
  - Zero-Malloc Object Pool 기반의 1MB Mega-Bulk 수집 코어 완성.
  - ANSI C++ 기반의 연구자 친화적 터미널 UX(초기화 요약표, 한 줄 모니터링) 적용 완료.

- [x] **Phase 2: Offline ROOT Production & Physics Analysis**
  - 트리거 래치 노이즈 마스킹 및 TTree 초고속 일괄 변환기 구현.
  - 데이터/바이너리 디렉토리 분리 및 시스템 자동화.
  - MINUIT2 엔진 기반 단일 광전자(SPE) 정밀 분석 매크로 완성.

- [ ] **Phase 3: Real-Time Online Monitor (CLI/ROOT-based)**
  - 과거 FADC400 및 FADC500 "Mini" 프로덕션 환경에서 검증된 CLI 기반의 실시간 온라인 모니터 프로그램 구축.
  - 데이터 획득(DAQ)과 동시에 ROOT `TApplication`과 `TCanvas`를 활용하여 실시간 파형(Waveform) 스냅샷 및 파형 적분 전하량(Charge Histogram) 분포를 즉각적으로 시각화.

- [ ] **Phase 4: Real-Time ZMQ Monitoring GUI (ROOT Native GUI)**
  - ROOT 기반 Native C++ GUI (`TGMainFrame`, `TRootEmbeddedCanvas`) 아키텍처 완벽 계승.
  - ZeroMQ PUB/SUB 패턴을 활용하여 수집 코어(DAQ)의 시스템 부하(Overhead)를 0으로 유지하는 비동기 통신 구현.
  - 빔 타임(Beam-time) 및 캘리브레이션 중 필수적인 물리 변수들을 종합적으로 모니터링하는 통합 디스플레이 구축 예정.
---

## Author & Acknowledgments

- **Author**: Choi Jiyoung (Junior Postdoctoral Researcher, Center for Precision Neutrino Research (CPNR), Department of Physics, Chonnam National University)
- **Acknowledgments**: 본 연구 및 시스템 개발은 대한민국 정부의 연구비 지원을 받아 성공적으로 수행되고 있습니다. 기초과학 발전을 위한 대한민국 국민들의 헌신적인 성원과 지원에 깊은 감사를 드립니다.
- **License**: `vendor/` 디렉토리 내의 소스 코드는 Notice Korea의 지적 재산권을 따릅니다.
