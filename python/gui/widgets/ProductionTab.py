import os
from datetime import datetime
from PySide6.QtWidgets import (QWidget, QVBoxLayout, QHBoxLayout, QGroupBox, 
                               QLineEdit, QPushButton, QRadioButton, QProgressBar, 
                               QInputDialog, QFileDialog)
from PySide6.QtCore import Slot
from core.ProcessManager import ProcessManager

class ProductionTab(QWidget):
    def __init__(self, db_manager, console_log_callback, db_refresh_callback):
        super().__init__()
        self.db = db_manager
        self.log_callback = console_log_callback
        self.db_refresh_callback = db_refresh_callback

        self.prod_manager = ProcessManager()
        self.prod_manager.log_signal.connect(self.log_callback)
        self.prod_manager.stat_signal.connect(self.update_prod_stats)
        self.prod_manager.process_finished.connect(self.on_prod_finished)

        self.prod_start_time = None
        self.prod_summary_cache = {} # 💡 청크 분할 대비 지표 캐싱 보관함
        self.init_ui()

    def init_ui(self):
        prod_layout = QVBoxLayout(self)

        grp_file = QGroupBox("1. Target Data File (.dat)")
        h_file = QHBoxLayout()
        self.in_prod_file = QLineEdit()
        btn_prod_browse = QPushButton("Browse")
        btn_prod_browse.clicked.connect(self.browse_prod_file)
        h_file.addWidget(self.in_prod_file)
        h_file.addWidget(btn_prod_browse)
        grp_file.setLayout(h_file)
        prod_layout.addWidget(grp_file)

        grp_batch = QGroupBox("2. Batch Production (ROOT Tree)")
        v_batch = QVBoxLayout()
        self.chk_wave = QRadioButton("Save Full Waveform (-w) - Heavy & Slow")
        self.chk_fast = QRadioButton("Fast Physics Mode (Charge/Peak only)")
        self.chk_fast.setChecked(True)
        
        h_batch_btn = QHBoxLayout()
        self.btn_batch_run = QPushButton("[ RUN BATCH ]")
        self.btn_batch_run.setStyleSheet("background-color: #673AB7; color: white; font-weight: bold; padding: 10px;")
        self.btn_batch_run.clicked.connect(self.start_prod_batch)
        
        self.btn_batch_stop = QPushButton("[ STOP BATCH ]")
        self.btn_batch_stop.setStyleSheet("background-color: #D32F2F; color: white; font-weight: bold; padding: 10px;")
        self.btn_batch_stop.clicked.connect(self.stop_prod_batch)
        self.btn_batch_stop.setEnabled(False)

        h_batch_btn.addWidget(self.btn_batch_run)
        h_batch_btn.addWidget(self.btn_batch_stop)

        self.prod_progress = QProgressBar()
        v_batch.addWidget(self.chk_fast)
        v_batch.addWidget(self.chk_wave)
        v_batch.addLayout(h_batch_btn)
        v_batch.addWidget(self.prod_progress)
        grp_batch.setLayout(v_batch)
        prod_layout.addWidget(grp_batch)

        grp_inter = QGroupBox("3. Interactive Event Display (-d Mode)")
        v_inter = QVBoxLayout()
        self.btn_inter_run = QPushButton("[ START INTERACTIVE DISPLAY ]")
        self.btn_inter_run.setStyleSheet("background-color: #009688; color: white; font-weight: bold; padding: 10px;")
        self.btn_inter_run.clicked.connect(self.start_prod_inter)
        
        h_inter_ctrl = QHBoxLayout()
        self.btn_iprev = QPushButton("Prev"); self.btn_iprev.clicked.connect(lambda: self.prod_manager.write_stdin("p"))
        self.btn_inext = QPushButton("Next"); self.btn_inext.clicked.connect(lambda: self.prod_manager.write_stdin("n"))
        self.btn_ijump = QPushButton("Jump"); self.btn_ijump.clicked.connect(self.prompt_jump)
        self.btn_iquit = QPushButton("Quit"); self.btn_iquit.clicked.connect(self.quit_interactive)
        self.btn_iprev.setEnabled(False); self.btn_inext.setEnabled(False); self.btn_ijump.setEnabled(False); self.btn_iquit.setEnabled(False)
        
        h_inter_ctrl.addWidget(self.btn_iprev)
        h_inter_ctrl.addWidget(self.btn_inext)
        h_inter_ctrl.addWidget(self.btn_ijump)
        h_inter_ctrl.addWidget(self.btn_iquit)
        v_inter.addWidget(self.btn_inter_run)
        v_inter.addLayout(h_inter_ctrl)
        grp_inter.setLayout(v_inter)
        prod_layout.addWidget(grp_inter)
        prod_layout.addStretch()

    def browse_prod_file(self):
        f, _ = QFileDialog.getOpenFileName(self, "Select Raw Data to Process", "data", "Data Files (*.dat)")
        if f: 
            self.in_prod_file.setText(f)

    def start_prod_batch(self):
        f = self.in_prod_file.text()
        if not f: return
        self.btn_batch_run.setEnabled(False)
        self.btn_batch_stop.setEnabled(True)
        self.prod_progress.setValue(0)
        self.prod_summary_cache = {} # 런 시작 시 캐시 초기화
        
        self.prod_start_time = datetime.now()
        self.prod_manager.start_prod(f, self.chk_wave.isChecked(), False)

    def stop_prod_batch(self):
        self.prod_manager.stop_process()
        self.btn_batch_stop.setEnabled(False)

    def start_prod_inter(self):
        f = self.in_prod_file.text()
        if not f: return
        self.btn_inter_run.setEnabled(False)
        self.btn_iprev.setEnabled(True); self.btn_inext.setEnabled(True)
        self.btn_ijump.setEnabled(True); self.btn_iquit.setEnabled(True)
        self.prod_manager.start_prod(f, False, True)

    def prompt_jump(self):
        num, ok = QInputDialog.getInt(self, "Jump to Event", "Enter Event ID:")
        if ok: 
            self.prod_manager.write_stdin(f"j\n{num}")

    def quit_interactive(self):
        self.prod_manager.write_stdin("q")
        self.prod_manager.stop_process()

    @Slot(dict)
    def update_prod_stats(self, stats):
        if 'prod_events' in stats: 
            self.prod_progress.setFormat(f"Processing... {stats['prod_events']} events")
            
        # 💡 지표가 도착하면 즉시 DB에 넣지 않고 캐시에 안전하게 모아둠
        if 'prod_final_events' in stats:
            self.prod_summary_cache['events'] = stats['prod_final_events']
        if 'prod_speed' in stats:
            self.prod_summary_cache['speed'] = stats['prod_speed']

    @Slot(int)
    def on_prod_finished(self, exit_code):
        self.btn_batch_run.setEnabled(True)
        self.btn_batch_stop.setEnabled(False)
        self.btn_inter_run.setEnabled(True)
        self.btn_iprev.setEnabled(False); self.btn_inext.setEnabled(False)
        self.btn_ijump.setEnabled(False); self.btn_iquit.setEnabled(False)
        
        # 💡 [핵심 패치] 프로세스 종료 시 캐시된 데이터를 취합하여 한 번에 DB 전송
        if 'events' in self.prod_summary_cache and 'speed' in self.prod_summary_cache:
            self.prod_progress.setValue(100)
            self.prod_progress.setFormat("Complete!")
            mode = "Full Waveform (-w)" if self.chk_wave.isChecked() else "Fast Physics"
            clean_filename = os.path.basename(self.in_prod_file.text())
            
            self.db.log_prod_run(clean_filename, mode, self.prod_summary_cache['events'], self.prod_summary_cache['speed'])
            self.log_callback(f"<span style='color:#388E3C; font-weight:bold;'>[DB:PROD] File {clean_filename} logged successfully.</span>")
            
            # DB 새로고침 트리거 명시적 호출
            self.db_refresh_callback()

    def force_shutdown(self):
        self.prod_manager.write_stdin("q")
        self.prod_manager.stop_process()