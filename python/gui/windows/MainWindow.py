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
from widgets.TriggerLUTTab import TriggerLUTTab
from widgets.HVControlTab import HVControlTab

class MainWindow(QMainWindow):
    def __init__(self):
        super().__init__()
        self.setWindowTitle("KFADC500 Professional Control Tower")
        self.resize(1500, 950)

        self.setStyleSheet("""
            QMainWindow { background-color: #FFFFFF; }
            QTabWidget::pane { border: 1px solid #CCCCCC; background: #FFFFFF; }
            QTabBar::tab { background: #F0F0F0; color: #333333; padding: 10px 20px; font-weight: bold; border: 1px solid #CCCCCC; }
            QTabBar::tab:selected { background: #FFFFFF; border-bottom: 3px solid #D62728; }
            QGroupBox { font-weight: bold; border: 1px solid #BDBDBD; border-radius: 5px; margin-top: 15px; background-color: #FFFFFF;}
            QGroupBox::title { subcontrol-origin: margin; left: 10px; padding: 0 3px; color: #1F77B4; }
            QTextEdit { background-color: #FFFFFF; color: #333333; border: 1px solid #BDBDBD; }
            QLineEdit, QSpinBox { background-color: #FFFFFF; color: #333333; padding: 5px; border: 1px solid #BDBDBD; }
            QComboBox { background-color: #FFFFFF; padding: 3px; border: 1px solid #BDBDBD; }
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
        self.tab_hv = HVControlTab(self.append_log)
        self.tab_run = RunControlTab(self.daq_manager, self.db_manager, self.dashboard, self.append_log, self.tab_hv.get_hv_summary)
        self.tab_monitor = LiveMonitorTab()
        self.tab_prod = ProductionTab(self.db_manager, self.append_log, self.tab_db.refresh_db)
        self.tab_cfg = ConfigTab(self.append_log, self.tab_run.parse_config_and_update_dashboard)
        self.tab_lut = TriggerLUTTab()

        self.tabs.addTab(self.tab_run, "Run Control")
        self.tabs.addTab(self.tab_monitor, "Live Monitor")
        self.tabs.addTab(self.tab_lut, "Trigger LUT Simulator")
        self.tabs.addTab(self.tab_hv, "High Voltage (HV) Control")
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

        # Production 오프라인 툴의 상태만 RunControl로 전달 (온라인 통계는 분리됨)
        self.daq_manager.process_finished.connect(self.tab_run.handle_daq_finished)
        self.daq_manager.process_finished.connect(lambda: self.tab_db.refresh_db())
        self.tab_prod.prod_manager.process_finished.connect(lambda: self.tab_db.refresh_db())

        self.receiver.data_ready.connect(self.dispatch_plot_data)
        self.tab_monitor.monitoring_toggled.connect(self.receiver.set_monitoring_state)
        self.tab_monitor.clear_requested.connect(self.receiver.request_clear)
        
        self.receiver.start() 

        self.clock_timer = QTimer(self)
        self.clock_timer.timeout.connect(self.global_clock_tick)
        self.clock_timer.start(1000)

    # 💡 [핵심 패치] ZmqWorker로부터 텔레메트리 데이터를 수신합니다.
    @Slot(object, object, int, dict)
    def dispatch_plot_data(self, waveforms, charges, samples_per_ch, telemetry):
        # 1. 텔레메트리 숫자(대시보드)는 모니터 화면이 꺼져있어도 100% 무조건 업데이트됩니다!
        if telemetry:
            self.tab_run.update_external_stats(telemetry)
            
        if isinstance(waveforms, str) and waveforms == "TELEMETRY_ONLY":
            return

        # 2. 무거운 배열 그리기 연산은 모니터 탭이 켜져 있을 때만 실행됩니다.
        is_visible = (self.tabs.currentIndex() == 1)
        self.tab_monitor.update_plots(waveforms, charges, samples_per_ch, is_visible)

    def global_clock_tick(self):
        now = datetime.now()
        self.dashboard.lbl_current_time.setText(now.strftime("%Y-%m-%d  %H:%M:%S"))

        if self.tab_run.start_time is not None:
            elapsed = now - self.tab_run.start_time
            h, rem = divmod(elapsed.seconds, 3600); m, s = divmod(rem, 60)
            self.dashboard.lbl_elapsed.setText(f"Elapsed: {h:02d}:{m:02d}:{s:02d}")
        
        total, used, free = shutil.disk_usage(os.getcwd())
        self.dashboard.lbl_disk.setText(f"Disk Free: {free // (2**30)} GB")

    # 💡 [핵심 패치] 덮어쓰기 로직을 지우고, 순수하게 콘솔 로깅 역할만 수행합니다.
    @Slot(str)
    def append_log(self, text):
        self.console.append(text)
        self.console.moveCursor(QTextCursor.End)

    def closeEvent(self, event):
        self.tab_run.stop_daq()
        self.tab_prod.force_shutdown()
        self.receiver.stop()
        event.accept()