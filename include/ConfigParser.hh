#pragma once
#include <string>

// 💡 KFADC500 장비 제어에 필요한 모든 레지스터 변수를 담는 구조체
struct KFADC500_Config {
    int vid;
    int pid;
    int filter;              // INPUT_FILTER
    int record_length;       // RECORD_LENGTH
    int trigger_lut;         // TRIGGER_LUT (Hex 지원)
    int pulse_width_en;      // PULSE_WIDTH_EN
    int pulse_count_en;      // PULSE_COUNT_EN
    int pulse_count_thr;     // PULSE_COUNT_THR
    int pulse_count_int;     // PULSE_COUNT_INT
    int pulse_width_thr;     // PULSE_WIDTH_THR
    int deadtime;            // DEADTIME
    int coincidence_width;   // COINCIDENCE_WIDTH

    // 채널별 설정 (인덱스 0~3 = CH0~CH3)
    int offset[4];
    int delay[4];
    int polarity[4];
    int threshold[4];
};

class ConfigParser {
public:
    static bool Parse(const std::string& filename, KFADC500_Config& config);
};