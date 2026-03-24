import sys
import shutil
import os
from datetime import datetime
from PySide6.QtWidgets import QMainWindow, QWidget, QVBoxLayout, QHBoxLayout, QTabWidget, QTextEdit, QLabel, QSplitter
from PySide6.QtCore import Slot, Qt, QTimer
from PySide6.QtGui import QFont, QTextCursor

from core.ProcessManager import ProcessManager
from core.DatabaseManager import DatabaseManager
from classes.ZmqWorker import ZmqWorker

from widgets.LiveDashboard import LiveDashboard
from widgets.LiveMonitorTab import LiveMonitorTab
from widgets.DatabaseTab import DatabaseTab
from widgets.ConfigTab import ConfigTab
from widgets.ProductionTab import ProductionTab
from widgets.RunControlTab import RunControlTab

class MainWindow(QMainWindow):
    def __init__(self):
        super().__init__()
        self.setWindowTitle("KFADC500 Professional Control Tower")
        self.resize(1500, 950)

        self.setStyleSheet("""
            QMainWindow { background-color: #FDF6E3; }
            QTabWidget::pane { border: 1px solid #CCCCCC; background: #FDF6E3; }
            QTabBar::tab { background: #EAE3CB; color: #333333; padding: 10px 20px; font-weight: bold; border: 1px solid #CCCCCC; }
            QTabBar::tab:selected { background: #FFFFFF; border-bottom: 3px solid #D62728; }
            QGroupBox { font-weight: bold; border: 1px solid #BDBDBD; border-radius: 5px; margin-top: 15px; background-color: #FFFFFF;}
            QGroupBox::title { subcontrol-origin: margin; left: 10px; padding: 0 3px; color: #1F77B4; }
            QTextEdit { background-color: #FFFFFF; color: #333333; border: 1px solid #BDBDBD; }
            QLineEdit, QSpinBox { background-color: #FFFFFF; color: #333333; padding: 5px; border: 1px solid #BDBDBD; }
        """)

        self.db_manager = DatabaseManager()
        self.daq_manager = ProcessManager()
        self.daq_manager.log_signal.connect(self.append_log)
        
        self.receiver = ZmqWorker()
        
        main_widget = QWidget()
        self.setCentralWidget(main_widget)
        main_splitter = QSplitter(Qt.Vertical)
        layout = QVBoxLayout(main_widget)
        layout.addWidget(main_splitter)

        top_widget = QWidget()
        top_layout = QHBoxLayout(top_widget)
        top_layout.setContentsMargins(0, 0, 0, 0)
        self.tabs = QTabWidget()
        top_layout.addWidget(self.tabs, stretch=7)

        self.dashboard = LiveDashboard()
        top_layout.addWidget(self.dashboard, stretch=3)

        self.tab_db = DatabaseTab(self.db_manager)
        self.tab_run = RunControlTab(self.daq_manager, self.db_manager, self.dashboard, self.append_log)
        self.tab_monitor = LiveMonitorTab()
        self.tab_prod = ProductionTab(self.db_manager, self.append_log, self.tab_db.refresh_db)
        self.tab_cfg = ConfigTab(self.append_log, self.tab_run.parse_config_and_update_dashboard)

        self.tabs.addTab(self.tab_run, "Run Control")
        self.tabs.addTab(self.tab_monitor, "Live Monitor")
        self.tabs.addTab(self.tab_prod, "Offline Production")
        self.tabs.addTab(self.tab_db, "Database")
        self.tabs.addTab(self.tab_cfg, "Configuration")

        main_splitter.addWidget(top_widget)

        console_widget = QWidget()
        console_layout = QVBoxLayout(console_widget)
        console_layout.setContentsMargins(0, 5, 0, 0)
        console_layout.addWidget(QLabel("<b>System Logging Console</b>"))
        self.console = QTextEdit()
        self.console.setReadOnly(True)
        self.console.setFont(QFont("Consolas", 10))
        console_layout.addWidget(self.console)
        main_splitter.addWidget(console_widget)
        main_splitter.setSizes([700, 250])

        self.daq_manager.stat_signal.connect(self.tab_run.update_external_stats)
        self.daq_manager.process_finished.connect(self.tab_run.handle_daq_finished)
        self.daq_manager.process_finished.connect(lambda: self.tab_db.refresh_db())
        self.tab_prod.prod_manager.process_finished.connect(lambda: self.tab_db.refresh_db())

        self.receiver.data_ready.connect(self.dispatch_plot_data)
        self.receiver.start() 

        # 1초 주기 시스템 관리 타이머
        self.clock_timer = QTimer(self)
        self.clock_timer.timeout.connect(self.global_clock_tick)
        self.clock_timer.start(1000)

    @Slot(object, object)
    def dispatch_plot_data(self, waveform, charge_list):
        is_visible = (self.tabs.currentIndex() == 1)
        self.tab_monitor.update_plots(waveform, charge_list, is_visible)

    def global_clock_tick(self):
        # 1. 실행 시간 업데이트
        if self.tab_run.start_time is not None:
            elapsed = datetime.now() - self.tab_run.start_time
            h, rem = divmod(elapsed.seconds, 3600); m, s = divmod(rem, 60)
            self.dashboard.lbl_elapsed.setText(f"Elapsed: {h:02d}:{m:02d}:{s:02d}")
        
        # 2. 디스크 용량 업데이트
        total, used, free = shutil.disk_usage(os.getcwd())
        self.dashboard.lbl_disk.setText(f"Disk Free: {free // (2**30)} GB")

        # 3. 💡 [버그 픽스] 충돌나는 시그널 대신, 타이머에서 안전하게 ZMQ 워커의 배열 크기 동기화
        if hasattr(self.tab_run, 'config_record_len'):
            if self.receiver.record_length != self.tab_run.config_record_len:
                self.receiver.update_record_length(self.tab_run.config_record_len)

    @Slot(str)
    def append_log(self, text):
        self.console.append(text)
        self.console.moveCursor(QTextCursor.End)

    def closeEvent(self, event):
        self.tab_run.stop_daq()
        self.tab_prod.force_shutdown()
        self.receiver.stop()
        event.accept()