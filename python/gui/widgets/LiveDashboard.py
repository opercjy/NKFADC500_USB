import os
from PySide6.QtWidgets import QGroupBox, QVBoxLayout, QHBoxLayout, QLabel, QLCDNumber

class LiveDashboard(QGroupBox):
    def __init__(self):
        super().__init__("Live DAQ Dashboard")
        self.setStyleSheet("QGroupBox { background-color: #EAE3CB; border: 2px solid #8C8C8C; min-width: 320px; max-width: 400px;}")
        self.init_ui()

    def init_ui(self):
        l_dash = QVBoxLayout(self)
        
        self.lbl_mode = QLabel("MODE: IDLE")
        self.lbl_mode.setStyleSheet("color: #1F77B4; font-size: 16px; font-weight: bold;")
        self.lbl_limit = QLabel("Limit: None"); self.lbl_limit.setStyleSheet("color: #555555; font-weight:bold;")
        self.lbl_file = QLabel("Current File: -"); self.lbl_file.setStyleSheet("color: #555555; font-weight:bold;")
        l_dash.addWidget(self.lbl_mode); l_dash.addWidget(self.lbl_limit); l_dash.addWidget(self.lbl_file)
        
        h_time = QHBoxLayout()
        self.lbl_start = QLabel("Start: --:--:--"); self.lbl_start.setStyleSheet("color: #333333;")
        self.lbl_elapsed = QLabel("Elapsed: 00:00:00"); self.lbl_elapsed.setStyleSheet("color: #D62728;")
        h_time.addWidget(self.lbl_start); h_time.addWidget(self.lbl_elapsed)
        l_dash.addLayout(h_time)

        l_dash.addWidget(QLabel("Total Events Acquired:"))
        self.lcd_evt = QLCDNumber()
        self.lcd_evt.setDigitCount(9); self.lcd_evt.setSegmentStyle(QLCDNumber.Flat)
        self.lcd_evt.setStyleSheet("color: #D62728; background: #FFFFFF; min-height: 50px;")
        l_dash.addWidget(self.lcd_evt)

        self.lbl_rate = QLabel("Rate: 0.0 Hz")
        self.lbl_rate.setStyleSheet("color: #2CA02C; font-size: 16px; font-weight: bold;")
        l_dash.addWidget(self.lbl_rate)
        
        self.lbl_size = QLabel("File Size: 0.00 MB"); self.lbl_size.setStyleSheet("color: #FF7F0E; font-size: 14px;")
        self.lbl_speed = QLabel("Speed: 0.00 MB/s"); self.lbl_speed.setStyleSheet("color: #1F77B4; font-size: 14px;")
        l_dash.addWidget(self.lbl_size); l_dash.addWidget(self.lbl_speed)

        h_sys = QHBoxLayout()
        self.lbl_q = QLabel("DataQ: 0"); self.lbl_q.setStyleSheet("background: #FFF9C4; border: 1px solid #FBC02D; padding: 3px; color: #F57F17;")
        self.lbl_p = QLabel("Pool: 1000"); self.lbl_p.setStyleSheet("background: #C8E6C9; border: 1px solid #388E3C; padding: 3px; color: #1B5E20;")
        h_sys.addWidget(self.lbl_q); h_sys.addWidget(self.lbl_p)
        l_dash.addLayout(h_sys)

        self.lbl_path = QLabel(f"Path: {os.getcwd()}"); self.lbl_path.setStyleSheet("color: #757575; font-size: 11px;")
        self.lbl_disk = QLabel("Disk Free: Checking..."); self.lbl_disk.setStyleSheet("color: #8E24AA; font-size: 12px;")
        l_dash.addWidget(self.lbl_path); l_dash.addWidget(self.lbl_disk)

        self.lbl_cfg_summary = QLabel("Config Parameters Loading...")
        self.lbl_cfg_summary.setStyleSheet("background-color: #FFFFFF; border: 1px solid #CCCCCC; padding: 5px;")
        self.lbl_cfg_summary.setWordWrap(True)
        l_dash.addWidget(QLabel("Current Configuration:")); l_dash.addWidget(self.lbl_cfg_summary)
        l_dash.addStretch()

    def update_stats(self, stats, record_length, elapsed_sec):
        if 'events' in stats:
            events = stats['events']
            self.lcd_evt.display(events)
            size_mb = (events * record_length * 512) / 1048576.0
            self.lbl_size.setText(f"File Size: {size_mb:.2f} MB")
            if elapsed_sec > 0:
                self.lbl_speed.setText(f"Speed: {size_mb / elapsed_sec:.2f} MB/s")
        if 'rate' in stats: self.lbl_rate.setText(f"Rate: {stats['rate']} Hz")
        if 'dataq' in stats: self.lbl_q.setText(f"DataQ: {stats['dataq']}")
        if 'pool' in stats: self.lbl_p.setText(f"Pool: {stats['pool']}")