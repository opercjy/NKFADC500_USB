import os
import numpy as np
import pyqtgraph as pg
from PySide6.QtWidgets import (QWidget, QVBoxLayout, QHBoxLayout, QCheckBox, 
                               QPushButton, QLabel, QGroupBox, QComboBox)
from PySide6.QtCore import Slot, Qt, Signal
import logging

logger = logging.getLogger(__name__)

# =========================================================================
# 💡 독립 팝업창 (Anomaly Inspector)
# =========================================================================
class AnomalyInspectorWindow(QWidget):
    def __init__(self):
        super().__init__()
        self.setWindowTitle("EFT / Anomaly Inspector (Active 5MHz Target)")
        self.resize(1000, 400)
        self.setWindowFlags(Qt.WindowStaysOnTopHint)
        self.setStyleSheet("background-color: #FFFFFF;") # 배경 강제 흰색
        
        layout = QHBoxLayout(self)
        
        self.plot_wave = pg.PlotWidget(title="Captured Anomaly [Time Domain]")
        self.plot_wave.showGrid(x=True, y=True, alpha=0.3)
        self.plot_wave.setLabel('bottom', 'Time Bins (2ns/bin)')
        self.plot_wave.setLabel('left', 'ADC Count')
        self.curve_wave = self.plot_wave.plot(pen=pg.mkPen(color='#D32F2F', width=2))
        layout.addWidget(self.plot_wave)
        
        self.plot_fft = pg.PlotWidget(title="FFT Power Spectrum [Freq Domain]")
        self.plot_fft.showGrid(x=True, y=True, alpha=0.3)
        self.plot_fft.setLabel('bottom', 'Frequency', units='MHz')
        self.plot_fft.setLabel('left', 'Power')
        self.plot_fft.setXRange(0, 20)
        self.curve_fft = self.plot_fft.plot(pen=pg.mkPen(color='#8E24AA', width=2), fillLevel=0, brush=(142,36,170,100))
        layout.addWidget(self.plot_fft)

    def update_anomaly(self, ch, wave, freqs_mhz, fft_power):
        self.plot_wave.setTitle(f"Captured Anomaly [Time Domain] - Source CH{ch}")
        self.curve_wave.setData(wave)
        self.plot_fft.setTitle(f"FFT Spectrum [Freq Domain] - Source CH{ch}")
        self.curve_fft.setData(freqs_mhz, fft_power)

