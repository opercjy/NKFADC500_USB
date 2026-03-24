#pragma once
#include <string>
#include <fstream>
#include <map>
#include <iostream>

struct KFADC500_Config {
    int filter = 0;
    int record_length = 4;
    unsigned int trigger_lut = 0xFFFF;
    int pulse_count_en = 0;
    int pulse_width_en = 0;
    int pulse_count_thr = 1;
    int pulse_count_int = 1000;
    int pulse_width_thr = 100;
    int deadtime = 0;
    int coincidence_width = 1000;

    int offset[4] = {0};
    int delay[4] = {0};
    int polarity[4] = {0};
    int threshold[4] = {50, 50, 50, 50};
};

class ConfigParser {
public:
    static bool Parse(const std::string& filename, KFADC500_Config& config) {
        std::ifstream file(filename);
        if (!file.is_open()) return false;

        std::string line, current_section = "";
        std::map<std::string, std::string> kv_map;

        while (std::getline(file, line)) {
            size_t comment_pos = line.find('#');
            if (comment_pos != std::string::npos) line = line.substr(0, comment_pos);
            line.erase(0, line.find_first_not_of(" \t\r\n"));
            line.erase(line.find_last_not_of(" \t\r\n") + 1);
            if (line.empty()) continue;

            if (line.front() == '[' && line.back() == ']') {
                current_section = line.substr(1, line.size() - 2);
                continue;
            }

            size_t delim_pos = line.find('=');
            if (delim_pos != std::string::npos) {
                std::string key = line.substr(0, delim_pos);
                std::string val = line.substr(delim_pos + 1);
                key.erase(line.find_last_not_of(" \t", delim_pos - 1) + 1);
                val.erase(0, val.find_first_not_of(" \t"));
                
                std::string full_key = current_section + "_" + key;
                kv_map[full_key] = val;
            }
        }

        try {
            if (kv_map.count("GLOBAL_RECORD_LENGTH")) config.record_length = std::stoi(kv_map["GLOBAL_RECORD_LENGTH"]);
            if (kv_map.count("GLOBAL_TRIGGER_LUT")) config.trigger_lut = std::stoul(kv_map["GLOBAL_TRIGGER_LUT"], nullptr, 16);
            if (kv_map.count("GLOBAL_INPUT_FILTER")) config.filter = std::stoi(kv_map["GLOBAL_INPUT_FILTER"]);
            if (kv_map.count("GLOBAL_DEADTIME")) config.deadtime = std::stoi(kv_map["GLOBAL_DEADTIME"]);
            
            for (int ch = 0; ch < 4; ++ch) {
                std::string prefix = "CH" + std::to_string(ch) + "_";
                if (kv_map.count(prefix + "THRESHOLD")) config.threshold[ch] = std::stoi(kv_map[prefix + "THRESHOLD"]);
                if (kv_map.count(prefix + "OFFSET")) config.offset[ch] = std::stoi(kv_map[prefix + "OFFSET"]);
                if (kv_map.count(prefix + "DELAY")) config.delay[ch] = std::stoi(kv_map[prefix + "DELAY"]);
                if (kv_map.count(prefix + "POLARITY")) config.polarity[ch] = std::stoi(kv_map[prefix + "POLARITY"]);
            }
        } catch (...) {
            std::cerr << "[CONFIG:ERROR] Number parsing error in config file.\n";
            return false;
        }
        return true;
    }
};