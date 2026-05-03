#include <TFile.h>
#include <TTree.h>
#include <TH1D.h>
#include <TH2D.h>
#include <TCanvas.h>
#include <TF1.h>
#include <TStyle.h>
#include <Math/MinimizerOptions.h>

// 💡 데이터 파일 기본 경로 업데이트 (예: data/run_001.root)
void AnalyzeSPE(const char* filename = "data/run_001.root", int target_ch = 0) {
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
    if (!tree) {
        printf("Error: 'kfadc_tree' not found in %s\n", filename);
        return;
    }

    TCanvas* c1 = new TCanvas("c1", "KFADC500 SPE Analysis", 1600, 600);
    c1->Divide(2, 1);

    // ==========================================================
    // 1. 2D 파형 산점도 (Persistence Plot)
    // ==========================================================
    c1->cd(1);
    
    // 💡 [패치] De-interleaved된 독립 벡터 사용 및 시간 축(ns) 변환
    // Iteration$은 ROOT TTree에서 벡터의 인덱스를 나타냄. (500MS/s = 2ns/sample)
    TString draw_cmd = Form("wave_ch%d:Iteration$ * 2.0", target_ch);

    // TH2D 생성 (시간 범위 0~2000ns, ADC 범위 -100~1000) - 현장 데이터에 맞게 조절하세요
    TH2D* h2_wave = new TH2D("h2_wave", Form("Waveform Persistence (Ch %d);Time (ns);Amplitude (ADC)", target_ch), 
                             200, 0, 2000, 100, -100, 1000); 
    
    // 더 이상 복잡한 cut_cmd가 필요 없습니다.
    tree->Draw(Form("%s>>h2_wave", draw_cmd.Data()), "", "colz");

    // ==========================================================
    // 2. 단일 광전자 (SPE) 전하합(Charge) 피팅 (MINUIT2)
    // ==========================================================
    c1->cd(2);
    
    // X축 범위는 PMT 증폭률에 맞게 조절 (-500 ~ 5000)
    TH1D* h_charge = new TH1D("h_charge", Form("Charge Spectrum (Ch %d);Integrated Charge (ADC Sum);Counts", target_ch), 200, -500, 5000);
    
    // 💡 [패치] 독립된 스칼라 브랜치 사용
    tree->Draw(Form("charge_ch%d>>h_charge", target_ch), "", "HIST");

    // 다중 가우시안 피팅 함수 정의 (Pedestal + 1 P.E.)
    TF1* spe_fit = new TF1("spe_fit", "gaus(0) + gaus(3)", -200, 3000);
    
    spe_fit->SetParName(0, "Ped_Const");
    spe_fit->SetParName(1, "Ped_Mean");
    spe_fit->SetParName(2, "Ped_Sigma");
    spe_fit->SetParName(3, "1PE_Const");
    spe_fit->SetParName(4, "1PE_Mean");
    spe_fit->SetParName(5, "1PE_Sigma");

    // 초기 파라미터 세팅
    spe_fit->SetParameter(1, 0);      // 💡 이미 베이스라인이 차감된 값이므로 페데스탈은 0 부근에 위치함
    spe_fit->SetParameter(2, 50);     // 페데스탈 폭
    spe_fit->SetParameter(4, 1000);   // 💡 1 P.E. 피크의 대략적인 위치 (현장 PMT 게인에 맞게 조절)
    spe_fit->SetParameter(5, 200);    // 1 P.E. 폭

    spe_fit->SetLineColor(kRed);
    
    // "L": Log-likelihood fit, "S": 피팅 결과 반환
    h_charge->Fit(spe_fit, "LS", "", -200, 3000); 

    c1->Update();
}
