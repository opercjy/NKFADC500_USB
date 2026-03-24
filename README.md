
# KFADC500 High-Performance DAQ Framework (Full-Stack Control Tower)

![C++17](https://img.shields.io/badge/C++-17-blue.svg) ![CMake](https://img.shields.io/badge/CMake-3.15+-green.svg) ![Python](https://img.shields.io/badge/Python-3.8+-yellow.svg) ![ZeroMQ](https://img.shields.io/badge/ZeroMQ-IPC-red.svg) ![PySide6](https://img.shields.io/badge/Qt-PySide6-brightgreen.svg) ![SQLite3](https://img.shields.io/badge/Database-SQLite3-lightblue.svg) ![Notice Certified](https://img.shields.io/badge/Notice_Hardware-Verified_Maintenance-orange.svg)

**License & Notice:** 본 프레임워크는 고에너지 물리(HEP) 실험 및 학술 연구 목적으로 개발 및 유지보수되고 있습니다. `vendor/` 디렉토리 내에 포함된 로우레벨 하드웨어 제어 C 드라이버 소스 코드의 모든 지적 재산권(Intellectual Property)은 **Notice Korea**에 귀속됩니다. 
*(본 문서는 대한민국 연구진을 위해 한국어로 작성되었습니다. 해외 연구자분들은 번역 플랫폼을 적극 활용해 주시기 바랍니다.)*

**KFADC500 High-Performance DAQ**는 Notice Korea의 USB 3.0 기반 고속 파형 디지타이저(KFADC500, 500MS/s, 4CH)를 위한 엔터프라이즈급 무결성 데이터 수집 프레임워크입니다. 

기존 제조사(Vendor)가 제공하는 기본 C API의 한계(메모리 누수, 패킷 찢어짐, 데드락, 유저 편의성 부재)를 극복하기 위해, **순수 C++17과 libusb-1.0을 결합하여 바닥부터 재설계(From-Scratch)** 되었습니다. 고에너지 물리 실험 및 극한의 노이즈가 발생하는 실제 빔 타임(Beam-time) 환경에서 시스템 다운 없이 최대 5Gbps 대역폭을 100% 소화하도록 설계되었습니다. 

현재 버전은 완벽한 Command Line Interface(CLI) 백엔드를 기반으로, **PySide6(Qt) 기반의 모듈형 통합 관제탑(GUI)**, CAEN 고전압(HV) 장비 연동, SQLite3 데이터베이스 로깅, 그리고 초고속 오프라인 프로덕션 파이프라인이 모두 구축된 풀스택(Full-Stack) 릴리즈입니다.

---

## Core Architecture & Features (The "Immortal" DAQ)

기존 랩실용 PoC 수준의 벤더 코드를 완전히 격리하고, 아래의 핵심 아키텍처를 도입했습니다.

1. **Zero-Malloc Object Pool (메모리 폭발 원천 봉쇄)**
   - 초당 수백 MB가 쏟아지는 환경에서 런타임 `new` / `malloc` 호출은 치명적인 CPU 오버헤드와 OOM을 유발합니다. 시작 시 1MB 단위의 대용량 데이터 블록 1,000개를 Pool에 미리 할당하여 동적 메모리 할당을 0회로 강제합니다.
2. **Mega-Bulk Fetch & Residual Stitching (찢어진 패킷 완벽 복원)**
   - USB 3.0 대역폭 극대화를 위해 `libusb_bulk_transfer`를 1MB 단위로 호출합니다. 통신 지연으로 인해 패킷 경계가 잘려서 들어오는 현상(Torn Packet)을 완벽히 이어 붙이는 알고리즘을 탑재했습니다.
3. **Graceful Shutdown & Interruption Handling (안전 종료 로직)**
   - DAQ 수집 중이거나 수십 GB의 ROOT 프로덕션 도중 언제든 인터럽트를 걸면, 즉각 하드웨어 래치를 잠그고 진행 중인 마지막 메모리 블록까지 디스크에 안전하게 기록한 뒤 우아하게 종료됩니다. (좀비 프로세스 및 데이터 유실 원천 차단)
4. **Hybrid ZMQ Pipeline (Language Decoupling)**
   - 무거운 C++ 코어는 오직 하드웨어 제어와 디스크 쓰기에만 집중하고, ZMQ PUB 소켓으로 바이너리 스트림을 송출합니다. 파이썬 기반의 ZmqWorker 스레드는 Non-blocking Poller를 사용하여 GUI 프리징 없이 초당 10프레임(10 FPS)으로 안전하게 렌더링을 갱신합니다.
5. **Data & Binary Isolation (안전한 파일 시스템 구조)**
   - 빌드 시 실행 파일(`bin/`), 데이터 저장소(`data/`), 터미널 환경(`python/cli/`), 그래픽 관제탑 환경(`python/gui/`)을 완벽히 분리하여 소스 코드 영역을 청결하게 유지합니다.

---

## Graphical Interface & User Experience (통합 관제탑 GUI)

본 프레임워크는 터미널 환경에 익숙하지 않은 연구원들도 직관적으로 장비를 운용할 수 있도록 **MVC(Model-View-Controller) 패턴 기반의 모듈형 GUI 프론트엔드**를 제공합니다. 메인 윈도우는 단순히 각 모듈을 조립하는 스위치보드 역할을 수행하며, 모든 기능은 철저히 캡슐화되어 있습니다.

* **Live DAQ Dashboard:** 화면 우측에 항시 고정되어 시스템의 심장 박동(수집 이벤트, 트리거 레이트, 디스크 여유 용량, ZMQ 큐 상태)을 실시간 LED 전광판 스타일로 브리핑합니다.
* **Run Control Tab:** 다중 런(Multi-Run) 자동화 및 임계값 자동 스캔(Auto Threshold Scan) 기능을 탑재하여 빔 타임 중 수면 시간을 보장합니다. 런이 끝날 때마다 파일명 꼬리표와 런 넘버가 알아서 증가합니다.
* **Live Monitor Tab:** PyQtGraph 엔진을 이용한 순백색 캔버스 위에서 4채널 실시간 파형(Waveform)과 전하량 스펙트럼(Spectrum)을 60FPS로 끊김 없이 렌더링합니다.
* **Trigger LUT Simulator Tab:** 복잡한 16-bit 트리거 로직을 15가지 채널 동시성(Coincidence) 체크박스로 직관적으로 시뮬레이션하고 16진수 코드를 양방향으로 자동 계산합니다.
* **High Voltage (HV) Control Tab:** `caen-libs`를 연동하여 CAEN SY5527, N1470 등의 고전압 장비를 원격 제어합니다. ORTEC 556 같은 아날로그 장비 선택 시 `QStackedWidget`을 통해 안전 대기 화면으로 자동 전환됩니다.
* **Offline Production Tab:** 수십 GB의 Raw 바이너리 파일을 ROOT 트리로 일괄 변환(Batch)하거나, GUI 상의 버튼으로 터미널 C++ 코어를 조종하여 파형을 탐색(Interactive)할 수 있습니다.
* **Database History Tab:** 모든 DAQ 수집 이력과 프로덕션 변환 결과(파일 크기, 속도, 설정값 등)가 내장 SQLite3 엔진에 의해 영구적으로 자동 기록되고 엑셀 형태의 표로 전시됩니다.

---

## Directory Structure

제조사의 레거시 드라이버 코드는 `vendor/` 디렉토리에 철저히 캡슐화되어 메인 DAQ 파이프라인의 안전성을 침해하지 않습니다. 파이썬 생태계 또한 용도에 맞게 이원화되었습니다.

```text
KFADC500_DAQ/
 ├── bin/                   # 컴파일된 최종 C++ 실행 파일 (kfadc500_daq, kfadc500_prod)
 ├── data/                  # 수집된 Raw Data(.dat), ROOT 파일(.root), SQLite DB 파일
 ├── config/                # 직관적인 INI 포맷의 장비 설정 파일
 ├── python/                # Python 생태계 (Bifurcated)
 │   ├── cli/               # 터미널 전용 가벼운 독립 실행형 스크립트 (online_monitor.py)
 │   └── gui/               # PySide6 기반 엔터프라이즈 통합 관제탑 (main.py, widgets, core)
 ├── rules/                 # Sudo 권한 없이 USB 접근을 허용하는 Udev 스크립트
 ├── vendor/                # Notice Korea 제공 로우레벨 래퍼 (안전 격리됨)
 ├── src/ & include/        # 메인 시스템 C++ 소스 코드 및 헤더
 ├── AnalyzeSPE.C           # MINUIT2 기반 단일 광전자(SPE) 정밀 피팅 매크로
 └── rebuild.sh             # 안전한 클린 빌드 자동화 스크립트
```

---

## Prerequisites & Build Instructions

### Dependencies (Rocky Linux 9 기준)
- `GCC/G++ 11.0+` (C++17 Support), `CMake 3.15+`
- `libusb-1.0-devel`, `zeromq-devel`, `sqlite-devel`
- `CERN ROOT 6.x`
- `Python 3.8+` (GUI 및 모니터링용)
- *Optional:* `caen-libs` & CAEN HV Wrapper Library (고전압 원격 제어용)

```bash
# OS 패키지 설치
sudo dnf install epel-release
sudo dnf install gcc-c++ cmake make pkgconf-pkg-config libusb1-devel zeromq-devel sqlite-devel

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

## Usage Guide (Command Line & GUI Modes)

본 시스템은 가벼운 터미널 작업과 무거운 종합 관제를 모두 지원합니다. 모든 명령어는 프로젝트 최상단 디렉토리에서 실행합니다.

### [MODE A] Graphical Interface Mode (통합 관제탑)
C++ 코어 명령어를 몰라도 마우스 클릭만으로 DAQ, 모니터링, HV 제어, ROOT 변환, DB 조회까지 모두 수행할 수 있는 마스터 애플리케이션입니다.
```bash
python python/gui/main.py
```

### [MODE B] Command Line Mode (전문가용 터미널 제어)
GUI 오버헤드 없이 스크립트로 장비를 굴리거나 원격 서버(SSH) 환경에서 가볍게 작업할 때 사용합니다.

**1. Online DAQ (`kfadc500_daq`)**
초고속 데이터 획득을 백그라운드에서 수행합니다. (종료 시 `Ctrl+C`)
```bash
./bin/kfadc500_daq -f config/kfadc500.config -o data/run_001.dat
```

**2. CLI Real-Time Monitoring (`online_monitor.py`)**
DAQ가 실행 중일 때 새로운 터미널을 열고 가볍게 파형만 띄워봅니다.
```bash
python python/cli/online_monitor.py
```

**3. Offline ROOT Production (`kfadc500_prod`)**
```bash
# 배치 모드 (-w 옵션 추가 시 파형 배열 통째로 저장)
./bin/kfadc500_prod data/run_001.dat -w

# 인터랙티브 모드 (ROOT TCanvas로 파형 탐색)
./bin/kfadc500_prod -d data/run_001.dat
```

---

## Screenshots (GUI Dashboard)

- **Live Monitor & Dashboard View:** <img width="1522" height="1010" alt="image" src="https://github.com/user-attachments/assets/b8707586-f31f-4e0d-9504-c2bd4cc13939" />
- **Run Control & Auto Scan Setup:** <img width="1522" height="1010" alt="image" src="https://github.com/user-attachments/assets/03d79b18-183f-4a9b-8baa-876ea108e13a" />
- **Trigger LUT Simulator:** <img width="1522" height="1010" alt="image" src="https://github.com/user-attachments/assets/868bef38-ec83-40db-ace2-653fea987907" />
- **CAEN High Voltage Control:** <img width="1522" height="1010" alt="image" src="https://github.com/user-attachments/assets/9573b84b-94f6-4650-86b2-343dfc81c550" />
- **SQLite Database History:** <img width="1522" height="1010" alt="image" src="https://github.com/user-attachments/assets/bc9dd778-65ed-4bc4-85be-fcb571066716" />
- **Offline Production 1** <img width="1220" height="840" alt="image" src="https://github.com/user-attachments/assets/05bb78a3-a7e2-480f-a50f-3a7a42fa50fe" />
- **Offline Production 2** <img width="1522" height="1010" alt="image" src="https://github.com/user-attachments/assets/37a890e1-0e84-49a3-9eea-eee56b9d86c8" />
- **Configuration** <img width="1522" height="1010" alt="image" src="https://github.com/user-attachments/assets/2164d248-cbd6-4dd3-b7f7-d999c3fa8e68" />

---

## Development Roadmap

- [x] **Phase 1: Core CLI & Raw Binary DAQ**
  - Zero-Malloc Object Pool 기반의 1MB Mega-Bulk 수집 코어 완성.
  - ANSI C++ 기반의 터미널 UX 및 `Ctrl+C` Graceful Shutdown 방어 로직 적용 완료.

- [x] **Phase 2: Offline ROOT Production & Physics Analysis**
  - 트리거 래치 노이즈 마스킹 및 TTree 초고속 일괄 변환기 구현.
  - MINUIT2 엔진 기반 단일 광전자(SPE) 정밀 분석 매크로 완성.

- [x] **Phase 3: Real-Time Online Monitor (CLI)**
  - C++ 백엔드(ZMQ PUB)와 Python 프론트엔드(ZMQ SUB)를 분리한 실시간 파형 뷰어 구축.

- [x] **Phase 4 & 5: Advanced Monitoring GUI & Database Integration**
  - PySide6 기반의 MVC 모듈형 통합 관제 대시보드(Full-Stack Control Tower) 구축 완료.
  - 다중 런 자동화, 임계값 스캔, SQLite3 영구 DB 로깅 연동 완료.

- [x] **Phase 6: Hardware Control Integration**
  - `caen-libs` 기반의 CAEN HV 디지털 원격 제어 및 ORTEC 아날로그 모드 분기 로직 탑재.
  - 16-bit Trigger LUT 개념 해설 및 대화형 시뮬레이터 탭 완성.

- [ ] **Phase 7: Advanced Calibration (TBD)**
  - 실시간 베이스라인 변동 추적 및 자동 캘리브레이션 알고리즘 탑재 예정.

---

## Acknowledgments
본 연구 및 시스템 개발은 대한민국 정부의 연구비 지원을 받아 수행되고 있습니다. 기초과학 발전을 위한 헌신적인 성원과 지원에 깊은 감사를 드립니다.
