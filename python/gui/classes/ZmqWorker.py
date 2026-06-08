import struct
import time
import numpy as np
import zmq
from PySide6.QtCore import QThread, Signal, Slot
import logging

logger = logging.getLogger(__name__)

class ZmqWorker(QThread):
    # Signal: waveforms, q_longs, events_processed, baseline_stats, anomaly_data
    data_ready = Signal(object, object, int, dict, dict)

    def __init__(self):
        super().__init__()
        self.running = True
        self.monitoring_enabled = False
        
        self.context = None
        self.socket = None
        
        self.dt = 2e-9 
        self.ped_samples = 500 
        self.baseline_stats = {ch: {"count": 0, "anomalies": 0, "rolling_median": 0.0} for ch in range(4)}

    @Slot(bool)
    def set_monitoring_state(self, state):
        self.monitoring_enabled = state

    @Slot()
    def request_clear(self):
        for ch in range(4):
            self.baseline_stats[ch] = {"count": 0, "anomalies": 0, "rolling_median": 0.0}

    def run(self):
        self.context = zmq.Context()
        self.socket = self.context.socket(zmq.SUB)
        self.socket.setsockopt(zmq.LINGER, 0)
        self.socket.setsockopt(zmq.CONFLATE, 1) # 최신 데이터만 유지 (Drop stale packets)
        self.socket.connect("tcp://127.0.0.1:5555")
        self.socket.setsockopt_string(zmq.SUBSCRIBE, "DATA")

        while self.running:
            if not self.monitoring_enabled:
                time.sleep(0.1)
                continue

            try:
                if self.socket.poll(100):
                    frames = self.socket.recv_multipart()
                    if len(frames) == 2 and frames[0] == b"DATA":
                        self.process_data(frames[1])
                        # 💡 [스로틀링] 사람이 시각적으로 인지하기 편안한 속도(약 3 FPS)로 제한. 
                        # GUI 멈춤(Freezing) 방지 및 CPU 최적화.
                        time.sleep(0.3) 
            except Exception as e:
                logger.error(f"ZMQ Worker Error: {e}")

        self.socket.close()
        self.context.term()

    def process_data(self, data_bytes):
        header_format = "=IIIQ"
        header_size = struct.calcsize(header_format)
        
        offset = 0
        total_size = len(data_bytes)
        events_processed = 0
        
        waveforms = {ch: [] for ch in range(4)}
        q_longs = {ch: [] for ch in range(4)}
        
        while offset + header_size <= total_size:
            magic, evt_size, evt_num, timestamp = struct.unpack_from(header_format, data_bytes, offset)
            if magic != 0x4B464144: break
            
            payload_offset = offset + header_size
            payload_size = evt_size - header_size
            
            if payload_offset + payload_size > total_size: break
                
            ch_format = "=BBH"
            ch_header_size = struct.calcsize(ch_format)
            curr_payload_offset = payload_offset
            
            while curr_payload_offset < payload_offset + payload_size:
                ch_id, trg_type, wave_len = struct.unpack_from(ch_format, data_bytes, curr_payload_offset)
                curr_payload_offset += ch_header_size
                
                wave_format = f"={wave_len}H"
                wave_size = struct.calcsize(wave_format)
                wave_data = struct.unpack_from(wave_format, data_bytes, curr_payload_offset)
                curr_payload_offset += wave_size
                
                if ch_id < 4:
                    waveforms[ch_id].append(wave_data)
                    q_longs[ch_id].append(sum(wave_data))
                    
            offset += evt_size
            events_processed += 1

        # =========================================================================
        # 이상 밴드 탐지 및 선택적 FFT 연산
        # =========================================================================
        anomaly_data = {ch: [] for ch in range(4)}
        
        for ch in range(4):
            if not waveforms[ch]: continue
            
            waves_arr = np.array(waveforms[ch])
            if waves_arr.shape[1] < 100: continue
            
            eval_len = min(self.ped_samples, waves_arr.shape[1])
            analysis_region = waves_arr[:, :eval_len]
            
            q1 = np.percentile(analysis_region, 25, axis=1)
            q3 = np.percentile(analysis_region, 75, axis=1)
            iqr = q3 - q1
            medians = np.median(analysis_region, axis=1)
            
            lower_bound = q1 - 2.0 * iqr
            upper_bound = q3 + 2.0 * iqr
            
            for i in range(len(waves_arr)):
                self.baseline_stats[ch]["count"] += 1
                outliers = np.sum((analysis_region[i] < lower_bound[i]) | (analysis_region[i] > upper_bound[i]))
                
                if outliers > (eval_len * 0.05):
                    self.baseline_stats[ch]["anomalies"] += 1
                    
                    wave = waves_arr[i]
                    wave_dc_removed = wave - np.mean(wave)
                    
                    fft_vals = np.fft.rfft(wave_dc_removed)
                    fft_power = np.abs(fft_vals) ** 2 
                    freqs = np.fft.rfftfreq(len(wave_dc_removed), d=self.dt)
                    
                    freqs_mhz = freqs / 1e6
                    anomaly_data[ch].append((wave, freqs_mhz, fft_power))
                    
        self.data_ready.emit(waveforms, q_longs, events_processed, self.baseline_stats, anomaly_data)

    def stop(self):
        self.running = False
        self.wait()
