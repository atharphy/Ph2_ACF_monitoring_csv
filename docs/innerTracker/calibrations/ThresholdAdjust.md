# Threshold Adjustment

## Purpose

The purpose of the Threshold Adjustment scan is to tune the mean hit detection threshold of all pixels to the desired value in electrons (e.g., 1500 electrons). It finds the global threshold DAC (GDAC) value that shifts the mean threshold as close to the target as possible.

## Method

This scan is derived from [PixelAlive](PixelAlive.md). First, it reads the `TargetThreshold` parameter value, given in electrons, then converts it to the target [$\Delta V_{Cal}$](TermExplanations.md#calibration-voltage-vcal) value (exact conversion for CROC is done [here](https://gitlab.cern.ch/cmsinnertracker/Ph2_ACF/-/blob/myDev/HWDescription/RD53B.cc\#:~:text=Charge2VCal)). Then, the `VCAL_HIGH` register value is set to `VCAL_MED` plus the target threshold value converted to $\Delta V_{Cal}$. After that, the algorithm searches for two different values for each of the global threshold registers `DAC_GDAC_X_LIN` (`X={L, M, R}` corresponds to different parts of the pixel matrix):

1. One that maximizes the average pixel occupancy for the given injection size.
2. Another one that gets the average pixel occupancy the closest to 50% for the given injection size (which is the definition of the threshold).

![`L`, `M`, `R` pixel column arrangement](../images/L_M_R.png){width=300}

First, the occupancy maximization is done by a [golden section search](https://en.wikipedia.org/w/index.php?title=Golden-section_search&oldid=1255052905) for each of the `DAC_GDAC_X_LIN` registers (`X={L, M, R}`) between the `DAC_GDAC_X_LIN + ThrRelStart` and `DAC_GDAC_X_LIN + ThrRelStart + ThrAmplitude` values set by the user on one chip at a time (but in parallel on multiple modules, if any). Then, the actual threshold (50% efficiency) is found using a [binary search](https://en.wikipedia.org/w/index.php?title=Binary_search&oldid=1250479543) between the same `DAC_GDAC_X_LIN + ThrRelStart` and `DAC_GDAC_X_LIN + ThrRelStart + ThrAmplitude` values. At each search step, a PixelAlive measurement is done, and the average occupancy is extracted, depending on which the next search step is decided. The suggested `DAC_GDAC_X_LIN` values are printed on the screen and should be put into the XML file by hand.
The scan is set up in such a way that it can find different optimal threshold values for the leftmost (`DAC_GDAC_L_LIN`), rightmost (`DAC_GDAC_R_LIN`), and middle (`DAC_GDAC_M_LIN`) pixels.

![Depiction of the scan range w.r.t. GDAC](images/thradj/ThrRelStartAmplitude.png){width=400}

**Scan command:** `thradj`

* Analog injection

!!! warning "You have to update the config manually in the xml file"
    * RD53B: `DAC_GDAC_{L,M,R}_LIN`
    * RD53A: `Vthreshold_LIN`

## Configuration parameters

|Name                   |Typical value|Description|
|-----------------------|-------------|-----------|
|`nEvents`              |100          |Number of injections per pixel|
|`nEvtsBurst`           |=`nEvents`   |Number of events readout from FPGA in one instance|
|`nTRIGxEvent`          |10           |Number of triggers for each injection|
|`INJtype`              |1            |Injection type, 1 – analog|
|`TargetThreshold`      |1500         |Desired hit detection threshold in \#electrons of charge|
|`DAC_GDAC_{L,M,R}_LIN` |450          |Threshold reference value for the scan|
|`ThrRelStart`          |-70          |Sets the actual starting value for the scan (`DAC_GDAC_*_LIN + ThrRelStart`)|
|`ThrAmplitude`         |50           |Scan range amplitude (up from the starting value)|

## Expected output

### Suggested threshold values

Histogram below contains only the suggested `DAC_GDAC_M_LIN` register value for 50% efficiency at target charge.

![Suggested GDAC values](images/thradj/Threshold.png){width=500}

### Final pixel efficiency

Picture below shows the distribution of pixel [efficiencies](TermExplanations.md#pixel-occupancy) at the suggested `DAC_GDAC_*_LIN` values. The ideal efficiency distribution should look like a (ideally narrow) Gaussian centered around 0.5 with no high "tails" at 0 or 1. However, as can be seen in this case, there is a significant excess of pixels with occupancy $\sim$1 because the threshold was adjusted to a fairly low value and some pixels became noisy or simply have a significantly lower threshold than average due to imperfect per-pixel threshold tuning. This may be fixed by running [Threshold Equalization](ThresholdEqualization.md).

![1D efficiency](images/thradj/Occ1D.png){width=500}

Picture below shows a 2D distribution of per-pixel efficiencies at the suggested `DAC_GDAC_*_LIN` values.

![2D efficiency](images/thradj/Occ2D.png){width=500}

### ToT

Pictures below show a 1D distribution and a 2D map of [ToT](TermExplanations.md#time-over-threshold-tot) values for each pixel at the suggested `DAC_GDAC_*_LIN` values. Since the injection size equals the average threshold, most pixels either see no hit or measure ToT=0.

![1D ToT](images/thradj/ToT1D.png){width=500}

![2D ToT](images/thradj/ToT2D.png){width=500}

### Calibration result

Pictures below show the threshold distribution from S-Curve measurement before and after the Threshold Adjustment to 2000 electrons. The default threshold without any adjustment is around 300 $\Delta V_{\text{Cal}}$.

![Default thresholds around 300 `DeltaVCal`](images/thradj/threshold_adjust_1.png){width=400}

![Adjusted threshold with target 2000 electrons](images/thradj/threshold_adjust_2.png){width=400}

## Tips

It should be noted that Threshold Adjustment should use a fairly high threshold (at least 2000 electrons) for the initial tuning before running [Threshold Equalization](ThresholdEqualization.md), as many pixels may become noisy and get unnecessarily masked due to their lower than average threshold. After running the Threshold Equalization, the Threshold Adjustment can be repeated with the threshold lowered to the desired value.