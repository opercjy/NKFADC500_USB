#include "RootProducer.hh"
#include <iostream>
#include <fstream>
#include <algorithm>
#include <TSystem.h>

RootProducer::RootProducer(const std::string& input_file, const std::string& output_file, bool save_waveform, bool display_mode)
    : in_filename_(input_file), out_filename_(output_file), 
      save_waveform_(save_waveform), display_mode_(display_mode),
      root_file_(nullptr), tree_(nullptr), event_id_(0), record_length_(0), preset_events_(0) {
    
    if (!display_mode_) {
        root_file_ = new TFile(out_filename_.c_str(), "RECREATE");
        root_file_->cd();
        tree_ = new TTree("kfadc_tree", "Notice KFADC500 Physics Data");

        tree_->Branch("event_id", &event_id_, "event_id/I");
        tree_->Branch("record_length", &record_length_, "record_length/I");
        tree_->Branch("pedestal", pedestal_, "pedestal[4]/D");
        tree_->Branch("charge", charge_, "charge[4]/D");
        tree_->Branch("peak", peak_, "peak[4]/D");
        
        if (save_waveform_) {
            tree_->Branch("raw_event_data", &raw_event_data_); 
        }
    }
}

RootProducer::~RootProducer() {
    if (root_file_) {
        root_file_->cd();
        if (tree_) tree_->Write();
        root_file_->Close();
        delete root_file_;
    }
}

void RootProducer::RunDisplayMode() {
    std::ifstream infile(in_filename_, std::ios::binary);
    if (!infile.is_open()) return;

    infile.read(reinterpret_cast<char*>(&record_length_), sizeof(int));
    infile.read(reinterpret_cast<char*>(&preset_events_), sizeof(int));

    infile.seekg(0, std::ios::end);
    long long file_size = infile.tellg();
    int event_size_bytes = record_length_ * 512;
    int num_shorts = event_size_bytes / 2;
    int samples_per_ch = num_shorts / 4; 
    
    int total_events = (file_size - 8) / event_size_bytes; 
    raw_event_data_.resize(num_shorts);

    const int SKIP_BINS = 20; 
    const int PED_START = 22;
    const int PED_END = 80;   

    TCanvas* c1 = new TCanvas("c1", "KFADC500 Waveform Viewer", 1200, 800);
    c1->Divide(2, 2);
    TH1D* h_wave[4];
    for (int ch = 0; ch < 4; ++ch) {
        h_wave[ch] = new TH1D(Form("h_wave_ch%d", ch), Form("Channel %d (Inverted);Sample Index;ADC", ch), samples_per_ch, 0, samples_per_ch);
        h_wave[ch]->SetLineColor(kBlue + ch);
        h_wave[ch]->SetStats(0); 
    }

    int current_id = 0;
    std::string input;

    while (true) {
        if (current_id < 0) current_id = 0;
        if (current_id >= total_events) current_id = total_events - 1;

        infile.seekg(8 + (long long)current_id * event_size_bytes, std::ios::beg);
        infile.read(reinterpret_cast<char*>(raw_event_data_.data()), event_size_bytes);

        for (int ch = 0; ch < 4; ++ch) { pedestal_[ch] = 0; h_wave[ch]->Reset(); }

        for (int ch = 0; ch < 4; ++ch) {
            int offset = ch * samples_per_ch; 
            
            int ped_start = std::min(PED_START, samples_per_ch);
            int ped_end = std::min(PED_END, samples_per_ch);
            int num_ped = ped_end - ped_start;
            
            for (int i = ped_start; i < ped_end; ++i) {
                pedestal_[ch] += raw_event_data_[offset + i];
            }
            if (num_ped > 0) pedestal_[ch] /= num_ped;

            for (int i = SKIP_BINS; i < samples_per_ch; ++i) {
                double inverted_adc = pedestal_[ch] - raw_event_data_[offset + i];
                h_wave[ch]->SetBinContent(i + 1, inverted_adc); 
            }
        }

        for (int ch = 0; ch < 4; ++ch) {
            c1->cd(ch + 1);
            h_wave[ch]->Draw("HIST");
        }
        c1->Update();
        gSystem->ProcessEvents();

        std::cout << "\r\033[1;33m[DISPLAY]\033[0m Event \033[1;32m" << current_id << "\033[0m / " << total_events - 1 
                  << " | [\033[1;36mn\033[0m]ext, [\033[1;36mp\033[0m]rev, [\033[1;36mj <id>\033[0m]ump, [\033[1;31mq\033[0m]uit : ";
        std::getline(std::cin, input);
        
        if (input == "q" || input == "Q") break;
        else if (input == "p" || input == "P") current_id--;
        else if (input == "n" || input == "N" || input.empty()) current_id++;
        else if (input.rfind("j ", 0) == 0 || input.rfind("J ", 0) == 0) {
            try { current_id = std::stoi(input.substr(2)); }
            catch (...) { std::cout << "Invalid jump format. Use 'j <id>'\n"; }
        }
    }
    delete c1;
}

void RootProducer::RunBatchMode() {
    std::ifstream infile(in_filename_, std::ios::binary);
    if (!infile.is_open()) return;

    infile.read(reinterpret_cast<char*>(&record_length_), sizeof(int));
    infile.read(reinterpret_cast<char*>(&preset_events_), sizeof(int));

    int event_size_bytes = record_length_ * 512;
    int num_shorts = event_size_bytes / 2; 
    int samples_per_ch = num_shorts / 4; 
    raw_event_data_.resize(num_shorts);

    const int SKIP_BINS = 20; 
    const int PED_START = 22;
    const int PED_END = 80;   

    event_id_ = 0;
    while (true) {
        infile.read(reinterpret_cast<char*>(raw_event_data_.data()), event_size_bytes);
        if (infile.gcount() < event_size_bytes) break;

        for (int ch = 0; ch < 4; ++ch) {
            pedestal_[ch] = 0; charge_[ch] = 0; peak_[ch] = -9999;
        }

        for (int ch = 0; ch < 4; ++ch) {
            int offset = ch * samples_per_ch;
            
            int ped_start = std::min(PED_START, samples_per_ch);
            int ped_end = std::min(PED_END, samples_per_ch);
            int num_ped = ped_end - ped_start;
            
            for (int i = ped_start; i < ped_end; ++i) {
                pedestal_[ch] += raw_event_data_[offset + i];
            }
            if (num_ped > 0) pedestal_[ch] /= num_ped;

            for (int i = SKIP_BINS; i < samples_per_ch; ++i) {
                double inverted_adc = pedestal_[ch] - raw_event_data_[offset + i];
                charge_[ch] += inverted_adc;
                if (inverted_adc > peak_[ch]) peak_[ch] = inverted_adc;
            }
        }

        tree_->Fill();
        event_id_++;

        if (event_id_ % 5000 == 0) {
            // 💡 [UX 패치] 터미널 도배를 방지하는 한 줄 프로그레스 바 
            std::cout << "\r\033[K\033[1;34m[PROD:INFO]\033[0m Processing... \033[1;32m" << event_id_ << "\033[0m events saved." << std::flush;
        }
    }
    std::cout << "\n\033[1;32m[PROD:SUCCESS] Total " << event_id_ << " events saved to ROOT TTree.\033[0m\n";
}