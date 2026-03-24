#include "ConfigParser.hh"
#include <fstream>
#include <iostream>
#include <algorithm>

bool ConfigParser::Parse(const std::string& filename, KFADC500_Config& config) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "[SYSTEM:ERROR] Cannot open config file: " << filename << "\n";
        return false;
    }

    // 기본값 초기화 (안전 장치)
    config.vid = 0x0547;
    config.pid = 0x1080;
    config.filter = 1;
    config.record_length = 8;
    config.trigger_lut = 0xFFFE;
    config.pulse_width_en = 1;
    config.pulse_count_en = 1;
    config.pulse_count_thr = 1;
    config.pulse_count_int = 1000;
    config.pulse_width_thr = 100;
    config.deadtime = 0;
    config.coincidence_width = 1000;
    for (int i = 0; i < 4; ++i) {
        config.offset[i] = 3500;
        config.delay[i] = 100;
        config.polarity[i] = 0;
        config.threshold[i] = 100;
    }

    auto trim = [](std::string& s) {
        size_t p = s.find_first_not_of(" \t\r\n");
        s.erase(0, p);
        p = s.find_last_not_of(" \t\r\n");
        if (std::string::npos != p) s.erase(p + 1);
    };

    std::string line;
    int current_ch = -1; // -1: GLOBAL, 0~3: CH0~CH3

    while (std::getline(file, line)) {
        // 주석 제거
        size_t comment_pos = line.find('#');
        if (comment_pos != std::string::npos) line = line.substr(0, comment_pos);
        
        trim(line);
        if (line.empty()) continue;

        // 섹션 파싱
        if (line.front() == '[' && line.back() == ']') {
            std::string section = line.substr(1, line.size() - 2);
            if (section == "GLOBAL") current_ch = -1;
            else if (section == "CH0") current_ch = 0;
            else if (section == "CH1") current_ch = 1;
            else if (section == "CH2") current_ch = 2;
            else if (section == "CH3") current_ch = 3;
            continue;
        }

        // Key = Value 파싱
        size_t eq_pos = line.find('=');
        if (eq_pos == std::string::npos) continue;

        std::string key = line.substr(0, eq_pos);
        std::string val = line.substr(eq_pos + 1);
        trim(key);
        trim(val);

        try {
            // std::stoi의 세 번째 인자 0은 '0x' 접두어가 있으면 16진수로, 아니면 10진수로 자동 파싱함
            int int_val = std::stoi(val, nullptr, 0);

            if (current_ch == -1) { // [GLOBAL]
                if (key == "VID") config.vid = int_val;
                else if (key == "PID") config.pid = int_val;
                else if (key == "INPUT_FILTER") config.filter = int_val;
                else if (key == "RECORD_LENGTH") config.record_length = int_val;
                else if (key == "TRIGGER_LUT") config.trigger_lut = int_val;
                else if (key == "PULSE_WIDTH_EN") config.pulse_width_en = int_val;
                else if (key == "PULSE_COUNT_EN") config.pulse_count_en = int_val;
                else if (key == "PULSE_COUNT_THR") config.pulse_count_thr = int_val;
                else if (key == "PULSE_COUNT_INT") config.pulse_count_int = int_val;
                else if (key == "PULSE_WIDTH_THR") config.pulse_width_thr = int_val;
                else if (key == "DEADTIME") config.deadtime = int_val;
                else if (key == "COINCIDENCE_WIDTH") config.coincidence_width = int_val;
            } else if (current_ch >= 0 && current_ch <= 3) { // [CH0] ~ [CH3]
                if (key == "OFFSET") config.offset[current_ch] = int_val;
                else if (key == "DELAY") config.delay[current_ch] = int_val;
                else if (key == "POLARITY") config.polarity[current_ch] = int_val;
                else if (key == "THRESHOLD") config.threshold[current_ch] = int_val;
            }
        } catch (const std::exception& e) {
            std::cerr << "[SYSTEM:WARN] Invalid value parsing format for key: " << key << " = " << val << "\n";
        }
    }

    return true;
}