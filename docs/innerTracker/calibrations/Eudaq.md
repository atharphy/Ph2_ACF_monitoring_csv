# EUDAQ

## Purpose
The EUDAQ mode has a very similar purpose to the [Physics](Physics.md) scan, which is actual physics data-taking during test beams.
However, EUDAQ itself is not really a part of Ph2_ACF. Instead, it is a Generic Multi-platform Data Acquisition Framework.
So, the EUDAQ mode in Ph2_ACF uses the EUDAQ framework to carry out the measurements.

You can read more about the EUDAQ famework [here](https://eudaq.github.io/).

https://gitlab.cern.ch/dinardo/TestbeamAnalysis

## Usage

To use the EUDAQ mode, one needs special flags to be turned on in the `setup.sh` file before compilation, namely:
```bash
export CompileWithEUDAQ=true
```
Further documentation related to package installation can be found [here](https://gitlab.cern.ch/dinardo/TestbeamAnalysis).

### Run command:
```bash
CMSITminiDAQ -f CMSIT.xml -c eudaq --eudaqRunCtr=tcp://localhost:44000
```
Here `eudaqRunCtr` allows to specify the IP address of the run-controller.

### Config file

Put CMSIT.cfg in Corry output directory. `cfg` can contain the (optional) sections:
```
[sensor.geometry]
pitch_hybridId0_chipId1 = "RD52A 25x100origR0C0 quad"

[sensor.calibration]
fileName_hybridId0_chipId1                = "../../Run000000_Gain.root"
slopeVCal2Electrons_hybridId0_chipId1     = 11.67
interceptVCal2Electrons_hybridId0_chipId1 = 64

[sensor.selection]
charge_hybridId0_chipId1        = 1500
triggerIdLow_hybridId0_chipId1  = 4
triggerIdHigh_hybridId0_chipId1 = 7

[converter.settings]
exitIfOutOfSync = true
```

Here, `fileName_hybridId0_chipId1` can be either an absolute or a relative path to the output directory.  
`hybridId` and `chipId` are needed to identify the chip in the data: e.g. EUDAQ shows `RD53` as plane "<t style="color: blue">1</t><t style="color: red">13</t>" ➜ `hybridId` = <t style="color: blue">0</t> and `chipId` = <t style="color: red">13</t>, or if it shows "<t style="color: blue">3</t><t style="color: red">04</t>" ➜ `hybridId` = <t style="color: blue">2</t> and `chipId` = <t style="color: red">04</t>

<code style="color: red">[sensor.geometry]</code>: needed to override the geometry specified in the data.
Available labels are:
* `25x100origR0C0`
* `25x100origR1C0`
* `50x50`
* together with `RD53A`, `CROC`, and `dual` or `quad` in case we are dealing dual or quad modules

<code style="color: red">[sensor.calibration]</code>: needed to specify the calibration file and the $V_{\text{Cal}}$ to electrons linear conversion coefficients.

<code style="color: red">[sensor.selection]</code>: needed to specify the software **charge-threshold** and **cut** on `triggerId` (a.k.a. bunch crossing).

<code style="color: red">[converter.settings]</code>: allows to set the flag for stopping the program in case an **out-of-sync** is detected.