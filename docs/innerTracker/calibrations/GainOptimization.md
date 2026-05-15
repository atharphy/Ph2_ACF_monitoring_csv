# Gain Optimization

!!! warning "This calibration was substantially changed in Ph2_ACF v6-30"
    If you are using an older Ph2_ACF version, you should refer to the following description [here](https://gitlab.cern.ch/cms_tk_ph2/Ph2_ACF/-/blob/v6-29/docs/innerTracker/calibrations/GainOptimization.md).

## Purpose

The purpose of the Gain Optimization is optimizing the signal gain for efficient measurement of the target charge. This calibration tunes the Krummenacher current $I_{\text{Krum}}$ that discharges the capacitor in the preamplifier circuit and directly affects the [ToT](TermExplanations.md#time-over-threshold-tot) vs [$\Delta V_{\text{Cal}}$](TermExplanations.md#calibration-voltage-vcal) slope (gain).

## Method

Gain Optimization is derived from [PixelAlive](PixelAlive.md).
It works by repeating the PixelAlive many times with the same injected charge (set by `targetCharge`), but different Krummenacher current (`DAC_KRUM_CURR_LIN`) values.
A [binary search](https://en.wikipedia.org/w/index.php?title=Binary_search&oldid=1250479543) in `DAC_KRUM_CURR_LIN` is performed between `KrumCurrStart` and `KrumCurrStop`, looking for the Krummenacher current that produces the average ToT equal to `targetToT`.

**Scan command:** `gainopt`

* Analog injection

!!! note "You have to update the config manually in the xml file"
    * RD53B: `DAC_KRUM_CURR_LIN`
    * RD53A: `KRUM_CURR_LIN`

## Configuration Parameters

|Name            |Typical Value| Description |
|----------------|-------------|-------------|
|`nEvents`       |100          |Number of injections per `DAC_KRUM_CURR_LIN` step|
|`nEvtsBurst`    |=`nEvents`   |Number of events readout from FPGA in one instance|
|`nTRIGxEvent`   |10           |Number of triggers for each injection|
|`INJtype`       |1            |Injection type, 1 – analog|
|`ToT6to4Mapping`|0            |Whether to use the [ToT](TermExplanations.md#time-over-threshold-tot) dual-slope mode (6-to-4 bit compression)|
|`KrumCurrStart` |0            |The lowest `DAC_KRUM_CURR_LIN` register value for the scan|
|`KrumCurrStop`  |127          |The highest `DAC_KRUM_CURR_LIN` register value for the scan|
|`targetCharge`  |6000         |Charge (in electrons) used for the scan|
|`targetToT`     |5.3 or 2     |ToT to be reached at target charge (5.3 for slow discharge, 2 for fast discharge)|

## Expected Output

Pictures below show the plots produced by the Gain Optimization with a target charge of 10000 electrons and target ToT of 3 for the single-slope mode.

### Suggested Krummenacher current

Picture below shows the suggested `DAC_KRUM_CURR_LIN` value in single-slope mode (165).
 <!-- and dual-slope modes respectively (127 for single-slope and 24 for dual-slope mode). -->

<!-- |Single-slope mode|Dual-slope mode (6-to-4 bit conversion)|
|-----------------|---------------------------------------|
|![KrumCurr single-slope](images/gainopt/KrumCurr1.png){width=350}|![KrumCurr dual-slope](images/gainopt/KrumCurr2.png){width=350}| -->

![KrumCurr](images/gainopt/NEW_KrumCurr.png){width=400}

<!-- ### Gain curves 

Pictures below show superimposed gain curves of each pixel in single and dual-slope modes respectively.

|Single-slope mode|Dual-slope mode (6-to-4 bit conversion)|
|-----------------|---------------------------------------|
|![Gain curves single-slope](images/gainopt/Gain1.png){width=350}|![Gain curves dual-slope](images/gainopt/Gain2.png){width=350}|

### 3D gain maps

Pictures below show gain maps for single and dual-slope modes respectively. These are 3D histograms with pixel row, column, and injected charge on the three axes, while the bin contents represented by color correspond to pixel occupancy. While impossible to read by themselves, these histograms can be used to make projections and extract individual gain curves of each pixel.

|Single-slope mode|Dual-slope mode (6-to-4 bit conversion)|
|-----------------|---------------------------------------|
|![Gain maps single-slope](images/gainopt/GainMap1.png){width=350}|![Gain maps dual-slope](images/gainopt/GainMap2.png){width=350}|

### Gain slope (low charge)

Pictures below show 1D distributions of slopes of the linear gain fit for single and dual-slope modes respectively. For the dual-slope mode, the distribution corresponds to the low-charge linear fit that goes up to ToT=8.

|Single-slope mode|Dual-slope mode (6-to-4 bit conversion)|
|-----------------|---------------------------------------|
|![1D Gain single-slope lowQ](images/gainopt/SlopeLowQ1D1.png){width=350}|![1D Gain dual-slope lowQ](images/gainopt/SlopeLowQ1D2.png){width=350}|

Pictures below show 2D maps of low-charge gain slopes for single and dual-slope modes respectively.

|Single-slope mode|Dual-slope mode (6-to-4 bit conversion)|
|-----------------|---------------------------------------|
|![2D Gain single-slope lowQ](images/gainopt/SlopeLowQ2D1.png){width=350}|![2D Gain dual-slope lowQ](images/gainopt/SlopeLowQ2D2.png){width=350}|

### Gain intercept (low charge)

Pictures below show 1D distributions of linear fit intercepts for single and dual-slope modes respectively (ToT<8 for the dual-slope mode).

|Single-slope mode|Dual-slope mode (6-to-4 bit conversion)|
|-----------------|---------------------------------------|
|![1D Intercept single-slope lowQ](images/gainopt/InterceptLowQ1D1.png){width=350}|![1D Intercept dual-slope lowQ](images/gainopt/InterceptLowQ1D2.png){width=350}|

Pictures below show 2D maps of low-charge gain intercepts for single and dual-slope modes respectively.

|Single-slope mode|Dual-slope mode (6-to-4 bit conversion)|
|-----------------|---------------------------------------|
|![2D Gain single-slope lowQ](images/gainopt/InterceptLowQ2D1.png){width=350}|![2D Gain dual-slope lowQ](images/gainopt/InterceptLowQ2D2.png){width=350}|

### Gain slope (high charge)

Pictures below show distributions of slopes of the linear fit for high charge (ToT$\geqslant$8), for single and dual-slope modes respectively. Since the former uses single-slope mode, there is no high-charge fit performed and the distribution contains only zeroes.

|Single-slope mode|Dual-slope mode (6-to-4 bit conversion)|
|-----------------|---------------------------------------|
|![1D Gain single-slope highQ](images/gainopt/SlopeHighQ1D1.png){width=350}|![1D Gain dual-slope highQ](images/gainopt/SlopeHighQ1D2.png){width=350}|

Pictures below show 2D maps of high-charge gain slopes for single and dual-slope modes respectively.

|Single-slope mode|Dual-slope mode (6-to-4 bit conversion)|
|-----------------|---------------------------------------|
|![2D Gain single-slope highQ](images/gainopt/SlopeHighQ2D1.png){width=350}|![2D Gain dual-slope highQ](images/gainopt/SlopeHighQ2D2.png){width=350}|

### Gain intercept (high charge)

Pictures below show distributions of linear fit intercepts for high charge (ToT$\geqslant$8), for single and dual-slope modes respectively. Again, the histogram for the single-slope mode contains only zeroes.

|Single-slope mode|Dual-slope mode (6-to-4 bit conversion)|
|-----------------|---------------------------------------|
|![1D Intercept single-slope highQ](images/gainopt/InterceptHighQ1D1.png){width=350}|![1D Intercept dual-slope highQ](images/gain/InterceptHighQ1D2.png){width=350}|

Pictures below show 2D maps of high-charge gainopt intercepts for single and dual-slope modes respectively.

|Single-slope mode|Dual-slope mode (6-to-4 bit conversion)|
|-----------------|---------------------------------------|
|![2D Gain single-slope highQ](images/gainopt/InterceptHighQ2D1.png){width=350}|![2D Gain dual-slope highQ](images/gainopt/InterceptHighQ2D2.png){width=350}|

### Fit $\chi^2/N_{\text{D.o.F.}}$

Pictures below show distributions of fit $\chi^2/N_{\text{D.o.F.}}$ values for single and dual-slope modes respectively. For the single slope mode, most fits have fairly low $\chi^2/N_{\text{D.o.F.}}$ values.

|Single-slope mode|Dual-slope mode (6-to-4 bit conversion)|
|-----------------|---------------------------------------|
|![1D chi2 single-slope](images/gainopt/Chi2DoF1D1.png){width=350}|![1D chi2 dual-slope](images/gainopt/Chi2DoF1D2.png){width=350}|

Pictures below show 2D maps of per-pixel fit $\chi^2/N_{\text{D.o.F.}}$ values for single and dual-slope modes respectively.

|Single-slope mode|Dual-slope mode (6-to-4 bit conversion)|
|-----------------|---------------------------------------|
|![2D chi2 single-slope](images/gainopt/Chi2DoF2D1.png){width=350}|![2D chi2 dual-slope](images/gainopt/Chi2DoF2D2.png){width=350}|

### Fit Errors

Pictures below show 2D maps of fit errors for single and dual-slope modes respectively. The pixels shown in yellow had the fit failing to converge.

|Single-slope mode|Dual-slope mode (6-to-4 bit conversion)|
|-----------------|---------------------------------------|
|![2D fit errors single-slope](images/gainopt/FitErrors1.png){width=350}|![2D fit errors dual-slope](images/gainopt/FitErrors2.png){width=350}| -->

### Final ToT distribution

The picture below shows the ToT distribution at the target charge with the optimal Krummenacher current value.

![FinalToT](images/gainopt/NEW_ToT1D.png){width=400}

### Final 2D pixel occupancy

The picture below shows the 2D distribution of all the pixel occupancies at the target charge with the optimal Krummenacher current. As we can see, nearly all the occupancies (apart from a few unresponsive pixels) are equal to one (as the target charge is way above the threshold).

![FinalPixelAlive](images/gainopt/NEW_PixelAlive2D.png){width=400}