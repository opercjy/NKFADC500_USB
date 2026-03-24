import sys
import os
import signal

current_dir = os.path.dirname(os.path.abspath(__file__))
if current_dir not in sys.path:
    sys.path.insert(0, current_dir)

import pyqtgraph as pg
from PySide6.QtWidgets import QApplication

# 💡 [경로 수정] 이제 MainWindow는 classes가 아니라 windows 폴더에서 가져옵니다!
from windows.MainWindow import MainWindow 

if __name__ == "__main__":
    print("\n\033[1;36m============================================================\033[0m")
    print("\033[1;36m [ KFADC500 Unified ZMQ Monitoring Dashboard ] \033[0m")
    print("\033[1;36m============================================================\033[0m\n")

    signal.signal(signal.SIGINT, signal.SIG_DFL)
    app = QApplication(sys.argv)
    pg.setConfigOptions(antialias=True)
    window = MainWindow()
    window.show()
    sys.exit(app.exec())