import os
import configparser
import numpy as np
import pyqtgraph as pg
from PySide6.QtWidgets import (QWidget, QVBoxLayout, QHBoxLayout, QCheckBox, 
                               QPushButton, QLabel, QComboBox)
from PySide6.QtCore import Slot, Qt, Signal
import logging

logger = logging.getLogger(__name__)

class AnomalyInspectorWindow(QWidget):
    def __init__(self):
        super().__init__()
        self.setWindowTitle("EFT / Anomaly Inspector (High-Res FFT & Persistence Mode)")
        self.resize(1100, 450)
        self.setWindowFlags(Qt.WindowStaysOnTopHint)
        self.setStyleSheet("background-color: #FFFFFF;") 
        
        self.max_history = 50 
        self.history_waves = []
        self.history_ffts = []
        
        layout = QHBoxLayout(self)
        
        self.plot_wave = pg.PlotWidget(title="Accumulated Anomalies [Time Domain (Raw)]")
        self.plot_wave.showGrid(x=True, y=True, alpha=0.3)
        self.plot_wave.setLabel('bottom', 'Time Bins (2ns/bin)')
        self.plot_wave.setLabel('left', 'ADC Count')
        layout.addWidget(self.plot_wave)
        
        self.plot_fft = pg.PlotWidget(title="High-Res FFT Power Spectrum [Freq Domain]")
        self.plot_fft.showGrid(x=True, y=True, alpha=0.3)
        self.plot_fft.setLabel('bottom', 'Frequency', units='MHz')
        self.plot_fft.setLabel('left', 'Power')
        self.plot_fft.setXRange(0, 20)
        layout.addWidget(self.plot_fft)

    def update_anomaly(self, ch, wave, freqs_mhz, fft_power):
        self.plot_wave.setTitle(f"Accumulated Anomalies [Time Domain] - Source CH{ch}")
        self.plot_fft.setTitle(f"High-Res FFT Spectrum [Freq Domain] - Source CH{ch}")
        
        wave_curve = self.plot_wave.plot(wave, pen=pg.mkPen(color=(211, 47, 47, 100), width=1.5))
        fft_curve = self.plot_fft.plot(freqs_mhz, fft_power, pen=pg.mkPen(color=(142, 36, 170, 100), width=1.5))
        
        self.history_waves.append(wave_curve)
        self.history_ffts.append(fft_curve)
        
        if len(self.history_waves) > self.max_history:
            old_w = self.history_waves.pop(0)
            old_f = self.history_ffts.pop(0)
            self.plot_wave.removeItem(old_w)
            self.plot_fft.removeItem(old_f)
            
    def clear_inspector(self):
        self.plot_wave.clear()
        self.plot_fft.clear()
        self.history_waves.clear()
        self.history_ffts.clear()


