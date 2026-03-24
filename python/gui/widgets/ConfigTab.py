from PySide6.QtWidgets import QWidget, QVBoxLayout, QHBoxLayout, QLabel, QLineEdit, QPushButton, QPlainTextEdit, QFileDialog
from PySide6.QtGui import QFont

class ConfigTab(QWidget):
    def __init__(self, log_callback, update_callback):
        super().__init__()
        self.log_callback = log_callback
        self.update_callback = update_callback
        self.init_ui()

    def init_ui(self):
        cfg_layout = QVBoxLayout(self)
        btn_h = QHBoxLayout()
        btn_h.addWidget(QLabel("Editing Config:"))
        self.in_cfg_edit = QLineEdit("config/kfadc500.config")
        
        btn_cfg_edit_browse = QPushButton("Browse")
        btn_cfg_edit_browse.clicked.connect(self.browse_config_edit)
        btn_load = QPushButton("Load")
        btn_load.clicked.connect(self.load_config)
        btn_save = QPushButton("[ SAVE CONFIG ]")
        btn_save.setStyleSheet("background-color: #2CA02C; color: white; font-weight:bold; padding: 5px 20px;")
        btn_save.clicked.connect(self.save_config)
        
        btn_h.addWidget(self.in_cfg_edit); btn_h.addWidget(btn_cfg_edit_browse)
        btn_h.addWidget(btn_load); btn_h.addStretch(); btn_h.addWidget(btn_save)
        cfg_layout.addLayout(btn_h)

        self.cfg_editor = QPlainTextEdit()
        self.cfg_editor.setFont(QFont("Consolas", 11))
        cfg_layout.addWidget(self.cfg_editor)
        self.load_config()

    def browse_config_edit(self):
        f, _ = QFileDialog.getOpenFileName(self, "Select Config to Edit", "config", "Config Files (*.config *.cfg);;All Files (*)")
        if f: 
            self.in_cfg_edit.setText(f)
            self.load_config()

    def load_config(self):
        try:
            with open(self.in_cfg_edit.text(), "r") as f:
                self.cfg_editor.setPlainText(f.read())
        except Exception as e:
            self.cfg_editor.setPlainText(f"# Error loading config: {e}")

    def save_config(self):
        try:
            with open(self.in_cfg_edit.text(), "w") as f:
                f.write(self.cfg_editor.toPlainText())
            self.log_callback("<span style='color:#388E3C;'><b>[GUI] Configuration saved successfully.</b></span>")
            self.update_callback()
        except Exception as e:
            self.log_callback(f"<span style='color:#D32F2F;'><b>[GUI:ERROR] Failed to save config: {e}</b></span>")