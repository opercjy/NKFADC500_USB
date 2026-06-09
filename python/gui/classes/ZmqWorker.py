import numpy as np
import zmq
import time
from PySide6.QtCore import QThread, Signal, Slot
import logging

logger = logging.getLogger(__name__)

class ZmqWorker(QThread):
    # 시그널 인자 규격: waveforms, charges, telemetry, anomaly_data
    data_ready = Signal(object, object, dict, dict)

    def __init__(self):
        super().__init__()
        self.running = True
        self.monitoring_enabled = False
        self.context = None
        self.socket = None
        self.dt = 2e-9 
        self.baseline_stats = {ch: {"count": 0, "anomalies": 0} for ch in range(4)}

    @Slot(bool)
    def set_monitoring_state(self, state):
        self.monitoring_enabled = state

    @Slot()
    def request_clear(self):
        for ch in range(4):
            self.baseline_stats[ch] = {"count": 0, "anomalies": 0}

    def run(self):
        self.context = zmq.Context()
        self.socket = self.context.socket(zmq.SUB)
        self.socket.setsockopt(zmq.LINGER, 0)
        self.socket.setsockopt(zmq.CONFLATE, 1)
        self.socket.connect("tcp://127.0.0.1:5555")
        self.socket.setsockopt_string(zmq.SUBSCRIBE, "")

        while self.running:
            if not self.monitoring_enabled:
                time.sleep(0.1)
                continue

            try:
                if self.socket.poll(100):
                    msg = self.socket.recv(flags=zmq.NOBLOCK)
                    self.process_data(msg)
            except Exception as e:
                logger.error(f"ZMQ Worker Error: {e}")

        self.socket.close()
        self.context.term()

    def process_data(self, data_bytes):
        # 195096 바이트 (LiveMonitorPacket 구조체 매핑)
        if len(data_bytes) < 195096:
            return
            
        header = np.frombuffer(data_bytes[:24], dtype=np.uint32)
        num_events = header[0]
        samples_per_ch = header[1]
        telemetry = {
            'events': header[2],
            'dataq': header[3],
            'pool': header[4]
        }
        
        if num_events == 0:
            self.data_ready.emit("TELEMETRY_ONLY", None, telemetry, {})
            return

        wave_offset = 24
        wave_size = 4 * 4096 * 8
        last_waveform = np.frombuffer(data_bytes[wave_offset:wave_offset+wave_size], dtype=np.float64).reshape((4, 4096))
        
        charge_offset = wave_offset + wave_size
        charge_size = 4 * 2000 * 8
        charge_array = np.frombuffer(data_bytes[charge_offset:charge_offset+charge_size], dtype=np.float64).reshape((4, 2000))

        waveforms = {}
        charges = {}
        anomaly_data = {ch: [] for ch in range(4)}

        for ch in range(4):
            valid_wave = last_waveform[ch, :samples_per_ch]
            waveforms[ch] = [valid_wave]
            charges[ch] = charge_array[ch, :num_events].tolist()
            
            # =========================================================================
            # [아노말리 탐지] 임의의 하드코딩 제거, 순수 통계적(IQR) 탐지로 복원
            # =========================================================================
            if len(valid_wave) > 100:
                analysis_region = valid_wave[20:80] 
                if len(analysis_region) > 10:
                    q1 = np.percentile(analysis_region, 25)
                    q3 = np.percentile(analysis_region, 75)
                    iqr = q3 - q1
                    
                    # 수학적으로 IQR이 0이 되는 극단적 상황만 방지
                    safe_iqr = iqr if iqr > 0 else 1.0 
                    lower_bound = q1 - 2.0 * safe_iqr
                    upper_bound = q3 + 2.0 * safe_iqr
                    
                    outliers = np.sum((analysis_region < lower_bound) | (analysis_region > upper_bound))
                    
                    # 10% 이상 이탈 시 이상 파형으로 간주
                    if outliers > (len(analysis_region) * 0.1): 
                        self.baseline_stats[ch]["anomalies"] += 1
                        wave_dc_removed = valid_wave - np.mean(valid_wave)
                        fft_vals = np.fft.rfft(wave_dc_removed)
                        fft_power = np.abs(fft_vals) ** 2 
                        freqs = np.fft.rfftfreq(len(wave_dc_removed), d=self.dt)
                        freqs_mhz = freqs / 1e6
                        anomaly_data[ch].append((valid_wave, freqs_mhz, fft_power))

        self.data_ready.emit(waveforms, charges, telemetry, anomaly_data)

    def stop(self):
        self.running = False
        self.wait()
