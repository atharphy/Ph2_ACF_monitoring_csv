# Tuning of CROC SCCs

## Tuning of the AFE registers

The full procedure as given by L. Gaioni can be found [here](https://indico.cern.ch/event/1247433/).

### Main contributions to the current consumption:

* Pre-amplifier =  3 μA per pixel (total additional 435 mA)
* Comparator = 1.1 μA per pixel for low thresholds,  
    2.2 μA per pixel for very high thresholds 
   (total additional 160 mA and 320 mA respectively)
* TDAC = 400 nA per pixel (total additional 58 mA)

### Procedure

1. Set all `GDAC_LIN` = 900, all `TDAC` = 0 and measure the total analog current $I_A$.
1. Set `COMP_LIN` = 0 and measure the current $I'_A$.
   The estimated current consumption of the comparator is $\frac{I_A - I'_A}{N_\textrm{pixels}}$
1. Tune `COMP_LIN` to the value corresponding to an additional current consumption of 2.2 μA per pixel.
1. After the tuning of the comparator, measure the new $I_A$ .
1. Set `PREAMP_BIAS_LIN` = 0 and measure $I'_A$. 
   The estimated current consumption of the pre-amplifier is $\frac{I_A - I'_A}{N_\textrm{pixels}}$
1. Tune `PREAMP_BIAS_LIN` to the value corresponding to an additional current consumption of 3 μA per pixel.
1. Set all `TDAC` = 16 and measure $I_A$, then set all `TDAC` = 0 and measure $I_A$. 
   The estimated current consumption of the TDAC is $\frac{I_A - I'_A}{N_\textrm{pixels}}$
1. Tune `LDAC_LIN` to target the default current consumption of 400 nA per pixel.
1. Scale `FC_BIAS_LIN` and `COMP_TA_LIN` by a factor `COMP_LIN` / 110.
1. Tune `KRUM_CURR_LIN` to an average `ToT` = 5.3 or average `ToT` = 2 for slow and fast discharge respectively,
   injecting a charge of 6000 e- with pixelalive and checking the ToT distribution in the Results folder.

##  Tuning of fresh and irradiated modules

Copy the `CMSIT_RD53B.txt` and `CMSIT_RD53B.xml` files into the main directory.  
Set `VCAL_MED` = 100 and `VCAL_HIGH` to inject the equivalent charge produced by a MIP in 150 μm of silicon (12000 e-):

* VCAL_HIGH = 2500 if SEL_CAL_RANGE = 0
* VCAL_HIGH = 1300 if SEL_CAL_RANGE = 1

1. Adjust the global threshold to 2000 e- for fresh modules or 3000 e- for irradiated modules:
    * `CMSITminiDAQ -f CMSIT_RD53B.xml -c thradj`
    * Update the `GDAC` values in `CMSIT_RD53B.xml`
1. Equalize the threshold setting DoNSteps = 0:
    * `CMSITminiDAQ -f CMSIT_RD53B.xml -c threqu`
1. Find the best latency and the best setting for `CAL_EDGE_FINE_DELAY`:
    * `CMSITminiDAQ -f CMSIT_RD53B.xml -c injdelay`
    * Update the values for TriggerConfig and `CAL_EDGE_FINE_DELAY` in `CMSIT_RD53B.xml`
    * If the newly found best value for `CAL_EDGE_FINE_DELAY` differs more than 4 DAC with respect to the previous one,
      redo the threshold adjustment (and optionally the equalization) and update the `GDAC` values in `CMSIT_RD53B.xml` before running a noise scan.
1. Mask the noisy pixels:
    * `CMSITminiDAQ -f CMSIT_RD53B.xml -c noise`
1. Check the distribution of the thresholds:
    * `CMSITminiDAQ -f CMSIT_RD53B.xml -c scurve`
1. Repeat this procedure again for a target threshold of 2000 e- for irradiated modules.

Then, assuming a target threshold of 1000 e-:

1. Adjust the global threshold to 1200 e-:
    * `CMSITminiDAQ -f CMSIT_RD53B.xml -c thradj`
    * Update the `GDAC` values in `CMSIT_RD53B.xml`
1. Equalize the threshold setting `DoNSteps` = 1:
    * `CMSITminiDAQ -f CMSIT_RD53B.xml -c threqu`
1. Find the best setting for `CAL_EDGE_FINE_DELAY`:
    * `CMSITminiDAQ -f CMSIT_RD53B.xml -c injdelay`
    * Update the value for `CAL_EDGE_FINE_DELAY` in `CMSIT_RD53B.xml`
    * If the newly found best value for `CAL_EDGE_FINE_DELAY` differs more than 4 DAC with respect to the previous one, 
      redo the threshold adjustment (and optionally the equalization) and update the `GDAC` values in `CMSIT_RD53B.xml` 
      before running a noise scan.
1. Mask the noisy pixels:
    * `CMSITminiDAQ -f CMSIT_RD53B.xml -c noise`
1. Check the distribution of the thresholds:
    * `CMSITminiDAQ -f CMSIT_RD53B.xml -c scurve`
1. Adjust the global threshold to 1000 e-:
    * `CMSITminiDAQ -f CMSIT_RD53B.xml -c thradj`
    * Update the `GDAC` values in `CMSIT_RD53B.xml`
1. Without re-running the threshold equalization, mask the noisy pixels:
    * `CMSITminiDAQ -f CMSIT_RD53B.xml -c noise`
1. Run a `pixelalive` setting the appropriate rate for `OccPerPixel `to mask the stuck pixels 
    (e. g. `OccPerPixel` = 0.9 to mask all pixels that detect less than 90 % of the injected signals):
    * `CMSITminiDAQ -f CMSIT_RD53B.xml -c pixelalive`
1. Check the distribution of the thresholds:
    * `CMSITminiDAQ -f CMSIT_RD53B.xml -c scurve`
1. Check the in-time threshold by setting `nTRIGxEvent `= 1:
    * `CMSITminiDAQ -f CMSIT_RD53B.xml -c scurve`
1. Once the target threshold is reached, run a `gain scan`:
    * `CMSITminiDAQ -f CMSIT_RD53B.xml -c gain`

If the aim is to tune to a higher threshold, replace the tuning procedure in steps
7 to 10 (1200 e-) with the desired target threshold and skip steps 12, 13 and 15.
