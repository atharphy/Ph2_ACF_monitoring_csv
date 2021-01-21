#ifndef CMFITS_H__
#define CMFITS_H__

#ifdef __USE_ROOT__

#include "TF1.h"
#include "TH1F.h"
#include "TMath.h"

#define NSAMPLINGSCMN 100
#define SIGMARANGE 6
#define NORMGAUS 3.98942280401432703e-01

inline double findMaximum(TH1F* histogram)
{
    int maxbin = histogram->GetMaximumBin();
    return histogram->GetXaxis()->GetBinCenter(maxbin);
}

inline double hitProbability(double threshold)
{
    return 0.5 - (TMath::Erf(threshold / sqrt(2)) / 2);
    // area above threshold under the gaussian curve.
    // The Factors are to only treat the positive half
    // 1-erf(x/sqrt(2)/2 + .5)
}

inline double inverse_hitProbability(double probability)
{
    // the inverse of the above function!
    return sqrt(2) * TMath::ErfInverse(1 - 2 * probability);
}

inline double binomialPdf(int n, int k, double p) { return TMath::Binomial(n, k) * pow(p, k) * pow((1 - p), n - k); }

inline double hitProbFunction(double* xStrips, Double_t* par)
{
    const double samplingHalfStep = SIGMARANGE / static_cast<double>(NSAMPLINGSCMN);
    double&      threshold        = par[0];
    double&      cmnFraction      = par[1];
    double&      nEvents          = par[2];
    double&      nActiveStrips    = par[3];

    double result = 0;
    double hitProb;
    double sampleProbability, x;

    int iStrips = int(ceil(*xStrips - 0.5));                 // round to nearest integer
    if((iStrips < 0) || (iStrips > nActiveStrips)) return 0; // only defined in range

    for(int j = 0; j < NSAMPLINGSCMN; ++j)
    {
        // loop over all x values
        x = -SIGMARANGE + j * 2 * samplingHalfStep;
        // approximate probability at sampling point by interpolating
        sampleProbability = hitProbability(x - samplingHalfStep);
        sampleProbability -= hitProbability(x + samplingHalfStep);

        // probability of hit taking cmn into account
        hitProb = hitProbability(threshold + x * cmnFraction);
        // distribution function scaled to nevents
        result += binomialPdf(int(nActiveStrips), iStrips, hitProb) * sampleProbability * nEvents;
    }
    return result;
}

inline void fitDistribution(TH1F* pHitCountHisto, TF1* pFit, int nActiveStrips)
{
    // First-order approximation
    double prob = findMaximum(pHitCountHisto) / pHitCountHisto->GetNbinsX();

    // retrieve the threshold from the maximum of the actual nhit distribution
    double threshold = inverse_hitProbability(prob);

    // initialize cmnFraction to 0 anc later extract from fit
    double cmnFraction = 0;
    pFit->SetRange(0, nActiveStrips);

    // Name Parameters
    pFit->SetParName(0, "threshold");
    pFit->SetParName(1, "cmnFraction");
    pFit->SetParName(2, "nEvents");
    pFit->SetParName(3, "nActiveStrips");

    // Set Parameters
    pFit->SetParameter(0, threshold);
    pFit->SetParameter(1, cmnFraction);

    // Fix Parameters nEvents & nActiveStrips as these I know
    pFit->FixParameter(2, pHitCountHisto->GetEntries());
    pFit->FixParameter(3, nActiveStrips);

    // Fit and return
    pHitCountHisto->Fit(pFit, "RQNM+");
}

inline TH1F* createNoiseDistribution(TH1F* pNoCM, double pThreshold, double pCMNFraction, int pNEvents, int pNActiveStrips)
{
    pNoCM->SetBins(pNActiveStrips + 1, -0.5, pNActiveStrips + 0.5);
    double par[4];
    par[0] = pThreshold;
    par[1] = pCMNFraction;
    par[2] = pNEvents;
    par[3] = pNActiveStrips;
    double x;

    for(int i = 0; i <= pNActiveStrips; ++i)
    {
        x = i;
        pNoCM->Fill(x, hitProbFunction(&x, par));
    }
    return pNoCM;
}

#endif
#endif