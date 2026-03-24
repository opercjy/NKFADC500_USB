import numpy as np
import pyqtgraph as pg
from PySide6.QtWidgets import QWidget, QVBoxLayout
from PySide6.QtCore import Slot, Qt

class LiveMonitorTab(QWidget):
    def __init__(self):
        super().__init__()
        self.line_colors = ['#1F77B4', '#D62728', '#FF7F0E', '#2CA02C']
        self.brush_colors = [(31, 119, 180, 100), (214, 39, 40, 100), (255, 127, 14, 100), (44, 160, 44, 100)]
        self.init_ui()

    def init_ui(self):
        mon_layout = QVBoxLayout(self)
        self.glw = pg.GraphicsLayoutWidget()
        self.glw.setBackground('#FDF6E3')
        mon_layout.addWidget(self.glw)

        self.wave_curves = []
        self.hist_curves = []
        self.hist_data = [np.array([]) for _ in range(4)] 

        for ch in range(4):
            p_wave = self.glw.addPlot(title=f"Channel {ch} Waveform")
            p_wave.showGrid(x=True, y=True, alpha=0.3) 
            p_wave.addItem(pg.InfiniteLine(angle=0, movable=False, pen=pg.mkPen('#FF5252', width=1.5, style=Qt.DashLine)))
            self.wave_curves.append(p_wave.plot(pen=pg.mkPen(color=self.line_colors[ch], width=1.8)))
            
            p_spec = self.glw.addPlot(title=f"Channel {ch} Charge Spectrum")
            p_spec.showGrid(x=True, y=True, alpha=0.3)
            self.hist_curves.append(p_spec.plot(stepMode="center", fillLevel=0, fillOutline=True, pen=self.line_colors[ch], brush=self.brush_colors[ch]))
            self.glw.nextRow()

    @Slot(object, object, bool)
    def update_plots(self, waveform, charge_list, is_visible=True):
        for ch in range(4):
            self.hist_data[ch] = np.append(self.hist_data[ch], charge_list[ch])[-50000:]

        # 💡 [핵심 패치] 가시성(is_visible) 데드락 제거
        # 사용자가 런 컨트롤 탭에 있을 때 도착한 첫 데이터 버스트를 버리지 않고 즉시 객체에 주입합니다.
        # PyQtGraph 엔진은 화면에 보이지 않는 위젯에 대해 자동으로 GPU 렌더링을 생략하므로, 
        # 수동으로 막을 필요가 없으며 오히려 탭 전환 시 화면이 갱신되지 않는 버그를 원천 차단합니다.
        for ch in range(4):
            self.wave_curves[ch].setData(waveform[ch, :])
            if len(self.hist_data[ch]) > 0:
                y, x_edges = np.histogram(self.hist_data[ch], bins=150, range=(-500, 5000))
                self.hist_curves[ch].setData(x_edges, y)