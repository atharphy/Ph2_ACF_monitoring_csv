# Configuration files

In regular scenarios, running IT tests on Ph2_ACF requires two types configuration files:

1. [**XML configuration file**](#xml-configuration-file):
    - Describes the testing hardware: backend boards (FC7), hybrids, frontend chips (RD53)
    - Stores the front-end and back-end register/setting values set by the user
    - Stores the scan/calibration parameters set by the user
    - Sets monitoring settings and communication parameters
    - **Does not get updated by Ph2_ACF** (should be updated manually)
2. [**txt configuration files**](#txt-configuration-files):
    - Describe the frontend chips (multiple txt files needed when testing modules/multiple SCC at the same time)
    - Describe the addresses, default values and bit sizes of the chip registers (XML overrides the txt values)
    - Configure each pixel of the chip: enable/mask a pixel for the injection/readout/HitOr, set its TDAC (threshold trimming) value
    - **Get updated automatically by Ph2_ACF** (but can also be edited manually)

## XML configuration file

Main configuration file describing the setup, scan and monitoring settings (can be outsourced in another file):

* `Ph2_ACF/settings/CMSIT_RD53A.xml` for RD53A
* `Ph2_ACF/settings/CMSIT_RD53B.xml` for RD53B

### Chip definition

The chip is defined as follows in the XML, followed by the chip settings:

* For RD53A:
  ```xml
  <RD53A Id="0" enable="1" Lane="0" configFile="CMSIT_RD53A.txt" RxGroups="NNN1" RxPolarity="0" TxGroup="2" TxChannel="0" TxPolarity="0" Comment="RD53A 50x50">
  ```
* For RD53Bv1 (CROCv1):
  ```xml
  <RD53Bv1 Id="15" enable="1" Lane="0" eFuseCode = "0" configFile="CMSIT_RD53Bv1.txt" RxGroups="NNN1" RxPolarity="0" TxGroup="2" TxChannel="0" TxPolarity="0" Comment="RD53B 50x50">
  ```
* For RD53Bv2 (CROCv2):
  ```xml
  <RD53Bv2 Id="15" enable="1" Lane="0" eFuseCode = "0" configFile="CMSIT_RD53Bv2.txt" RxGroups="NNN1" RxPolarity="0" TxGroup="2" TxChannel="0" TxPolarity="0" Comment="RD53B 50x50">
  ```
The corresponding configuration txt files should be copied from the `Ph2_ACF/settings/RD53Files` directory to the working directory and set to the `configFile` setting accordingly.
Note that for RD53Bv2 chips, the correct `eFuseCode` must be entered from the [database](https://cmsdca.cern.ch/trk_cmsr/construct/parts/).

For modules, one has to define 2 or 4 chips in the XML file, depending on the module type.
They are defined exactly the same as written above, only that one has to set a correct `Id` and `Lane` for each chip.
Typical `Id` and `Lane` mappings can be found [here](ModuleTesting.md#lane-mapping)

#### Data merging

```xml
<LaneConfig primary="1" 
            outputLanes="0001"         ⟵ [4th ch, 3rd ch, 2nd ch, 1st ch]
            singleChannelInputs="0000"
            dualChannelInput="0000"
/>
```

* primary (1/0): says whether the chip is the master
* outputLanes (0-4): says which output lanes are enabled and how they are mapped
   * For example:  
      `outputLanes="0001"` ➜ only one lane is enabled and it’s mapped to the rst line  
      `outputLanes="4321"` ➜ all four lanes are enabled and they are mapped in the natural way  
      `outputLanes="1234"` ➜ all four lanes are enabled and they are mapped in the reverse way  
* `singleChannelInputs (1/0)`: says which input channel is enabled as single lane
* `dualChannelInput (1/0)`: says which input channel is enabled as “double-channel” i.e. bonded
   * For example:  
     `dualChannelInput="0101"` ➜ the first and third channels are bonded

!!! info "Running CROC SCCs in KSU hybrid ports 2 and 3"
    As the Aurora lanes are inverted w.r.t. the RD53A SCC, only CROC lanes 2 and 3 are connected for these connectors.
    Set `outputLanes` to `1234` for this module.

#### Chip registers

The XML file can configure the readout chip registers.
**Only the readout chip registers in the XML file will be downloaded to the chip.**
The only exception are the registers configuring the analog frontend, which are always downloaded to the chip.
For example:
1. In the `Settings` section of the XML file, the user writes:  
```xml
<Settings
    VCAL_HIGH = "2000"
/>
```
then, the value 2000 will be downloaded to the chip register `VCAL_HIGH`

2. In the `Settings` section of the XML file the user writes:  
```xml
<Settings
    VCAL_HIGH = ""
/>
```
then, the value present in the [txt file](#txt-configuration-files) will be downloaded to the chip register `VCAL_HIGH`

3. If there is no mention of `VCAL_HIGH` in the `Settings` section of the XML file, then nothing will be downloaded to the `VCAL_HIGH` register

This very same behaviour works also for the Global settings.

**The registers configuring the analog frontend are:**

| RD53A            | CROC                |
| ---------------- | ------------------- |
| `PA_IN_BIAS_LIN` | `DAC_PREAMP_L_LIN`  |
| `FC_BIAS_LIN`    | `DAC_PREAMP_R_LIN`  |
| `KRUM_CURR_LIN`  | `DAC_PREAMP_TL_LIN` |
| `LDAC_LIN`       | `DAC_PREAMP_TR_LIN` |
| `COMP_LIN`       | `DAC_PREAMP_T_LIN`  |
| `REF_KRUM_LIN`   | `DAC_PREAMP_M_LIN`  |
| `Vthreshold_LIN` | `DAC_FC_LIN`        |
|                  | `DAC_KRUM_CURR_LIN` |
|                  | `DAC_REF_KRUM_LIN`  |
|                  | `DAC_COMP_LIN`      |
|                  | `DAC_COMP_TA_LIN`   |
|                  | `DAC_GDAC_L_LIN`    |
|                  | `DAC_GDAC_R_LIN`    |
|                  | `DAC_GDAC_M_LIN`    |
|                  | `DAC_LDAC_LIN`      |

For CROC chips, some frontend registers have `_L_`, `_R_`, or `_M_` in their names.
These correspond to different parts of the pixel matrix: `L` corresponds to the two leftmost columns, `R` – to the two rightmost columns, and `M` – to all other pixels. This allows setting different values to the edge columns of the pixel array w.r.t. other columns.

![`L`, `M`, `R` pixel column arrangement](images/L_M_R.png){width=300}

Some registers must be present in the XML file in order to make the chip working properly.
They are typically under the `Global` settings.

**Compulsory `Global` settings:**
```xml
<Global
    TriggerConfig     = "136"
    EN_CORE_COL_0     = "65535"
    EN_CORE_COL_1     = "65535"
    EN_CORE_COL_2     = "65535"
    EN_CORE_COL_3     = "63"
    SEL_CAL_RANGE     = "0"     ➜ choose between 5 (0) or 10 (1) electrons per VCal
    HIT_SAMPLE_MODE   = "1"     ➜ asynchronous (0) or synchronous (1) hit detection
    CDR_CONFIG_SEL_PD = "0"     ➜ detect PLL phase on both (0) or one clock edge (1)
/>
```

#### Chip monitoring

The chip **monitoring setup** is described [here](Monitoring.md).

### Scan settings

!!! info "Outsourcing scan settings"
    The `<settings>` section of the main XML file can be moved to a separate file to disentagle setup and scan configuration. See `-s` flag in `CMSITminiDAQ`

```xml
<Setting name="nEvents">     100 </Setting> 
<Setting name="nEvtsBurst">  100 </Setting>
```

#### Injection

```xml
<Setting name="nTRIGxEvent"> 10 </Setting> <!-- 0: #bunch crossings readout at every trigger -->
<Setting name="INJtype">      1 </Setting> <!-- 0: no injection 
                                                1: analog 
                                                2: digital 
                                                3: self-trigger
                                                4: custom from txt 
                                                5: X-talk coupled pixels (analog) 
                                                6: X-talk decoupled pixels (analog) -->
```

#### Masks and regions

```xml
<Setting name="ResetMask">         0 </Setting> <!-- 0: leave mask as is; 1: enable all channels -->
<Setting name="ResetTDAC">        -1 </Setting> <!-- -1: leave TDAC as is; >=0: set TDAC to value -->
<Setting name="DoDataIntegrity">   0 </Setting> <!-- Used in Pixelalive -->
  
<Setting name="ROWstart">          0 </Setting> <!-- Select interesting region, all the rest is masked -->
<Setting name="ROWstop">         335 </Setting>
<Setting name="COLstart">          0 </Setting>
<Setting name="COLstop">         431 </Setting>
```

#### Latency scan

Used in [`latency`](calibrations/LatencyScan.md), [`injdelay`](calibrations/InjectionDelayScan.md), and [`clkdelay`](calibrations/ClockDelayScan.md) scans

```xml
<Setting name="LatencyStart">   0 </Setting>
<Setting name="LatencyStop">  511 </Setting>
```

#### VCAL settings

Used in [`scurve`](calibrations/SCurve.md), [`gain`](calibrations/GainScan.md), [`gainopt`](calibrations/GainOptimization.md) and [`thradj`](calibrations/ThresholdAdjust.md) calibrations

```xml
<Setting name="VCalHstart">   100 </Setting>
<Setting name="VCalHstop">   1100 </Setting>
<Setting name="VCalHnsteps">   50 </Setting>
<Setting name="VCalMED">      100 </Setting>
```

#### Gain Optimization

Used in [`gainopt`](calibrations/GainOptimization.md) calibration

```xml
<Setting name="TargetCharge">  10000 </Setting>
<Setting name="KrumCurrStart">     0 </Setting>
<Setting name="KrumCurrStop">    210 </Setting>
```

#### Threshold Equalisation

Used in [`threqu`](calibrations/ThresholdEqualization.md) calibration

```xml
<Setting name="TDACGainStart">  140 </Setting>
<Setting name="TDACGainStop">   140 </Setting>
<Setting name="TDACGainNSteps">   0 </Setting>
<Setting name="DoNSteps">         0 </Setting>
```

#### Threshold Adjustment and Minimization

Used in [`thradj`](calibrations/ThresholdAdjust.md) and [`thrmin`](calibrations/ThresholdMinimization.md) calibrations

```xml
<Setting name="ThrRelStart">      -70 </Setting> <!-- used in thrmin and thdadj -->
<Setting name="ThrAmplitude">      50 </Setting> <!-- used in thrmin and thdadj -->
<Setting name="TargetThr">       2000 </Setting> <!-- used in thradj calibration -->
<Setting name="TargetOcc">       1e-6 </Setting> <!-- used in thrmin calibration -->
<Setting name="MaxMaskedPixels">    1 </Setting> <!-- used in thrmin calibration -->
```

* In a [`thrmin`](calibrations/ThresholdMinimization.md) run, whose purpose is to find the lowest threshold based on noise occupancy, the threshold is lowered within the range `[ThrStart, ThrStop]` in such a way to have an average occupancy that meets `TargetOcc`

#### Pixelalive and Noise masking

Used in [`pixelalive`](calibrations/PixelAlive.md) and [`noise`](calibrations/Noise.md) scans

```xml
<Setting name="OccPerPixel">   1e-4 </Setting> <!-- used in pixelalive and noise -->
<Setting name="UnstuckPixels">    0 </Setting> <!-- used in pixelalive -->
```

* In a [`noise`](calibrations/Noise.md) run, whose purpose is to find noisy pixels, any pixel which has an occupancy greater than `OccPerPixel` is consequently masked
* In a [`pixelalive`](calibrations/PixelAlive.md) run, whose purpose is to test the pixels and find out whether there are dead ones, any pixel which has an occupancy smaller than `OccPerPixel` is consequently masked

#### Voltage Tuning

Used in [`voltagetuning`](calibrations/VoltageTuning.md) calibration

```xml
<Setting name="VDDDTrimTarget">     1.20 </Setting>
<Setting name="VDDATrimTarget">     1.20 </Setting>
<Setting name="VDDDTrimTolerance">  0.02 </Setting>
<Setting name="VDDATrimTolerance">  0.02 </Setting>
```

#### Data Readback Optimization

Used in [`datarbopt`](calibrations/DataReadBackOptimisation.md) calibration

```xml
<Setting name="TAP0Start">    0 </Setting>
<Setting name="TAP0Stop">  1023 </Setting>
<Setting name="TAP1Start">    0 </Setting>
<Setting name="TAP1Stop">   511 </Setting>
<Setting name="InvTAP1">      1 </Setting>
<Setting name="TAP2Start">    0 </Setting>
<Setting name="TAP2Stop">   511 </Setting>
<Setting name="InvTAP2">      0 </Setting>
```

#### Bit Error Rate Test

Used in [`bertest`](calibrations/BERTest.md) calibration

```xml
<Setting name="chain2Test">    0 </Setting>
<Setting name="byTime">        1 </Setting>
<Setting name="framesORtime"> 10 </Setting>
```

#### Generic DAC-DAC scan

Used in [`genericdac`](calibrations/GenericDacDac.md) scan

```xml
<Setting name="RegNameDAC1"> user.ctrl_regs.fast_cmd_reg_5.delay_after_inject_pulse </Setting>
<Setting name="StartValueDAC1">       28 </Setting>
<Setting name="StopValueDAC1">        50 </Setting>
<Setting name="StepDAC1">              1 </Setting>
<Setting name="RegNameDAC2">   VCAL_HIGH </Setting>
<Setting name="StartValueDAC2">      300 </Setting>
<Setting name="StopValueDAC2">      1000 </Setting>
<Setting name="StepDAC2">             20 </Setting>
```

#### VTRX scan

Used in [`vtrx`](calibrations/VTRxScan.md) scan

```xml
<Setting name="VTRxBiasStart">        40 </Setting>
<Setting name="VTRxBiasStop">         56 </Setting>
<Setting name="VTRxBiasStep">          1 </Setting>
<Setting name="VTRxModulationStart">  24 </Setting>
<Setting name="VTRxModulationStop">   40 </Setting>
<Setting name="VTRxModulationStep">    1 </Setting>
```

#### LpGBT Eye Opening scan

Used in [`eye`](calibrations/LpGBTeye.md) scan

```xml
<Setting name="LpGBTattenuation">  0 </Setting>
```

#### Further scan settings

```xml
<Setting name="DoOnlyNGroups">  0 </Setting> <!-- = 0: run on all pix.
                                                  > 0: run a subset of groups
                                                  useful to speed up scans and calibrations -->

<Setting name="DisplayHisto">   0 </Setting> <!-- = 0: don't display histograms on screen
                                                  = 1: display histograms on screen -->

<Setting name="UpdateChipCfg">  1 </Setting> <!-- = 0: don't update chip config file
                                                  = 1: update chip config file -->

<Setting name="DisableChannelsAtExit"> 1 </Setting>

<Setting name="StopIfCommFails"> 1 </Setting> <!-- = 0: don't exit if some AURORA lanes are down
                                                   = 1: exit if some AURORA lanes are down -->
```

#### Expert settings

```xml
<Setting name="ShowRunProgress">       1 </Setting> <!-- whether to show the run progress -->

<Setting name="DoSplitByBoardHybrid">  0 </Setting> <!-- whether to split the ROOT file in sub-files,
                                                         each containing the data of one Board&Hybrid -->

<Setting name="DataOutputDir">           </Setting> <!-- set the output directory, default = ./Results/ -->

<Setting name="SaveBinaryData">        0 </Setting> <!-- whether to save the raw data in binary format -->

<Setting name="nHITxCol">              1 </Setting> <!-- number of simultaneously injected pixels
                                                         per column (must be a divider of chip rows) -->

<Setting name="InjLatency">           32 </Setting> <!-- time difference between injection and trigger
                                                         in terms of 100ns period (up to 4095) -->

<Setting name="nClkDelays">         1300 </Setting> <!-- delay between two consecutive injections
                                                         in terms of 100 ns period (up to 4095) -->
```

### Register block

!!! info "Exporting `Register` block"
    To be noted that the `Register` block can be put in a separate XML file:

    * Add the file name within the `BeBoard` node of your main XML file in this way:
    ```xml
    <configuration file_name="RegisterFile.xml"/>
    ```
    
    * The RegisterFile.xml should contain:  
    ```xml
    <?xml version="1.0" encoding="utf-8"?> 
    <BeBoardRegister>
    ... the Register section ...
    </BeBoardRegister>
    ```

#### GTX polarity

```xml
<Register name="gtx_rx_polarity">
  <Register name="fmc_l12"> 1024 </Register> <!-- 12-bit number where for each bit
                                                  0=default value, 1=invert polarity
                                                  on one of the GTX lanes -->

  <Register name="fmc_l8">     0 </Register> <!-- 8-bit number where for each bit
                                                  0=default value, 1=invert polarity
                                                  on one of the GTX lanes -->

  <Register name="cmd_strobe"> 0 </Register> <!-- whether to download new polarity values -->
</Register>
```

#### Trigger

```xml
<Register name="fast_cmd_reg_2">
  <Register name="trigger_source"> 2 </Register> <!-- 1=IPBus
                                                      2=Test-FSM (default)
                                                      3=TTC
                                                      4=TLU (via DIO5 board)
                                                      5=External
                                                      6=Hit-Or
                                                      7=User-defined frequency -->
  <Register name="HitOr_enable_l12"> 0 </Register>
</Register>
```

Important points:

* Setting `trigger_source` = 2 can provide not only internal triggers, but the firmware can also send a trigger if a HitOr signal if present. In that case, HitOr signals have to be enabled both in the chip and firmware.
* `HitOr_enable_l12`: Enable HitOr port on the KSU FMC: set `trigger_source` to proper value then this register, `1` enable HitOr from left-most connector (on the bottom row of MiniDP connectors), `2` enable HitOr from right-most connector
* You can read more about triggering using HitOr in these sections: [Self-trigger](SelfTrigger.md), [External HitOr trigger](ExternalTriggers.md#triggering-on-the-croc-hitor)

#### Clock

```xml
<Register name="ext_tlu_reg2">
  <Register name="tlu_delay"> 0 </Register> <!-- Needed to align TLU trigger tag with FC7 clock
                                                 (values 0-15 in unit of 1/16 of 40 MHz clk cycle) -->
</Register>

<Register name="reset_reg">
  <Register name="ext_clk_en"> 0 </Register> <!-- 0=Internal clock (default)
                                                  1=External Clock -->
</Register>
```

You can read more about using external clock [here](ExternalTriggers.md#dio5-and-tlu).

#### DIO5 and TLU registers

```xml
        <Register name="ext_tlu_reg1">
          <Register name="dio5_ch1_thr"> 128 </Register>
          <Register name="dio5_ch2_thr">  40 </Register>
        </Register>

        <Register name="ext_tlu_reg2">
          <Register name="dio5_ch3_thr"> 128 </Register>
          <Register name="dio5_ch4_thr"> 128 </Register>
          <Register name="dio5_ch5_thr"> 128 </Register>

          <Register name="tlu_delay"> 0 </Register> <!-- Needed to align TLU trigger tag with FC7 clock
                                                         (values 0-15 in unit of 1/16 of 40 MHz clk cycle) -->
        </Register>
```

You can read more about using DIO5 and TLU [here](ExternalTriggers.md#dio5-and-tlu).

#### External clock and triggers for `physics`

```xml

        <Register name="reset_reg">
          <Register name="ext_clk_en"> 0 </Register> <!-- 0: (default) internal clock
                                                          1: external clock -->
        </Register>

        <Register name="fast_cmd_reg_3">
          <Register name="triggers_to_accept"> 10 </Register> <!-- used in physics: total number of 
                                                                   triggers to readout (0 = no limit) -->
        </Register>

        <Register name="fast_cmd_reg_7">
          <Register name="autozero_freq"> 1000 </Register>
          <!-- In units of 10MHz clk cyles -->
        </Register>
```

You can read more about the `physics` scan [here](calibrations/Physics.md).

## txt configuration files

The configuration file describing each chip in the test setup.
You will need as many files, as you have chips (e.g., 4 txt files when testing a single quad module).
The default files can be found here:

* `Ph2_ACF/settings/RD53Files/CMSIT_RD53A.txt` for RD53A
* `Ph2_ACF/settings/RD53Files/CMSIT_RD53Bv1.txt` for RD53Bv1 (CROCv1)
* `Ph2_ACF/settings/RD53Files/CMSIT_RD53Bv2.txt` for RD53Bv2 (RD53C/CROCv2)

### txt file structure

The txt files consist of two main parts:

1. Chip register description
2. Pixel configuration

#### Chip register description

The txt file usually lists all the chip registers, describing their addresses, default values, currently set values, and bit sizes. For example:

```
*-----------------------------------------------
* RegName    Addr   Defval    Value     BitSize
*-----------------------------------------------
PIX_PORTAL   0x00   0d0       0d0          16
REGION_COL   0x01   0d0       0d0           8
REGION_ROW   0x02   0d0       0d0           9
PIX_MODE     0x03   0b01010   0b01010       5
. . .
```

However, these values do not get downloaded to the chip automatically (apart from the analog frontend registers), unless they are also listed in the [XML file](#xml-configuration-file), as described [above](#chip-registers).
Additionally, the values set in the XML file override the values given in the txt file.
Therefore, it is generally recommended to set the desired register values in the XML file, and leave the chip register section of the txt file as is.
Some [IT calibrations](calibrations/index.md) in Ph2_ACF will update the txt file automatically, if necessary.

The most important chip registers were already listed in "[Chip Registers](#chip-registers)" section of the XML file description above.

#### Pixel configuration

The txt file also allows configuring each pixel, more specifically:

* Enable/disable the readout of the pixel (`ENABLE`)
* Enable/disable the HitOr output of the pixel (`HITBUS`)
* Enable/disable the injection circuit of the pixel (`INJEN`)
* Set the TDAC (threshold trimming bits) of the pixel (`TDAC`)

The settings are arranged column-wise:

```
*---------------------------------------------------------------------------------------------
PIXELCONFIGURATION
*---------------------------------------------------------------------------------------------
COL    000                             ➜ column number
ENABLE 1,1,1,1,1,1,1,1, . . .          ➜ each number enables a different pixel in this column
HITBUS 0,0,0,0,0,0,0,0, . . .          ➜ same but for HitOr
INJEN  0,0,0,0,0,0,0,0, . . .          ➜ same but for injections
TDAC   16,16,16,16,16,16,16,16, . . .  ➜ same but for TDAC

COL    001
ENABLE . . .
. . .
```

It is technically possible to edit these per-pixel configuration settings to create some interesting injection/readout/trigger patterns.
E.g., In order to use the [HitOr self-trigger](SelfTrigger.md), all the HITBUS values should be changed to 1 (apart from the masked pixels, otherwise they may still keep producing the triggers).
However, for more complicated patters, this might become very tedious and is not generally recommended to do by hand. There are scripts available to do this (see next section).

#### Pixel mask manipulation scripts

Some complex masking patterns can be generated using the [`pythonUtils/pyUtilsIT/ManipulateITchipMask.py`](https://gitlab.cern.ch/cmsinnertracker/Ph2_ACF/-/blob/master/pythonUtils/pyUtilsIT/ManipulateITchipMask.py?ref_type=heads) script.
Using this script, users can specify an enable/injection file of the form:
```
row 0 col 130 en 
row 1 col 130 inj
row 2 col 130 en 

row 0 col 129 en
row 1 col 129 en
row 2 col 129 en

row 0 col 131 en
row 1 col 131 en
row 2 col 131 en
```

Users can also specify the group number, i.e. pattern number among the several possible `(0, NROWS - 1).
A png image is also saved to show the chosen pattern.
This is very useful, e.g., for [crosstalk (X-talk) studies](calibrations/XTalk.md).

#### Copying the pixel masks

The pixel masks can be copied from one txt file to another using the [`pythonUtils/pyUtilsIT/CopyITchipMask.py`](https://gitlab.cern.ch/cmsinnertracker/Ph2_ACF/-/blob/master/pythonUtils/pyUtilsIT/CopyITchipMask.py?ref_type=heads) script.
It allows users to port the pixel mask from, e.g., a txt file from an older version of Ph2_ACF, to a new txt from a newer version of Ph2_ACF where the description of the chip registers may change but the mask description will always stay the same.