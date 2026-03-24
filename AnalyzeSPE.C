#include <TFile.h>
#include <TTree.h>
#include <TH1D.h>
#include <TH2D.h>
#include <TCanvas.h>
#include <TF1.h>
#include <TStyle.h>
#include <Math/MinimizerOptions.h>

// 💡 데이터 파일 기본 경로를 data/ 로 설정
void AnalyzeSPE(const char* filename = "data/kfadc500_data.root", int target_ch = 0) {
    // ROOT의 기본 피팅 엔진을 강력한 MINUIT2로 강제 설정
    ROOT::Math::MinimizerOptions::SetDefaultMinimizer("Minuit2");
    gStyle->SetOptFit(1111); // 피팅 파라미터 박스 표시
    gStyle->SetPalette(kRainBow);

    TFile* file = new TFile(filename, "READ");
    if (!file || file->IsZombie()) {
        printf("Error: Cannot open %s\n", filename);
        return;
    }
    TTree* tree = (TTree*)file->Get("kfadc_tree");

    TCanvas* c1 = new TCanvas("c1", "KFADC500 Analysis", 1600, 600);
    c1->Divide(2, 1);

    // ==========================================================
    // 1. 2D 파형 산점도 (Persistence Plot)
    // ==========================================================
    c1->cd(1);
    
    // X축: 샘플 인덱스, Y축: 반전된 ADC 값
    TString draw_cmd = Form("raw_event_data:Iteration$ - (%d * record_length * 128)", target_ch);
    TString cut_cmd = Form("Iteration$ >= (%d * record_length * 128) && Iteration$ < ((%d+1) * record_length * 128)", target_ch, target_ch);

    // TH2D 생성 (Y축 범위 -100 ~ 1000 은 현장 데이터 피크에 맞게 조절하세요)
    TH2D* h2_wave = new TH2D("h2_wave", Form("Waveform Persistence (Ch %d);Sample Index;ADC", target_ch), 
                             200, 0, 200, 100, -100, 1000); 
    
    tree->Draw(Form("%s>>h2_wave", draw_cmd.Data()), cut_cmd, "colz");

    // ==========================================================
    // 2. 단일 광전자 (SPE) 전하합(Charge) 피팅 (MINUIT2)
    // ==========================================================
    c1->cd(2);
    
    // X축 범위는 PMT 증폭률에 맞게 조절 (-500 ~ 5000)
    TH1D* h_charge = new TH1D("h_charge", Form("Charge Spectrum (Ch %d);Charge (ADC Sum);Counts", target_ch), 200, -500, 5000);
    tree->Draw(Form("charge[%d]>>h_charge", target_ch), "", "HIST");

    // 다중 가우시안 피팅 함수 정의 (Pedestal + 1 P.E.)
    TF1* spe_fit = new TF1("spe_fit", "gaus(0) + gaus(3)", -200, 3000);
    
    spe_fit->SetParName(0, "Ped_Const");
    spe_fit->SetParName(1, "Ped_Mean");
    spe_fit->SetParName(2, "Ped_Sigma");
    spe_fit->SetParName(3, "1PE_Const");
    spe_fit->SetParName(4, "1PE_Mean");
    spe_fit->SetParName(5, "1PE_Sigma");

    // 초기 파라미터 세팅
    spe_fit->SetParameter(1, 0);      // 페데스탈은 보통 0 부근
    spe_fit->SetParameter(2, 50);     // 페데스탈 폭
    spe_fit->SetParameter(4, 1000);   // 💡 1 P.E. 피크의 대략적인 위치 (현장 PMT 게인에 맞게 조절)
    spe_fit->SetParameter(5, 200);    // 1 P.E. 폭

    spe_fit->SetLineColor(kRed);
    
    // "L": Log-likelihood fit, "S": 피팅 결과 반환
    h_charge->Fit(spe_fit, "LS", "", -200, 3000); 

    c1->Update();
}