# =========================================================================
# 메인 Live Monitor 탭
# =========================================================================
class LiveMonitorTab(QWidget):
    monitoring_toggled = Signal(bool)
    clear_requested = Signal()

    def __init__(self):
        super().__init__()
        
        # 💡 [OCP 패치] main.py를 건드리지 않고 이 탭이 메모리에 올라올 때 전역 테마를 화이트/알록달록으로 세팅
        pg.setConfigOptions(antialias=True)
        pg.setConfigOption('background', '#FFFFFF')
        pg.setConfigOption('foreground', '#333333')
        
        self.line_colors = ['#1F77B4', '#D62728', '#FF7F0E', '#2CA02C']
        self.brush_colors = [(31, 119, 180, 100), (214, 39, 40, 100), (255, 127, 14, 100), (44, 160, 44, 100)]
        
        self.dump_dir = "debug_dumps"
        os.makedirs(self.dump_dir, exist_ok=True)
        self.save_enabled = False
        
        self.anomaly_window = None 
        self.init_ui()

    def init_ui(self):
        mon_layout = QVBoxLayout(self)

        ctrl_layout = QHBoxLayout()
        self.chk_enable = QCheckBox("Enable Live Monitoring")
        self.chk_enable.setStyleSheet("font-weight: bold; font-size: 14px;")
        self.chk_enable.stateChanged.connect(self.on_enable_changed)
        
        self.chk_anomaly = QCheckBox("Open Anomaly Inspector")
        self.chk_anomaly.setStyleSheet("font-weight: bold; color: #D32F2F;")
        self.chk_anomaly.stateChanged.connect(self.on_anomaly_checked)
        
        self.cmb_save = QComboBox()
        self.cmb_save.addItems(["Auto-Dump: OFF", "Immediate Dump (Append CSV)"])
        self.cmb_save.currentIndexChanged.connect(self.on_save_mode_changed)
        
        self.btn_clear = QPushButton("[ Clear All Spectra ]")
        self.btn_clear.clicked.connect(self.clear_requested.emit)
        
        ctrl_layout.addWidget(self.chk_enable)
        ctrl_layout.addWidget(self.chk_anomaly)
        ctrl_layout.addWidget(self.cmb_save)
        ctrl_layout.addWidget(self.btn_clear)
        ctrl_layout.addStretch()
        
        self.lbl_stats = []
        for ch in range(4):
            lbl = QLabel(f"CH{ch}: Wait...")
            lbl.setStyleSheet("padding: 2px; border: 1px solid gray;")
            self.lbl_stats.append(lbl)
            ctrl_layout.addWidget(lbl)
            
        mon_layout.addLayout(ctrl_layout)

        plot_layout = QHBoxLayout()
        
        wave_layout = QVBoxLayout()
        self.wave_plots = []
        self.curves_wave = {}
        for ch in range(4):
            plot = pg.PlotWidget(title=f"CH{ch} Live Waveform")
            plot.showGrid(x=True, y=True, alpha=0.3)
            self.wave_plots.append(plot)
            self.curves_wave[ch] = plot.plot(pen=pg.mkPen(color=self.line_colors[ch], width=1.5))
            wave_layout.addWidget(plot)

        hist_layout = QVBoxLayout()
        self.hist_plots = []
        self.hist_curves = {}
        for ch in range(4):
            plot = pg.PlotWidget(title=f"CH{ch} Charge Spectrum")
            plot.showGrid(x=True, y=True, alpha=0.3)
            self.hist_plots.append(plot)
            self.hist_curves[ch] = plot.plot(stepMode="center", fillLevel=0, brush=self.brush_colors[ch], pen=self.line_colors[ch])
            hist_layout.addWidget(plot)

        plot_layout.addLayout(wave_layout, stretch=1)
        plot_layout.addLayout(hist_layout, stretch=1)
        mon_layout.addLayout(plot_layout, stretch=1)

    def on_enable_changed(self, state):
        self.monitoring_toggled.emit((state == Qt.Checked.value) or (state == 2))

    def on_save_mode_changed(self, idx):
        self.save_enabled = (idx == 1)

    def on_anomaly_checked(self, state):
        is_checked = (state == Qt.Checked.value) or (state == 2)
        if is_checked:
            if self.anomaly_window is None:
                self.anomaly_window = AnomalyInspectorWindow()
            self.anomaly_window.show()
        else:
            if self.anomaly_window:
                self.anomaly_window.hide()

    def immediate_dump(self, ch, wave):
        if not self.save_enabled: return
        filepath = os.path.join(self.dump_dir, f"EFT_Anomaly_CH{ch}.csv")
        try:
            with open(filepath, 'a') as f:
                line = ",".join(map(str, wave))
                f.write(line + "\n")
        except Exception as e:
            logger.error(f"Dump Failed: {e}")

    @Slot(object, object, int, dict, dict)
    def update_plots(self, waveforms, q_longs, events_processed, baseline_stats, anomaly_data):
        if not self.chk_enable.isChecked():
            return

        for ch in range(4):
            if ch in waveforms and len(waveforms[ch]) > 0:
                self.curves_wave[ch].setData(waveforms[ch][-1])
                
            if ch in baseline_stats:
                st = baseline_stats[ch]
                if st["count"] > 0:
                    rate = (st["anomalies"] / st["count"]) * 100
                    self.lbl_stats[ch].setText(f"CH{ch} Err: {rate:.1f}%")
                    if rate > 2.0:
                        self.lbl_stats[ch].setStyleSheet("color: red; font-weight: bold;")
                    else:
                        self.lbl_stats[ch].setStyleSheet("color: black;")

        for ch in range(4):
            if ch in anomaly_data and anomaly_data[ch]:
                wave, freqs_mhz, fft_power = anomaly_data[ch][-1]
                
                if self.anomaly_window and self.anomaly_window.isVisible():
                    self.anomaly_window.update_anomaly(ch, wave, freqs_mhz, fft_power)
                
                for w, _, _ in anomaly_data[ch]:
                    self.immediate_dump(ch, w)
