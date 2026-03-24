import sys
import signal
import numpy as np
import zmq
from PySide6.QtWidgets import QApplication, QMainWindow, QWidget, QVBoxLayout, QHBoxLayout
from PySide6.QtCore import QThread, Signal, Slot, Qt
import pyqtgraph as pg

# =====================================================================
# 1. ZMQ 수신 및 NumPy 초고속 디코딩 스레드 (안전한 종료 보장)
# =====================================================================
class ZmqReceiverThread(QThread):
    data_ready = Signal(object, object) 

    def __init__(self, record_length=8):
        super().__init__()
        self.running = True
        self.record_length = record_length
        self.samples_per_ch = (record_length * 512) // 2 // 4 

        self.skip_bins = 20
        self.ped_start = 22
        self.ped_end = 80
        
        self.context = None
        self.socket = None

    def run(self):
        self.context = zmq.Context()
        self.socket = self.context.socket(zmq.SUB)
        self.socket.setsockopt(zmq.LINGER, 0) # 데드락 원천 차단
        
        self.socket.connect("tcp://127.0.0.1:5555")
        self.socket.setsockopt_string(zmq.SUBSCRIBE, "")

        while self.running:
            try:
                msg = self.socket.recv(flags=zmq.NOBLOCK)
            except zmq.Again:
                self.msleep(10)
                continue
            except zmq.ContextTerminated:
                break
            except Exception:
                break

            if not self.running:
                break

            raw_data = np.frombuffer(msg, dtype=np.uint16)
            event_size_shorts = 4 * self.samples_per_ch
            num_events = len(raw_data) // event_size_shorts
            
            if num_events == 0:
                continue

            events = raw_data[:num_events * event_size_shorts].reshape((num_events, 4, self.samples_per_ch))

            pedestal = np.mean(events[:, :, self.ped_start:self.ped_end], axis=2, keepdims=True)
            inverted_waveforms = pedestal - events
            
            # OOM 방지: 매 연산 직후 NumPy 가비지 컬렉팅 범위 내에서 고정 메모리만 사용
            charge = np.sum(inverted_waveforms[:, :, self.skip_bins:], axis=2)
            last_waveform = inverted_waveforms[-1, :, :]
            
            if self.running:
                self.data_ready.emit(last_waveform, charge)

        if self.socket:
            self.socket.close()
        if self.context:
            self.context.term()

    def stop(self):
        self.running = False
        self.wait(500) # 스레드 즉각 회수

# =====================================================================
# 2. 프로페셔널 라이트 테마 (베이지 배경) UI/UX 코어
# =====================================================================
class OnlineMonitor(QMainWindow):
    def __init__(self):
        super().__init__()
        self.setWindowTitle("KFADC500 Professional Real-Time Monitor")
        self.resize(1600, 900)

        # 💡 따뜻하고 눈이 편안한 오프화이트/베이지 배경 및 짙은 회색 텍스트
        pg.setConfigOption('background', '#FDF6E3') # Solarized Light 베이지
        pg.setConfigOption('foreground', '#333333') # 진한 차콜 텍스트

        main_widget = QWidget()
        self.setCentralWidget(main_widget)
        layout = QHBoxLayout(main_widget)

        # 💡 밝은 배경에 최적화된 높은 가시성의 클래식 색상 (파랑, 빨강, 진주황, 초록)
        self.line_colors = ['#1F77B4', '#D62728', '#FF7F0E', '#2CA02C']
        self.brush_colors = [(31, 119, 180, 100), (214, 39, 40, 100), (255, 127, 14, 100), (44, 160, 44, 100)]

        # --- 파형 뷰어 영역 (왼쪽) ---
        wave_layout = QVBoxLayout()
        self.wave_plots = []
        self.wave_curves = []
        for ch in range(4):
            plot = pg.PlotWidget(title=f"Channel {ch} Waveform")
            # 밝은 배경에 맞는 진한 회색 눈금선
            plot.showGrid(x=True, y=True, alpha=0.3) 
            
            # 💡 0점 베이스라인 표시 (눈에 확 띄는 붉은색 점선)
            baseline = pg.InfiniteLine(angle=0, movable=False, pen=pg.mkPen('#FF5252', width=1.5, style=Qt.DashLine))
            plot.addItem(baseline)

            curve = plot.plot(pen=pg.mkPen(color=self.line_colors[ch], width=1.5))
            self.wave_plots.append(plot)
            self.wave_curves.append(curve)
            wave_layout.addWidget(plot)
        layout.addLayout(wave_layout, stretch=1)

        # --- 전하량 히스토그램 영역 (오른쪽) ---
        hist_layout = QVBoxLayout()
        self.hist_plots = []
        self.hist_data = [np.array([]) for _ in range(4)] 
        
        for ch in range(4):
            plot = pg.PlotWidget(title=f"Channel {ch} Charge Spectrum")
            plot.showGrid(x=True, y=True, alpha=0.3)
            self.hist_plots.append(plot)
            hist_layout.addWidget(plot)
        layout.addLayout(hist_layout, stretch=1)

        self.receiver = ZmqReceiverThread()
        self.receiver.data_ready.connect(self.update_plots)
        self.receiver.start()

    @Slot(object, object)
    def update_plots(self, waveform, charge):
        for ch in range(4):
            self.wave_curves[ch].setData(waveform[ch, :])

        # 누적 전하량 메모리 5만 개로 제한 (OOM 절대 방지)
        for ch in range(4):
            self.hist_data[ch] = np.append(self.hist_data[ch], charge[:, ch])[-50000:]
            y, x = np.histogram(self.hist_data[ch], bins=150, range=(-500, 5000))
            self.hist_plots[ch].clear()
            self.hist_plots[ch].plot(x, y, stepMode="center", fillLevel=0, 
                                     fillOutline=True, pen=self.line_colors[ch], brush=self.brush_colors[ch])

    def closeEvent(self, event):
        self.receiver.stop()
        event.accept()

if __name__ == "__main__":
    signal.signal(signal.SIGINT, signal.SIG_DFL)
    app = QApplication(sys.argv)
    pg.setConfigOptions(antialias=True)
    window = OnlineMonitor()
    window.show()
    sys.exit(app.exec())