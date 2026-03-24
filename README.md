
# KFADC500 High-Performance DAQ Framework

![C++17](https://img.shields.io/badge/C++-17-blue.svg) ![CMake](https://img.shields.io/badge/CMake-3.15+-green.svg) ![libusb](https://img.shields.io/badge/libusb-1.0-orange.svg) ![ZeroMQ](https://img.shields.io/badge/ZeroMQ-IPC-red.svg) ![ROOT](https://img.shields.io/badge/CERN_ROOT-6.x-white.svg)

**Notice:** 본 문서는 대한민국 연구진 및 사용자를 위해 한국어로 작성되었습니다. 영어나 기타 언어를 사용하시는 연구자분들은 웹 브라우저나 번역 플랫폼의 번역 기능(Translation tools)을 적극 활용해 주시기 바랍니다.

**KFADC500 High-Performance DAQ**는 Notice Korea의 USB 3.0 기반 고속 파형 메모리화기(KFADC500)를 위한 엔터프라이즈급 무결성 데이터 수집 프레임워크입니다. 

기존 제조사(Vendor)가 제공하는 기본 C API의 한계(메모리 누수, 패킷 찢어짐, 데드락, 유저 편의성 부재)를 극복하기 위해, **순수 C++17과 libusb-1.0을 결합하여 바닥부터 재설계(From-Scratch)** 되었습니다. 고에너지 물리(HEP) 실험 및 극한의 노이즈가 발생하는 실제 빔 타임(Beam-time) 환경에서 시스템 다운 없이 최대 5Gbps 대역폭을 100% 소화하도록 설계되었습니다.

---

## Core Architecture & Features (The "Immortal" DAQ)

기존 랩실용 PoC 수준의 제조사 코드를 완전히 격리하고, 다음 5가지 핵심 아키텍처를 도입했습니다.

1. **Mega-Bulk Fetch & Residual Stitching (찢어진 패킷 완벽 복원)**
   - USB 3.0 대역폭 극대화를 위해 `libusb_bulk_transfer`를 1MB 단위로 묶어서 호출합니다.
   - 통신 지연으로 인해 패킷 경계가 잘려서 들어오는 현상(Torn Packet) 방어를 위해, **자투리 버퍼를 다음 Fetch 루프의 선두에 이어 붙이는(Stitching) 알고리즘**을 탑재하여 물리 이벤트의 정렬(Alignment) 붕괴를 영구 차단합니다.

2. **Zero-Malloc Object Pool (메모리 폭발 원천 봉쇄)**
   - 초당 수백 MB가 쏟아지는 환경에서 런타임 `new` / `malloc` 호출은 치명적인 CPU 오버헤드와 OOM을 유발합니다.
   - 프로그램 시작 시 1,000개의 대용량 데이터 블록을 풀(Pool)에 미리 할당하여 수집-전송-반납의 전 과정에서 **동적 메모리 할당을 0회로 강제**합니다.

3. **Zero-Copy IPC via ZeroMQ (초고속 비동기 전송)**
   - 무거운 직렬화(Serialization) 과정을 폐기하고, C++ 백엔드가 획득한 메모리 포인터(Flat Binary)를 ZMQ PUB 소켓에 그대로 바인딩합니다.
   - ZMQ 네트워크 발송 완료 시 **Custom Deallocator Callback**이 발동하여 데이터 블록을 자동으로 Object Pool에 반환합니다.

4. **Self-Healing FSM (데드락 자동 복구 및 100% Drain)**
   - 노이즈 폭주로 USB 컨트롤러가 STALL 상태에 빠지더라도, 에러 감지 즉시 `libusb_clear_halt`를 트리거하고 파이프 내의 잔여 데이터를 소거(Drain)하여 시스템을 스스로 부활시킵니다.

5. **User-Friendly CLI & Config Parser (연구자 친화적 인터페이스)**
   - `scanf` 기반의 하드코딩된 예제 코드를 배제하고, 직관적인 INI 스타일의 `.config` 파서와 리눅스 표준 옵션 파서(`getopt`)를 적용하여 배치(Batch) 스크립트 기반의 자동화 연동을 극대화했습니다.

---

## Directory Structure

제조사의 레거시 코드는 `vendor/` 디렉토리에 철저히 캡슐화되어 있으며 메인 DAQ 수집 루프에는 관여하지 않습니다.

```text
KFADC500_DAQ/
 ├── CMakeLists.txt         # 최상위 빌드 스크립트
 ├── config/
 │   └── kfadc500.config    # 직관적인 INI 포맷의 장비 설정 파일
 ├── vendor/                # Notice Korea 제공 로우레벨 래퍼 (격리됨)
 │   ├── NoticeKFADC500USB.c / .h
 │   ├── nkusb.c / .h
 │   └── usb3com.c / .h     
 ├── include/               # C++17 고속 DAQ 코어 및 오프라인 헤더
 │   ├── ConfigParser.hh    # 설정 파서
 │   ├── ReadDataWorker.hh  # 통신 및 버퍼 스티칭, Self-Healing 로직
 │   ├── ZmqPublisher.hh    # Zero-Copy IPC 브로커
 │   ├── ObjectPool.hh      # Zero-Malloc 메모리 관리자
 │   └── RootProducer.hh    # 오프라인 ROOT 변환기
 └── src/                   # C++17 소스 코드
     ├── main.cc            # 온라인 DAQ 진입점
     ├── production_main.cc # 오프라인 ROOT 변환기 진입점
     ├── ReadDataWorker.cc
     ├── ZmqPublisher.cc
     └── RootProducer.cc
```

---

## Prerequisites & Build Instructions

### Dependencies
본 프로젝트는 **Rocky Linux 9** 환경에 최적화되어 있습니다.
- `GCC/G++ 9.0+` (C++17 Support)
- `CMake 3.15+`
- `libusb-1.0`
- `ZeroMQ (libzmq)`
- `CERN ROOT 6.x` (오프라인 데이터 프로덕션용)

```bash
sudo dnf install epel-release
sudo dnf install gcc-c++ cmake make libusb1-devel zeromq-devel root root-core
```

### Build (Out-of-source)
```bash
git clone [https://github.com/opercjy/KFADC500_DAQ.git](https://github.com/opercjy/KFADC500_DAQ.git)
cd KFADC500_DAQ
mkdir build && cd build
cmake ..
make -j4
```
빌드가 완료되면 `kfadc500_daq` (온라인 수집용)와 `kfadc500_prod` (오프라인 ROOT 변환용) 두 개의 실행 파일이 생성됩니다.

---

## Usage Guide (Online DAQ)

`kfadc500_daq`는 전통적인 커맨드 라인 인자를 완벽하게 지원하며, 터미널 출력은 향후 GUI(PyQt5) 파싱을 위해 태그(`[MONITOR]`, `[SUMMARY]`)와 함께 출력됩니다.

```bash
# 기본 실행 (무한 수집, Ctrl+C로 종료)
./kfadc500_daq -f ../config/kfadc500.config -o run_001.dat

# 이벤트 개수 제한 수집 (-n: 100,000개 도달 시 자동 종료)
./kfadc500_daq -f ../config/kfadc500.config -o run_001.dat -n 100000

# 시간 제한 수집 (-t: 3600초(1시간) 도달 시 자동 종료)
./kfadc500_daq -f ../config/kfadc500.config -o run_001.dat -t 3600
```

| Option | Description | Default |
|--------|-------------|---------|
| `-f`   | 장비 레지스터 세팅이 담긴 Config 파일 경로 (필수) | N/A |
| `-o`   | 수집된 Raw Binary 데이터를 저장할 대상 파일명 | `kfadc500_data.dat` |
| `-n`   | 수집할 최대 이벤트 개수 한계점 (0 = 무한) | `0` |
| `-t`   | 수집할 최대 시간 한계점(초 단위) (0 = 무한) | `0` |
| `-s`   | 다중 보드 사용 시 접근할 Device ID (SID) | `0` |

### Configuration File (`kfadc500.config`)
채널별 개별 설정이 가능한 INI 포맷을 사용합니다.
```ini
[GLOBAL]
RECORD_LENGTH = 8
TRIGGER_LUT = 0xFFFE
INPUT_FILTER = 1

[CH0]
OFFSET = 3500
DELAY = 100
POLARITY = 0
THRESHOLD = 150
# CH1, CH2, CH3 독립 설정 가능...
```

---

## Offline Production (Raw to ROOT)

DAQ가 수집한 고속 바이너리 파일(`.dat`)을 물리학 연구자들이 직관적으로 분석할 수 있도록 CERN ROOT의 `TTree` 자료구조(`.root`)로 초고속 일괄 변환합니다. `Charge`, `Amplitude`, `Baseline` 등의 1차 물리량이 자동으로 계산되어 적재됩니다.

```bash
# 사용법: ./kfadc500_prod <input.dat> <output.root>
./kfadc500_prod run_001.dat run_001.root
```
생성된 `run_001.root` 파일을 ROOT `TBrowser`로 열어 즉시 스펙트럼 히스토그램을 확인할 수 있습니다.

---

## Development Roadmap

- [x] **Phase 1: Core CLI & Raw Binary DAQ**
  - 제조사 라이브러리 격리 및 정적 빌드 환경 구축
  - USB 3.0 Mega-Bulk Read 워커 구현 및 `.dat` 로컬 덤프 검증
- [x] **Phase 2: IPC & Offline ROOT Production**
  - ZeroMQ Zero-Copy 파이프라인 개통
  - `RootProducer`를 이용한 오프라인 TTree 변환기 구축
- [x] **Phase 3: System Stabilization**
  - Residual Stitching (Torn Packet 방어) 적용
  - Deadlock Self-Healing, Auto-Stop(-n, -t), 및 Graceful Shutdown 구현
- [ ] **Phase 4: Global GUI Integration & Optimization**
  - PyQt5 기반 Online Monitor 연동 및 실시간 파형 렌더링
  - FADC500_mini 및 VME400 시리즈와의 아키텍처 대통합

---

## Author & Acknowledgments

- **Author**: Choi Jiyoung (Center for Precision Neutrino Research, Chonnam National University)
- **Acknowledgments**: 본 연구 및 시스템 개발은 대한민국 정부의 연구비 지원을 받아 수행되었습니다. 기초과학 발전을 위한 대한민국 국민들의 헌신적인 성원과 지원에 깊은 감사를 드립니다.
- **License**: MIT License. (※ `vendor/` 디렉토리 내의 소스 코드는 Notice Korea의 지적 재산권을 따릅니다.)
```

