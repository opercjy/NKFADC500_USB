from PySide6.QtWidgets import (QWidget, QVBoxLayout, QHBoxLayout, QGroupBox, 
                               QCheckBox, QLabel, QLineEdit, QPushButton, QGridLayout)
from PySide6.QtCore import Qt
from PySide6.QtGui import QFont

class TriggerLUTTab(QWidget):
    def __init__(self):
        super().__init__()
        self.init_ui()

    def init_ui(self):
        layout = QHBoxLayout(self)

        # =========================================================
        # 좌측 패널: 개념 설명 및 빠른 프리셋
        # =========================================================
        left_layout = QVBoxLayout()
        grp_desc = QGroupBox("Trigger Look-Up Table (LUT) Concept")
        lbl_desc = QLabel(
            "KFADC500 utilizes a 16-bit Trigger LUT to determine the global trigger.\n\n"
            "The 4 independent channels generate a 4-bit local trigger pattern (CH4, CH3, CH2, CH1) based on their threshold crossings. "
            "This 4-bit pattern (value 0 to 15) acts as a direct index to the 16-bit LUT register.\n\n"
            "[ Example Scenario ]\n"
            "1. If CH1 and CH2 fire simultaneously, the pattern is 0011 (Binary) = 3 (Decimal).\n"
            "2. The DAQ checks the 3rd bit of the LUT Register.\n"
            "3. If the 3rd bit is '1', a global trigger is issued. If '0', it is ignored.\n\n"
            "Check the boxes on the right to design your physics trigger logic."
        )
        lbl_desc.setStyleSheet("font-size: 13px; line-height: 1.5; color: #333333;")
        lbl_desc.setWordWrap(True)
        l_desc = QVBoxLayout()
        l_desc.addWidget(lbl_desc)
        grp_desc.setLayout(l_desc)
        left_layout.addWidget(grp_desc)

        grp_preset = QGroupBox("Quick Logic Presets")
        l_preset = QVBoxLayout()
        
        btn_or = QPushButton("OR Trigger (Any Channel Fires) -> 0xFFFE")
        btn_or.clicked.connect(lambda: self.in_hex.setText("0xFFFE"))
        
        btn_and = QPushButton("AND Trigger (All 4 Channels Coincidence) -> 0x8000")
        btn_and.clicked.connect(lambda: self.in_hex.setText("0x8000"))
        
        btn_ch1 = QPushButton("CH1 Only (Exact Calibration) -> 0x0002")
        btn_ch1.clicked.connect(lambda: self.in_hex.setText("0x0002"))
        
        btn_clear = QPushButton("Clear All (Mute Trigger) -> 0x0000")
        btn_clear.clicked.connect(lambda: self.in_hex.setText("0x0000"))
        
        l_preset.addWidget(btn_or); l_preset.addWidget(btn_and)
        l_preset.addWidget(btn_ch1); l_preset.addWidget(btn_clear)
        grp_preset.setLayout(l_preset)
        left_layout.addWidget(grp_preset)
        left_layout.addStretch()

        # =========================================================
        # 우측 패널: 15-Case 인터랙티브 그리드 시뮬레이터
        # =========================================================
        right_layout = QVBoxLayout()
        grp_grid = QGroupBox("15-Case Coincidence Logic Simulator")
        grid = QGridLayout()

        self.boxes = []
        headers = ["Decimal", "Pattern (4-3-2-1)", "Channel Combination", "Enable Trigger (LUT Bit)"]
        for col, text in enumerate(headers):
            lbl = QLabel(f"<b>{text}</b>")
            lbl.setAlignment(Qt.AlignCenter)
            lbl.setStyleSheet("color: #1F77B4;")
            grid.addWidget(lbl, 0, col)

        for i in range(16):
            bin_str = f"{i:04b}"
            ch_combo = []
            if i & 1: ch_combo.append("CH1")
            if i & 2: ch_combo.append("CH2")
            if i & 4: ch_combo.append("CH3")
            if i & 8: ch_combo.append("CH4")

            combo_str = " + ".join(ch_combo) if ch_combo else "NO HIT (Usually Disabled)"

            lbl_dec = QLabel(str(i)); lbl_dec.setAlignment(Qt.AlignCenter)
            lbl_bin = QLabel(bin_str); lbl_bin.setAlignment(Qt.AlignCenter)
            lbl_combo = QLabel(combo_str); lbl_combo.setAlignment(Qt.AlignCenter)

            if i == 0:
                lbl_combo.setStyleSheet("color: #9E9E9E;")

            chk = QCheckBox(f"Enable Bit {i}")
            chk.stateChanged.connect(self.update_hex_from_boxes)
            self.boxes.append(chk)

            grid.addWidget(lbl_dec, i+1, 0)
            grid.addWidget(lbl_bin, i+1, 1)
            grid.addWidget(lbl_combo, i+1, 2)
            grid.addWidget(chk, i+1, 3)

        grp_grid.setLayout(grid)
        right_layout.addWidget(grp_grid, stretch=8)

        # 16진수 계산기 섹션
        grp_hex = QGroupBox("Calculated LUT Hex Value")
        h_hex = QHBoxLayout()
        h_hex.addWidget(QLabel("<b>TRIGGER_LUT = </b>"))
        self.in_hex = QLineEdit("0xFFFE")
        self.in_hex.setFont(QFont("Consolas", 18, QFont.Bold))
        self.in_hex.setStyleSheet("color: #D62728; background-color: #F0F0F0; padding: 5px;")
        self.in_hex.textChanged.connect(self.update_boxes_from_hex)
        
        btn_copy = QPushButton("Copy to Clipboard")
        btn_copy.clicked.connect(self.copy_to_clipboard)
        
        h_hex.addWidget(self.in_hex)
        h_hex.addWidget(btn_copy)
        grp_hex.setLayout(h_hex)
        right_layout.addWidget(grp_hex, stretch=1)

        layout.addLayout(left_layout, stretch=3)
        layout.addLayout(right_layout, stretch=5)

        # 초기 동기화
        self.update_boxes_from_hex(self.in_hex.text())

    def update_hex_from_boxes(self):
        """체크박스가 클릭되면 16진수 텍스트를 역계산합니다."""
        if self.in_hex.hasFocus(): return # 텍스트 입력 중 루프 방지
        val = 0
        for i in range(16):
            if self.boxes[i].isChecked():
                val |= (1 << i)
        
        self.in_hex.blockSignals(True)
        self.in_hex.setText(f"0x{val:04X}")
        self.in_hex.blockSignals(False)

    def update_boxes_from_hex(self, text):
        """16진수 텍스트가 입력되면 체크박스를 자동으로 켭니다."""
        try:
            val = int(text, 16)
            for i in range(16):
                self.boxes[i].blockSignals(True)
                self.boxes[i].setChecked(bool(val & (1 << i)))
                self.boxes[i].blockSignals(False)
        except ValueError:
            pass

    def copy_to_clipboard(self):
        import PySide6.QtGui as QtGui
        import PySide6.QtWidgets as QtWidgets
        clipboard = QtWidgets.QApplication.clipboard()
        clipboard.setText(self.in_hex.text())