import os
import re
from datetime import datetime
from PySide6.QtWidgets import (QWidget, QVBoxLayout, QHBoxLayout, QGroupBox, 
                               QLineEdit, QPushButton, QLabel, QSpinBox, QRadioButton, QFileDialog, QComboBox)
from PySide6.QtCore import QTimer

class RunControlTab(QWidget):
    def __init__(self, daq_manager, db_manager, dashboard, log_callback, get_hv_cb=None):
        super().__init__()
        self.daq = daq_manager
        self.db = db_manager
        self.dash = dashboard
        self.log_callback = log_callback
        self.get_hv_cb = get_hv_cb

        self.start_time = None
        self.last_events = 0
        self.config_record_len = 8
        self.auto_mode = "NONE"
        self.current_subrun = 1
        self.max_subruns = 1
        self.scan_queue = []
        self.active_config_text = ""

        self.init_ui()
        self.parse_config_and_update_dashboard()
        self.load_last_run_settings()
        
        # 💡 [핵심 패치] 하드웨어가 준비되었다는 신호를 받으면, 비로소 시간을 재기 시작합니다!
        self.daq.run_started.connect(self.on_actual_run_started)

    def init_ui(self):
        run_layout = QVBoxLayout(self)
        
        grp_basic = QGroupBox("Basic DAQ Settings")
        l_basic = QVBoxLayout()
        
        h_cfg = QHBoxLayout()
        h_cfg.addWidget(QLabel("Config File:")); self.in_cfg = QLineEdit("config/kfadc500.config"); h_cfg.addWidget(self.in_cfg)
        btn_cfg_browse = QPushButton("Browse"); btn_cfg_browse.clicked.connect(self.browse_config_file); h_cfg.addWidget(btn_cfg_browse)
        l_basic.addLayout(h_cfg)

        h_dir = QHBoxLayout()
        h_dir.addWidget(QLabel("Output Dir:"))
        self.in_out_dir = QLineEdit("data")
        btn_dir_browse = QPushButton("Browse")
        btn_dir_browse.clicked.connect(self.browse_output_dir)
        h_dir.addWidget(self.in_out_dir); h_dir.addWidget(btn_dir_browse)
        l_basic.addLayout(h_dir)

        h_name = QHBoxLayout()
        h_name.addWidget(QLabel("Prefix:"))
        self.in_prefix = QLineEdit("test") 
        h_name.addWidget(self.in_prefix)

        h_name.addWidget(QLabel("Run No:"))
        self.sp_run_no = QSpinBox()
        self.sp_run_no.setRange(1, 99999)
        self.sp_run_no.setValue(1)
        h_name.addWidget(self.sp_run_no)

        h_name.addWidget(QLabel("Tag:"))
        self.cb_tag = QComboBox()
        self.cb_tag.addItems(["physics", "calibration", "test", "pedestal", "dark_noise"])
        h_name.addWidget(self.cb_tag)
        l_basic.addLayout(h_name)
        
        h_limit = QHBoxLayout()
        self.rb_cont = QRadioButton("Continuous"); self.rb_cont.setChecked(True)
        self.rb_evt = QRadioButton("Max Events")
        self.rb_time = QRadioButton("Max Time (s)")
        self.sp_limit = QSpinBox(); self.sp_limit.setRange(1, 99999999); self.sp_limit.setValue(10000)
        h_limit.addWidget(self.rb_cont); h_limit.addWidget(self.rb_evt); h_limit.addWidget(self.rb_time); h_limit.addWidget(self.sp_limit)
        l_basic.addLayout(h_limit)
        
        grp_basic.setLayout(l_basic); run_layout.addWidget(grp_basic)

        grp_multi = QGroupBox("Multi-Run / Long-Term DAQ")
        l_multi = QHBoxLayout()
        self.sp_sub_max = QSpinBox(); self.sp_sub_max.setRange(1, 9999); self.sp_sub_max.setValue(1)
        self.sp_sub_idle = QSpinBox(); self.sp_sub_idle.setRange(0, 3600); self.sp_sub_idle.setValue(5)
        l_multi.addWidget(QLabel("Total Sub-runs:")); l_multi.addWidget(self.sp_sub_max)
        l_multi.addWidget(QLabel("Idle Time (s):")); l_multi.addWidget(self.sp_sub_idle)
        l_multi.addStretch()
        grp_multi.setLayout(l_multi); run_layout.addWidget(grp_multi)

        grp_scan = QGroupBox("Auto Threshold Scan")
        l_scan = QHBoxLayout()
        self.sp_start = QSpinBox(); self.sp_start.setRange(10, 1000); self.sp_start.setValue(50)
        self.sp_end = QSpinBox(); self.sp_end.setRange(10, 1000); self.sp_end.setValue(150)
        self.sp_step = QSpinBox(); self.sp_step.setRange(1, 100); self.sp_step.setValue(10)
        self.sp_scan_idle = QSpinBox(); self.sp_scan_idle.setRange(0, 3600); self.sp_scan_idle.setValue(3)
        l_scan.addWidget(QLabel("Start THR:")); l_scan.addWidget(self.sp_start)
        l_scan.addWidget(QLabel("End THR:")); l_scan.addWidget(self.sp_end)
        l_scan.addWidget(QLabel("Step:")); l_scan.addWidget(self.sp_step)
        l_scan.addWidget(QLabel("Idle (s):")); l_scan.addWidget(self.sp_scan_idle)
        grp_scan.setLayout(l_scan); run_layout.addWidget(grp_scan)

        h_btns = QHBoxLayout()
        self.btn_start = QPushButton("[ START STANDARD DAQ ]")
        self.btn_start.setStyleSheet("background-color: #2CA02C; color: white; padding: 15px; font-weight:bold;")
        self.btn_start.clicked.connect(lambda: self.start_standard_daq(1))
        
        self.btn_scan = QPushButton("[ START THR SCAN ]")
        self.btn_scan.setStyleSheet("background-color: #FF7F0E; color: white; padding: 15px; font-weight:bold;")
        self.btn_scan.clicked.connect(self.start_scan_daq)

        self.btn_stop = QPushButton("[ STOP DAQ ]")
        self.btn_stop.setStyleSheet("background-color: #D62728; color: white; padding: 15px; font-weight:bold;")
        self.btn_stop.setEnabled(False)
        self.btn_stop.clicked.connect(self.stop_daq)

        h_btns.addWidget(self.btn_start); h_btns.addWidget(self.btn_scan); h_btns.addWidget(self.btn_stop)
        run_layout.addLayout(h_btns); run_layout.addStretch()

    def on_actual_run_started(self):
        # 하드웨어 세팅(약 2~3초)이 끝난 후 이 함수가 불리면 그때 시간을 잽니다.
        self.start_time = datetime.now()
        self.dash.lbl_start.setText(f"Start: {self.start_time.strftime('%H:%M:%S')}")
        self.log_callback("<span style='color:#009688;'><b>[GUI] Synchronized Timer with Hardware Start.</b></span>")

    def load_last_run_settings(self):
        last_file = self.db.get_last_daq_run()
        if not last_file: return
        basename = os.path.basename(last_file) 
        name_no_ext = os.path.splitext(basename)[0] 
        match = re.search(r'_(?P<num>\d+)_?(?P<tag>[a-zA-Z_]+)?(?:_sub\d+)?$', name_no_ext)
        if match:
            prefix = name_no_ext[:match.start()]
            if prefix: self.in_prefix.setText(prefix)
            last_num = int(match.group('num'))
            self.sp_run_no.setValue(last_num + 1)
            tag = match.group('tag')
            if tag:
                index = self.cb_tag.findText(tag)
                if index >= 0: self.cb_tag.setCurrentIndex(index)

    def browse_config_file(self):
        f, _ = QFileDialog.getOpenFileName(self, "Select DAQ Config", "config", "Config Files (*.config *.cfg);;All Files (*)")
        if f: 
            self.in_cfg.setText(f)
            self.parse_config_and_update_dashboard()
    
    def browse_output_dir(self):
        d = QFileDialog.getExistingDirectory(self, "Select Output Directory", "data")
        if d: 
            self.in_out_dir.setText(d)

    def parse_config_and_update_dashboard(self):
        params = {}
        self.active_config_text = ""
        try:
            with open(self.in_cfg.text(), 'r') as f:
                for line in f:
                    clean = line.split('#')[0].strip()
                    if clean and "=" in clean:
                        k, v = clean.split('=', 1)
                        params[k.strip()] = v.strip()
                        self.active_config_text += f"{k.strip()}={v.strip()}; "
        except Exception: 
            return

        self.config_record_len = int(params.get('RECORD_LENGTH', '8'))
        html = f"""
        <table style='width:100%; font-size:12px; color:#333333;'>
            <tr><td style='padding:3px;'><b>RL:</b> {params.get('RECORD_LENGTH', '-')}</td><td style='padding:3px;'><b>TLT:</b> {params.get('TRIGGER_LUT', '-')}</td></tr>
            <tr><td style='padding:3px;'><b>POL:</b> {params.get('POLARITY', '-')}</td><td style='padding:3px;'><b>CW:</b> {params.get('COINCIDENCE_WIDTH', '-')}</td></tr>
            <tr><td style='padding:3px;'><b>DLY:</b> {params.get('DELAY', '-')}</td><td style='padding:3px;'><b>OFF:</b> {params.get('OFFSET', '-')}</td></tr>
            <tr><td colspan='2' style='padding:3px; color:#D32F2F;'><b>THR:</b> {params.get('THRESHOLD', '-')}</td></tr>
        </table>
        """
        self.dash.lbl_cfg_summary.setText(html)

    def update_config_threshold(self, new_thr):
        try:
            with open(self.in_cfg.text(), "r") as f: 
                content = f.read()
            new_content = re.sub(r'(?i)(THRESHOLD\s*=\s*)\d+', rf'\g<1>{new_thr}', content)
            with open(self.in_cfg.text(), "w") as f: 
                f.write(new_content)
            self.parse_config_and_update_dashboard()
        except Exception as e:
            self.log_callback(f"<span style='color:#D32F2F;'><b>[GUI:ERROR] Auto config update failed: {e}</b></span>")

    def setup_run_limits(self):
        if self.rb_cont.isChecked(): 
            self.dash.lbl_limit.setText("Limit: Continuous")
            return 0, 0
        elif self.rb_evt.isChecked(): 
            self.dash.lbl_limit.setText(f"Limit: Max {self.sp_limit.value()} Evts")
            return self.sp_limit.value(), 0
        else: 
            self.dash.lbl_limit.setText(f"Limit: Max {self.sp_limit.value()} Sec")
            return 0, self.sp_limit.value()

    def get_auto_filename(self, suffix=""):
        base_dir = self.in_out_dir.text().strip()
        prefix = self.in_prefix.text().strip()
        run_no = self.sp_run_no.value()
        tag = self.cb_tag.currentText()
        filename = f"{prefix}_{run_no:03d}_{tag}{suffix}.dat"
        return os.path.join(base_dir, filename)

    def increment_run_number(self):
        self.sp_run_no.setValue(self.sp_run_no.value() + 1)
        self.log_callback(f"<span style='color:#1976D2; font-weight:bold;'>[SYSTEM] Ready for next run. Target Run Number: {self.sp_run_no.value():03d}</span>")

    def start_standard_daq(self, subrun_idx=1):
        self.auto_mode = "STANDARD"
        self.current_subrun = subrun_idx
        self.max_subruns = self.sp_sub_max.value()
        
        self.btn_start.setEnabled(False); self.btn_scan.setEnabled(False); self.btn_stop.setEnabled(True)
        self.dash.lbl_mode.setText(f"MODE: RUN [{self.current_subrun}/{self.max_subruns}]")
        
        # 아직 셋업 단계이므로 타이머를 켜지 않습니다.
        self.start_time = None
        self.dash.lbl_start.setText("Start: Waiting for HW...")
        self.last_events = 0
        
        cfg = self.in_cfg.text()
        suffix = f"_sub{self.current_subrun:02d}" if self.max_subruns > 1 else ""
        out = self.get_auto_filename(suffix)
        
        self.dash.lbl_file.setText(f"File: {os.path.basename(out)}")
        evts, time = self.setup_run_limits()
        self.parse_config_and_update_dashboard()
        self.daq.start_daq(cfg, out, evts, time)

    def start_scan_daq(self):
        self.auto_mode = "SCAN"
        self.btn_start.setEnabled(False); self.btn_scan.setEnabled(False); self.btn_stop.setEnabled(True)
        self.scan_queue = list(range(self.sp_start.value(), self.sp_end.value() + 1, self.sp_step.value()))
        self.log_callback("<span style='color:#FF7F0E;'><b>[AUTO SCAN] Sequence Initiated.</b></span>")
        self.run_scan_step()

    def run_scan_step(self):
        if self.auto_mode != "SCAN": return
        if not self.scan_queue:
            self.log_callback("<span style='color:#388E3C;'><b>[AUTO SCAN] All Steps Completed!</b></span>")
            self.auto_mode = "NONE"
            self.increment_run_number() 
            self.handle_daq_finished(0)
            return

        curr_thr = self.scan_queue.pop(0)
        self.dash.lbl_mode.setText(f"MODE: SCAN [THR {curr_thr}]")
        self.update_config_threshold(curr_thr)
        
        self.start_time = None
        self.dash.lbl_start.setText("Start: Waiting for HW...")
        self.last_events = 0
        
        cfg = self.in_cfg.text()
        out = self.get_auto_filename(f"_scan_thr{curr_thr}")
        
        self.dash.lbl_file.setText(f"File: {os.path.basename(out)}")
        self.dash.lbl_limit.setText("Limit: Scan Config")
        
        evts = self.sp_limit.value() if self.rb_evt.isChecked() else 0
        time = self.sp_limit.value() if self.rb_time.isChecked() else 10
        self.daq.start_daq(cfg, out, evts, time)

    def stop_daq(self):
        self.auto_mode = "NONE"
        self.scan_queue.clear()
        self.max_subruns = 1
        self.daq.stop_process()

    def handle_daq_finished(self, exit_code):
        if self.start_time is not None:
            end_t = datetime.now()
            rate = float(self.dash.lbl_rate.text().replace("Rate: ", "").replace(" Hz", "")) if "Rate: " in self.dash.lbl_rate.text() else 0.0
            size = self.last_events * self.config_record_len * 512 / 1048576.0
            
            clean_filename = self.dash.lbl_file.text().replace("File: ", "").strip()
            hv_info = self.get_hv_cb() if self.get_hv_cb else ""
            full_config_summary = f"{self.active_config_text} {hv_info}"
            
            self.db.log_daq_run(self.auto_mode, clean_filename, 
                                self.start_time.strftime("%Y-%m-%d %H:%M:%S"), end_t.strftime("%Y-%m-%d %H:%M:%S"), 
                                self.last_events, size, rate, full_config_summary)

        if self.auto_mode == "STANDARD" and self.current_subrun < self.max_subruns:
            idle_sec = self.sp_sub_idle.value()
            self.dash.lbl_mode.setText(f"MODE: IDLE ({idle_sec}s)")
            self.start_time = None
            self.log_callback(f"<span style='color:#7B1FA2;'><b>[AUTO] Waiting {idle_sec}s for next sub-run...</b></span>")
            QTimer.singleShot(idle_sec * 1000, lambda: self.start_standard_daq(self.current_subrun + 1))
        
        elif self.auto_mode == "STANDARD" and self.current_subrun == self.max_subruns:
            self.increment_run_number() 
            self.auto_mode = "NONE"; self.dash.lbl_mode.setText("MODE: IDLE"); self.start_time = None
            self.btn_start.setEnabled(True); self.btn_scan.setEnabled(True); self.btn_stop.setEnabled(False)

        elif self.auto_mode == "SCAN" and len(self.scan_queue) > 0:
            idle_sec = self.sp_scan_idle.value()
            self.dash.lbl_mode.setText(f"MODE: IDLE ({idle_sec}s)")
            self.start_time = None
            QTimer.singleShot(idle_sec * 1000, self.run_scan_step)
            
        else:
            self.auto_mode = "NONE"; self.dash.lbl_mode.setText("MODE: IDLE"); self.start_time = None
            self.btn_start.setEnabled(True); self.btn_scan.setEnabled(True); self.btn_stop.setEnabled(False)

    def update_external_stats(self, stats):
        if 'events' in stats: 
            self.last_events = stats['events']
        elapsed_sec = (datetime.now() - self.start_time).total_seconds() if self.start_time else 0
        self.dash.update_stats(stats, self.config_record_len, elapsed_sec)