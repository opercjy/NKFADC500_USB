import sqlite3
import os
from datetime import datetime

class DatabaseManager:
    def __init__(self, db_path="data/kfadc500_history.db"):
        os.makedirs("data", exist_ok=True)
        self.db_path = db_path
        self.init_db()

    def init_db(self):
        conn = sqlite3.connect(self.db_path)
        c = conn.cursor()
        c.execute('''CREATE TABLE IF NOT EXISTS daq_runs 
                     (id INTEGER PRIMARY KEY AUTOINCREMENT, run_mode TEXT, file_name TEXT, 
                      start_time TEXT, end_time TEXT, total_events INTEGER, 
                      size_mb REAL, avg_rate_hz REAL, config_summary TEXT)''')
        c.execute('''CREATE TABLE IF NOT EXISTS prod_runs 
                     (id INTEGER PRIMARY KEY AUTOINCREMENT, file_name TEXT, process_time TEXT, 
                      prod_mode TEXT, total_events INTEGER, speed_mbps REAL)''')
        conn.commit()
        conn.close()

    def log_daq_run(self, run_mode, file_name, start, end, events, size, rate, config):
        conn = sqlite3.connect(self.db_path)
        c = conn.cursor()
        c.execute('''INSERT INTO daq_runs (run_mode, file_name, start_time, end_time, total_events, size_mb, avg_rate_hz, config_summary) 
                     VALUES (?, ?, ?, ?, ?, ?, ?, ?)''', 
                  (run_mode, file_name, start, end, events, size, rate, config))
        conn.commit()
        conn.close()

    def log_prod_run(self, file_name, mode, events, speed):
        now = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
        conn = sqlite3.connect(self.db_path)
        c = conn.cursor()
        c.execute('''INSERT INTO prod_runs (file_name, process_time, prod_mode, total_events, speed_mbps) 
                     VALUES (?, ?, ?, ?, ?)''', 
                  (file_name, now, mode, events, speed))
        conn.commit()
        conn.close()
        
    def get_daq_history(self):
        conn = sqlite3.connect(self.db_path)
        c = conn.cursor()
        c.execute("SELECT * FROM daq_runs ORDER BY id DESC")
        rows = c.fetchall()
        conn.close()
        return rows
        
    def get_prod_history(self):
        conn = sqlite3.connect(self.db_path)
        c = conn.cursor()
        c.execute("SELECT * FROM prod_runs ORDER BY id DESC")
        rows = c.fetchall()
        conn.close()
        return rows

    # 💡 [핵심 패치] 마지막 DAQ 파일명을 가져와서 Prefix, Number, Tag를 추론하기 위한 함수
    def get_last_daq_run(self):
        conn = sqlite3.connect(self.db_path)
        c = conn.cursor()
        c.execute("SELECT file_name FROM daq_runs ORDER BY id DESC LIMIT 1")
        row = c.fetchone()
        conn.close()
        return row[0] if row else None