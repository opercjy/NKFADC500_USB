import time
import traceback
import numpy as np
import zmq
from PySide6.QtCore import QThread, Signal

class ZmqWorker(QThread):
    data_ready = Signal(object, object) 

    def __init__(self, record_length=8):
        super().__init__()
        self.running = True
        self.record_length = record_length
        # KFADC500 데이터 규격: 1 Record Length = 512 Bytes. 2바이트(short) 단위이므로 나누기 2, 4채널이므로 나누기 4
        self.samples_per_ch = (self.record_length * 512) // 2 // 4 
        
        self.skip_bins = 20
        self.ped_start = 22
        self.ped_end = 80
        
        self.context = None
        self.socket = None
        self.poller = None

    def update_record_length(self, new_rl):
        """외부 Config가 변경되었을 때 배열 크기를 동적으로 보정하기 위한 안전 함수"""
        self.record_length = new_rl
        self.samples_per_ch = (self.record_length * 512) // 2 // 4

    def run(self):
        self.context = zmq.Context()
        self.socket = self.context.socket(zmq.SUB)
        self.socket.setsockopt(zmq.LINGER, 0)
        self.socket.setsockopt(zmq.RCVHWM, 10) # 수신 버퍼를 10개로 제한하여 파이썬 메모리 폭발 원천 봉쇄
        self.socket.connect("tcp://127.0.0.1:5555")
        self.socket.setsockopt_string(zmq.SUBSCRIBE, "")

        self.poller = zmq.Poller()
        self.poller.register(self.socket, zmq.POLLIN)

        last_emit_time = time.time()
        accumulated_charge = [np.array([]) for _ in range(4)]

        while self.running:
            try:
                # 100ms 단위로 소켓 상태를 점검 (GIL 블로킹 및 GUI 프리징 방지)
                socks = dict(self.poller.poll(100))
                
                if self.socket in socks and socks[self.socket] == zmq.POLLIN:
                    msg = self.socket.recv(flags=zmq.NOBLOCK)
                    
                    raw_data = np.frombuffer(msg, dtype=np.uint16)
                    event_size_shorts = 4 * self.samples_per_ch
                    
                    # [동적 크기 보정 및 예외 방어]
                    # 찢어진 패킷이나 설정 변경으로 배열 크기가 안 맞을 경우, 온전한 이벤트 단위까지만 잘라냄
                    if len(raw_data) % event_size_shorts != 0:
                        num_events = len(raw_data) // event_size_shorts
                        if num_events == 0:
                            continue
                        valid_len = num_events * event_size_shorts
                        raw_data = raw_data[:valid_len]
                    else:
                        num_events = len(raw_data) // event_size_shorts

                    if num_events == 0:
                        continue

                    # 다차원 배열 변환
                    events = raw_data.reshape((num_events, 4, self.samples_per_ch))

                    # 베이스라인(페데스탈) 보정 및 파형 반전
                    pedestal = np.mean(events[:, :, self.ped_start:self.ped_end], axis=2, keepdims=True)
                    inverted_waveforms = pedestal - events
                    
                    # 지정된 구간(skip_bins) 이후의 전하량 적분
                    charge = np.sum(inverted_waveforms[:, :, self.skip_bins:], axis=2)
                    
                    for ch in range(4):
                        accumulated_charge[ch] = np.append(accumulated_charge[ch], charge[:, ch])

                    # 화면에 그릴 마지막 파형 하나만 추출
                    last_waveform = inverted_waveforms[-1, :, :]
                    
                    current_time = time.time()
                    # 초당 10번(10 FPS)으로 GUI 업데이트 속도 제한 (렌더링 큐 마비 방지)
                    if current_time - last_emit_time >= 0.1: 
                        if self.running:
                            self.data_ready.emit(last_waveform, accumulated_charge)
                            accumulated_charge = [np.array([]) for _ in range(4)]
                            last_emit_time = current_time

            except zmq.ZMQError:
                continue
            except Exception as e:
                # [예외 추적 Traceback] 스레드가 조용히 죽어버리는 현상(Silent Crash) 방지
                print(f"\n[ZMQ WORKER ERROR] {e}")
                traceback.print_exc()
                continue

        # 루프 탈출 시 자원 안전 해제
        if self.poller:
            self.poller.unregister(self.socket)
        if self.socket:
            self.socket.close()
        if self.context:
            self.context.term()

    def stop(self):
        self.running = False
        self.wait(1000)