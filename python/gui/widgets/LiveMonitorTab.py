import os
import numpy as np
import pyqtgraph as pg
from PySide6.QtWidgets import (QWidget, QVBoxLayout, QHBoxLayout, QCheckBox, 
                               QPushButton, QLabel, QGroupBox, QComboBox)
from PySide6.QtCore import Slot, Qt, Signal
import logging

logger = logging.getLogger(__name__)

class LiveMonitorTab(QWidget):
    monitoring_toggled = Signal(bool)
    clear_requested = Signal()

    def __init__(self):
        super().__init__()
        self.line_colors = ['#1F77B4', '#D62728', '#FF7F0E', '#2CA02C']
        self.brush_colors = [(31, 119, 180, 100), (214, 39, 40, 100), (255, 127, 14, 100), (44, 160, 44, 100)]
        
        # 디버그 캡처 덤프 폴더
        self.dump_dir = "debug_dumps"
        os.makedirs(self.dump_dir, exist_ok=True)
        self.save_enabled = False
        
        self.init_ui()

    def init_ui(self):
        mon_layout = QVBoxLayout(self)

        # =========================================================================
        # 1. 상단 컨트롤 패널
        # =========================================================================
        ctrl_layout = QHBoxLayout()
        self.chk_enable = QCheckBox("Enable Live Monitoring")
        self.chk_enable.setStyleSheet("font-weight: bold; font-size: 14px;")
        self.chk_enable.stateChanged.connect(self.on_enable_changed)
        
        self.cmb_save = QComboBox()
        self.cmb_save.addItems(["Auto-Dump: OFF", "Immediate Dump (Append CSV)"])
        self.cmb_save.currentIndexChanged.connect(self.on_save_mode_changed)
        
        self.btn_clear = QPushButton("[ Clear All Spectra ]")
        self.btn_clear.clicked.connect(self.clear_requested.emit)
        
        ctrl_layout.addWidget(self.chk_enable)
        ctrl_layout.addWidget(self.cmb_save)
        ctrl_layout.addWidget(self.btn_clear)
        ctrl_layout.addStretch()
        mon_layout.addLayout(ctrl_layout)

        # =========================================================================
        # 💡 2. 확장 기능: FFT 및 이상 파형 인스펙터 (화면 상단 분할)
        # =========================================================================
        self.grp_anomaly = QGroupBox("Target Inspector: High-Freq Baseline Anomaly (Time vs FFT)")
        self.grp_anomaly.setStyleSheet("QGroupBox { color: #8E24AA; font-weight: bold; font-size: 13px; }")
        anomaly_layout = QHBoxLayout()
        
        # 좌측: Time Domain (파형)
        self.plot_ano_wave = pg.PlotWidget(title="Captured Anomaly [Time Domain]")
        self.plot_ano_wave.showGrid(x=True, y=True, alpha=0.4)
        self.plot_ano_wave.setLabel('bottom', 'Time Bins (2ns/bin)')
        self.plot_ano_wave.setLabel('left', 'ADC Count')
        self.curve_ano_wave = self.plot_ano_wave.plot(pen=pg.mkPen(color='#D32F2F', width=2))
        anomaly_layout.addWidget(self.plot_ano_wave, stretch=1)
        
        # 우측: Frequency Domain (FFT 주파수)
        self.plot_ano_fft = pg.PlotWidget(title="FFT Power Spectrum [Freq Domain]")
        self.plot_ano_fft.showGrid(x=True, y=True, alpha=0.4)
        self.plot_ano_fft.setLabel('bottom', 'Frequency', units='MHz')
        self.plot_ano_fft.setLabel('left', 'Power')
        self.plot_ano_fft.setXRange(0, 20) # 0~20MHz 대역에 집중
        self.curve_ano_fft = self.plot_ano_fft.plot(pen=pg.mkPen(color='#512DA8', width=2), fillLevel=0, brush=(81,45,168,100))
        anomaly_layout.addWidget(self.plot_ano_fft, stretch=1)

        self.grp_anomaly.setLayout(anomaly_layout)
        # 상단 패널 비율
        mon_layout.addWidget(self.grp_anomaly, stretch=2)

        # =========================================================================
        # 3. 원본 기능: 기존 4채널 원시 파형 및 Charge 히스토그램 (화면 하단 분할)
        # =========================================================================
        normal_layout = QHBoxLayout()
        
        # 원본 Waveform 레이아웃
        wave_layout = QVBoxLayout()
        self.wave_plots = []
        self.curves_wave = {}
        for ch in range(4):
            plot = pg.PlotWidget(title=f"CH{ch} Live Waveform")
            plot.showGrid(x=True, y=True, alpha=0.3)
            self.wave_plots.append(plot)
            self.curves_wave[ch] = plot.plot(pen=pg.mkPen(color=self.line_colors[ch], width=1.5))
            wave_layout.addWidget(plot)

        # 원본 Charge 히스토그램 레이아웃
        hist_layout = QVBoxLayout()
        self.hist_plots = []
        self.hist_curves = {}
        for ch in range(4):
            plot = pg.PlotWidget(title=f"CH{ch} Charge Spectrum")
            plot.showGrid(x=True, y=True, alpha=0.3)
            self.hist_plots.append(plot)
            self.hist_curves[ch] = plot.plot(stepMode="center", fillLevel=0, brush=self.brush_colors[ch], pen=self.line_colors[ch])
            hist_layout.addWidget(plot)

        normal_layout.addLayout(wave_layout, stretch=1)
        normal_layout.addLayout(hist_layout, stretch=1)
        # 하단 패널 비율 (더 크게)
        mon_layout.addLayout(normal_layout, stretch=3)

    def on_enable_changed(self, state):
        self.monitoring_toggled.emit((state == Qt.Checked.value) or (state == 2))

    def on_save_mode_changed(self, idx):
        self.save_enabled = (idx == 1)

    def immediate_dump(self, ch, wave):
        """[확장 기능] 동기화율이 낮아도 프로그램 크래시를 방지하는 실시간 덧붙임(Append) 저장"""
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

        # -------------------------------------------------------------------------
        # 1. 원본 모니터링 업데이트 (기존 기능 손실 없음)
        # -------------------------------------------------------------------------
        for ch in range(4):
            if ch in waveforms and len(waveforms[ch]) > 0:
                self.curves_wave[ch].setData(waveforms[ch][-1])

            # (Charge 히스토그램 업데이트는 기존 로직을 따라 외부에서 처리되거나, 
            #  여기서 히스토그램 데이터를 누적하여 setData를 호출합니다. 기존 구조 유지.)

        # -------------------------------------------------------------------------
        # 2. 확장 기능: 이상 파형 탐지 시 FFT 패널 갱신 및 덤프 수행
        # -------------------------------------------------------------------------
        anomaly_found = False
        for ch in range(4):
            if ch in anomaly_data and anomaly_data[ch]:
                # 뷰어에는 가장 마지막에 발생한 대표 1개만 오버레이 표시
                wave, freqs_mhz, fft_power = anomaly_data[ch][-1]
                
                self.plot_ano_wave.setTitle(f"Captured Anomaly [Time] - Source CH{ch}")
                self.curve_ano_wave.setData(wave)
                
                self.plot_ano_fft.setTitle(f"FFT Spectrum [Freq] - Source CH{ch}")
                self.curve_ano_fft.setData(freqs_mhz, fft_power)
                
                # 저장 모드 켜져있으면 탐지된 모든 에러 펄스를 안전하게 덤프
                for w, _, _ in anomaly_data[ch]:
                    self.immediate_dump(ch, w)
                    
                anomaly_found = True
        
        # 이상 감지 시 패널 시각 효과 (빨간 테두리)
        if anomaly_found:
            self.grp_anomaly.setStyleSheet("QGroupBox { color: #FFFFFF; background-color: #311B92; font-weight: bold; border: 2px solid red; }")
        else:
            self.grp_anomaly.setStyleSheet("QGroupBox { color: #8E24AA; font-weight: bold; font-size: 13px; }")
