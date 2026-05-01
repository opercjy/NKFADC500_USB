import os
import re
import signal
from PySide6.QtCore import QObject, QProcess, Signal

class ProcessManager(QObject):
    log_signal = Signal(str)
    process_finished = Signal(int)
    stat_signal = Signal(dict) 
    run_started = Signal() # 💡 실제 측정 시작을 알리는 시그널

    def __init__(self, parent=None):
        super().__init__(parent)
        self.process = QProcess(self)
        self.process.readyReadStandardOutput.connect(self.handle_stdout)
        self.process.readyReadStandardError.connect(self.handle_stderr)
        self.process.finished.connect(self.handle_finished)
        self.ansi_escape = re.compile(r'\x1B(?:[@-Z\\-_]|\[[0-?]*[ -/]*[@-~])')

    def start_daq(self, config_file, output_file, events=0, time=0):
        if self.process.state() == QProcess.Running: return
        args = ["-f", config_file, "-o", output_file]
        if events > 0: args.extend(["-n", str(events)])
        if time > 0: args.extend(["-t", str(time)])
        self.log_signal.emit(f"<span style='color:#1976D2;'><b>[SYSTEM] DAQ Executing...</b></span>")
        self.process.start("./bin/kfadc500_daq", args)

    def start_prod(self, input_file, save_wave=False, interactive=False):
        if self.process.state() == QProcess.Running: return
        args = [input_file]
        if save_wave: args.append("-w")
        if interactive: args.append("-d")
        self.log_signal.emit(f"<span style='color:#7B1FA2;'><b>[PROD] Executing ROOT Production...</b></span>")
        self.process.start("./bin/kfadc500_prod", args)

    def stop_process(self):
        if self.process.state() == QProcess.Running:
            self.log_signal.emit("<span style='color:#FF7F0E;'><b>[SYSTEM] Terminating process...</b></span>")
            self.process.terminate() 
            if not self.process.waitForFinished(2000):
                self.process.kill()

    def write_stdin(self, text):
        if self.process.state() == QProcess.Running:
            self.process.write((text + "\n").encode('utf-8'))

    def handle_stdout(self):
        raw_data = bytes(self.process.readAllStandardOutput()).decode('utf-8', errors='ignore')
        for line in raw_data.splitlines():
            clean_line = self.ansi_escape.sub('', line).strip()
            if not clean_line: continue
            
            # 💡 [핵심 패치] 하드웨어가 켜지면 GUI 타이머를 리셋하라는 시그널을 보냄
            if "Trigger FSM Armed" in clean_line:
                self.run_started.emit()

            if "events saved" in clean_line:
                m_pe = re.search(r'Processing\.\.\.\s*(\d+)', clean_line)
                if m_pe: self.stat_signal.emit({'prod_events': int(m_pe.group(1))})
                
            if "Processed Events" in clean_line:
                m_te = re.search(r'Processed Events\s*:\s*(\d+)', clean_line)
                if m_te: self.stat_signal.emit({'prod_final_events': int(m_te.group(1))})
            elif "Conversion Speed" in clean_line:
                m_ps = re.search(r'Speed\s*:\s*([0-9.]+)\s*MB/s', clean_line)
                if m_ps: self.stat_signal.emit({'prod_speed': float(m_ps.group(1))})

            html_line = clean_line.replace('[SYSTEM:INFO]', '<span style="color:#388E3C; font-weight:bold;">[SYSTEM:INFO]</span>') \
                                  .replace('[DAQ:INFO]', '<span style="color:#1F77B4; font-weight:bold;">[DAQ:INFO]</span>') \
                                  .replace('[SYSTEM:WARN]', '<span style="color:#F57C00; font-weight:bold;">[SYSTEM:WARN]</span>') \
                                  .replace('Info:', '<span style="color:#388E3C;">Info:</span>')
            
            self.log_signal.emit(html_line)

    def handle_stderr(self):
        data = self.process.readAllStandardError().data().decode('utf-8', errors='ignore')
        for line in data.splitlines():
            if line.strip(): self.log_signal.emit(f"<span style='color:#D32F2F; font-weight:bold;'>{line.strip()}</span>")

    def handle_finished(self, exit_code):
        self.log_signal.emit(f"<b>[SYSTEM] Process Finished with exit code {exit_code}.</b>")
        self.process_finished.emit(exit_code)