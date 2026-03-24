from PySide6.QtWidgets import QWidget, QVBoxLayout, QPushButton, QTabWidget, QTableWidget, QTableWidgetItem, QHeaderView
from PySide6.QtCore import Qt

class DatabaseTab(QWidget):
    def __init__(self, db_manager):
        super().__init__()
        self.db = db_manager
        self.init_ui()

    def init_ui(self):
        db_layout = QVBoxLayout(self)
        btn_refresh = QPushButton("[ Refresh Database ]")
        btn_refresh.clicked.connect(self.refresh_db)
        db_layout.addWidget(btn_refresh)
        
        self.db_tabs = QTabWidget()
        self.tbl_daq = QTableWidget()
        self.tbl_daq.setColumnCount(9)
        self.tbl_daq.setHorizontalHeaderLabels(["ID", "Mode", "File", "Start", "End", "Events", "Size(MB)", "Rate(Hz)", "Config"])
        self.tbl_daq.horizontalHeader().setSectionResizeMode(QHeaderView.Stretch)
        self.db_tabs.addTab(self.tbl_daq, "DAQ History")
        
        self.tbl_prod = QTableWidget()
        self.tbl_prod.setColumnCount(6)
        self.tbl_prod.setHorizontalHeaderLabels(["ID", "File", "Process Time", "Mode", "Events", "Speed(MB/s)"])
        self.tbl_prod.horizontalHeader().setSectionResizeMode(QHeaderView.Stretch)
        self.db_tabs.addTab(self.tbl_prod, "Production History")
        
        db_layout.addWidget(self.db_tabs)
        self.refresh_db()

    def refresh_db(self):
        self.tbl_daq.setRowCount(0); self.tbl_prod.setRowCount(0)
        daq_rows = self.db.get_daq_history()
        for r, row in enumerate(daq_rows):
            self.tbl_daq.insertRow(r)
            for c, val in enumerate(row):
                it = QTableWidgetItem(str(val)); it.setFlags(Qt.ItemIsEnabled | Qt.ItemIsSelectable)
                self.tbl_daq.setItem(r, c, it)
                
        prod_rows = self.db.get_prod_history()
        for r, row in enumerate(prod_rows):
            self.tbl_prod.insertRow(r)
            for c, val in enumerate(row):
                it = QTableWidgetItem(str(val)); it.setFlags(Qt.ItemIsEnabled | Qt.ItemIsSelectable)
                self.tbl_prod.setItem(r, c, it)