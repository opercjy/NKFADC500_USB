import os
import configparser
import numpy as np
import zmq
import time
from PySide6.QtCore import QThread, Signal, Slot
import logging

logger = logging.getLogger(__name__)

class ZmqWorker(QThread):
    data_ready = Signal(object, object, object, object)

    def __init__(self):
        super().__init__()
        self.running = True
        self.monitoring_enabled = False
        self.context = None
        self.socket = None
        self.dt = 2e-9 
        
        # 💡 [SSOT] 하드웨어 Config 저장소
        self.hw_config = {ch: {"offset": 3500.0, "polarity": 0, "threshold": 20.0} for ch in range(4)}
        self.baseline_stats = {ch: {"count": 0, "anomalies": 0, "real_mean": 3500.0} for ch in range(4)}
        
        self.last_telemetry_time = time.time()
        self.last_events = 0
        self.current_rate = 0.0

    def reload_config(self):
        """💡 kfadc500.config 파일을 파싱하여 하드웨어 설정값을 동기화합니다."""
        config_path = os.path.join(os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(__file__)))), 'config', 'kfadc500.config')
        if not os.path.exists(config_path):
            config_path = "config/kfadc500.config" # Fallback path
            
        parser = configparser.ConfigParser()
        try:
            parser.read(config_path)
            for ch in range(4):
                sec = f"CH{ch}"
                if parser.has_section(sec):
                    self.hw_config[ch]["offset"] = parser.getfloat(sec, "OFFSET", fallback=3500.0)
                    self.hw_config[ch]["polarity"] = parser.getint(sec, "POLARITY", fallback=0)
                    self.hw_config[ch]["threshold"] = parser.getfloat(sec, "THRESHOLD", fallback=20.0)
        except Exception as e:
            logger.error(f"ZmqWorker Config Parse Error: {e}")

    @Slot(bool)
    def set_monitoring_state(self, state):
        self.monitoring_enabled = state
        if state:
            self.reload_config() # 모니터링 시작 시 최신 Config 로드

    @Slot()
    def request_clear(self):
        for ch in range(4):
            self.baseline_stats[ch] = {"count": 0, "anomalies": 0, "real_mean": self.hw_config[ch]["offset"]}

    def run(self):
        self.context = zmq.Context()
        self.socket = self.context.socket(zmq.SUB)
        self.socket.setsockopt(zmq.LINGER, 0)
        self.socket.setsockopt(zmq.CONFLATE, 1)
        self.socket.connect("tcp://127.0.0.1:5555")
        self.socket.setsockopt_string(zmq.SUBSCRIBE, "")

        while self.running:
            try:
                if self.socket.poll(100):
                    msg = self.socket.recv(flags=zmq.NOBLOCK)
                    self.process_data(msg)
            except Exception as e:
                logger.error(f"ZMQ Worker Error: {e}")

        self.socket.close()
        self.context.term()

    def process_data(self, data_bytes):
        if len(data_bytes) < 195096:
            return
            
        header = np.frombuffer(data_bytes[:24], dtype=np.uint32)
        num_events = int(header[0])
        samples_per_ch = int(header[1])
        total_events = int(header[2])
        queue_size = int(header[3])
        pool_free_size = int(header[4])
        
        current_time = time.time()
        elapsed = current_time - self.last_telemetry_time
        if elapsed >= 0.5: 
            self.current_rate = (total_events - self.last_events) / elapsed
            self.last_telemetry_time = current_time
            self.last_events = total_events

        telemetry = {
            'events': total_events,
            'rate': round(float(self.current_rate), 1),
            'dataq': queue_size,
            'pool': pool_free_size
        }
        
        if not self.monitoring_enabled or num_events == 0 or samples_per_ch == 0:
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
            
            # 💡 [제1원리 탐지] Config의 OFFSET과 THRESHOLD를 기준으로 직접 평가
            hw_offset = self.hw_config[ch]["offset"]
            hw_thr = self.hw_config[ch]["threshold"]
            
            if len(valid_wave) > 80:
                pedestal_region = valid_wave[20:80]
                self.baseline_stats[ch]["count"] += 1
                self.baseline_stats[ch]["real_mean"] = float(np.mean(pedestal_region))
                
                # 방사선 펄스가 없는 프리트리거 구간의 요동이 설정된 문턱값의 80%를 넘으면 공통모드 EFT 노이즈로 확정
                max_deviation = np.max(np.abs(pedestal_region - hw_offset))
                if max_deviation > (hw_thr * 0.8): 
                    self.baseline_stats[ch]["anomalies"] += 1
                    
                    # FFT 수행 시 동적 평균이 아닌 '하드웨어 절대 오프셋'을 빼서 진짜 흔들림의 크기를 주파수화 함
                    wave_dc_removed = valid_wave - hw_offset
                    fft_vals = np.fft.rfft(wave_dc_removed)
                    fft_power = np.abs(fft_vals) ** 2 
                    freqs = np.fft.rfftfreq(len(wave_dc_removed), d=self.dt)
                    freqs_mhz = freqs / 1e6
                    anomaly_data[ch].append((valid_wave, freqs_mhz, fft_power))

        telemetry['baseline'] = self.baseline_stats
        self.data_ready.emit(waveforms, charges, telemetry, anomaly_data)

    def stop(self):
        self.running = False
        self.wait()
