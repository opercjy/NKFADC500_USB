import traceback
from PySide6.QtWidgets import (QWidget, QVBoxLayout, QHBoxLayout, QGroupBox, 
                               QComboBox, QLineEdit, QPushButton, QLabel, 
                               QTableWidget, QTableWidgetItem, QHeaderView, 
                               QMessageBox, QStackedWidget)
from PySide6.QtCore import Qt, QTimer
from PySide6.QtGui import QColor

# CAEN 공식 라이브러리 임포트 시도 및 예외 처리
try:
    from caen_libs import caenhvwrapper as hv
    from caen_libs import caenhvwrapperflags as hvflags
    from caen_libs.error import Error as CaenError
    CAEN_LIBS_AVAILABLE = True
except ImportError:
    CAEN_LIBS_AVAILABLE = False

class HVControlTab(QWidget):
    def __init__(self, log_callback):
        super().__init__()
        self.log_callback = log_callback
        self.device = None
        self.is_connected = False
        self.num_channels = 4 
        self.target_slot = 0  

        self.init_ui()

        # 실시간 VMon, IMon 모니터링 타이머 (1초 주기)
        self.monitor_timer = QTimer(self)
        self.monitor_timer.timeout.connect(self.poll_hv_status)

    def init_ui(self):
        layout = QVBoxLayout(self)

        # =========================================================
        # 라이브러리 의존성 경고 패널
        # =========================================================
        if not CAEN_LIBS_AVAILABLE:
            lbl_warn = QLabel(
                "<b>[CRITICAL WARNING]</b> <code>caen-libs</code> Python binding is not installed.<br>"
                "To control CAEN modules, install it via: <code>pip install caen-libs</code><br>"
                "You also need the CAEN HV Wrapper Library (C/C++) from the official website."
            )
            lbl_warn.setStyleSheet("background-color: #FFEBEE; color: #D32F2F; border: 1px solid #D32F2F; padding: 10px;")
            layout.addWidget(lbl_warn)

        # =========================================================
        # 최상단: 운용 모드 선택 (가장 중요한 분기점)
        # =========================================================
        h_mode = QHBoxLayout()
        h_mode.addWidget(QLabel("<b>HV Operation Mode:</b>"))
        self.combo_op_mode = QComboBox()
        self.combo_op_mode.addItems([
            "Software Control Mode (CAEN Digital)", 
            "Manual Operation Mode (ORTEC Analog)"
        ])
        self.combo_op_mode.setStyleSheet("font-size: 14px; font-weight: bold; padding: 5px;")
        self.combo_op_mode.currentIndexChanged.connect(self.on_op_mode_changed)
        h_mode.addWidget(self.combo_op_mode)
        h_mode.addStretch()
        layout.addLayout(h_mode)

        # 💡 [핵심 패치] QStackedWidget을 이용한 다중 화면(윈도우) 스위칭
        self.stacked_widget = QStackedWidget()
        layout.addWidget(self.stacked_widget, stretch=1)

        # ---------------------------------------------------------
        # [화면 1] CAEN 디지털 제어 윈도우 (인덱스 0)
        # ---------------------------------------------------------
        self.page_digital = QWidget()
        v_dig = QVBoxLayout(self.page_digital)
        v_dig.setContentsMargins(0, 10, 0, 0)

        grp_conn = QGroupBox("CAEN High Voltage Connection")
        h_conn = QHBoxLayout()
        h_conn.addWidget(QLabel("Target Model:"))
        self.combo_model = QComboBox()
        self.combo_model.addItems(["CAEN NIM 8-Ch Smart HV (N1470)", "CAEN SY4527LC (Mainframe)", "CAEN SY5527 (Mainframe)"])
        h_conn.addWidget(self.combo_model)

        h_conn.addWidget(QLabel("Link / IP Address:"))
        self.in_ip = QLineEdit("192.168.0.10")
        h_conn.addWidget(self.in_ip)

        self.btn_connect = QPushButton("[ CONNECT HV ]")
        self.btn_connect.setStyleSheet("background-color: #1F77B4; color: white; font-weight: bold; padding: 5px 15px;")
        self.btn_connect.clicked.connect(self.toggle_connection)
        h_conn.addWidget(self.btn_connect)
        grp_conn.setLayout(h_conn)
        v_dig.addWidget(grp_conn)

        self.grp_ctrl = QGroupBox("Channel Control & Real-time Monitoring")
        v_ctrl = QVBoxLayout()
        self.tbl_ch = QTableWidget()
        self.tbl_ch.setColumnCount(8)
        self.tbl_ch.setHorizontalHeaderLabels(["Ch", "V0Set (V)", "VMon (V)", "I0Set (uA)", "IMon (uA)", "Status", "Power", "Action"])
        self.tbl_ch.horizontalHeader().setSectionResizeMode(QHeaderView.Stretch)
        self.build_channel_table(self.num_channels)
        v_ctrl.addWidget(self.tbl_ch)

        h_apply = QHBoxLayout()
        h_apply.addStretch()
        self.btn_apply = QPushButton("[ APPLY ALL VSet / ISet ]")
        self.btn_apply.setStyleSheet("background-color: #FF7F0E; color: white; font-weight: bold; padding: 10px;")
        self.btn_apply.setEnabled(False)
        self.btn_apply.clicked.connect(self.apply_settings)
        h_apply.addWidget(self.btn_apply)
        v_ctrl.addLayout(h_apply)
        self.grp_ctrl.setLayout(v_ctrl)
        v_dig.addWidget(self.grp_ctrl, stretch=1)
        
        self.stacked_widget.addWidget(self.page_digital)

        # ---------------------------------------------------------
        # [화면 2] ORTEC 아날로그 수동 대기 윈도우 (인덱스 1)
        # ---------------------------------------------------------
        self.page_analog = QWidget()
        v_ana = QVBoxLayout(self.page_analog)
        
        lbl_ana = QLabel(
            "<span style='font-size: 24px; color: #D32F2F;'><b>ANALOG MANUAL MODE ACTIVE</b></span><br><br>"
            "<span style='font-size: 16px; color: #555555;'>"
            "Background Polling and Software Controls are <b>SAFELY DISABLED</b>.<br><br>"
            "Please adjust the High Voltage directly using the dials on the <b>ORTEC 556</b> front panel.</span>"
        )
        lbl_ana.setAlignment(Qt.AlignCenter)
        lbl_ana.setStyleSheet("background-color: #F5F5F5; border: 3px dashed #BDBDBD; border-radius: 15px;")
        v_ana.addWidget(lbl_ana)
        
        self.stacked_widget.addWidget(self.page_analog)

    def on_op_mode_changed(self, index):
        """콤보박스 선택에 따라 화면을 완전히 교체하고 풀링을 제어합니다."""
        if index == 1: # Analog Mode
            # 아날로그 모드로 넘어가면 혹시 열려있던 디지털 통신을 강제로 끊고 타이머 정지
            if self.is_connected:
                self.disconnect_hv()
            self.stacked_widget.setCurrentIndex(1)
            self.log_callback("<span style='color:#7B1FA2;'><b>[HV:SYSTEM] Switched to Analog Mode. Background polling disabled.</b></span>")
        else: # Digital Mode
            self.stacked_widget.setCurrentIndex(0)
            self.log_callback("<span style='color:#1976D2;'><b>[HV:SYSTEM] Switched to CAEN Digital Control Mode.</b></span>")

    def build_channel_table(self, channels):
        self.tbl_ch.setRowCount(channels)
        for i in range(channels):
            self.tbl_ch.setItem(i, 0, QTableWidgetItem(f"CH {i}"))
            self.tbl_ch.setItem(i, 1, QTableWidgetItem("0.0"))
            
            vmon_item = QTableWidgetItem("0.0")
            vmon_item.setFlags(vmon_item.flags() & ~Qt.ItemIsEditable)
            self.tbl_ch.setItem(i, 2, vmon_item)
            
            self.tbl_ch.setItem(i, 3, QTableWidgetItem("100.0"))
            
            imon_item = QTableWidgetItem("0.0")
            imon_item.setFlags(imon_item.flags() & ~Qt.ItemIsEditable)
            self.tbl_ch.setItem(i, 4, imon_item)
            
            status_item = QTableWidgetItem("OFF")
            status_item.setFlags(status_item.flags() & ~Qt.ItemIsEditable)
            status_item.setForeground(QColor("#757575"))
            status_item.setTextAlignment(Qt.AlignCenter)
            self.tbl_ch.setItem(i, 5, status_item)

            pw_item = QTableWidgetItem("-")
            pw_item.setFlags(pw_item.flags() & ~Qt.ItemIsEditable)
            pw_item.setTextAlignment(Qt.AlignCenter)
            self.tbl_ch.setItem(i, 6, pw_item)

            btn_pw = QPushButton("TURN ON")
            btn_pw.setStyleSheet("background-color: #E0E0E0; color: #333;")
            btn_pw.setEnabled(False)
            btn_pw.clicked.connect(lambda checked, ch=i: self.toggle_channel_power(ch))
            self.tbl_ch.setCellWidget(i, 7, btn_pw)

    def toggle_connection(self):
        if not self.is_connected:
            self.connect_hv()
        else:
            self.disconnect_hv()

    def get_system_type(self):
        model = self.combo_model.currentText()
        if "N1470" in model: return hv.SystemType.N1470
        elif "SY4527" in model: return hv.SystemType.SY4527
        elif "SY5527" in model: return hv.SystemType.SY5527
        return hv.SystemType.N1470

    def connect_hv(self):
        if not CAEN_LIBS_AVAILABLE:
            QMessageBox.critical(self, "Dependency Error", "CAEN HV Wrapper Library and caen-libs are required!")
            self.log_callback("<span style='color:#D32F2F;'><b>[HV:ERROR] Connection aborted due to missing CAEN libraries.</b></span>")
            return

        sys_type = self.get_system_type()
        link_str = self.in_ip.text().strip()
        
        link_type = hv.LinkType.TCPIP
        if "USB" in link_str or "COM" in link_str or "tty" in link_str:
            link_type = hv.LinkType.USB_VCP

        self.log_callback(f"<span style='color:#1976D2;'><b>[HV:INFO] Connecting to {sys_type.name} via {link_type.name} ({link_str})...</b></span>")
        
        try:
            self.device = hv.Device.open(sys_type, link_type, link_str, "admin", "admin")
            
            crate_map = self.device.get_crate_map()
            for board in crate_map:
                if board is not None:
                    self.target_slot = board.slot
                    self.num_channels = board.n_channel
                    break
            
            self.build_channel_table(self.num_channels)

            self.is_connected = True
            self.btn_connect.setText("[ DISCONNECT ]")
            self.btn_connect.setStyleSheet("background-color: #D32F2F; color: white; font-weight: bold; padding: 5px 15px;")
            self.combo_model.setEnabled(False)
            self.in_ip.setEnabled(False)
            self.btn_apply.setEnabled(True)

            for i in range(self.num_channels):
                self.tbl_ch.cellWidget(i, 7).setEnabled(True)

            self.monitor_timer.start(1000)
            self.log_callback(f"<span style='color:#388E3C;'><b>[HV:SUCCESS] Connected to Slot {self.target_slot} ({self.num_channels} Ch). Polling started.</b></span>")

        except CaenError as e:
            self.log_callback(f"<span style='color:#D32F2F;'><b>[HV:ERROR] Connection Failed: {e.message} (Code: {e.code.name})</b></span>")
        except Exception as e:
            self.log_callback(f"<span style='color:#D32F2F;'><b>[HV:ERROR] Unexpected Exception: {str(e)}</b></span>")
            traceback.print_exc()

    def disconnect_hv(self):
        self.monitor_timer.stop()
        if self.device is not None:
            try:
                self.device.close()
            except Exception as e:
                self.log_callback(f"<span style='color:#D32F2F;'><b>[HV:ERROR] Error during disconnect: {str(e)}</b></span>")
            self.device = None

        self.is_connected = False
        self.btn_connect.setText("[ CONNECT HV ]")
        self.btn_connect.setStyleSheet("background-color: #1F77B4; color: white; font-weight: bold; padding: 5px 15px;")
        
        self.combo_model.setEnabled(True)
        self.in_ip.setEnabled(True)
        self.btn_apply.setEnabled(False)

        for i in range(self.num_channels):
            self.tbl_ch.cellWidget(i, 7).setEnabled(False)
            self.tbl_ch.item(i, 2).setText("0.0")
            self.tbl_ch.item(i, 4).setText("0.0")
            self.tbl_ch.item(i, 5).setText("OFF")
            self.tbl_ch.item(i, 6).setText("-")

        self.log_callback("<span style='color:#FF7F0E;'><b>[HV:INFO] Disconnected from HV Mainframe.</b></span>")

    def apply_settings(self):
        if not self.is_connected or self.device is None: return

        try:
            ch_list = list(range(self.num_channels))
            vset_list = [float(self.tbl_ch.item(i, 1).text()) for i in range(self.num_channels)]
            iset_list = [float(self.tbl_ch.item(i, 3).text()) for i in range(self.num_channels)]
            
            for ch in ch_list:
                self.device.set_ch_param(self.target_slot, [ch], "V0Set", vset_list[ch])
                self.device.set_ch_param(self.target_slot, [ch], "I0Set", iset_list[ch])
            
            self.log_callback("<span style='color:#0288D1;'><b>[HV:INFO] V0Set / I0Set parameters successfully pushed to hardware.</b></span>")
        except ValueError:
            self.log_callback("<span style='color:#D32F2F;'><b>[HV:ERROR] Invalid input in VSet or ISet fields. Numeric values required.</b></span>")
        except CaenError as e:
            self.log_callback(f"<span style='color:#D32F2F;'><b>[HV:ERROR] Failed to apply settings: {e.message}</b></span>")

    def toggle_channel_power(self, ch):
        if not self.is_connected or self.device is None: return
        
        current_pw = self.tbl_ch.item(ch, 6).text()
        target_pw = 1 if current_pw == "0" or current_pw == "-" else 0
        
        try:
            self.device.set_ch_param(self.target_slot, [ch], "Pw", target_pw)
            self.log_callback(f"<span style='color:#0288D1;'><b>[HV:INFO] CH{ch} Power commanded to {'ON' if target_pw else 'OFF'}.</b></span>")
        except CaenError as e:
            self.log_callback(f"<span style='color:#D32F2F;'><b>[HV:ERROR] Failed to toggle power: {e.message}</b></span>")

    def poll_hv_status(self):
        if not self.is_connected or self.device is None: return

        try:
            sys_type = self.get_system_type()
            ch_list = list(range(self.num_channels))
            
            vmon_vals = self.device.get_ch_param(self.target_slot, ch_list, "VMon")
            imon_vals = self.device.get_ch_param(self.target_slot, ch_list, "IMon")
            status_vals = self.device.get_ch_param(self.target_slot, ch_list, "Status")
            pw_vals = self.device.get_ch_param(self.target_slot, ch_list, "Pw")

            for i in range(self.num_channels):
                self.tbl_ch.item(i, 2).setText(f"{vmon_vals[i]:.2f}")
                self.tbl_ch.item(i, 4).setText(f"{imon_vals[i]:.2f}")
                self.tbl_ch.item(i, 6).setText(str(pw_vals[i]))
                
                status_str = hvflags.decode_ch_status(sys_type, status_vals[i])
                if not status_str: status_str = "OFF" if pw_vals[i] == 0 else "ON"

                item_status = self.tbl_ch.item(i, 5)
                item_status.setText(status_str)
                
                if "UP" in status_str: item_status.setForeground(QColor("#FF9800"))
                elif "DOWN" in status_str: item_status.setForeground(QColor("#03A9F4"))
                elif "ON" in status_str: item_status.setForeground(QColor("#388E3C"))
                elif status_str == "OFF": item_status.setForeground(QColor("#757575"))
                else: item_status.setForeground(QColor("#D32F2F")) 

                btn = self.tbl_ch.cellWidget(i, 7)
                if pw_vals[i] == 1:
                    btn.setText("TURN OFF")
                    btn.setStyleSheet("background-color: #F44336; color: white; font-weight: bold;")
                else:
                    btn.setText("TURN ON")
                    btn.setStyleSheet("background-color: #4CAF50; color: white; font-weight: bold;")

        except CaenError:
            pass
        except Exception:
            pass