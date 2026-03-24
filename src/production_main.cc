#include <iostream>
#include <fstream>
#include <vector>
#include "RootProducer.hh"

// KFADC500의 1개 이벤트 크기 (채널수, 헤더 유무에 따라 변경 필요)
constexpr size_t RECORD_LENGTH = 1024; 
constexpr size_t EVENT_BYTE_SIZE = RECORD_LENGTH * sizeof(uint16_t);

int main(int argc, char** argv) {
    if (argc < 3) {
        std::cerr << "Usage: kfadc500_prod <input_raw.dat> <output_data.root>\n";
        return 1;
    }

    std::string in_file = argv[1];
    std::string out_file = argv[2];

    std::ifstream fin(in_file, std::ios::in | std::ios::binary);
    if (!fin.is_open()) {
        std::cerr << "[PROD:FATAL] Cannot open input file: " << in_file << "\n";
        return 1;
    }

    RootProducer producer(out_file);
    
    std::vector<uint16_t> buffer(RECORD_LENGTH);
    uint32_t event_id = 0;

    std::cout << "[PROD:INFO] Starting Data Conversion: " << in_file << " -> " << out_file << "\n";

    // 💡 고속 Binary to ROOT 변환 루프
    while (fin.read(reinterpret_cast<char*>(buffer.data()), EVENT_BYTE_SIZE)) {
        producer.ProcessEvent(event_id, buffer.data(), RECORD_LENGTH);
        event_id++;

        if (event_id % 10000 == 0) {
            std::cout << "\r[PROD:PROC] Processed Events: " << event_id << std::flush;
        }
    }

    std::cout << "\n[PROD:INFO] End of File reached.\n";
    
    fin.close();
    producer.Close();

    return 0;
}