import sys
import numpy as np
import zmq
from PySide6.QtWidgets import QApplication, QMainWindow, QWidget, QVBoxLayout, QHBoxLayout
from PySide6.QtCore import QThread, Signal, Slot
import pyqtgraph as pg

# =====================================================================
# 1. ZMQ 수신 및 NumPy 초고속 디코딩 스레드
# =====================================================================
class ZmqReceiverThread(QThread):
    data_ready = Signal(object, object) 

    def __init__(self, record_length=8):
        super().__init__()
        self.running = True
        self.record_length = record_length
        self.samples_per_ch = (record_length * 512) // 2 // 4 

        # 분석 최적화 윈도우 
        self.skip_bins = 20
        self.ped_start = 22
        self.ped_end = 80

    def run(self):
        context = zmq.Context()
        socket = context.socket(zmq.SUB)
        socket.connect("tcp://127.0.0.1:5555")
        socket.setsockopt_string(zmq.SUBSCRIBE, "")

        while self.running:
            try:
                msg = socket.recv(flags=zmq.NOBLOCK)
            except zmq.Again:
                self.msleep(10)
                continue

            raw_data = np.frombuffer(msg, dtype=np.uint16)
            event_size_shorts = 4 * self.samples_per_ch
            num_events = len(raw_data) // event_size_shorts
            
            if num_events == 0:
                continue

            events = raw_data[:num_events * event_size_shorts].reshape((num_events, 4, self.samples_per_ch))

            pedestal = np.mean(events[:, :, self.ped_start:self.ped_end], axis=2, keepdims=True)
            inverted_waveforms = pedestal - events
            charge = np.sum(inverted_waveforms[:, :, self.skip_bins:], axis=2)

            last_waveform = inverted_waveforms[-1, :, :]
            self.data_ready.emit(last_waveform, charge)

    def stop(self):
        self.running = False
        self.wait()

# =====================================================================
# 2. PyQtGraph 기반 고속 실시간 렌더링 GUI
# =====================================================================
class OnlineMonitor(QMainWindow):
    def __init__(self):
        super().__init__()
        self.setWindowTitle("KFADC500 Real-Time Monitor")
        self.resize(1400, 800)

        main_widget = QWidget()
        self.setCentralWidget(main_widget)
        layout = QHBoxLayout(main_widget)

        self.colors = [(0, 255, 255), (255, 0, 255), (255, 255, 0), (0, 255, 0)]

        # 파형 뷰어 영역 (왼쪽)
        wave_layout = QVBoxLayout()
        self.wave_plots = []
        self.wave_curves = []
        for ch in range(4):
            plot = pg.PlotWidget(title=f"Channel {ch} Waveform")
            plot.showGrid(x=True, y=True)
            curve = plot.plot(pen=pg.mkPen(color=self.colors[ch], width=2))
            self.wave_plots.append(plot)
            self.wave_curves.append(curve)
            wave_layout.addWidget(plot)
        layout.addLayout(wave_layout, stretch=1)

        # 전하량 히스토그램 영역 (오른쪽)
        hist_layout = QVBoxLayout()
        self.hist_plots = []
        self.hist_data = [np.array([]) for _ in range(4)] 
        
        for ch in range(4):
            plot = pg.PlotWidget(title=f"Channel {ch} Charge Spectrum")
            plot.showGrid(x=True, y=True)
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

        for ch in range(4):
            self.hist_data[ch] = np.append(self.hist_data[ch], charge[:, ch])[-100000:]
            y, x = np.histogram(self.hist_data[ch], bins=100, range=(-500, 5000))
            self.hist_plots[ch].clear()
            self.hist_plots[ch].plot(x, y, stepMode="center", fillLevel=0, 
                                     fillOutline=True, brush=self.colors[ch])

    def closeEvent(self, event):
        self.receiver.stop()
        event.accept()

if __name__ == "__main__":
    app = QApplication(sys.argv)
    pg.setConfigOptions(antialias=True)
    window = OnlineMonitor()
    window.show()
    sys.exit(app.exec())