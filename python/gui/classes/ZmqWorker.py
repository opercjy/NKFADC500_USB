import time
import numpy as np
import zmq
from PySide6.QtCore import QThread, Signal, Slot

class ZmqWorker(QThread):
    # data_ready 시그널: (waveforms, charges, samples_per_ch, telemetry_dict)
    data_ready = Signal(object, object, int, dict) 

    def __init__(self):
        super().__init__()
        self.running = True
        self.monitoring_enabled = False 
        
        self.context = None
        self.socket = None
        self.poller = None

        # 새로운 패킷 규격: 헤더(24) + 파형(131072) + 전하량(64000) = 195096 bytes
        self.packet_size = 195096
        self.needs_clear = False
        self.baseline_hist = np.zeros((4, 150), dtype=np.int32)
        
        self.last_telemetry_time = time.time()
        self.last_events = 0
        self.current_rate = 0.0

    @Slot(bool)
    def set_monitoring_state(self, state):
        self.monitoring_enabled = state

    @Slot()
    def request_clear(self):
        self.needs_clear = True 

    def run(self):
        self.context = zmq.Context()
        self.socket = self.context.socket(zmq.SUB)
        self.socket.setsockopt(zmq.LINGER, 0)
        self.socket.setsockopt(zmq.CONFLATE, 1) 
        self.socket.connect("tcp://127.0.0.1:5555")
        self.socket.setsockopt_string(zmq.SUBSCRIBE, "")

        self.poller = zmq.Poller()
        self.poller.register(self.socket, zmq.POLLIN)

        while self.running:
            try:
                socks = dict(self.poller.poll(100))
                if self.socket in socks and socks[self.socket] == zmq.POLLIN:
                    msg = self.socket.recv(flags=zmq.NOBLOCK)
                    
                    if len(msg) != self.packet_size:
                        continue

                    # 1. 헤더 추출 (Telemetry)
                    header = np.frombuffer(msg, dtype=np.uint32, count=6, offset=0)
                    num_events = header[0]
                    samples_per_ch = header[1]
                    total_events = header[2]
                    queue_size = header[3]
                    pool_free_size = header[4]

                    current_time = time.time()
                    elapsed = current_time - self.last_telemetry_time
                    if elapsed >= 0.5:
                        self.current_rate = (total_events - self.last_events) / elapsed
                        self.last_telemetry_time = current_time
                        self.last_events = total_events

                    telemetry = {
                        'events': total_events,
                        'rate': round(self.current_rate, 1),
                        'dataq': queue_size,
                        'pool': pool_free_size
                    }

                    if not self.monitoring_enabled or num_events == 0 or samples_per_ch == 0:
                        self.data_ready.emit("TELEMETRY_ONLY", None, 0, telemetry)
                        continue

                    # 2. 파형 추출 (offset 24 byte)
                    waveforms_full = np.frombuffer(msg, dtype=np.float64, offset=24, count=4*4096).reshape((4, 4096))
                    valid_waveforms = waveforms_full[:, :samples_per_ch]
                    
                    # 3. 전하량 추출 (offset 24 + 131072 = 131096 byte)
                    charges_full = np.frombuffer(msg, dtype=np.float64, offset=131096, count=4*2000).reshape((4, 2000))
                    valid_charges = charges_full[:, :num_events]
                    
                    if self.needs_clear:
                        self.data_ready.emit("CLEAR", None, 0, telemetry)
                        self.needs_clear = False
                    
                    self.data_ready.emit(valid_waveforms, valid_charges, samples_per_ch, telemetry)

            except zmq.ZMQError:
                continue
            except Exception as e:
                print(f"[ZMQ WORKER ERROR] {e}")
                self.msleep(50)

        if self.poller: self.poller.unregister(self.socket)
        if self.socket: self.socket.close()
        if self.context: self.context.term()

    def stop(self):
        self.running = False
        self.wait(500)