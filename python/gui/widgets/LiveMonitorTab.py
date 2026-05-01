import numpy as np
import pyqtgraph as pg
from PySide6.QtWidgets import QWidget, QVBoxLayout, QHBoxLayout, QCheckBox, QPushButton, QLabel
from PySide6.QtCore import Slot, Qt, Signal

class LiveMonitorTab(QWidget):
    monitoring_toggled = Signal(bool) 
    clear_requested = Signal() 

    def __init__(self):
        super().__init__()
        self.line_colors = ['#1F77B4', '#D62728', '#FF7F0E', '#2CA02C']
        self.brush_colors = [(31, 119, 180, 100), (214, 39, 40, 100), (255, 127, 14, 100), (44, 160, 44, 100)]
        self.init_ui()

    def init_ui(self):
        mon_layout = QVBoxLayout(self)

        ctrl_layout = QHBoxLayout()
        self.chk_enable = QCheckBox("Enable Live Monitoring")
        self.chk_enable.setStyleSheet("font-weight: bold; color: #D32F2F; font-size: 14px;")
        self.chk_enable.stateChanged.connect(self.on_enable_changed)
        
        self.btn_clear = QPushButton("[ Clear Spectrum ]")
        self.btn_clear.setStyleSheet("background-color: #757575; color: white; font-weight: bold;")
        self.btn_clear.clicked.connect(self.clear_requested.emit)

        ctrl_layout.addWidget(self.chk_enable)
        ctrl_layout.addStretch()
        ctrl_layout.addWidget(QLabel("Note: Disable monitoring during long-term DAQ to save CPU/GPU resources."))
        ctrl_layout.addStretch()
        ctrl_layout.addWidget(self.btn_clear)
        
        mon_layout.addLayout(ctrl_layout)

        self.glw = pg.GraphicsLayoutWidget()
        self.glw.setBackground('#FFFFFF')
        mon_layout.addWidget(self.glw)

        self.wave_curves = []
        self.hist_curves = []
        self.hist_data = [np.array([]) for _ in range(4)]

        for ch in range(4):
            # 💡 [물리 단위 맵핑] 라벨 추가
            p_wave = self.glw.addPlot(title=f"Channel {ch} Waveform")
            p_wave.setLabel('bottom', "Time", units="ns")
            p_wave.setLabel('left', "Amplitude", units="mV")
            p_wave.showGrid(x=True, y=True, alpha=0.3) 
            p_wave.addItem(pg.InfiniteLine(angle=0, movable=False, pen=pg.mkPen('#FF5252', width=1.5, style=Qt.DashLine)))
            self.wave_curves.append(p_wave.plot(pen=pg.mkPen(color=self.line_colors[ch], width=1.8)))
            
            p_spec = self.glw.addPlot(title=f"Channel {ch} Charge Spectrum")
            p_spec.setLabel('bottom', "Integrated Charge")
            p_spec.showGrid(x=True, y=True, alpha=0.3)
            self.hist_curves.append(p_spec.plot(stepMode="center", fillLevel=0, fillOutline=True, pen=self.line_colors[ch], brush=self.brush_colors[ch]))
            self.glw.nextRow()

    def on_enable_changed(self, state):
        is_enabled = (state == Qt.Checked.value)
        if is_enabled:
            self.chk_enable.setStyleSheet("font-weight: bold; color: #388E3C; font-size: 14px;")
        else:
            self.chk_enable.setStyleSheet("font-weight: bold; color: #D32F2F; font-size: 14px;")
        self.monitoring_toggled.emit(is_enabled)

    @Slot(object, object, int, bool)
    def update_plots(self, waveforms, charges, samples_per_ch, is_visible=True):
        if not self.chk_enable.isChecked(): return

        if isinstance(waveforms, str) and waveforms == "CLEAR":
            self.hist_data = [np.array([]) for _ in range(4)]
            for ch in range(4):
                self.hist_curves[ch].setData([], [])
                self.wave_curves[ch].setData([], [])
            return

        if not is_visible:
            for ch in range(4):
                self.hist_data[ch] = np.append(self.hist_data[ch], charges[ch])[-50000:]
            return

        # 💡 [핵심 물리 변환] X축 Time (2ns 간격) & Y축 Voltage (0.488 mV/ADC)
        time_axis = np.arange(samples_per_ch) * 2.0 
        voltage_factor = 2000.0 / 4096.0

        for ch in range(4):
            # 메모리 누수 방지 (최대 5만 개 이벤트)
            self.hist_data[ch] = np.append(self.hist_data[ch], charges[ch])[-50000:]
            
            # 파형 단위 변환 후 렌더링
            voltage_wave = waveforms[ch, :] * voltage_factor
            self.wave_curves[ch].setData(time_axis, voltage_wave)
            
            if len(self.hist_data[ch]) > 0:
                # 💡 [동적 범위 할당] 가장 큰 펄스가 들어와도 X축이 알아서 늘어납니다.
                min_val = min(-100, np.min(self.hist_data[ch]))
                max_val = max(5000, np.max(self.hist_data[ch]))
                
                y, x_edges = np.histogram(self.hist_data[ch], bins=200, range=(min_val, max_val))
                self.hist_curves[ch].setData(x_edges, y)