class LiveMonitorTab(QWidget):
    monitoring_toggled = Signal(bool)
    clear_requested = Signal()

    def __init__(self):
        super().__init__()
        
        pg.setConfigOptions(antialias=True)
        pg.setConfigOption('background', '#FFFFFF')
        pg.setConfigOption('foreground', '#333333')
        
        self.line_colors = ['#1F77B4', '#D62728', '#FF7F0E', '#2CA02C']
        self.brush_colors = [(31, 119, 180, 100), (214, 39, 40, 100), (255, 127, 14, 100), (44, 160, 44, 100)]
        
        self.dump_dir = "debug_dumps"
        os.makedirs(self.dump_dir, exist_ok=True)
        self.save_enabled = False
        
        self.charge_history = {ch: [] for ch in range(4)}
        self.anomaly_window = None 
        
        self.hw_config = {ch: {"offset": 3500.0, "polarity": 0, "threshold": 20.0} for ch in range(4)}
        self.init_ui()

    def reload_config_and_update_lines(self):
        """💡 Config를 파싱하여 점선의 위치를 물리적으로 정확하게 재배치합니다."""
        config_path = os.path.join(os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(__file__)))), 'config', 'kfadc500.config')
        if not os.path.exists(config_path):
            config_path = "config/kfadc500.config"
            
        parser = configparser.ConfigParser()
        try:
            parser.read(config_path)
            for ch in range(4):
                sec = f"CH{ch}"
                if parser.has_section(sec):
                    off = parser.getfloat(sec, "OFFSET", fallback=3500.0)
                    pol = parser.getint(sec, "POLARITY", fallback=0)
                    thr = parser.getfloat(sec, "THRESHOLD", fallback=20.0)
                    self.hw_config[ch] = {"offset": off, "polarity": pol, "threshold": thr}
                    
                    # 1. 붉은색 베이스라인 설정
                    self.baseline_lines[ch].setValue(off)
                    
                    # 2. 오렌지색 트리거 한계선 설정 (극성에 따른 전압 강하 방향 적용)
                    trigger_val = (off - thr) if pol == 0 else (off + thr)
                    self.threshold_lines[ch].setValue(trigger_val)
                    
                    self.lbl_config_info[ch].setText(f"[ Off: {off:.0f} | Thr: {thr:.0f} ]")
        except Exception as e:
            logger.error(f"LiveMonitor Config Parse Error: {e}")

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
        self.btn_clear.clicked.connect(self.on_clear_requested)
        
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
        self.baseline_lines = {}  # 붉은색 기준선
        self.threshold_lines = {} # 💡 오렌지색 트리거선
        self.lbl_config_info = {} # 💡 파형 뷰어 내부 정보 라벨
        
        for ch in range(4):
            plot = pg.PlotWidget(title=f"CH{ch} Live Waveform (Raw)")
            plot.showGrid(x=True, y=True, alpha=0.3)
            
            # 베이스라인 (빨간 점선)
            bline = pg.InfiniteLine(angle=0, movable=False, pen=pg.mkPen('#D32F2F', width=2, style=Qt.DashLine))
            plot.addItem(bline)
            self.baseline_lines[ch] = bline
            
            # 💡 트리거 한계선 (오렌지색 촘촘한 점선)
            tline = pg.InfiniteLine(angle=0, movable=False, pen=pg.mkPen('#F57C00', width=2, style=Qt.DotLine))
            plot.addItem(tline)
            self.threshold_lines[ch] = tline
            
            # 설정 정보 텍스트 오버레이
            info_label = pg.TextItem(text="Loading...", color='#1565C0', anchor=(0, 1))
            plot.addItem(info_label)
            info_label.setPos(0, 0) # 우측 상단 배치를 위해 나중에 뷰포트 고정 처리 가능
            self.lbl_config_info[ch] = info_label
            
            self.wave_plots.append(plot)
            self.curves_wave[ch] = plot.plot(pen=pg.mkPen(color=self.line_colors[ch], width=1.5))
            wave_layout.addWidget(plot)

        hist_layout = QVBoxLayout()
        self.hist_plots = []
        self.hist_curves = {}
        for ch in range(4):
            plot = pg.PlotWidget(title=f"CH{ch} Charge Spectrum (Inverted)")
            plot.showGrid(x=True, y=True, alpha=0.3)
            self.hist_plots.append(plot)
            self.hist_curves[ch] = plot.plot(stepMode="center", fillLevel=0, brush=self.brush_colors[ch], pen=self.line_colors[ch])
            hist_layout.addWidget(plot)

        plot_layout.addLayout(wave_layout, stretch=1)
        plot_layout.addLayout(hist_layout, stretch=1)
        mon_layout.addLayout(plot_layout, stretch=1)

    def on_enable_changed(self, state):
        is_checked = (state == Qt.Checked.value) or (state == 2)
        if is_checked:
            self.reload_config_and_update_lines() # 💡 켤 때마다 최신 Config 적용
        self.monitoring_toggled.emit(is_checked)

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

    def on_clear_requested(self):
        for ch in range(4):
            self.charge_history[ch].clear()
            self.hist_curves[ch].setData([], [])
        if self.anomaly_window:
            self.anomaly_window.clear_inspector()
        self.clear_requested.emit()

    def immediate_dump(self, ch, wave):
        if not self.save_enabled: return
        filepath = os.path.join(self.dump_dir, f"EFT_Anomaly_CH{ch}.csv")
        try:
            with open(filepath, 'a') as f:
                line = ",".join(map(str, wave))
                f.write(line + "\n")
        except Exception as e:
            logger.error(f"Dump Failed: {e}")

    @Slot(object, object, bool, object)
    def update_plots(self, waveforms, charges, is_visible, anomaly_data):
        if not self.chk_enable.isChecked() or not is_visible:
            return

        for ch in range(4):
            if ch in waveforms and len(waveforms[ch]) > 0:
                self.curves_wave[ch].setData(waveforms[ch][-1])
                
            if ch in charges and len(charges[ch]) > 0:
                self.charge_history[ch].extend(charges[ch])
                self.charge_history[ch] = self.charge_history[ch][-10000:]
                y, x = np.histogram(self.charge_history[ch], bins=100)
                self.hist_curves[ch].setData(x, y)
                
            if ch in anomaly_data and len(anomaly_data[ch]) > 0:
                self.lbl_stats[ch].setText(f"CH{ch} Anomaly Det!")
                self.lbl_stats[ch].setStyleSheet("color: red; font-weight: bold;")
            else:
                self.lbl_stats[ch].setText(f"CH{ch} Normal")
                self.lbl_stats[ch].setStyleSheet("color: black;")

        for ch in range(4):
            if ch in anomaly_data and anomaly_data[ch]:
                if self.anomaly_window and self.anomaly_window.isVisible():
                    for wave, freqs_mhz, fft_power in anomaly_data[ch]:
                        self.anomaly_window.update_anomaly(ch, wave, freqs_mhz, fft_power)
                
                for w, _, _ in anomaly_data[ch]:
                    self.immediate_dump(ch, w)
