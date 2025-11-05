# Description of 2S test results

This documentation is adapted from the [2S testing with Ph2_ACF tutorial](https://indico.cern.ch/event/1540157/). This focuses on the tests performed during module production with the goal of qualify the modules.

**For production testing, official releases and tools as GIPHT should be used.**

For other testing the main commands after installing the software are:

```
cd Ph2_ACF  # change the directory accordingly
source setup.sh 
```

If not already done, insert the correct machine and IP address in the
xml file located in the settings directory that you are going to use. The recommended XML is [settings/2S_Module.xml](../../settings/2S_Module.xml)

```
<connection id="board" uri="localhost://192.168.0.12:50001" address_table="<file://settings/address_tables/uDTC_OT_address_table.xml>"
```

List firmwares on SD card:

``` fpgaconfig -c settings/MyXML.xml -l ```

To load a new firmware: The firmware are stored in
<https://udtc-ot-firmware.web.cern.ch/>. Be sure the version you are
downloading is compatible with the code version you are using, the
module type, the CIC version etc.

```
fpgaconfig -c settings/MyXML.xml -f PATH_TO_FILE -i FW_NICKNAME
fpgaconfig -c settings/MyXML.xml -i FW_NICKNAME
```

To load an existing firmware just use the second line above.

Blue light on FC7 blinks at 1Hz if firmware loaded properly.

To run a test

```
runCalibration -f settings/MyXML.xml   -c CALIBRATION_NAME
```

To list the availaible calibrations

```
runCalibration -h
```

## Test result description
The description below is for the standard `2SfullTest` that is a very comprehensive set of measurements. The `2SquickTest` is a subset of that.

When running, two files are created:
- The `Results.root` that contains all the calibration results and the metadata
- The monitoring that contains the variables that we  would like to monitor, for example temperatures, voltages, in some cases also currents

### Results description

#### Metadata
When you open the file, the main folder inside is the `Detector` folder, containing metadata.

Metadata are storing what in root is called the object string and the reason for that is that if you store anything else that is not an object you need to create a dictionary. With a string we can basically store everything that we need.


For instance, `Username_Detector` contains the user name of the computer where the tests is carried out and `HostName_Detector` is the name of the computer.

`GitCommitHash_Detector` and `GitTag_Detector` are used verify that the user is in the right tag and commit for production. These fields will be used by the Potato grading software.

The `CalibrationName_Detector` is the name of the calibration, for instance `2SfullTest` to track which type of test you run.

The `InitialDetectorConfiguration_Detector` is basically a copy of the `HWDescription` section of the ` settings/MyXML.xml` used to run the test. The `FinalDetectorConfiguration_Detector` is similar to the `Initial` one with possible updated values due to the performed calibrations.

In the `CalibrationStartTimestamp_Detector` and `SubCalibrationNameAndType_Detecor` there is information about the time of each calibration. The calibration start time timestamp is stored as soon as you start configuring the module. Then since multiple steps are done, the different start of the steps (SubCalibration) are listed next to the steps. This is not really needed for any particular information for the QA but   it's really to keep track of the time for developers. The `CalibrationstubTimeStamp_Detector` is the time stamp for when the calibration is finished.

 
There is a minor difference of a few seconds in what you see in the metadata and in the printout on the terminal because in the beginning time stamp in the metadata the time used to read the configurations is not considered.

After the `Detector` folder we have the `Board`. Here we have three metadata:
- `D_NameID_Board` is the IP address
- `D_InitialBoardConfiguration_Board` is the XML for the board configuration, typically what you have in [settings/BeBoardFiles/uDTC_registers_2S.xml](../../settings/BeBoardFiles/uDTC_registers_2S.xml)
- `D_FinalBoardConfiguration_Board` is the same as above with possible updated values depending on the performed calibration

This is mainly for debugging.

After the `Board`, there is the `OpticalGroup`.
The `NameId` is empty if you run manually while if you run with `GIPHT` the module ID will be stored. Calibrations should always be performed with `GIPHT` or the Burnin box controller.

At the optical group level we have two chips, the LpGBT and the VTRX.
In the `InitialLpGBTConfiguration` (`FinalLpGBTConfiguration`) the starting (final) values of all the registers are stored. 
The `LpGBTFuseId` and `VTRxFuseId` are the IDs stored in the chip. 

The `IsLpGBTCalibrated` is set to 1 if the calibration data is found in [the calibration file](../../settings/lpGBTFiles/lpgbt_calibration.csv) and used for that LpGBT.

After the `OpticalGroup`, we move to the `Hybrid` level. A fully working module has two hybrids.
The `NameId` is used to store the Hybrid ID. Similar to the optical group, there is no direct interaction with the DB in Ph2_ACF and it will be filled at a later stage in the analysis and grading procedure. 

Each hybrid has a CIC chip for which three metadata are stored: `CICFuseId` and the `InitialCICConfiguration` and `FinalCICConfiguration` with the starting and final 
values of all chip registers.

Every hybrid contains 8 CBC chips. There is a directory for each of them containg the same metadata `NameId` with the fuse ID, `InitialReadoutChipConfiguration` and `FinalReadoutChipConfiguration` with the registers.

#### Calibrations 

The first step is the configuration where we just load all the registers into the various chips. Basically we just take the information that are stored in the values configuration file and we just load them into the chips.

The other steps are more elaborate and produce result plots described below.

##### TuneLpGBTVref - OpticalGroup

Vref is basically the reference voltage for the LpGBT ADC converter. It's needed for converting into meaningful values the ADC that are read by the LpGBT. This step is  basically the loading (not really a tuning) of a value that the LpGBT group gave us and is stored in [the calibration file](../../settings/lpGBTFiles/lpgbt_calibration.csv). The information for a specific LpGBT can be found by the fuse ID. In this step we retrieve the value from the file and we store it in the chip. No plots are produced.

##### OTVTRxLightYieldScan - OpticalGroup

This test is performed to verify that we are able to change the VTRX settings to increase or decrease the optical power that is emitted.

![VTRx_LightYieldScan](../images/OTtesting/common/VTRx_LightYieldScan.png)

The bias and modulation, shown on the x and y axes of the plot, are two registers of the VTRX that control the laser driver. On the Z axis we have the power in microWatt that is measured by the the SFP connector, the receiver on the FC7.

We obtain the distribution measuring the power varying the bias and modulation.
Moving from the left to the right, the power increases.
For the modulation, when you increase it, you decrease the power. The reason is that basically the bias sets the high level and instead the modulation controls the swing down.

If the power in the x and y direction is not changing, something is going on with the driver controller. Moreover, the optical power that you receive might be lower in case you have some damage on the fibers - less obvious and this will be handled by potato. Very likely you will see other problems.

<details>
  <summary>Known issues</summary>

A distribution like the one below may be due to a problematic SFP connector on the FC7 or dirt in the fibers.

![VTRx_LightYieldScan_buggy](../images/OTtesting/common/VTRx_LightYieldScan_buggy.png)
</details>


##### OTLpGBTEyeOpeningTest - OpticalGroup

The calibration is performed for three different values of the electrical attenuation that the LpGBT applies to the VTRx signal that is proportional to the optical power that is received. The signal can be attenuated to 1/3, 2/3 or not attenuated. Here we show and explain the result for one of the attenuations.

![LpGBT_EyeOpeningScan](../images/OTtesting/common/LpGBT_EyeOpeningScan.png)

The eye opening is a capability of the LpGBT and more info can be found in the manual. A high count rate (z-axis) corresponds to the center of the eye, above the lower part of the signal (voltage) but below the high part of the signal (voltage) - the large yellow area. Outside this range of the signal there is a lower count. The absolute numbers are not easy to interpret. The relevant part is the transition region between the two yellow areas. 
This plot was especially relevant for LpGBT v1 that had known issues with the transistion part moving up and down with the power.

#FIXME - check this part for clarity on the above
[27:54.000 --> 27:56.000]  imagine you have
[27:58.000 --> 28:00.000]  a signal that it
[28:00.000 --> 28:02.000]  looks like
[28:02.000 --> 28:04.000]  that okay
[28:04.000 --> 28:06.000]  okay so what the eye-opening
[28:06.000 --> 28:08.000]  does is that
[28:08.000 --> 28:10.000]  basically plot this one
[28:10.000 --> 28:12.000]  on top of the inverted one
[28:12.000 --> 28:14.000]  mm-hmm
[28:16.000 --> 28:18.000]  that is gonna be look like that
[28:18.000 --> 28:20.000]  okay
[28:20.000 --> 28:22.000]  so this is the eye-opening
[28:22.000 --> 28:24.000]  so basically if you set
[28:26.000 --> 28:28.000]  a point in time here
[28:28.000 --> 28:30.000]  and a threshold here
[28:30.000 --> 28:32.000]  if you take
[28:32.000 --> 28:34.000]  the bar if you check
[28:34.000 --> 28:36.000]  the value of the voltage that you receive
[28:36.000 --> 28:38.000]  is above the threshold
[28:38.000 --> 28:40.000]  below the threshold
[28:40.000 --> 28:42.000]  you will be able to distinguish
[28:42.000 --> 28:44.000]  the ones
[28:44.000 --> 28:46.000]  from the zeros
[28:46.000 --> 28:48.000]  okay
[28:48.000 --> 28:50.000]  so one of the problem the LpGBT
[28:50.000 --> 28:52.000]  is that
[28:52.000 --> 28:54.000]  sometimes
[28:55.000 --> 28:57.000]  you get
[28:58.000 --> 29:00.000]  something that
[29:00.000 --> 29:02.000]  it looks like this
[29:03.000 --> 29:05.000]  so you add the up one
[29:05.000 --> 29:07.000]  that is still fine
[29:07.000 --> 29:09.000]  but the
[29:09.000 --> 29:11.000]  mm-hmm
[29:11.000 --> 29:13.000]  how can I go with
[29:15.000 --> 29:17.000]  the bottom one
[29:17.000 --> 29:19.000]  that I
[29:19.000 --> 29:21.000]  would say basically look like this
[29:21.000 --> 29:23.000]  so that this transition
[29:23.000 --> 29:25.000]  is all the way up here
[29:25.000 --> 29:27.000]  okay
[29:29.000 --> 29:31.000]  where is the problem now
[29:31.000 --> 29:33.000]  so
[29:35.000 --> 29:37.000]  then you probably don't have the transition
[29:37.000 --> 29:39.000]  at the same voltage
[29:39.000 --> 29:41.000]  they are too different
[29:43.000 --> 29:45.000]  I
[29:45.000 --> 29:47.000]  start to have some doubt of what I was saying
[29:47.000 --> 29:49.000]  this is not something that we like
[29:49.000 --> 29:51.000]  too much I guess the main problem
[29:51.000 --> 29:53.000]  is that you have this lowest loop
[29:53.000 --> 29:55.000]  and
[29:55.000 --> 29:57.000]  then since the slope
[29:57.000 --> 29:59.000]  is lower
[29:59.000 --> 30:01.000]  then you have a smaller
[30:01.000 --> 30:03.000]  phase
[30:03.000 --> 30:05.000]  a long slope
[30:05.000 --> 30:07.000]  you have a smaller phase in which you can find
[30:07.000 --> 30:09.000]  the alignment
[30:09.000 --> 30:11.000]  so I think in reality it is actually like this
[30:13.000 --> 30:15.000]  so they cross like that
[30:15.000 --> 30:17.000]  so you have a less room
[30:17.000 --> 30:19.000]  to identify the one
[30:19.000 --> 30:21.000]  from the zeros and that is the main problem
[30:21.000 --> 30:23.000]  and there is where
[30:23.000 --> 30:25.000]  you may have communication issues
[30:27.000 --> 30:29.000]  okay
[30:29.000 --> 30:31.000]  no competition
[30:31.000 --> 30:33.000]  why is so bad
[30:33.000 --> 30:35.000]  but they so
[30:35.000 --> 30:37.000]  what they did is that they kind of correlate
[30:37.000 --> 30:39.000]  so
[30:39.000 --> 30:41.000]  the packet of the
[30:41.000 --> 30:43.000]  LpGBT comes with an error correction
[30:43.000 --> 30:45.000]  in our case
[30:45.000 --> 30:47.000]  we use the effect 5 which means that
[30:47.000 --> 30:49.000]  up to 5 errors we can
[30:49.000 --> 30:51.000]  correct the effect states for
[30:51.000 --> 30:53.000]  forward error corrections
[30:53.000 --> 30:55.000]  so I think what Atlas found out
[30:55.000 --> 30:57.000]  was that
[30:57.000 --> 30:59.000]  you get
[30:59.000 --> 31:01.000]  for some LpGBT
[31:01.000 --> 31:03.000]  you are getting quite large number of
[31:03.000 --> 31:05.000]  forward error correction
[31:05.000 --> 31:07.000]  meaning that you can still recover them
[31:07.000 --> 31:09.000]  but something is going wrong
[31:09.000 --> 31:11.000]  and then
[31:11.000 --> 31:13.000]  they kind of correlated this
[31:13.000 --> 31:15.000]  high number of effect error correction
[31:15.000 --> 31:17.000]  with some problem with the eye opening
[31:17.000 --> 31:19.000]  so that is basically the symptom
[31:19.000 --> 31:21.000]  that is telling you that when you place it
[31:21.000 --> 31:23.000]  into a real module
[31:23.000 --> 31:25.000]  you may have errors
[31:25.000 --> 31:27.000]  in the communication
[31:27.000 --> 31:29.000]  and
[31:29.000 --> 31:31.000]  it is good that you are able to correct them
[31:31.000 --> 31:33.000]  but if you already start with a baseline
[31:33.000 --> 31:35.000]  that you have a lot of
[31:35.000 --> 31:37.000]  error that you can correct
[31:37.000 --> 31:39.000]  if you get more than 5 bits
[31:39.000 --> 31:41.000]  that are flipped into the same packet
[31:41.000 --> 31:43.000]  you cannot correct anymore
[31:43.000 --> 31:45.000]  and this is unrecoverable anymore
[31:47.000 --> 31:49.000]  thanks
[31:55.000 --> 31:57.000]  and


##### OTalignLpGBTinputs - OpticalGroup

To better understand the alignment steps, please refer to the [2S module communication scheme on slide 5](https://indico.cern.ch/event/1540157/contributions/6481541/attachments/3057152/5426570/FRavera_2025_04_28_2Sschool.pdf) where we see the 8 CBCs per side that communicate with one CIC. The CIC (one per side) communicate with the LpGBT and the LpGBT with the FPGA.

#FIXME - minutes 32 - 33 
For the data rate that can be handled in the FPGA, the signal coming from the module is then split in separate components for the two FEH and in L1 (red line) and stub data (blue lines).

This test is used to make sure that the LpGBT understands what the CIC sends.
Basically the LpGBT samples the data received and this test finds the correct sampling phase. If the phase is not correct, sometimes a one can be interpreted as a zero, or vice versa, and therefore the communication will not work. 

This is an automatic procedure done by the LpGBT that has this automatic phase alignment. The test sets the CIC in a state that sends a specific pattern and ask the LpGBT to align, ie to find the best phase for sampling the incoming data from the CIC.

There are three plots associated with this test, identified with `CICtoLpGBT` string. 


![CICtoLpGBT_PhaseAlignmentEfficiency](../images/OTtesting/common/CICtoLpGBT_PhaseAlignmentEfficiency.png)

The *Phase Alignment Efficiency* is obtained repeting the automic phase alignment 100 times (default value that can be configured in the XML) and counting how many times the alignment succeeded. If there are no troubles you should see 100% efficiency. This is telling us how well the automatic procedure on the LpGBT works.

Then we want also to extract the best phase that allow the LpGBT to properly sample the data. This is shown in the next two plots.

![CICtoLpGBT_FoundPhaseDistribution](../images/OTtesting/common/CICtoLpGBT_FoundPhaseDistribution.png)

On the X axis the various lines between CIC and LpGBT are shown: 1 L1, 5 stub lines for the right (R) and left (L) FEHs. On the Y axis the phase. The LpGBT scans phases between 0 to 14, covering two clock cycles. The 15 is an error code that is used by the LpGBT. When repeating the measurement 100 times, we store basically the frequency for which one phase is chosen and this is shown on the Z-axis.

Usually you see bins with roughly 1 (yellow) on one phase and in some cases two phases are picked with a similar frequency. 
If a vertical line of bins with some frequency  is seen, it means that LpGBT was not able to choose any particular phase and that something is going on  with that particular line. The line goes to the connector between the service hybrid and the front end hybrid so something may be wrong with that connector, since the hybrids were already tested and it would be quite unlikely that LpGBT or the CIC are the problem or their connection with the hybrid is the problem.

The best phase (the one with the highest frequency) is chosen from the previous plot, used by the LpGBT and shown in the plot below, one for each line.
![CICtoLpGBT_BestPhase](../images/OTtesting/common/CICtoLpGBT_BestPhase.png)

##### OTalignBoardDataWord - Hybrid
After we have done the alignment of the LpGBT, we align the data word into the FC7. 
Once more, we set the CIC such that it keeps sending data through the lines described in the [OTalignLpGBTinputs - OpticalGroup](#otalignlpgbtinputs---opticalgroup). This time we want to identify the data in the FPGA.

The CIC sends a packet of 8 bits and we need to verify that the 8 bits are properly identified into the FPGA, by identifying the first of the 8 bits.
For instance, if the CIC sends a sequence of 101010 then you need to know that the first one has to be a 1 and then the second one has to be a 0.

To do the alignment, the test tells the FPGA which is the expected pattern.

Once the FPGA receives a packet, it is checked the delay that is needed  such that the first bit of the packet is the first bit of the expected pattern. The delay is called FPGA *bitslips*. The chosen bitslip for each line is shown in the plot below. There is one plot per hybrid. This plots have the purpose to store the found value.

![Board_WordAlignmentBitSlipValues_Hybrid](../images/OTtesting/common/Board_WordAlignmentBitSlipValues_Hybrid.png)

The plot below stores the number of retries. The alignment procedure is tried for a maximum of 10 times in case of failures. Retries can indicate instabilities. The retry number is stored per each line since each line is handled separately.

![Board_WordAlignmentRetryNumbers_Hybrid](../images/OTtesting/common/Board_WordAlignmentRetryNumbers_Hybrid.png)


It is not uncommon to have one or two retries as there are some instabilities when writing some particular registers into the board. That's why we try multiple times. If the test retries 10 times, very likely means that it never manages to align and it would be good to check the connections between the CIC and the LpGBT.

##### OTverifyBoardDataWord - Hybrid

This test is designed to verify that the alignment performed in the previous steps properly succeeded. Once again the CIC is set in the same configuration to send a pattern through each line. The test checks if the pattern sent by the CIC matches the pattern received by the LpGBT.

Two plots (per hybrid) are stored for this test.

One plot contains the number of bits used for the test. For every line, it shows how many bits were tested. The difference between the number of bits we are testing between the stubs and the Level-1 is only for timing purposes.  

The stub line can be implemented directly in the firmware, which is very fast.  

In contrast, the Level-1 implementation would require a major firmware update, and we currently don’t have the resources for that. That’s why you see fewer bits for Level-1.  

However, it’s still on the order of 10⁶ bits — not a small number — but lower than the number of stub bits. This corresponds to the number of tester bits.

![CICtoLpGBT_PatternMatchingTestedBits_Hybrid](../images/OTtesting/common/CICtoLpGBT_PatternMatchingTestedBits_Hybrid.png)

We also have the error rate.  
This value ranges from 0 to 1, where 1 means 100% errors and 0 means no errors.  

From what we have observed so far, this part of the test is very stable.  

An error rate around 10⁻⁶ or 10⁻⁷ might just be a glitch.  
If the error rate is higher than that, check the connections between the hybrids and the connectors, as that might be the cause.


![CICtoLpGBT_PatternMatchingErrorRate_Hybrid](../images/OTtesting/common/CICtoLpGBT_PatternMatchingErrorRate_Hybrid.png)


Now, we are sure that the communication between the CIC and the board works fine.

##### OTCICphaseAlignment - Hybrid
Now, we can begin aligning the CBC with the CIC.  
This step is conceptually similar to the alignment between the CIC and the LpGBT, but it’s a bit more complicated. The goal is to synchronize all the lines between CIC and CBC.

Since the CBC cannot generate an arbitrary test pattern, we inject a few strips to produce a recognizable pattern on the data lines.  
The CIC then uses this pattern to adjust its phase and achieve proper alignment.

The phase alignment logic in the CIC is a simplified version of that used in the LpGBT, since the same block was copied into the CIC design.  
The process is therefore similar:  
- Send a known pattern.  
- The CIC scans phases to find the one that decodes the pattern correctly.

To check stability, the CIC is asked to align 100 times.  
This ensures that the alignment procedure is reliable and repeatable; if the result is consistent across all runs, the alignment is stable.  
At the end of each alignment, we can query whether the procedure succeeded or not.

The resulting plots below resemble those used for the CIC-LpGBT phase alignment and are produced at the hybrid level.

We can plot the efficiency of alignment.  
Each vertical bin represents different elements: one line for the Level-1, 5 lines for the stubs. On the X-Axis we have one column for every CBC.  
On the z-axis, we display the efficiency, ranging from 0 to 1.  
If there are any issues, the efficiency will appear noticeably below 1.  

![CBCtoCIC_LockingEfficiency_Hybrid](../images/OTtesting/2S/CBCtoCIC_LockingEfficiency_Hybrid.png)


We also plot, for each line, the phase and the frequency at which each phase was chosen.  
If you zoom in on the X-axis, you can see, for every CBC, the stub lines and the Level-1 lines all in the same plot. We combined them into a single plot to avoid creating too many separate plots.  

As for the LpGBT, you will often see cases where two phases are essentially equivalent. In these cases, we choose the phase with the highest probability.

![CBCtoCIC_InputPhaseDistribution_Hybrid](../images/OTtesting/2S/CBCtoCIC_InputPhaseDistribution_Hybrid.png)

This phase scan above is performed over two clock cycles.  
For example, if one phase is around 5, the equivalent phase on the next cycle would be 5 + 8 = 13.  
So effectively, there are two working points for each line, and the system will pick one of the two.  

Looking at five phases individually might be misleading, since some of them are equivalent due to the two-cycle scan.  

The error code is represented by the number 15.  
If a value of 15 appears, it means the alignment failed.  
However, we rarely see this because the hybrids and modules we receive are generally good, and the CBCs within the same hybrid have already been tested.  

If an issue does occur, the lock efficiency should reflect it — for example, a 15 in the phase scan would likely correspond to a low or failing lock efficiency.

Then we show the best input phase in a 2D plot.  
On the y-axis, we have the different lines, and on the x-axis, the different CBCs.  
The z-axis represents the best phase — basically the most probable phase for each line and CBC combination.

![CBCtoCIC_BestInputPhases_Hybrid](../images/OTtesting/2S/CBCtoCIC_BestInputPhases_Hybrid.png)


##### OTCICwordAlignment - Hybrid
The next step to be addressed is the CBC's processing of stubs. The goal is to align all lines with the 40MHz clock. 

Now, the CIC can correctly identify ones and zeros coming from the CBC, but the CIC also needs to process the stub information.  
Each CBC sends stubs in a specific format: three lines for the stub address, one and a half lines for the bending, and one line for the error code [(See slide 8)](https://indico.cern.ch/event/1540157/contributions/6481541/attachments/3057152/5426570/FRavera_2025_04_28_2Sschool.pdf).  

The CIC must understand these bits and decide which stubs to actually send, because it cannot send all stubs at once. Each CIC can handle only a limited number of stubs.  

This step is called word alignment because the CIC needs to identify not only the first bit but also its position in the full data stream.  
The procedure is similar to what is done in the FC7 for bit identification. The CBC is set to send a specific pattern and we tell the CIC what to expect. 

This creates a single plot showing the delay applied on each line of the CBC.  
Since the lines are very similar in length, values should be roughly identical.  
If any line shows a value drastically different from the average, it may indicate a problem.  
![CBCtoCIC_WordAlignmentDelay_Hybrid](../images/OTtesting/2S/CBCtoCIC_WordAlignmentDelay_Hybrid.png)

These plots are not used for debugging or QA; they are mainly to store the values chosen. Unlike previous scans, this one only scans a single phase, so there is just one working point for each line.


##### OTCICBX0Alignment - Hybrid
This is the last step of the CBC-CIC alignment. It is more relevant for PS modules where the stub info is sent over 2words but it is performed also for 2S ones even if the stub info is sent into a single word.

Since all CBCs and lines are synchronized, only one of the chip and lines is set to send a pattern and used for the measurement of the BX0 delay. The BX0 delay is measured between a Resync and the reception of the pattern in the CIC.

![CICBX0AlignmentDelay_Hybrid](../images/OTtesting/2S/CICBX0AlignmentDelay_Hybrid.png)

An empty plot shows that the alignment fails. This could be due to a problem on the CBC/CBC line chosen for the alignment or on a problem in the CIC.

##### OTalignStubPackage - OpticalGroup

This is the last alignment step of the stub package. There are 5 stub lines between the CIC and the LpGBT and the stub info is sent following [the scheme on slide 10](https://indico.cern.ch/event/1540157/contributions/6481541/attachments/3057152/5426570/FRavera_2025_04_28_2Sschool.pdf).

There is 1 bit that indicates if the pattern is coming from the CBC or the MPA.
We consider the CBC case. Then we have status bits that indicate errors.  
Next, we have the bunch crossing IDs, which tell you the bunch crossing at which the pattern or packet was sent.  

We also include the number of stubs, indicating how many stubs the packet contains, followed by all the stub data.  
For the time being, ignore the stub information, since there is no alignment between the CBC and the CIC.  

However, the CIC still sends this packet.  
Everything that comes after the stub number is meaningless, but the beginning of the pattern makes complete sense at this point, because we know these lines are properly aligned.

#FIXME double check minute 49
The goal is to determine which of the eight packets is the first one in the sequence, so we can correctly interpret the data that follows.  

The packet is sent to the FPGA, and we already know that the lines between CIC and FPGA are properly aligned — we can identify the first bit of each of the eight sub-packages contained within the packet.  
However, we still need to determine which of these eight packets is the very first one.  
This is essential, because without a header, we cannot align based on a predefined marker — we must instead understand where to start reading the data.  

To find the first packet, we look at the bunch crossing ID.  
Between two (or more) consecutive packets, the bunch crossing ID changes by a specific amount that depends on the time interval between them.  
By comparing successive packets and checking how their bunch crossing IDs change, we can infer which packet comes first.  

In practice, we collect successive stub events, knowing exactly the time difference between two consecutive events.  
Then, in the firmware, we adjust the delay applied to each packet and verify whether the consecutive packets increase by the expected number of bunch crossing IDs.  

This process involves scanning delay values from 0 to 7 — corresponding to the eight possible packets — to find the correct delay that ensures the first interpreted packet is indeed the first one in the sequence.  

![Board_BestStubPackageDelay_OpticalGroup](../images/OTtesting/common/Board_BestStubPackageDelay_OpticalGroup.png)

On the y-axis, we show the right hybrid and the left hybrid.  
For each, we display a single number that indicates which stub package delay was chosen.  

You generally don’t need to check anything specific here.  

From the module QA point of view, there isn’t much to check here.  
If you do see an issue in this plot, it most likely indicates a problem elsewhere in the setup, for instance a failed BXO alignment.

##### OTverifyCICdataWord - Hybrid


At this point, all chips are aligned: the CIC is synced with the LpGBT, the CBC data are correctly decoded by both the FPGA and the CIC, and all chips are communicating. The final step is to check the connection quality between the CBC and CIC.
We set the CBC to send a specific pattern and check if the received data matches. The resulting plots show cumulative errors for all stub lines and Level-1 lines together. At this stage, we cannot pinpoint which line caused an issue without additional, more time-consuming steps, so we just look at a combined value.


Stub tested bits, showed below, are higher because their pattern matching is done in firmware, while Level-1 errors are computed in software, which takes longer. 
![CBCtoCIC_PatternMatchingTestedBits_Hybrid](../images/OTtesting/2S/CBCtoCIC_PatternMatchingTestedBits_Hybrid.png)

Small error rates (around 0.01–0.1%) are normal and not a concern. Large errors, e.g., 20%, would indicate a real problem.
This concludes the alignment section.

![CBCtoCIC_PatternMatchingErrorRate_Hybrid](../images/OTtesting/2S/CBCtoCIC_PatternMatchingErrorRate_Hybrid.png)

[01:11:51.000 --> 01:11:53.000]  do you have any
[01:11:53.000 --> 01:11:55.000]  further question in this part you can ask
[01:11:55.000 --> 01:11:57.000]  of course anytime but since we are going to
[01:11:57.000 --> 01:11:59.000]  move on
[01:11:59.000 --> 01:12:01.000]  from the alignment part
[01:12:01.000 --> 01:12:03.000]  that is honestly quite lengthy
[01:12:03.000 --> 01:12:05.000]  and tough
[01:12:05.000 --> 01:12:07.000]  just please ask
[01:12:11.000 --> 01:12:13.000]  the other question
[01:12:13.000 --> 01:12:15.000]  why do some of the steps of OT and some of them don't
[01:12:17.000 --> 01:12:19.000]  is simply for
[01:12:19.000 --> 01:12:21.000]  a historical reason
[01:12:21.000 --> 01:12:23.000]  the newer steps that we had
[01:12:23.000 --> 01:12:25.000]  recently just
[01:12:25.000 --> 01:12:27.000]  to be a bit more consistent
[01:12:27.000 --> 01:12:29.000]  I just put OT in front of them
[01:12:29.000 --> 01:12:31.000]  this one was the
[01:12:31.000 --> 01:12:33.000]  developer
[01:12:33.000 --> 01:12:35.000]  way before
[01:12:35.000 --> 01:12:37.000]  the inner trackers
[01:12:37.000 --> 01:12:39.000]  started joining the group
[01:12:39.000 --> 01:12:41.000]  and that's why they were
[01:12:41.000 --> 01:12:43.000]  they didn't have OT
[01:12:43.000 --> 01:12:45.000]  I just didn't want to change names everywhere
[01:12:45.000 --> 01:12:47.000]  and this one I think is the only one
[01:12:47.000 --> 01:12:49.000]  legit
[01:12:49.000 --> 01:12:51.000]  because it is the same procedure
[01:12:51.000 --> 01:12:53.000]  for the inner trackers
[01:12:53.000 --> 01:12:55.000]  okay thanks
[01:12:55.000 --> 01:12:57.000]  no problem
[01:13:01.000 --> 01:13:03.000]  okay
[01:13:03.000 --> 01:13:05.000]  and go on yes
[01:13:05.000 --> 01:13:07.000]  when we are
[01:13:07.000 --> 01:13:09.000]  performing this test
[01:13:09.000 --> 01:13:11.000]  there are some messages during
[01:13:11.000 --> 01:13:13.000]  running of the test
[01:13:13.000 --> 01:13:15.000]  so can you point
[01:13:15.000 --> 01:13:17.000]  out some important messages
[01:13:17.000 --> 01:13:19.000]  which we take care when we are
[01:13:19.000 --> 01:13:21.000]  doing the test
[01:13:21.000 --> 01:13:23.000]  so
[01:13:23.000 --> 01:13:25.000]  um
[01:13:27.000 --> 01:13:29.000]  so unfortunately I don't have
[01:13:29.000 --> 01:13:31.000]  a log I think
[01:13:31.000 --> 01:13:33.000]  with me
[01:13:33.000 --> 01:13:35.000]  I don't so to be honest
[01:13:35.000 --> 01:13:37.000]  I'm trying not to use
[01:13:37.000 --> 01:13:39.000]  too much the
[01:13:39.000 --> 01:13:41.000]  so any important message
[01:13:41.000 --> 01:13:43.000]  that is happening over there
[01:13:43.000 --> 01:13:45.000]  it will reflect into
[01:13:45.000 --> 01:13:47.000]  um
[01:13:47.000 --> 01:13:49.000]  something that you can see directly from the plots
[01:13:49.000 --> 01:13:51.000]  so
[01:13:51.000 --> 01:13:53.000]  the reason why I'm not
[01:13:53.000 --> 01:13:55.000]  I prefer not to use the log file
[01:13:57.000 --> 01:13:59.000]  the debug file it might be useful
[01:13:59.000 --> 01:14:01.000]  mainly is for me to understand
[01:14:01.000 --> 01:14:03.000]  what is going on
[01:14:03.000 --> 01:14:05.000]  try to understand
[01:14:05.000 --> 01:14:07.000]  where the problem is occurring
[01:14:09.000 --> 01:14:11.000]  the reason why I don't like it too much
[01:14:11.000 --> 01:14:13.000]  is that
[01:14:15.000 --> 01:14:17.000]  the log file are gonna disappear
[01:14:17.000 --> 01:14:19.000]  after some time
[01:14:19.000 --> 01:14:21.000]  there is a limit on how much
[01:14:21.000 --> 01:14:23.000]  you can store into a log file
[01:14:23.000 --> 01:14:25.000]  so I will really just use the plots
[01:14:25.000 --> 01:14:27.000]  all the plots contains
[01:14:27.000 --> 01:14:29.000]  all the information that
[01:14:29.000 --> 01:14:31.000]  you need
[01:14:31.000 --> 01:14:33.000]  you need to use for understanding
[01:14:33.000 --> 01:14:35.000]  how the modules are running
[01:14:35.000 --> 01:14:37.000]  so
[01:14:37.000 --> 01:14:39.000]  I don't have any particular
[01:14:39.000 --> 01:14:41.000]  message that will not shown
[01:14:41.000 --> 01:14:43.000]  in any of these plots
[01:14:43.000 --> 01:14:45.000]  I
[01:14:47.000 --> 01:14:49.000]  I'm asking only in a sense that
[01:14:49.000 --> 01:14:51.000]  when you start the test of course
[01:14:51.000 --> 01:14:53.000]  it takes some time to finish the test
[01:14:53.000 --> 01:14:55.000]  but
[01:14:55.000 --> 01:14:57.000]  you know
[01:14:57.000 --> 01:14:59.000]  suppose in the start you feel there is
[01:14:59.000 --> 01:15:01.000]  something very problematic
[01:15:01.000 --> 01:15:03.000]  then you stub the run and then you
[01:15:03.000 --> 01:15:05.000]  try to fix it
[01:15:05.000 --> 01:15:07.000]  so
[01:15:07.000 --> 01:15:09.000]  the
[01:15:09.000 --> 01:15:11.000]  uh
[01:15:11.000 --> 01:15:13.000]  so if there is something very problematic
[01:15:15.000 --> 01:15:17.000]  the module will be
[01:15:17.000 --> 01:15:19.000]  will be disabled
[01:15:19.000 --> 01:15:21.000]  and once you don't have any module to run
[01:15:21.000 --> 01:15:23.000]  on the program will stub
[01:15:23.000 --> 01:15:25.000]  so it's
[01:15:25.000 --> 01:15:27.000]  something this bad is
[01:15:27.000 --> 01:15:29.000]  something that makes the module
[01:15:29.000 --> 01:15:31.000]  inoperable
[01:15:31.000 --> 01:15:33.000]  the program will
[01:15:33.000 --> 01:15:35.000]  disable and stub it
[01:15:37.000 --> 01:15:39.000]  so
[01:15:41.000 --> 01:15:43.000]  my suggestion and also shouldn't take
[01:15:43.000 --> 01:15:45.000]  too much to run
[01:15:45.000 --> 01:15:47.000]  it's only 14 minutes
[01:15:47.000 --> 01:15:49.000]  if you have any troubles
[01:15:49.000 --> 01:15:51.000]  just run the quick test
[01:15:51.000 --> 01:15:53.000]  so the quick test is going to tell you
[01:15:53.000 --> 01:15:55.000]  basically
[01:15:55.000 --> 01:15:57.000]  all the major problems that you can address
[01:15:57.000 --> 01:15:59.000]  the full test
[01:15:59.000 --> 01:16:01.000]  it gives you a full idea of the module
[01:16:01.000 --> 01:16:03.000]  it doesn't mean that
[01:16:03.000 --> 01:16:05.000]  so if there is anything
[01:16:05.000 --> 01:16:07.000]  as problematic that the module doesn't work
[01:16:07.000 --> 01:16:09.000]  is the quick test
[01:16:09.000 --> 01:16:11.000]  it gives you immediate feedback and goes very fast
[01:16:13.000 --> 01:16:15.000]  the full test it looks
[01:16:15.000 --> 01:16:17.000]  to a wider perspective
[01:16:17.000 --> 01:16:19.000]  on how well the module works
[01:16:19.000 --> 01:16:21.000]  and basically
[01:16:21.000 --> 01:16:23.000]  whatever is after
[01:16:23.000 --> 01:16:25.000]  what the quick test does
[01:16:25.000 --> 01:16:27.000]  is not really something
[01:16:27.000 --> 01:16:29.000]  that you can easily fix
[01:16:31.000 --> 01:16:33.000]  it's basically
[01:16:33.000 --> 01:16:35.000]  so
[01:16:35.000 --> 01:16:37.000]  it's basically telling you that
[01:16:37.000 --> 01:16:39.000]  your module is not 100%
[01:16:39.000 --> 01:16:41.000]  but it's something that you have to
[01:16:41.000 --> 01:16:43.000]  live with
[01:16:43.000 --> 01:16:45.000]  so
[01:16:45.000 --> 01:16:47.000]  that's why we run the quick test
[01:16:47.000 --> 01:16:49.000]  in particular that's why we run it
[01:16:49.000 --> 01:16:51.000]  after
[01:16:51.000 --> 01:16:53.000]  before encapsulation
[01:16:53.000 --> 01:16:55.000]  because this is going to tell you
[01:16:55.000 --> 01:16:57.000]  the module has
[01:16:57.000 --> 01:16:59.000]  some major issues that you can
[01:16:59.000 --> 01:17:01.000]  still address
[01:17:01.000 --> 01:17:03.000]  the full test
[01:17:03.000 --> 01:17:05.000]  is just going to tell you the same
[01:17:05.000 --> 01:17:07.000]  because you can answer the same steps
[01:17:07.000 --> 01:17:09.000]  but it tells you more
[01:17:09.000 --> 01:17:11.000]  and what is more
[01:17:11.000 --> 01:17:13.000]  basically it will not
[01:17:13.000 --> 01:17:15.000]  tell you anything that you can fix
[01:17:15.000 --> 01:17:17.000]  okay
[01:17:17.000 --> 01:17:19.000]  thank you
[01:17:23.000 --> 01:17:25.000]  okay in general
[01:17:25.000 --> 01:17:27.000]  so we are trying to use a little bit more
[01:17:27.000 --> 01:17:29.000]  a consistent format
[01:17:29.000 --> 01:17:31.000]  so the error messages
[01:17:31.000 --> 01:17:33.000]  are going to have a background
[01:17:33.000 --> 01:17:35.000]  and the warning
[01:17:35.000 --> 01:17:37.000]  message are going to have a background
[01:17:37.000 --> 01:17:39.000]  in yellow so that might give you some
[01:17:39.000 --> 01:17:41.000]  I mean those are the messages
[01:17:41.000 --> 01:17:43.000]  that you might look for
[01:17:43.000 --> 01:17:45.000]  if you feel that there is
[01:17:45.000 --> 01:17:47.000]  any problem but it will just
[01:17:47.000 --> 01:17:49.000]  let you run because otherwise you don't
[01:17:49.000 --> 01:17:51.000]  really get the full picture so it might be
[01:17:51.000 --> 01:17:53.000]  a little bit misleading just to
[01:17:53.000 --> 01:17:55.000]  rely on an error message
[01:17:55.000 --> 01:17:57.000]  or on something else
[01:17:59.000 --> 01:18:01.000]  okay
[01:18:03.000 --> 01:18:05.000]  hi Fabio
[01:18:05.000 --> 01:18:07.000]  so regarding this
[01:18:07.000 --> 01:18:09.000]  color coding for like red
[01:18:09.000 --> 01:18:11.000]  or yellow so I
[01:18:11.000 --> 01:18:13.000]  was running a full test
[01:18:13.000 --> 01:18:15.000]  and I saw why it was trying
[01:18:15.000 --> 01:18:17.000]  to do the alignment probably
[01:18:17.000 --> 01:18:19.000]  and then I tried several times
[01:18:19.000 --> 01:18:21.000]  alignment on line 5
[01:18:21.000 --> 01:18:23.000]  failed it trying 9 more times before giving up
[01:18:23.000 --> 01:18:25.000]  and then there was a red line that
[01:18:25.000 --> 01:18:27.000]  failed to align optical group 0
[01:18:27.000 --> 01:18:29.000]  I did 1 line
[01:18:29.000 --> 01:18:31.000]  5 0 something 5.00
[01:18:31.000 --> 01:18:33.000]  and then finally it got succeeded
[01:18:33.000 --> 01:18:35.000]  but there were plenty of time
[01:18:35.000 --> 01:18:37.000]  that I got this red
[01:18:37.000 --> 01:18:39.000]  errors probably failed to align optical
[01:18:39.000 --> 01:18:41.000]  group 0 so is there something
[01:18:41.000 --> 01:18:43.000]  going on?
[01:18:43.000 --> 01:18:45.000]  so I think I guess probably
[01:18:45.000 --> 01:18:47.000]  what you are referring
[01:18:47.000 --> 01:18:49.000]  is
[01:18:49.000 --> 01:18:51.000]  happening during
[01:18:51.000 --> 01:18:53.000]  other steps that are in the electric
[01:18:53.000 --> 01:18:55.000]  chain validation
[01:18:55.000 --> 01:18:57.000]  so
[01:18:57.000 --> 01:18:59.000]  this is something that I realized and fixed
[01:18:59.000 --> 01:19:01.000]  in the newer version
[01:19:01.000 --> 01:19:03.000]  so because
[01:19:03.000 --> 01:19:05.000]  what
[01:19:05.000 --> 01:19:07.000]  so after we move
[01:19:07.000 --> 01:19:09.000]  the pattern matching to the
[01:19:09.000 --> 01:19:11.000]  into the
[01:19:11.000 --> 01:19:13.000]  firmware then
[01:19:13.000 --> 01:19:15.000]  we needed to do extra alignment steps
[01:19:15.000 --> 01:19:17.000]  in order to make the pattern matching work
[01:19:17.000 --> 01:19:19.000]  especially when you do face
[01:19:19.000 --> 01:19:21.000]  scan where things are moving around
[01:19:21.000 --> 01:19:23.000]  so you need to do them again
[01:19:23.000 --> 01:19:25.000]  and the same procedure
[01:19:25.000 --> 01:19:27.000]  is the procedure that
[01:19:27.000 --> 01:19:29.000]  is run is the same procedure
[01:19:29.000 --> 01:19:31.000]  that is run into align B
[01:19:31.000 --> 01:19:33.000]  bolder data
[01:19:33.000 --> 01:19:35.000]  and
[01:19:35.000 --> 01:19:37.000]  here at this step you want to see if there are
[01:19:37.000 --> 01:19:39.000]  errors and later we
[01:19:39.000 --> 01:19:41.000]  expect that for some phases there is going to be
[01:19:41.000 --> 01:19:43.000]  an error and I didn't realize
[01:19:43.000 --> 01:19:45.000]  that this might have been a little
[01:19:45.000 --> 01:19:47.000]  misleading so if you take
[01:19:47.000 --> 01:19:49.000]  the newer version
[01:19:49.000 --> 01:19:51.000]  the target
[01:19:51.000 --> 01:19:53.000]  I think now is B6.9
[01:19:53.000 --> 01:19:55.000]  I suppress
[01:19:55.000 --> 01:19:57.000]  all the error messages
[01:19:57.000 --> 01:19:59.000]  into the second part
[01:19:59.000 --> 01:20:01.000]  where you expect that some of the
[01:20:01.000 --> 01:20:03.000]  some of the phases that you're using will not
[01:20:03.000 --> 01:20:05.000]  align so now it should be
[01:20:05.000 --> 01:20:07.000]  much more consistent
[01:20:07.000 --> 01:20:09.000]  and get all the
[01:20:09.000 --> 01:20:11.000]  that you get yet and sorry that was
[01:20:11.000 --> 01:20:13.000]  my mistake I just didn't realize
[01:20:13.000 --> 01:20:15.000]  that was misleading
[01:20:15.000 --> 01:20:17.000]  and this error is after this
[01:20:17.000 --> 01:20:19.000]  OT chip to CIC
[01:20:19.000 --> 01:20:21.000]  ECV after that step I
[01:20:21.000 --> 01:20:23.000]  was getting that red errors
[01:20:23.000 --> 01:20:25.000]  but it's okay as you mentioned
[01:20:25.000 --> 01:20:27.000]  the color coding so I thought that
[01:20:27.000 --> 01:20:29.000]  it's better to
[01:20:29.000 --> 01:20:31.000]  you were absolutely right
[01:20:31.000 --> 01:20:33.000]  it was just misleading that
[01:20:33.000 --> 01:20:35.000]  I didn't suppress the message
[01:20:35.000 --> 01:20:37.000]  you were not the only one
[01:20:37.000 --> 01:20:39.000]  and a few people asked me
[01:20:39.000 --> 01:20:41.000]  the same question I realized that
[01:20:41.000 --> 01:20:43.000]  was not a great idea just to leave it
[01:20:43.000 --> 01:20:45.000]  out
[01:20:49.000 --> 01:20:51.000]  okay
[01:20:51.000 --> 01:20:53.000]  I'm going to move on
[01:20:53.000 --> 01:20:55.000]  to the
[01:20:55.000 --> 01:20:57.000]  probably the two most known
[01:20:57.000 --> 01:20:59.000]  calibration so
[01:20:59.000 --> 01:21:01.000]  I think you know very well
[01:21:01.000 --> 01:21:03.000]  so
[01:21:03.000 --> 01:21:05.000]  it is telequalization so
[01:21:05.000 --> 01:21:07.000]  by construction
[01:21:07.000 --> 01:21:09.000]  the comparator that are
[01:21:09.000 --> 01:21:11.000]  setting the threshold
[01:21:11.000 --> 01:21:13.000]  for every channel
[01:21:13.000 --> 01:21:15.000]  are subjected
[01:21:15.000 --> 01:21:17.000]  to
[01:21:17.000 --> 01:21:19.000]  production variation
[01:21:19.000 --> 01:21:21.000]  cannot be done better than that
[01:21:21.000 --> 01:21:23.000]  and so basically
[01:21:23.000 --> 01:21:25.000]  every chip, every doubt chip
[01:21:25.000 --> 01:21:27.000]  that you're going to find out
[01:21:27.000 --> 01:21:29.000]  on the market not only on the tracker
[01:21:29.000 --> 01:21:31.000]  have some
[01:21:31.000 --> 01:21:33.000]  functionality that allow to compensate
[01:21:33.000 --> 01:21:35.000]  for that and in this case
[01:21:35.000 --> 01:21:37.000]  for the CBC
[01:21:37.000 --> 01:21:39.000]  is an offset
[01:21:39.000 --> 01:21:41.000]  that you can apply basically
[01:21:41.000 --> 01:21:43.000]  to the signal that comes from the amplifier
[01:21:45.000 --> 01:21:47.000]  that allow to raise a little bit
[01:21:47.000 --> 01:21:49.000]  lower a little bit
[01:21:49.000 --> 01:21:51.000]  the signal, the pedestal of the signal
[01:21:51.000 --> 01:21:53.000]  such that
[01:21:53.000 --> 01:21:55.000]  the threshold that you
[01:21:55.000 --> 01:21:57.000]  apply is actually uniform
[01:21:57.000 --> 01:21:59.000]  across every channel
[01:21:59.000 --> 01:22:01.000]  so
[01:22:01.000 --> 01:22:03.000]  during the pedestal equalization
[01:22:03.000 --> 01:22:05.000]  what we do is that
[01:22:05.000 --> 01:22:07.000]  we
[01:22:07.000 --> 01:22:09.000]  set these values
[01:22:09.000 --> 01:22:11.000]  channel by channel such that
[01:22:11.000 --> 01:22:13.000]  we can even equalize
[01:22:13.000 --> 01:22:15.000]  the threshold
[01:22:15.000 --> 01:22:17.000]  so
[01:22:17.000 --> 01:22:19.000]  for this
[01:22:19.000 --> 01:22:21.000]  for this
[01:22:23.000 --> 01:22:25.000]  calibration
[01:22:25.000 --> 01:22:27.000]  the plots are stored
[01:22:27.000 --> 01:22:29.000]  at the level of the chip
[01:22:29.000 --> 01:22:31.000]  just go from one
[01:22:31.000 --> 01:22:33.000]  and are the
[01:22:33.000 --> 01:22:35.000]  two first plots
[01:22:35.000 --> 01:22:37.000]  they are not
[01:22:37.000 --> 01:22:39.000]  they are not going to tell you too much
[01:22:39.000 --> 01:22:41.000]  because
[01:22:41.000 --> 01:22:43.000]  most of the understanding will come from
[01:22:43.000 --> 01:22:45.000]  the later step that is
[01:22:45.000 --> 01:22:47.000]  the noise step
[01:22:47.000 --> 01:22:49.000]  but at the end of the callization
[01:22:49.000 --> 01:22:51.000]  you get this plot that shows
[01:22:51.000 --> 01:22:53.000]  forever the channel
[01:22:53.000 --> 01:22:55.000]  in the CBC
[01:22:55.000 --> 01:22:57.000]  the offset that was chosen
[01:22:57.000 --> 01:22:59.000]  I would say the only thing
[01:22:59.000 --> 01:23:01.000]  here
[01:23:01.000 --> 01:23:03.000]  if you see something weird later
[01:23:03.000 --> 01:23:05.000]  you can come back and see
[01:23:05.000 --> 01:23:07.000]  if any of the offset for some reason
[01:23:07.000 --> 01:23:09.000]  went all the way to
[01:23:09.000 --> 01:23:11.000]  255 which is the maximum
[01:23:11.000 --> 01:23:13.000]  they were down to 0
[01:23:13.000 --> 01:23:15.000]  this might mean that something failed
[01:23:15.000 --> 01:23:17.000]  so far I think
[01:23:17.000 --> 01:23:19.000]  for the CBC is very stable
[01:23:19.000 --> 01:23:21.000]  so I don't recall seeing anything like that
[01:23:21.000 --> 01:23:23.000]  and another
[01:23:23.000 --> 01:23:25.000]  information that you can see
[01:23:25.000 --> 01:23:27.000]  is that
[01:23:27.000 --> 01:23:29.000]  might also help you if something wrong
[01:23:29.000 --> 01:23:31.000]  happen
[01:23:31.000 --> 01:23:33.000]  by the way I just realized
[01:23:33.000 --> 01:23:35.000]  preparing this slide that there was a mistake
[01:23:35.000 --> 01:23:37.000]  in this label that I corrected
[01:23:37.000 --> 01:23:39.000]  here you have the channel
[01:23:39.000 --> 01:23:41.000]  on the x-axis
[01:23:41.000 --> 01:23:43.000]  and the y-channel is not the offset
[01:23:43.000 --> 01:23:45.000]  it's actually the occupancy
[01:23:45.000 --> 01:23:47.000]  because the idea is that we want to
[01:23:47.000 --> 01:23:49.000]  get to around 50%
[01:23:49.000 --> 01:23:51.000]  occupancy it's like higher for
[01:23:51.000 --> 01:23:53.000]  technical reason
[01:23:55.000 --> 01:23:57.000]  so it means that basically
[01:23:57.000 --> 01:23:59.000]  even that particular offset
[01:23:59.000 --> 01:24:01.000]  you get an occupancy
[01:24:01.000 --> 01:24:03.000]  that is around 50%
[01:24:03.000 --> 01:24:05.000]  that should be the occupancy that you get
[01:24:05.000 --> 01:24:07.000]  after they pay the staff
[01:24:07.000 --> 01:24:09.000]  so here again something
[01:24:09.000 --> 01:24:11.000]  weird happens
[01:24:11.000 --> 01:24:13.000]  to the
[01:24:13.000 --> 01:24:15.000]  two S steps
[01:24:15.000 --> 01:24:17.000]  you can come back here and see for any reason
[01:24:17.000 --> 01:24:19.000]  some channel have a super high occupancy
[01:24:19.000 --> 01:24:21.000]  super low occupancy
[01:24:21.000 --> 01:24:23.000]  this might indicate that something didn't work perfectly
[01:24:23.000 --> 01:24:25.000]  but the idea should be that you have something
[01:24:25.000 --> 01:24:27.000]  kind of uniform
[01:24:27.000 --> 01:24:29.000]  it's not gonna be super uniform
[01:24:29.000 --> 01:24:31.000]  because you still
[01:24:31.000 --> 01:24:33.000]  you still have a
[01:24:33.000 --> 01:24:35.000]  duck that you have to play with
[01:24:35.000 --> 01:24:37.000]  the discrete step that you
[01:24:37.000 --> 01:24:39.000]  can apply but something
[01:24:39.000 --> 01:24:41.000]  like this should be
[01:24:41.000 --> 01:24:43.000]  reasonable
[01:24:51.000 --> 01:24:53.000]  sorry I can't hear you
[01:24:53.000 --> 01:24:55.000]  very well, can you try to
[01:24:55.000 --> 01:24:57.000]  speak closer to the mic
[01:25:05.000 --> 01:25:07.000]  yeah so
[01:25:07.000 --> 01:25:09.000]  as I was saying these steps
[01:25:09.000 --> 01:25:11.000]  the plot is but this step doesn't tell
[01:25:11.000 --> 01:25:13.000]  you too much because right after
[01:25:13.000 --> 01:25:15.000]  we get the
[01:25:15.000 --> 01:25:17.000]  pay the noise so
[01:25:17.000 --> 01:25:19.000]  here we do a scan
[01:25:19.000 --> 01:25:21.000]  okay we know very well but
[01:25:21.000 --> 01:25:23.000]  very briefly we do a scan
[01:25:23.000 --> 01:25:25.000]  of the threshold that we apply
[01:25:25.000 --> 01:25:27.000]  and we measure the occupancy
[01:25:27.000 --> 01:25:29.000]  and in an ideal case
[01:25:29.000 --> 01:25:31.000]  you would expect a step function
[01:25:31.000 --> 01:25:33.000]  but in reality
[01:25:33.000 --> 01:25:35.000]  you have the noises so this is
[01:25:35.000 --> 01:25:37.000]  basically a step function
[01:25:37.000 --> 01:25:39.000]  convoluted with a Gaussian and the Gaussian
[01:25:39.000 --> 01:25:41.000]  is the
[01:25:41.000 --> 01:25:43.000]  the noise that you have so it becomes
[01:25:43.000 --> 01:25:45.000]  an S shape
[01:25:45.000 --> 01:25:47.000]  and that's what I call S curves
[01:25:47.000 --> 01:25:49.000]  so
[01:25:49.000 --> 01:25:51.000]  there are a few plots over here
[01:25:51.000 --> 01:25:53.000]  hopefully I'm gonna
[01:25:53.000 --> 01:25:55.000]  remember them all
[01:25:55.000 --> 01:25:57.000]  so
[01:25:57.000 --> 01:25:59.000]  we have at the level of the chip
[01:25:59.000 --> 01:26:01.000]  well this one I think you
[01:26:01.000 --> 01:26:03.000]  saw it one billion times
[01:26:03.000 --> 01:26:05.000]  is the
[01:26:05.000 --> 01:26:07.000]  is the
[01:26:07.000 --> 01:26:09.000]  the noise
[01:26:09.000 --> 01:26:11.000]  here for the channel on the y-axis
[01:26:11.000 --> 01:26:13.000]  on the x-axis you have the
[01:26:13.000 --> 01:26:15.000]  sorry on the x-axis of the channel
[01:26:15.000 --> 01:26:17.000]  on the y-axis you have the threshold
[01:26:17.000 --> 01:26:19.000]  in the CTH and
[01:26:19.000 --> 01:26:21.000]  this number you can find in the tweak
[01:26:21.000 --> 01:26:23.000]  at the beginning but one step
[01:26:23.000 --> 01:26:25.000]  is 156
[01:26:25.000 --> 01:26:27.000]  electrons
[01:26:27.000 --> 01:26:29.000]  and on the z-axis
[01:26:29.000 --> 01:26:31.000]  you have the occupancy
[01:26:31.000 --> 01:26:33.000]  and each one of these
[01:26:33.000 --> 01:26:35.000]  lines
[01:26:37.000 --> 01:26:39.000]  actually I can show it to you
[01:26:39.000 --> 01:26:41.000]  each one of these
[01:26:41.000 --> 01:26:43.000]  vertical lines is
[01:26:43.000 --> 01:26:45.000]  nascar
[01:26:47.000 --> 01:26:49.000]  I should then close it
[01:26:51.000 --> 01:26:53.000]  okay
[01:26:53.000 --> 01:26:55.000]  so
[01:26:55.000 --> 01:26:57.000]  then each one of these
[01:26:57.000 --> 01:26:59.000]  S curve is fitted
[01:26:59.000 --> 01:27:01.000]  you can actually look into the fit
[01:27:01.000 --> 01:27:03.000]  that you get for everyone
[01:27:03.000 --> 01:27:05.000]  of these channels
[01:27:05.000 --> 01:27:07.000]  into the channel folder
[01:27:07.000 --> 01:27:09.000]  I'm gonna pick one random
[01:27:09.000 --> 01:27:11.000]  and
[01:27:11.000 --> 01:27:13.000]  if you zoom in it's basically the projection
[01:27:13.000 --> 01:27:15.000]  that I was showing you before
[01:27:15.000 --> 01:27:17.000]  with the shape
[01:27:17.000 --> 01:27:19.000]  the point that I collected
[01:27:19.000 --> 01:27:21.000]  the fit
[01:27:21.000 --> 01:27:23.000]  I think I was quite lucky because the fit
[01:27:23.000 --> 01:27:25.000]  was very good
[01:27:25.000 --> 01:27:27.000]  on this fit
[01:27:27.000 --> 01:27:29.000]  then we are able to extract from
[01:27:29.000 --> 01:27:31.000]  the convoluting
[01:27:31.000 --> 01:27:33.000]  the Gaussian
[01:27:33.000 --> 01:27:35.000]  we can extract the noise that is the width of the Gaussian
[01:27:35.000 --> 01:27:37.000]  and by the convoluting
[01:27:37.000 --> 01:27:39.000]  the Gaussian we get the step function
[01:27:39.000 --> 01:27:41.000]  and the step
[01:27:41.000 --> 01:27:43.000]  the step is the pedestal
[01:27:43.000 --> 01:27:45.000]  here is a little bit more clear because
[01:27:45.000 --> 01:27:47.000]  it is at 50%
[01:27:47.000 --> 01:27:49.000]  I forgot to mention this is done without injection
[01:27:49.000 --> 01:27:51.000]  so this is
[01:27:51.000 --> 01:27:53.000]  really the pedestal that we are measuring
[01:27:55.000 --> 01:27:57.000]  then we add
[01:27:57.000 --> 01:27:59.000]  a few more plots
[01:27:59.000 --> 01:28:01.000]  here you get one for every channel
[01:28:01.000 --> 01:28:03.000]  a few more plots
[01:28:05.000 --> 01:28:07.000]  so from this car we can extract
[01:28:07.000 --> 01:28:09.000]  the pedestal distribution
[01:28:09.000 --> 01:28:11.000]  so this is simply
[01:28:11.000 --> 01:28:13.000]  for the convoluting distribution
[01:28:13.000 --> 01:28:15.000]  of the pedestal for every channel
[01:28:15.000 --> 01:28:17.000]  this should be very sharp
[01:28:17.000 --> 01:28:19.000]  so if something fails you will start
[01:28:19.000 --> 01:28:21.000]  seeing into this plot
[01:28:21.000 --> 01:28:23.000]  with some long tail
[01:28:23.000 --> 01:28:25.000]  and all simply
[01:28:25.000 --> 01:28:27.000]  out layers
[01:28:27.000 --> 01:28:29.000]  and
[01:28:29.000 --> 01:28:31.000]  to get the distribution of the pedestal across
[01:28:31.000 --> 01:28:33.000]  the
[01:28:33.000 --> 01:28:35.000]  all the channel
[01:28:35.000 --> 01:28:37.000]  we have also the channel pedestal plot
[01:28:37.000 --> 01:28:39.000]  let's show you for every channel
[01:28:39.000 --> 01:28:41.000]  the threshold
[01:28:41.000 --> 01:28:43.000]  at which you have the pedestal
[01:28:43.000 --> 01:28:45.000]  and you see at the scale
[01:28:45.000 --> 01:28:47.000]  it's very very uniform
[01:28:47.000 --> 01:28:49.000]  within a few BCTH
[01:28:49.000 --> 01:28:51.000]  again this is
[01:28:51.000 --> 01:28:53.000]  156 electrons
[01:28:53.000 --> 01:28:55.000]  so we speak about
[01:28:55.000 --> 01:28:57.000]  a few hundred of electrons
[01:28:57.000 --> 01:28:59.000]  of distribution width
[01:29:01.000 --> 01:29:03.000]  then in the same
[01:29:03.000 --> 01:29:05.000]  from the same plot
[01:29:05.000 --> 01:29:07.000]  we also get
[01:29:07.000 --> 01:29:09.000]  the noise distribution
[01:29:09.000 --> 01:29:11.000]  for all the
[01:29:11.000 --> 01:29:13.000]  so this is the convoluting distribution
[01:29:13.000 --> 01:29:15.000]  for all the channels
[01:29:15.000 --> 01:29:17.000]  and as for the pedestal you get
[01:29:17.000 --> 01:29:19.000]  the channel noise distribution
[01:29:19.000 --> 01:29:21.000]  of the noise
[01:29:21.000 --> 01:29:23.000]  for every channel
[01:29:23.000 --> 01:29:25.000]  and
[01:29:25.000 --> 01:29:27.000]  these are all the plots at the level of
[01:29:27.000 --> 01:29:29.000]  the CBC
[01:29:29.000 --> 01:29:31.000]  then there are more cumulative plots
[01:29:31.000 --> 01:29:33.000]  at the level of the
[01:29:33.000 --> 01:29:35.000]  hybrid
[01:29:35.000 --> 01:29:37.000]  to show you the behavior of all the hybrid
[01:29:37.000 --> 01:29:39.000]  so here is the noise distribution
[01:29:39.000 --> 01:29:41.000]  for all the channels
[01:29:43.000 --> 01:29:45.000]  for every so for every strips on the hybrid
[01:29:45.000 --> 01:29:47.000]  these include
[01:29:47.000 --> 01:29:49.000]  both top and bottom
[01:29:49.000 --> 01:29:51.000]  so we usually look
[01:29:51.000 --> 01:29:53.000]  separately
[01:29:53.000 --> 01:29:55.000]  the bottom
[01:29:55.000 --> 01:29:57.000]  that is slightly higher because
[01:29:57.000 --> 01:29:59.000]  we have the fold over so the lines
[01:29:59.000 --> 01:30:01.000]  that allow us to
[01:30:01.000 --> 01:30:03.000]  while going to the
[01:30:03.000 --> 01:30:05.000]  ship
[01:30:05.000 --> 01:30:07.000]  had to go down to the hybrid
[01:30:07.000 --> 01:30:09.000]  so a bit longer and that's probably a bit of extra noise
[01:30:09.000 --> 01:30:11.000]  due to the extra capacitance
[01:30:11.000 --> 01:30:13.000]  of these lines
[01:30:13.000 --> 01:30:15.000]  and the same from the top
[01:30:15.000 --> 01:30:17.000]  that if I move back and forth
[01:30:17.000 --> 01:30:19.000]  the top is a little bit lower
[01:30:19.000 --> 01:30:21.000]  but I guess everybody knows
[01:30:21.000 --> 01:30:23.000]  at this point
[01:30:23.000 --> 01:30:25.000]  and finally
[01:30:25.000 --> 01:30:27.000]  the last one is the cumulative
[01:30:27.000 --> 01:30:29.000]  noise distribution for the hybrid
[01:30:33.000 --> 01:30:35.000]  and these are all the plots
[01:30:35.000 --> 01:30:37.000]  for the noise
[01:30:37.000 --> 01:30:39.000]  so I need
[01:30:39.000 --> 01:30:41.000]  I'm just going to go ahead but stub me
[01:30:41.000 --> 01:30:43.000]  if you have any questions
[01:30:43.000 --> 01:30:45.000]  okay so
[01:30:45.000 --> 01:30:47.000]  I think basically
[01:30:47.000 --> 01:30:49.000]  up to now we cover
[01:30:49.000 --> 01:30:51.000]  all the steps
[01:30:51.000 --> 01:30:53.000]  done also by the quick test
[01:30:53.000 --> 01:30:55.000]  there were some extra steps that in quick test
[01:30:55.000 --> 01:30:57.000]  are not done like
[01:30:57.000 --> 01:30:59.000]  no I'm joking
[01:30:59.000 --> 01:31:01.000]  there is nothing that is not done
[01:31:01.000 --> 01:31:03.000]  for the quick test so far
[01:31:03.000 --> 01:31:05.000]  so
[01:31:05.000 --> 01:31:07.000]  so from now on we are going to move
[01:31:07.000 --> 01:31:09.000]  just to the
[01:31:09.000 --> 01:31:11.000]  test
[01:31:11.000 --> 01:31:13.000]  that are done
[01:31:13.000 --> 01:31:15.000]  for
[01:31:15.000 --> 01:31:17.000]  the full test
[01:31:17.000 --> 01:31:19.000]  I'm going to start
[01:31:19.000 --> 01:31:21.000]  from the injection delay
[01:31:21.000 --> 01:31:23.000]  optimization
[01:31:23.000 --> 01:31:25.000]  so yes
[01:31:25.000 --> 01:31:27.000]  sorry I want to ask
[01:31:27.000 --> 01:31:29.000]  one more question if I may
[01:31:29.000 --> 01:31:31.000]  I saw
[01:31:31.000 --> 01:31:33.000]  for some module
[01:31:33.000 --> 01:31:35.000]  I saw that noise distribution
[01:31:35.000 --> 01:31:37.000]  maybe in the cumulative one
[01:31:37.000 --> 01:31:39.000]  or maybe in the cheap way
[01:31:39.000 --> 01:31:41.000]  yeah
[01:31:41.000 --> 01:31:43.000]  maybe per cheap so I saw
[01:31:43.000 --> 01:31:45.000]  two distinct peak
[01:31:45.000 --> 01:31:47.000]  in the noise distribution
[01:31:47.000 --> 01:31:49.000]  so
[01:31:49.000 --> 01:31:51.000]  and it was not very old quite recently
[01:31:51.000 --> 01:31:53.000]  so I was trying to think that
[01:31:53.000 --> 01:31:55.000]  yeah
[01:31:55.000 --> 01:31:57.000]  whatever thank you for asking me
[01:31:57.000 --> 01:31:59.000]  because I actually
[01:31:59.000 --> 01:32:01.000]  I forgot two plots so the
[01:32:01.000 --> 01:32:03.000]  channel noise distribution was also
[01:32:03.000 --> 01:32:05.000]  the level of the
[01:32:05.000 --> 01:32:07.000]  of the
[01:32:07.000 --> 01:32:09.000]  chip so I think
[01:32:09.000 --> 01:32:11.000]  so if I move back
[01:32:11.000 --> 01:32:13.000]  actually
[01:32:13.000 --> 01:32:15.000]  can I do the same with this thing
[01:32:15.000 --> 01:32:17.000]  impose
[01:32:19.000 --> 01:32:21.000]  before
[01:32:21.000 --> 01:32:23.000]  okay so if I
[01:32:23.000 --> 01:32:25.000]  go together
[01:32:25.000 --> 01:32:27.000]  the top and the bottom
[01:32:27.000 --> 01:32:29.000]  the level of the chip but also the high
[01:32:29.000 --> 01:32:31.000]  but you see that the bottom is slightly higher
[01:32:31.000 --> 01:32:33.000]  and this is because
[01:32:33.000 --> 01:32:35.000]  what I was saying that you have
[01:32:35.000 --> 01:32:37.000]  you have these
[01:32:37.000 --> 01:32:39.000]  fold over that increase
[01:32:39.000 --> 01:32:41.000]  the length of the lines that are going
[01:32:41.000 --> 01:32:43.000]  to the wall bond pad for the bottom
[01:32:43.000 --> 01:32:45.000]  one and
[01:32:45.000 --> 01:32:47.000]  and this is
[01:32:47.000 --> 01:32:49.000]  why sometimes when
[01:32:49.000 --> 01:32:51.000]  these
[01:32:51.000 --> 01:32:53.000]  differences lightly more pronounced which I think
[01:32:53.000 --> 01:32:55.000]  was the case in the pasta and also
[01:32:55.000 --> 01:32:57.000]  is the case I think
[01:32:57.000 --> 01:32:59.000]  if you don't apply any
[01:32:59.000 --> 01:33:01.000]  voltage maybe or lower
[01:33:01.000 --> 01:33:03.000]  voltage when you look at
[01:33:03.000 --> 01:33:05.000]  these actually you can see
[01:33:05.000 --> 01:33:06.000]  there are
[01:33:06.000 --> 01:33:08.000]  and these two peaks are simply
[01:33:08.000 --> 01:33:10.000]  due to the fact that the top and the bottom
[01:33:11.000 --> 01:33:13.000]  channels have
[01:33:13.000 --> 01:33:15.000]  a slightly different noise
[01:33:15.000 --> 01:33:17.000]  I asked because
[01:33:17.000 --> 01:33:19.000]  the noise distribution plot
[01:33:19.000 --> 01:33:21.000]  you showed before probably it had one
[01:33:21.000 --> 01:33:23.000]  peak so I thought but your this noise
[01:33:23.000 --> 01:33:25.000]  levels have different have
[01:33:25.000 --> 01:33:27.000]  had difference for bottom
[01:33:27.000 --> 01:33:29.000]  and top so I thought okay
[01:33:29.000 --> 01:33:31.000]  maybe there is some other reasons but thanks
[01:33:31.000 --> 01:33:33.000]  no I simply that it seems
[01:33:33.000 --> 01:33:35.000]  that the resolution this bit is not
[01:33:35.000 --> 01:33:37.000]  super high when you look at all the
[01:33:37.000 --> 01:33:39.000]  plot all the chip together
[01:33:39.000 --> 01:33:41.000]  then these get
[01:33:41.000 --> 01:33:43.000]  smooth and a little bit
[01:33:43.000 --> 01:33:45.000]  because
[01:33:45.000 --> 01:33:47.000]  let me see so
[01:33:47.000 --> 01:33:49.000]  for example
[01:33:51.000 --> 01:33:53.000]  let's see
[01:33:57.000 --> 01:33:59.000]  you see that the lines
[01:33:59.000 --> 01:34:01.000]  between the two start to be a little bit
[01:34:01.000 --> 01:34:03.000]  more less pronounced
[01:34:03.000 --> 01:34:05.000]  because for example this chip is likely
[01:34:05.000 --> 01:34:07.000]  higher noise
[01:34:07.000 --> 01:34:09.000]  and that is kind of the same
[01:34:09.000 --> 01:34:11.000]  the top becomes
[01:34:11.000 --> 01:34:13.000]  kind of the same top over here so
[01:34:13.000 --> 01:34:15.000]  when you put everything together
[01:34:15.000 --> 01:34:17.000]  basically the double peak structure
[01:34:17.000 --> 01:34:19.000]  kind of disappear
[01:34:19.000 --> 01:34:21.000]  so for the single chip
[01:34:21.000 --> 01:34:23.000]  is a little bit more
[01:34:25.000 --> 01:34:27.000]  clear let me go back
[01:34:27.000 --> 01:34:29.000]  yeah
[01:34:29.000 --> 01:34:31.000]  I can
[01:34:31.000 --> 01:34:33.000]  find a complete
[01:34:33.000 --> 01:34:35.000]  wrong spot
[01:34:35.000 --> 01:34:37.000]  yeah here it basically gets
[01:34:37.000 --> 01:34:39.000]  smooth and out
[01:34:39.000 --> 01:34:41.000]  okay
[01:34:45.000 --> 01:34:47.000]  okay
[01:34:47.000 --> 01:34:49.000]  let me just close
[01:34:49.000 --> 01:34:51.000]  a few blocks it becomes
[01:34:51.000 --> 01:34:53.000]  less crowded
[01:34:53.000 --> 01:34:55.000]  so injection delay
[01:34:55.000 --> 01:34:57.000]  optimization so
[01:34:59.000 --> 01:35:01.000]  so after this test
[01:35:01.000 --> 01:35:03.000]  we will need to do some
[01:35:03.000 --> 01:35:05.000]  test with injection
[01:35:05.000 --> 01:35:07.000]  the problem when you
[01:35:07.000 --> 01:35:09.000]  start injecting is that
[01:35:09.000 --> 01:35:11.000]  you also need to find
[01:35:11.000 --> 01:35:13.000]  the proper timer which inject
[01:35:13.000 --> 01:35:15.000]  because as long as you work only
[01:35:15.000 --> 01:35:17.000]  with the
[01:35:17.000 --> 01:35:19.000]  pedestal then the pedestal
[01:35:19.000 --> 01:35:21.000]  is going to be the same every time
[01:35:21.000 --> 01:35:23.000]  point in time
[01:35:23.000 --> 01:35:25.000]  when you start injecting
[01:35:25.000 --> 01:35:27.000]  then you have to make sure that
[01:35:27.000 --> 01:35:29.000]  you are reading
[01:35:29.000 --> 01:35:31.000]  the heat at the correct time
[01:35:31.000 --> 01:35:33.000]  otherwise you might read it too early
[01:35:33.000 --> 01:35:35.000]  or too late
[01:35:39.000 --> 01:35:41.000]  so for doing this
[01:35:41.000 --> 01:35:43.000]  what we do is that we do a scan
[01:35:43.000 --> 01:35:45.000]  of
[01:35:45.000 --> 01:35:47.000]  the
[01:35:47.000 --> 01:35:49.000]  threshold
[01:35:49.000 --> 01:35:51.000]  of the injection
[01:35:51.000 --> 01:35:53.000]  delay
[01:35:53.000 --> 01:35:55.000]  that you see on the y-axis and for each
[01:35:55.000 --> 01:35:57.000]  of this point we measure
[01:35:57.000 --> 01:35:59.000]  the threshold
[01:35:59.000 --> 01:36:01.000]  at which the occupancy
[01:36:01.000 --> 01:36:03.000]  50% so basically
[01:36:03.000 --> 01:36:05.000]  this means that at this
[01:36:05.000 --> 01:36:07.000]  point
[01:36:07.000 --> 01:36:09.000]  50% of the time
[01:36:09.000 --> 01:36:11.000]  the
[01:36:11.000 --> 01:36:13.000]  signal at the input comparator
[01:36:13.000 --> 01:36:15.000]  is above the
[01:36:15.000 --> 01:36:17.000]  threshold and 50% of the time
[01:36:17.000 --> 01:36:19.000]  is below the threshold which means basically
[01:36:19.000 --> 01:36:21.000]  you are setting the comparator
[01:36:21.000 --> 01:36:23.000]  exactly the same at the value
[01:36:23.000 --> 01:36:25.000]  which the signal is
[01:36:25.000 --> 01:36:27.000]  and therefore by changing
[01:36:27.000 --> 01:36:29.000]  the delay
[01:36:29.000 --> 01:36:31.000]  with given a certain amount of
[01:36:31.000 --> 01:36:33.000]  injecting charge you can
[01:36:33.000 --> 01:36:35.000]  reconstruct the full
[01:36:35.000 --> 01:36:37.000]  distribution of the signal
[01:36:39.000 --> 01:36:41.000]  at this point
[01:36:41.000 --> 01:36:43.000]  you can identify the working point
[01:36:43.000 --> 01:36:45.000]  and the working point
[01:36:45.000 --> 01:36:47.000]  for the
[01:36:47.000 --> 01:36:49.000]  the
[01:36:49.000 --> 01:36:51.000]  time is quite easy
[01:36:51.000 --> 01:36:53.000]  to be identified because
[01:36:53.000 --> 01:36:55.000]  we just set up the peak
[01:36:55.000 --> 01:36:57.000]  the peak in time and first
[01:36:57.000 --> 01:36:59.000]  approximation is independent from the amount of charge
[01:36:59.000 --> 01:37:01.000]  so if you set up the peak
[01:37:01.000 --> 01:37:03.000]  the injection delay
[01:37:03.000 --> 01:37:05.000]  with this point
[01:37:05.000 --> 01:37:07.000]  the sampling time
[01:37:07.000 --> 01:37:09.000]  you know that even if you inject
[01:37:09.000 --> 01:37:11.000]  less the peak is going to be
[01:37:11.000 --> 01:37:13.000]  always the same so it will always
[01:37:13.000 --> 01:37:15.000]  pass the threshold at that instant
[01:37:15.000 --> 01:37:17.000]  time and the other thing
[01:37:17.000 --> 01:37:19.000]  that you need to set
[01:37:19.000 --> 01:37:21.000]  is the distance from the pedestal
[01:37:21.000 --> 01:37:23.000]  in order to remove the noise
[01:37:23.000 --> 01:37:25.000]  that is coming from
[01:37:25.000 --> 01:37:27.000]  the pedestal so
[01:37:27.000 --> 01:37:29.000]  at the moment what we are in
[01:37:29.000 --> 01:37:31.000]  the configuration file is that
[01:37:31.000 --> 01:37:33.000]  set at
[01:37:33.000 --> 01:37:35.000]  5
[01:37:35.000 --> 01:37:37.000]  time the noise from this pedestal
[01:37:37.000 --> 01:37:39.000]  and since the noise was just
[01:37:39.000 --> 01:37:41.000]  measure the previous calibration
[01:37:41.000 --> 01:37:43.000]  during the discards
[01:37:43.000 --> 01:37:45.000]  we already know how much
[01:37:45.000 --> 01:37:47.000]  we need to move away from this value
[01:37:47.000 --> 01:37:49.000]  and therefore we can
[01:37:49.000 --> 01:37:51.000]  identify this point
[01:37:51.000 --> 01:37:53.000]  in
[01:37:53.000 --> 01:37:55.000]  no I wanted to put the same one
[01:37:55.000 --> 01:37:57.000]  so
[01:37:57.000 --> 01:37:59.000]  superimpose
[01:37:59.000 --> 01:38:01.000]  we can identify the working point
[01:38:01.000 --> 01:38:03.000]  so here correspond to the
[01:38:03.000 --> 01:38:05.000]  peak
[01:38:05.000 --> 01:38:07.000]  and the distance between
[01:38:07.000 --> 01:38:09.000]  these correspond to 5 times
[01:38:09.000 --> 01:38:11.000]  the noise
[01:38:11.000 --> 01:38:13.000]  at this point onwards
[01:38:13.000 --> 01:38:15.000]  anytime we are going to have to inject
[01:38:15.000 --> 01:38:17.000]  we know that we need to set
[01:38:17.000 --> 01:38:19.000]  this injection delay
[01:38:19.000 --> 01:38:21.000]  and this threshold
[01:38:21.000 --> 01:38:23.000]  in order to do the following measurement
[01:38:23.000 --> 01:38:25.000]  ok
[01:38:31.000 --> 01:38:33.000]  technical thing
[01:38:33.000 --> 01:38:35.000]  Irina can you lower a little bit the volume
[01:38:35.000 --> 01:38:37.000]  sorry Irina is better
[01:38:37.000 --> 01:38:39.000]  sorry
[01:38:39.000 --> 01:38:41.000]  Amid
[01:38:41.000 --> 01:38:43.000]  oopsie
[01:38:43.000 --> 01:38:45.000]  I can hear myself in the nearby
[01:38:45.000 --> 01:38:47.000]  ok thank you
[01:38:49.000 --> 01:38:51.000]  ok then at this point
[01:38:51.000 --> 01:38:53.000]  we know that the following test
[01:38:53.000 --> 01:38:55.000]  the one at the current injection will be done
[01:38:55.000 --> 01:38:57.000]  in the proper way because at this point
[01:38:57.000 --> 01:38:59.000]  we know that whatever charge
[01:38:59.000 --> 01:39:01.000]  we inject we are going to be at the current value
[01:39:01.000 --> 01:39:03.000]  so
[01:39:03.000 --> 01:39:05.000]  the next steps
[01:39:05.000 --> 01:39:07.000]  are different
[01:39:07.000 --> 01:39:09.000]  the occupancy measurement
[01:39:09.000 --> 01:39:11.000]  a different injection
[01:39:11.000 --> 01:39:13.000]  and the reason why we want to do that
[01:39:13.000 --> 01:39:15.000]  is because we want to have first of all
[01:39:15.000 --> 01:39:17.000]  a reference which we know
[01:39:17.000 --> 01:39:19.000]  what is going to be the occupancy for every channel
[01:39:19.000 --> 01:39:21.000]  with
[01:39:21.000 --> 01:39:23.000]  without injection
[01:39:23.000 --> 01:39:25.000]  so basically the occupancy
[01:39:25.000 --> 01:39:27.000]  just due to the noise
[01:39:27.000 --> 01:39:29.000]  and then the occupancy
[01:39:29.000 --> 01:39:31.000]  the deficiency in
[01:39:31.000 --> 01:39:33.000]  measuring
[01:39:33.000 --> 01:39:35.000]  events
[01:39:35.000 --> 01:39:37.000]  with a given amount of charge
[01:39:37.000 --> 01:39:39.000]  released by the particle
[01:39:39.000 --> 01:39:41.000]  in this case not a particle it's just
[01:39:41.000 --> 01:39:43.000]  an injection but we know
[01:39:43.000 --> 01:39:45.000]  how much we need to inject
[01:39:45.000 --> 01:39:47.000]  for a producer a certain amount of charge
[01:39:47.000 --> 01:39:49.000]  and for these
[01:39:49.000 --> 01:39:51.000]  we have three different
[01:39:51.000 --> 01:39:53.000]  no sorry five different plots
[01:39:53.000 --> 01:39:55.000]  that go now
[01:39:55.000 --> 01:39:57.000]  and so the really first one
[01:39:57.000 --> 01:39:59.000]  is occupancy
[01:39:59.000 --> 01:40:01.000]  with
[01:40:01.000 --> 01:40:03.000]  out injection
[01:40:03.000 --> 01:40:05.000]  is
[01:40:05.000 --> 01:40:07.000]  is done
[01:40:07.000 --> 01:40:09.000]  without an injection
[01:40:09.000 --> 01:40:11.000]  set the threshold in this case
[01:40:11.000 --> 01:40:13.000]  also the injection delay but it doesn't matter
[01:40:13.000 --> 01:40:15.000]  since we are not injecting
[01:40:15.000 --> 01:40:17.000]  and we read
[01:40:17.000 --> 01:40:19.000]  the occupancy
[01:40:19.000 --> 01:40:21.000]  and
[01:40:21.000 --> 01:40:23.000]  we are one for every chip
[01:40:23.000 --> 01:40:25.000]  so there might be some that you can actually
[01:40:25.000 --> 01:40:27.000]  see something
[01:40:27.000 --> 01:40:29.000]  but in general
[01:40:29.000 --> 01:40:31.000]  since we are five times the noise
[01:40:31.000 --> 01:40:33.000]  I don't see anything else here
[01:40:33.000 --> 01:40:35.000]  you shouldn't see
[01:40:35.000 --> 01:40:37.000]  basically anything
[01:40:37.000 --> 01:40:39.000]  or just a little
[01:40:39.000 --> 01:40:41.000]  very small amount
[01:40:41.000 --> 01:40:43.000]  of injected
[01:40:43.000 --> 01:40:45.000]  of
[01:40:49.000 --> 01:40:51.000]  and then
[01:40:51.000 --> 01:40:53.000]  after that we also
[01:40:53.000 --> 01:40:55.000]  inject
[01:40:55.000 --> 01:40:57.000]  close to much
[01:40:57.000 --> 01:40:59.000]  okay
[01:40:59.000 --> 01:41:01.000]  we inject
[01:41:01.000 --> 01:41:03.000]  a quarter of a meep
[01:41:03.000 --> 01:41:05.000]  and roughly a quarter of a meep
[01:41:05.000 --> 01:41:07.000]  correspond to the sigma
[01:41:07.000 --> 01:41:09.000]  that you set
[01:41:09.000 --> 01:41:11.000]  at
[01:41:11.000 --> 01:41:13.000]  when you use
[01:41:13.000 --> 01:41:15.000]  a pressure that is
[01:41:15.000 --> 01:41:17.000]  a five times the noise so that's why you get around
[01:41:17.000 --> 01:41:19.000]  50%
[01:41:19.000 --> 01:41:21.000]  so it means that if you have
[01:41:21.000 --> 01:41:23.000]  a quarter of a meep
[01:41:23.000 --> 01:41:25.000]  in your sensor you are going to have
[01:41:25.000 --> 01:41:27.000]  an efficiency of 50%
[01:41:27.000 --> 01:41:29.000]  it's important to
[01:41:29.000 --> 01:41:31.000]  also check smaller quantities
[01:41:31.000 --> 01:41:33.000]  than a meep
[01:41:33.000 --> 01:41:35.000]  of course because you
[01:41:35.000 --> 01:41:37.000]  just giving you the most probable value
[01:41:37.000 --> 01:41:39.000]  but you have lower value
[01:41:39.000 --> 01:41:41.000]  but even more important is that
[01:41:41.000 --> 01:41:43.000]  you need to
[01:41:43.000 --> 01:41:45.000]  you need to have
[01:41:45.000 --> 01:41:47.000]  good efficiency
[01:41:47.000 --> 01:41:49.000]  a lower amount of charge in order to increase the cluster size
[01:41:49.000 --> 01:41:51.000]  that's important for the resolution
[01:41:51.000 --> 01:41:53.000]  then
[01:41:53.000 --> 01:41:55.000]  we have other plots and it should be
[01:41:55.000 --> 01:41:57.000]  hopefully 100%
[01:41:57.000 --> 01:41:59.000]  so every time
[01:41:59.000 --> 01:42:01.000]  so this is the occupancy for every channel
[01:42:01.000 --> 01:42:03.000]  for
[01:42:03.000 --> 01:42:05.000]  a half a meep
[01:42:05.000 --> 01:42:07.000]  one meep and I close it
[01:42:07.000 --> 01:42:09.000]  too early and at two minutes
[01:42:09.000 --> 01:42:11.000]  so
[01:42:11.000 --> 01:42:13.000]  here
[01:42:13.000 --> 01:42:15.000]  in order to see
[01:42:15.000 --> 01:42:17.000]  if you have any troubles
[01:42:17.000 --> 01:42:19.000]  so okay just saying
[01:42:19.000 --> 01:42:21.000]  everything
[01:42:21.000 --> 01:42:23.000]  will be handled by potato
[01:42:23.000 --> 01:42:25.000]  so
[01:42:25.000 --> 01:42:27.000]  because there are tons of plot to be checked
[01:42:27.000 --> 01:42:29.000]  so I have to do it for a thousand of modules
[01:42:29.000 --> 01:42:31.000]  it's going to be really pain
[01:42:31.000 --> 01:42:33.000]  but I would say if you have any
[01:42:33.000 --> 01:42:35.000]  modules that are
[01:42:35.000 --> 01:42:37.000]  flagged as bad by potato
[01:42:37.000 --> 01:42:39.000]  then you can start looking to these plots
[01:42:39.000 --> 01:42:41.000]  and try to understand better what it is
[01:42:41.000 --> 01:42:43.000]  so you don't maybe at the beginning
[01:42:43.000 --> 01:42:45.000]  check them all but slowly
[01:42:45.000 --> 01:42:47.000]  with time you should rely more on potato
[01:42:47.000 --> 01:42:49.000]  and then just check the bad ones
[01:42:49.000 --> 01:42:51.000]  okay
[01:42:51.000 --> 01:42:53.000]  and you have one for every CBC
[01:42:53.000 --> 01:42:55.000]  so every CBC is going to be
[01:42:55.000 --> 01:42:57.000]  slightly different so you have this information
[01:42:57.000 --> 01:42:59.000]  for every CBC and every channel
[01:42:59.000 --> 01:43:01.000]  so this will allow you to
[01:43:01.000 --> 01:43:03.000]  inspect both
[01:43:03.000 --> 01:43:05.000]  channels with
[01:43:05.000 --> 01:43:07.000]  high noise because we'll stick out
[01:43:07.000 --> 01:43:09.000]  into these plots
[01:43:09.000 --> 01:43:11.000]  the new modules are pretty good so we really
[01:43:11.000 --> 01:43:13.000]  see something and you will also see
[01:43:13.000 --> 01:43:15.000]  channels with
[01:43:15.000 --> 01:43:17.000]  for any reason low efficiency because
[01:43:17.000 --> 01:43:19.000]  the comparator got damaged
[01:43:19.000 --> 01:43:21.000]  or something like that
[01:43:21.000 --> 01:43:23.000]  and we will spot them
[01:43:23.000 --> 01:43:25.000]  into these plots
[01:43:29.000 --> 01:43:31.000]  okay
[01:43:31.000 --> 01:43:33.000]  then
[01:43:33.000 --> 01:43:35.000]  common more noise
[01:43:35.000 --> 01:43:37.000]  so
[01:43:37.000 --> 01:43:39.000]  these are
[01:43:39.000 --> 01:43:41.000]  quite a lot of plots
[01:43:41.000 --> 01:43:43.000]  and I will for sure
[01:43:43.000 --> 01:43:45.000]  forget
[01:43:45.000 --> 01:43:47.000]  some of them
[01:43:47.000 --> 01:43:49.000]  so there is this one you want to
[01:43:51.000 --> 01:43:53.000]  to check
[01:43:53.000 --> 01:43:55.000]  if you have
[01:43:55.000 --> 01:43:57.000]  some
[01:43:57.000 --> 01:43:59.000]  basically
[01:43:59.000 --> 01:44:01.000]  cross interaction across
[01:44:01.000 --> 01:44:03.000]  channels so in theory
[01:44:03.000 --> 01:44:05.000]  on a perfect example you're going to have
[01:44:05.000 --> 01:44:07.000]  that every channel is perfectly independent
[01:44:07.000 --> 01:44:09.000]  from the others
[01:44:09.000 --> 01:44:11.000]  and therefore
[01:44:11.000 --> 01:44:13.000]  the noise
[01:44:17.000 --> 01:44:19.000]  the distribution
[01:44:19.000 --> 01:44:21.000]  of the
[01:44:23.000 --> 01:44:25.000]  of the number of hits
[01:44:25.000 --> 01:44:27.000]  that you record in every channel
[01:44:27.000 --> 01:44:29.000]  would be I think a portion
[01:44:29.000 --> 01:44:31.000]  in reality
[01:44:31.000 --> 01:44:33.000]  is not like that because
[01:44:33.000 --> 01:44:35.000]  the channels are belonging to the same chip
[01:44:35.000 --> 01:44:37.000]  so they share the same ground
[01:44:37.000 --> 01:44:39.000]  they share the same voltage
[01:44:39.000 --> 01:44:41.000]  and
[01:44:41.000 --> 01:44:43.000]  and physically the same
[01:44:43.000 --> 01:44:45.000]  they share the same
[01:44:45.000 --> 01:44:47.000]  piece of silicon if you speak about the same
[01:44:47.000 --> 01:44:49.000]  channel the channels are the same
[01:44:49.000 --> 01:44:51.000]  so
[01:44:51.000 --> 01:44:53.000]  here during this test
[01:44:53.000 --> 01:44:55.000]  we want to see if there is any correlation
[01:44:57.000 --> 01:44:59.000]  is honestly a little bit more
[01:44:59.000 --> 01:45:01.000]  tricky
[01:45:01.000 --> 01:45:03.000]  to look at them
[01:45:03.000 --> 01:45:05.000]  all the way through
[01:45:05.000 --> 01:45:07.000]  but
[01:45:07.000 --> 01:45:09.000]  I will say this
[01:45:09.000 --> 01:45:11.000]  will be end or mainly
[01:45:11.000 --> 01:45:13.000]  by potato but you can always check this
[01:45:13.000 --> 01:45:15.000]  if you see something that is standing up
[01:45:15.000 --> 01:45:17.000]  so
[01:45:17.000 --> 01:45:19.000]  these
[01:45:19.000 --> 01:45:21.000]  is the
[01:45:21.000 --> 01:45:23.000]  so we do actually two
[01:45:23.000 --> 01:45:25.000]  measurement of the common mode noise
[01:45:25.000 --> 01:45:27.000]  one that is
[01:45:27.000 --> 01:45:29.000]  called occupancy driven
[01:45:29.000 --> 01:45:31.000]  and these in the case
[01:45:31.000 --> 01:45:33.000]  of the twist is such that
[01:45:33.000 --> 01:45:35.000]  the occupancy expected occupancy
[01:45:35.000 --> 01:45:37.000]  is 50%
[01:45:37.000 --> 01:45:39.000]  we can do that because
[01:45:39.000 --> 01:45:41.000]  we already
[01:45:41.000 --> 01:45:43.000]  tuned
[01:45:43.000 --> 01:45:45.000]  the pedestal and therefore we know
[01:45:45.000 --> 01:45:47.000]  that
[01:45:47.000 --> 01:45:49.000]  once we set a threshold all the channels should be
[01:45:49.000 --> 01:45:51.000]  more or less to the same occupancy
[01:45:51.000 --> 01:45:53.000]  and in this particular case the target
[01:45:53.000 --> 01:45:55.000]  occupancy is 50%
[01:45:55.000 --> 01:45:57.000]  what you would expect in reality
[01:45:57.000 --> 01:45:59.000]  in any ideal case
[01:45:59.000 --> 01:46:01.000]  if you have a perfect distribution
[01:46:01.000 --> 01:46:03.000]  a perfect
[01:46:03.000 --> 01:46:05.000]  uncorrelated hips
[01:46:05.000 --> 01:46:07.000]  you will expect that
[01:46:07.000 --> 01:46:09.000]  you are a perfect person in distribution
[01:46:09.000 --> 01:46:11.000]  centered in
[01:46:11.000 --> 01:46:13.000]  half of the
[01:46:13.000 --> 01:46:15.000]  number of hits that you are
[01:46:15.000 --> 01:46:17.000]  the maximum number of hits that you are getting
[01:46:17.000 --> 01:46:19.000]  so you have a 256 channel
[01:46:19.000 --> 01:46:21.000]  set to
[01:46:21.000 --> 01:46:23.000]  S or 54 channels
[01:46:23.000 --> 01:46:25.000]  set to a threshold of
[01:46:25.000 --> 01:46:27.000]  such that every channel is 50%
[01:46:27.000 --> 01:46:29.000]  you expect the average
[01:46:29.000 --> 01:46:31.000]  number of hits in every event is
[01:46:31.000 --> 01:46:33.000]  half of the number of channels
[01:46:33.000 --> 01:46:35.000]  then in reality
[01:46:35.000 --> 01:46:37.000]  since you have
[01:46:37.000 --> 01:46:39.000]  the situation is a little bit more complex
[01:46:39.000 --> 01:46:41.000]  you have tail on the left
[01:46:41.000 --> 01:46:43.000]  and on the right
[01:46:43.000 --> 01:46:45.000]  and based on the width
[01:46:45.000 --> 01:46:47.000]  of how much events you have
[01:46:47.000 --> 01:46:49.000]  with
[01:46:49.000 --> 01:46:51.000]  low number of hits or high number of hits
[01:46:51.000 --> 01:46:53.000]  you can basically determine
[01:46:53.000 --> 01:46:55.000]  what is the rate of the common
[01:46:55.000 --> 01:46:57.000]  noise
[01:46:57.000 --> 01:46:59.000]  I would say just ignore the fit
[01:46:59.000 --> 01:47:01.000]  because we saw so far that the fit
[01:47:01.000 --> 01:47:03.000]  is a 2 and ideal case
[01:47:03.000 --> 01:47:05.000]  and we will probably remove it in the next version
[01:47:05.000 --> 01:47:07.000]  what we are actually
[01:47:07.000 --> 01:47:09.000]  doing in potatoes just we count
[01:47:09.000 --> 01:47:11.000]  many events we have
[01:47:11.000 --> 01:47:13.000]  in the tails and this will give us a good idea
[01:47:13.000 --> 01:47:15.000]  what is the amount
[01:47:15.000 --> 01:47:17.000]  of common noise
[01:47:17.000 --> 01:47:19.000]  and this was done at the level of the chip
[01:47:19.000 --> 01:47:21.000]  so common noise within the same chip
[01:47:21.000 --> 01:47:23.000]  then
[01:47:23.000 --> 01:47:25.000]  you also have
[01:47:25.000 --> 01:47:27.000]  a similar plot but just separated
[01:47:27.000 --> 01:47:29.000]  from the top and from the bottom
[01:47:29.000 --> 01:47:31.000]  because
[01:47:31.000 --> 01:47:33.000]  you can imagine that since
[01:47:33.000 --> 01:47:35.000]  these channels
[01:47:35.000 --> 01:47:37.000]  are all connected to the same sensor
[01:47:37.000 --> 01:47:39.000]  you might expect to have
[01:47:39.000 --> 01:47:41.000]  slightly different behavior from
[01:47:41.000 --> 01:47:43.000]  the top and the bottom
[01:47:43.000 --> 01:47:45.000]  sounds
[01:47:51.000 --> 01:47:53.000]  so if
[01:47:53.000 --> 01:47:55.000]  we should have also
[01:47:55.000 --> 01:47:57.000]  top-bottom correlation
[01:47:57.000 --> 01:47:59.000]  so basically this is the same plot
[01:47:59.000 --> 01:48:01.000]  the same two plots that are shown here
[01:48:01.000 --> 01:48:03.000]  but instead just showing the
[01:48:03.000 --> 01:48:05.000]  two plots
[01:48:05.000 --> 01:48:07.000]  separately now we
[01:48:07.000 --> 01:48:09.000]  show them also in
[01:48:09.000 --> 01:48:11.000]  a correlation plot
[01:48:11.000 --> 01:48:13.000]  so you see that actually there is
[01:48:13.000 --> 01:48:15.000]  correlation because there is
[01:48:15.000 --> 01:48:17.000]  a diagonal
[01:48:17.000 --> 01:48:19.000]  you don't have a perfect center
[01:48:19.000 --> 01:48:21.000]  honestly
[01:48:21.000 --> 01:48:23.000]  nothing really concerning for what we see
[01:48:23.000 --> 01:48:25.000]  just that there are
[01:48:25.000 --> 01:48:27.000]  everything these objects are on the same chip
[01:48:27.000 --> 01:48:29.000]  so you kind of expect
[01:48:29.000 --> 01:48:31.000]  a sort of a bit of correlation
[01:48:31.000 --> 01:48:33.000]  but nothing really
[01:48:33.000 --> 01:48:35.000]  too serious
[01:48:35.000 --> 01:48:37.000]  and then
[01:48:37.000 --> 01:48:39.000]  nothing
[01:48:39.000 --> 01:48:41.000]  no sorry
[01:48:41.000 --> 01:48:43.000]  I went too far
[01:48:47.000 --> 01:48:49.000]  okay yeah I went too far
[01:48:49.000 --> 01:48:51.000]  and this was
[01:48:51.000 --> 01:48:53.000]  at the level of
[01:48:53.000 --> 01:48:55.000]  the
[01:48:55.000 --> 01:48:57.000]  chip
[01:48:57.000 --> 01:48:59.000]  then we had the same
[01:48:59.000 --> 01:49:01.000]  information on the level of the hybrid
[01:49:03.000 --> 01:49:05.000]  and as before
[01:49:05.000 --> 01:49:07.000]  we have the distribution
[01:49:07.000 --> 01:49:09.000]  of
[01:49:09.000 --> 01:49:11.000]  number of hits per event
[01:49:11.000 --> 01:49:13.000]  in
[01:49:15.000 --> 01:49:17.000]  the overall hybrid so in this case
[01:49:17.000 --> 01:49:19.000]  it can go from 0 to
[01:49:19.000 --> 01:49:21.000]  2096
[01:49:23.000 --> 01:49:25.000]  maybe
[01:49:25.000 --> 01:49:27.000]  I'll remember
[01:49:27.000 --> 01:49:29.000]  is the 224 by 8
[01:49:33.000 --> 01:49:35.000]  and
[01:49:35.000 --> 01:49:37.000]  as for before we have
[01:49:37.000 --> 01:49:39.000]  them separated from the top sensor
[01:49:39.000 --> 01:49:41.000]  and the bottom sensor
[01:49:41.000 --> 01:49:43.000]  and as for before we have the correlation
[01:49:43.000 --> 01:49:45.000]  between the top
[01:49:45.000 --> 01:49:47.000]  strips and the bottom strips
[01:49:47.000 --> 01:49:49.000]  and also see
[01:49:49.000 --> 01:49:51.000]  there is quite a big correlation
[01:49:51.000 --> 01:49:53.000]  again this is the reality
[01:49:53.000 --> 01:49:55.000]  and I don't think there is
[01:49:55.000 --> 01:49:57.000]  too much to do
[01:49:57.000 --> 01:49:59.000]  so
[01:49:59.000 --> 01:50:01.000]  I will not worry about too much
[01:50:01.000 --> 01:50:03.000]  about this
[01:50:03.000 --> 01:50:05.000]  and
[01:50:05.000 --> 01:50:07.000]  I think that's all of the level of the hybrid
[01:50:07.000 --> 01:50:09.000]  and we have also
[01:50:09.000 --> 01:50:11.000]  something on the level of the
[01:50:13.000 --> 01:50:15.000]  module
[01:50:15.000 --> 01:50:17.000]  let me close that
[01:50:21.000 --> 01:50:23.000]  sorry
[01:50:23.000 --> 01:50:25.000]  common noise
[01:50:25.000 --> 01:50:27.000]  at the level of the module
[01:50:29.000 --> 01:50:31.000]  as before here you have
[01:50:31.000 --> 01:50:33.000]  more than 4000 strips
[01:50:33.000 --> 01:50:35.000]  and as before
[01:50:35.000 --> 01:50:37.000]  we have the
[01:50:37.000 --> 01:50:39.000]  separated in top and bottom
[01:50:39.000 --> 01:50:41.000]  and we have
[01:50:41.000 --> 01:50:43.000]  the correlation
[01:50:45.000 --> 01:50:47.000]  between
[01:50:47.000 --> 01:50:49.000]  top and bottom
[01:50:49.000 --> 01:50:51.000]  and
[01:50:51.000 --> 01:50:53.000]  you see that here is a little bit less
[01:50:53.000 --> 01:50:55.000]  correlated because I put in two
[01:50:55.000 --> 01:50:57.000]  hybrids and the two hybrids
[01:50:57.000 --> 01:50:59.000]  should be a bit less correlated
[01:50:59.000 --> 01:51:01.000]  and
[01:51:01.000 --> 01:51:03.000]  they are actually
[01:51:03.000 --> 01:51:05.000]  uncorrelated
[01:51:05.000 --> 01:51:07.000]  between left and right side
[01:51:07.000 --> 01:51:09.000]  so
[01:51:09.000 --> 01:51:11.000]  I honestly struggle a little bit more
[01:51:11.000 --> 01:51:13.000]  to interpret these results
[01:51:13.000 --> 01:51:15.000]  because this is
[01:51:15.000 --> 01:51:17.000]  the reality of the module works
[01:51:17.000 --> 01:51:19.000]  so
[01:51:19.000 --> 01:51:21.000]  I will just say if you see
[01:51:21.000 --> 01:51:23.000]  some particular
[01:51:23.000 --> 01:51:25.000]  tail on some particular
[01:51:25.000 --> 01:51:27.000]  weird noise into the
[01:51:27.000 --> 01:51:29.000]  results from the pedenoise
[01:51:29.000 --> 01:51:31.000]  I would suggest
[01:51:31.000 --> 01:51:33.000]  to check these plots
[01:51:33.000 --> 01:51:35.000]  to see if
[01:51:35.000 --> 01:51:37.000]  for any reason these plots are
[01:51:37.000 --> 01:51:39.000]  starting out particularly
[01:51:39.000 --> 01:51:41.000]  if you have any particular
[01:51:41.000 --> 01:51:43.000]  with the distribution
[01:51:43.000 --> 01:51:45.000]  into these and I will take
[01:51:45.000 --> 01:51:47.000]  as a reference
[01:51:47.000 --> 01:51:49.000]  other
[01:51:49.000 --> 01:51:51.000]  common noise plots
[01:51:51.000 --> 01:51:53.000]  from the
[01:51:53.000 --> 01:51:55.000]  module that
[01:51:55.000 --> 01:51:57.000]  we can see that the
[01:51:57.000 --> 01:51:59.000]  noise distribution
[01:51:59.000 --> 01:52:01.000]  from the pedenoise results
[01:52:01.000 --> 01:52:03.000]  are the expected one
[01:52:03.000 --> 01:52:05.000]  with form moving on
[01:52:05.000 --> 01:52:07.000]  I just
[01:52:07.000 --> 01:52:09.000]  skip
[01:52:09.000 --> 01:52:11.000]  completely the other
[01:52:11.000 --> 01:52:13.000]  common more noise that are done
[01:52:13.000 --> 01:52:15.000]  so if I enlarge
[01:52:15.000 --> 01:52:17.000]  a little bit you see
[01:52:17.000 --> 01:52:19.000]  that all these common more noise
[01:52:19.000 --> 01:52:21.000]  that I show you so far have these occupancy
[01:52:21.000 --> 01:52:23.000]  driven
[01:52:23.000 --> 01:52:25.000]  but then after
[01:52:25.000 --> 01:52:27.000]  Giovanni suggested
[01:52:27.000 --> 01:52:29.000]  we also included another
[01:52:29.000 --> 01:52:31.000]  measurement that is
[01:52:31.000 --> 01:52:33.000]  measured except the same
[01:52:33.000 --> 01:52:35.000]  plot but at three
[01:52:35.000 --> 01:52:37.000]  sigma
[01:52:37.000 --> 01:52:39.000]  and the reason
[01:52:39.000 --> 01:52:41.000]  for that
[01:52:41.000 --> 01:52:43.000]  is that is also another way
[01:52:43.000 --> 01:52:45.000]  to better visualize
[01:52:45.000 --> 01:52:47.000]  the effect of the noise so when you
[01:52:47.000 --> 01:52:49.000]  look at the
[01:52:49.000 --> 01:52:51.000]  zero sigma
[01:52:51.000 --> 01:52:53.000]  occupancy that
[01:52:53.000 --> 01:52:55.000]  is expected to be a 50%
[01:52:55.000 --> 01:52:57.000]  you can look both
[01:52:57.000 --> 01:52:59.000]  the left
[01:52:59.000 --> 01:53:01.000]  tail and the right
[01:53:01.000 --> 01:53:03.000]  tail because you don't expect
[01:53:03.000 --> 01:53:05.000]  that all the channel fire at the same time
[01:53:05.000 --> 01:53:07.000]  instead for the three sigma
[01:53:07.000 --> 01:53:09.000]  you can focus just
[01:53:09.000 --> 01:53:11.000]  on the right tail
[01:53:11.000 --> 01:53:13.000]  and so if I zoom in you see that
[01:53:13.000 --> 01:53:15.000]  it's basically the same idea
[01:53:15.000 --> 01:53:17.000]  but focusing mainly
[01:53:17.000 --> 01:53:19.000]  to the right tail so
[01:53:19.000 --> 01:53:21.000]  I will say this is just another way
[01:53:21.000 --> 01:53:23.000]  to see exactly the same results
[01:53:23.000 --> 01:53:25.000]  but for much like a different perspective
[01:53:25.000 --> 01:53:27.000]  so focusing just on the right
[01:53:27.000 --> 01:53:29.000]  tail and
[01:53:29.000 --> 01:53:31.000]  not on the left
[01:53:31.000 --> 01:53:33.000]  and the right tail
[01:53:33.000 --> 01:53:35.000]  so
[01:53:35.000 --> 01:53:37.000]  given the amount of data
[01:53:37.000 --> 01:53:39.000]  we collected so far we don't see
[01:53:39.000 --> 01:53:41.000]  much of
[01:53:41.000 --> 01:53:43.000]  a reason to choose
[01:53:43.000 --> 01:53:45.000]  to do the QA on one or the two
[01:53:45.000 --> 01:53:47.000]  so since we're at the beginning
[01:53:47.000 --> 01:53:49.000]  we decided to include both of them
[01:53:49.000 --> 01:53:51.000]  the time required to
[01:53:51.000 --> 01:53:53.000]  collect this data is very short
[01:53:53.000 --> 01:53:55.000]  so there was basically no
[01:53:55.000 --> 01:53:57.000]  go back and
[01:53:57.000 --> 01:53:59.000]  we now have available both
[01:53:59.000 --> 01:54:01.000]  so I would say
[01:54:01.000 --> 01:54:03.000]  basically
[01:54:03.000 --> 01:54:05.000]  the reference of the user
[01:54:05.000 --> 01:54:07.000]  you can focus more on this
[01:54:07.000 --> 01:54:09.000]  this one is lightly more obvious
[01:54:09.000 --> 01:54:11.000]  because you just see the tail
[01:54:11.000 --> 01:54:13.000]  on one side the other one
[01:54:13.000 --> 01:54:15.000]  might be slightly more
[01:54:15.000 --> 01:54:17.000]  tricky because you have to consider both
[01:54:17.000 --> 01:54:19.000]  that you said
[01:54:21.000 --> 01:54:23.000]  okay so
[01:54:23.000 --> 01:54:25.000]  for the
[01:54:25.000 --> 01:54:27.000]  occupancy driven that is zero
[01:54:27.000 --> 01:54:29.000]  sigma and the three sigma
[01:54:29.000 --> 01:54:31.000]  plots are identical so I'm
[01:54:31.000 --> 01:54:33.000]  just not going to go through all of them
[01:54:33.000 --> 01:54:35.000]  the information that we
[01:54:35.000 --> 01:54:37.000]  do is exactly the same
[01:54:37.000 --> 01:54:39.000]  the only difference is that now
[01:54:39.000 --> 01:54:41.000]  the plots are all shifted to the left
[01:54:41.000 --> 01:54:43.000]  because we expect
[01:54:43.000 --> 01:54:45.000]  low average occupancy
[01:54:51.000 --> 01:54:53.000]  zero as well
[01:54:53.000 --> 01:54:55.000]  when the person just told me
[01:54:55.000 --> 01:54:57.000]  sorry
[01:54:57.000 --> 01:54:59.000]  so we covered this
[01:54:59.000 --> 01:55:01.000]  okay
[01:55:01.000 --> 01:55:03.000]  now
[01:55:03.000 --> 01:55:05.000]  we go
[01:55:05.000 --> 01:55:07.000]  add
[01:55:07.000 --> 01:55:09.000]  to the
[01:55:09.000 --> 01:55:11.000]  electric chain validation
[01:55:11.000 --> 01:55:13.000]  so this is going to be
[01:55:13.000 --> 01:55:15.000]  a bit complicated
[01:55:15.000 --> 01:55:17.000]  and there are really
[01:55:17.000 --> 01:55:19.000]  a lot of plots
[01:55:19.000 --> 01:55:21.000]  but I think the main concept
[01:55:21.000 --> 01:55:23.000]  is that if you
[01:55:23.000 --> 01:55:25.000]  understood
[01:55:25.000 --> 01:55:27.000]  how these
[01:55:27.000 --> 01:55:29.000]  verify steps work
[01:55:31.000 --> 01:55:33.000]  then at this point
[01:55:33.000 --> 01:55:35.000]  you understood
[01:55:37.000 --> 01:55:39.000]  you understand
[01:55:39.000 --> 01:55:41.000]  what the electric chain validation does
[01:55:41.000 --> 01:55:43.000]  because the only difference with respect to the electric chain
[01:55:43.000 --> 01:55:45.000]  to the verified plot
[01:55:45.000 --> 01:55:47.000]  is that during the electric chain validation
[01:55:47.000 --> 01:55:49.000]  we don't choose
[01:55:49.000 --> 01:55:51.000]  only the phase
[01:55:51.000 --> 01:55:53.000]  that is identified by
[01:55:53.000 --> 01:55:55.000]  either the CIC
[01:55:55.000 --> 01:55:57.000]  or the LpGBT to be the best one
[01:55:57.000 --> 01:55:59.000]  we change them manually
[01:55:59.000 --> 01:56:01.000]  because we want to see how wide
[01:56:01.000 --> 01:56:03.000]  is the work in area
[01:56:03.000 --> 01:56:05.000]  you don't want to have
[01:56:05.000 --> 01:56:07.000]  a single phase that works
[01:56:07.000 --> 01:56:09.000]  because it means that
[01:56:09.000 --> 01:56:11.000]  you are a little bit at the edge
[01:56:11.000 --> 01:56:13.000]  as you might have that
[01:56:13.000 --> 01:56:15.000]  things might be working now
[01:56:15.000 --> 01:56:17.000]  and after installing to the detector
[01:56:17.000 --> 01:56:19.000]  and that single phase is not anymore
[01:56:19.000 --> 01:56:21.000]  so the whole idea
[01:56:21.000 --> 01:56:23.000]  of the electric chain validation
[01:56:23.000 --> 01:56:25.000]  is how wide is the work in area
[01:56:25.000 --> 01:56:27.000]  so
[01:56:27.000 --> 01:56:29.000]  I'm going to start
[01:56:29.000 --> 01:56:31.000]  from the
[01:56:31.000 --> 01:56:33.000]  CIC to LpGBT validation
[01:56:33.000 --> 01:56:35.000]  so we are basically
[01:56:35.000 --> 01:56:37.000]  looking at this phase
[01:56:37.000 --> 01:56:39.000]  these lines over here
[01:56:39.000 --> 01:56:41.000]  so we change
[01:56:41.000 --> 01:56:43.000]  the sampling phase of the LpGBT
[01:56:43.000 --> 01:56:45.000]  and we see if
[01:56:45.000 --> 01:56:47.000]  the signal
[01:56:47.000 --> 01:56:49.000]  the
[01:56:49.000 --> 01:56:51.000]  pattern that are sent by the CIC
[01:56:51.000 --> 01:56:53.000]  are properly
[01:56:53.000 --> 01:56:55.000]  reconstructed into the SPGA
[01:56:57.000 --> 01:56:59.000]  for each of these phase
[01:56:59.000 --> 01:57:01.000]  so the plots
[01:57:01.000 --> 01:57:03.000]  are saved
[01:57:03.000 --> 01:57:05.000]  at the high B level
[01:57:05.000 --> 01:57:07.000]  and
[01:57:07.000 --> 01:57:09.000]  all the plots are
[01:57:09.000 --> 01:57:11.000]  CIC to LpGBT
[01:57:11.000 --> 01:57:13.000]  pattern matching
[01:57:13.000 --> 01:57:15.000]  and for each one of these
[01:57:15.000 --> 01:57:17.000]  there is the error rate
[01:57:17.000 --> 01:57:19.000]  and the tested bits
[01:57:19.000 --> 01:57:21.000]  as before, let's open one quickly
[01:57:21.000 --> 01:57:23.000]  here is the number of bits
[01:57:23.000 --> 01:57:25.000]  that we
[01:57:25.000 --> 01:57:27.000]  tested for every one of these lines
[01:57:27.000 --> 01:57:29.000]  and
[01:57:29.000 --> 01:57:31.000]  these
[01:57:31.000 --> 01:57:33.000]  are the number of
[01:57:33.000 --> 01:57:35.000]  error that we measure
[01:57:35.000 --> 01:57:37.000]  for every line. I'm going to
[01:57:37.000 --> 01:57:39.000]  go more into details, no more about it
[01:57:39.000 --> 01:57:41.000]  then
[01:57:41.000 --> 01:57:43.000]  so
[01:57:43.000 --> 01:57:45.000]  what we can also change
[01:57:45.000 --> 01:57:47.000]  is
[01:57:47.000 --> 01:57:49.000]  the
[01:57:49.000 --> 01:57:51.000]  strength
[01:57:51.000 --> 01:57:53.000]  of the current used
[01:57:53.000 --> 01:57:55.000]  to drive the lines
[01:57:55.000 --> 01:57:57.000]  by the CIC
[01:57:57.000 --> 01:57:59.000]  so the CIC is sending data
[01:57:59.000 --> 01:58:01.000]  to these and you can set
[01:58:01.000 --> 01:58:03.000]  how much current is used
[01:58:03.000 --> 01:58:05.000]  to drive the lines
[01:58:05.000 --> 01:58:07.000]  and
[01:58:07.000 --> 01:58:09.000]  you can expect that by changing these
[01:58:09.000 --> 01:58:11.000]  you can, sorry
[01:58:11.000 --> 01:58:13.000]  to change these current
[01:58:13.000 --> 01:58:15.000]  you might have different
[01:58:15.000 --> 01:58:17.000]  behavior in the LpGBT
[01:58:17.000 --> 01:58:19.000]  reconstructing them
[01:58:19.000 --> 01:58:21.000]  then we have the
[01:58:21.000 --> 01:58:23.000]  LpGBT clock polarity
[01:58:23.000 --> 01:58:25.000]  so the LpGBT sends
[01:58:25.000 --> 01:58:27.000]  the
[01:58:27.000 --> 01:58:29.000]  provides the clock
[01:58:29.000 --> 01:58:31.000]  to the
[01:58:31.000 --> 01:58:33.000]  hybrid so
[01:58:33.000 --> 01:58:35.000]  you reconstruct the clock from the incoming data
[01:58:35.000 --> 01:58:37.000]  and provides it to the hybrid
[01:58:37.000 --> 01:58:39.000]  and you can set the polarity of the clock so
[01:58:39.000 --> 01:58:41.000]  you can basically change its phase
[01:58:41.000 --> 01:58:43.000]  by 50%
[01:58:43.000 --> 01:58:45.000]  and these are
[01:58:45.000 --> 01:58:47.000]  see that we do both of them
[01:58:47.000 --> 01:58:49.000]  and finally
[01:58:49.000 --> 01:58:51.000]  the CIC clock strength
[01:58:51.000 --> 01:58:53.000]  as for these lines
[01:58:53.000 --> 01:58:55.000]  the LpGBT sends the clock
[01:58:55.000 --> 01:58:57.000]  and you can change the current
[01:58:57.000 --> 01:58:59.000]  that is used to drive the line of the clock
[01:58:59.000 --> 01:59:01.000]  so I have to
[01:59:01.000 --> 01:59:03.000]  set this by an overkill but this was
[01:59:03.000 --> 01:59:05.000]  at the beginning so we were not sure
[01:59:05.000 --> 01:59:07.000]  how much we need to test so
[01:59:07.000 --> 01:59:09.000]  we can imagine that in the future we can
[01:59:09.000 --> 01:59:11.000]  drop some of these
[01:59:11.000 --> 01:59:13.000]  so I'm just going to focus
[01:59:13.000 --> 01:59:15.000]  probably on one of them
[01:59:27.000 --> 01:59:29.000]  let me try to see if there is some
[01:59:29.000 --> 01:59:31.000]  a little bit more
[01:59:31.000 --> 01:59:33.000]  representative so maybe
[01:59:33.000 --> 01:59:35.000]  so it will be clear
[01:59:35.000 --> 01:59:37.000]  because
[01:59:37.000 --> 01:59:39.000]  these phases are usually
[01:59:39.000 --> 01:59:41.000]  pretty good
[01:59:41.000 --> 01:59:43.000]  so you don't see too much
[01:59:43.000 --> 01:59:45.000]  which is good which means that
[01:59:45.000 --> 01:59:47.000]  basically
[01:59:47.000 --> 01:59:49.000]  several of these phases
[01:59:49.000 --> 01:59:51.000]  keep open the problem
[01:59:51.000 --> 01:59:53.000]  several of the phases
[01:59:53.000 --> 01:59:55.000]  are going to be
[01:59:55.000 --> 01:59:57.000]  good for working
[01:59:57.000 --> 01:59:59.000]  maybe this one is better
[01:59:59.000 --> 02:00:01.000]  ok
[02:00:03.000 --> 02:00:05.000]  let's take this
[02:00:05.000 --> 02:00:07.000]  so in this plot
[02:00:07.000 --> 02:00:09.000]  what we show is that
[02:00:09.000 --> 02:00:11.000]  for each on the Y axis
[02:00:11.000 --> 02:00:13.000]  we show the line
[02:00:13.000 --> 02:00:15.000]  ID so these
[02:00:15.000 --> 02:00:17.000]  are the lines
[02:00:19.000 --> 02:00:21.000]  and on the X axis
[02:00:21.000 --> 02:00:23.000]  we show the
[02:00:23.000 --> 02:00:25.000]  phase that is selected
[02:00:25.000 --> 02:00:27.000]  so the phase can go from 0 to
[02:00:27.000 --> 02:00:29.000]  14 so we change it manually
[02:00:29.000 --> 02:00:31.000]  the LPGPT phase
[02:00:31.000 --> 02:00:33.000]  we don't let the LPGPT adjust
[02:00:33.000 --> 02:00:35.000]  we change it manually because we also want
[02:00:35.000 --> 02:00:37.000]  to work in the area where the LPGPT doesn't
[02:00:37.000 --> 02:00:39.000]  see anything
[02:00:39.000 --> 02:00:41.000]  and then on the Y axis
[02:00:41.000 --> 02:00:43.000]  the Z axis
[02:00:43.000 --> 02:00:45.000]  we add a number of test bits
[02:00:45.000 --> 02:00:47.000]  and as before
[02:00:47.000 --> 02:00:49.000]  the stub pattern matches down the firmware
[02:00:49.000 --> 02:00:51.000]  to be faster while the level 1
[02:00:51.000 --> 02:00:53.000]  pattern matches down on the
[02:00:53.000 --> 02:00:55.000]  VR software and that's why it takes
[02:00:55.000 --> 02:00:57.000]  longer time
[02:00:57.000 --> 02:00:59.000]  and therefore
[02:00:59.000 --> 02:01:01.000]  we don't scan as many bits
[02:01:01.000 --> 02:01:03.000]  as for the stubs
[02:01:03.000 --> 02:01:05.000]  and then for each one of these
[02:01:05.000 --> 02:01:07.000]  we get a plot like this
[02:01:07.000 --> 02:01:09.000]  where X and Y axes are the same
[02:01:09.000 --> 02:01:11.000]  so the lines
[02:01:11.000 --> 02:01:13.000]  and the phases
[02:01:13.000 --> 02:01:15.000]  why the Z axis
[02:01:15.000 --> 02:01:17.000]  is the number
[02:01:17.000 --> 02:01:19.000]  there already
[02:01:19.000 --> 02:01:21.000]  it's written a bit small
[02:01:21.000 --> 02:01:23.000]  but it's written the time
[02:01:23.000 --> 02:01:25.000]  so
[02:01:25.000 --> 02:01:27.000]  you can expect that some
[02:01:27.000 --> 02:01:29.000]  of the lines will not work
[02:01:29.000 --> 02:01:31.000]  because you are sampling
[02:01:31.000 --> 02:01:33.000]  the incoming data from the SSC
[02:01:33.000 --> 02:01:35.000]  when the incoming data are transitioning
[02:01:35.000 --> 02:01:37.000]  and this is definitely not a good place
[02:01:37.000 --> 02:01:39.000]  because you are misinterpreting
[02:01:39.000 --> 02:01:41.000]  the Z on Y
[02:01:41.000 --> 02:01:43.000]  so
[02:01:43.000 --> 02:01:45.000]  how we identify a good module
[02:01:45.000 --> 02:01:47.000]  is basically
[02:01:47.000 --> 02:01:49.000]  how wide is the area
[02:01:49.000 --> 02:01:51.000]  in which you can find a good phase
[02:01:51.000 --> 02:01:53.000]  so for example here
[02:01:53.000 --> 02:01:55.000]  the
[02:01:55.000 --> 02:01:57.000]  the width of the phase actually
[02:01:57.000 --> 02:01:59.000]  the one on the left but just
[02:01:59.000 --> 02:02:01.000]  let's pretend that something here is bad
[02:02:01.000 --> 02:02:03.000]  the width of
[02:02:03.000 --> 02:02:05.000]  the phase in which you can work
[02:02:05.000 --> 02:02:07.000]  is this amount
[02:02:07.000 --> 02:02:09.000]  imagine you have
[02:02:09.000 --> 02:02:11.000]  a really bad scenario in which
[02:02:11.000 --> 02:02:13.000]  you have
[02:02:13.000 --> 02:02:15.000]  several values
[02:02:15.000 --> 02:02:17.000]  which you don't have a good phase
[02:02:17.000 --> 02:02:19.000]  and maybe I think I spot one before
[02:02:21.000 --> 02:02:23.000]  yes
[02:02:23.000 --> 02:02:25.000]  so
[02:02:29.000 --> 02:02:31.000]  yes
[02:02:31.000 --> 02:02:33.000]  so
[02:02:33.000 --> 02:02:35.000]  this one is a really bad scenario
[02:02:35.000 --> 02:02:37.000]  because you see that basically
[02:02:37.000 --> 02:02:39.000]  there is no phase in which
[02:02:39.000 --> 02:02:41.000]  things are working fine
[02:02:41.000 --> 02:02:43.000]  why that?
[02:02:43.000 --> 02:02:45.000]  because we are very likely using
[02:02:45.000 --> 02:02:47.000]  a combination clock polarity
[02:02:47.000 --> 02:02:49.000]  and to abstract that are not
[02:02:49.000 --> 02:02:51.000]  as obvious current is the minimum
[02:02:51.000 --> 02:02:53.000]  and basically you cannot
[02:02:53.000 --> 02:02:55.000]  find any phases in which things are working
[02:02:55.000 --> 02:02:57.000]  you can imagine also that
[02:02:57.000 --> 02:02:59.000]  you have something in between in which
[02:02:59.000 --> 02:03:01.000]  for example this one is a zero
[02:03:01.000 --> 02:03:03.000]  and you have just one phase that works
[02:03:03.000 --> 02:03:05.000]  and this would be a bad mortgage because
[02:03:05.000 --> 02:03:07.000]  you are really relying on this phase
[02:03:07.000 --> 02:03:09.000]  and never ever being
[02:03:09.000 --> 02:03:11.000]  always being good and which is unsafe
[02:03:11.000 --> 02:03:13.000]  so basically the distance between
[02:03:13.000 --> 02:03:15.000]  at the length
[02:03:15.000 --> 02:03:17.000]  of which you get zero error
[02:03:17.000 --> 02:03:19.000]  indicates you how wide is
[02:03:19.000 --> 02:03:21.000]  the working area
[02:03:21.000 --> 02:03:23.000]  and how well your module
[02:03:23.000 --> 02:03:25.000]  can absorb
[02:03:25.000 --> 02:03:27.000]  variation in that
[02:03:27.000 --> 02:03:29.000]  standard condition that may change
[02:03:29.000 --> 02:03:31.000]  the width of the phase
[02:03:31.000 --> 02:03:33.000]  so as I was saying you have really a lot of them
[02:03:33.000 --> 02:03:35.000]  because it's a
[02:03:35.000 --> 02:03:37.000]  scanner multi-parameters so we are changing
[02:03:37.000 --> 02:03:39.000]  the
[02:03:39.000 --> 02:03:41.000]  C-C clock
[02:03:41.000 --> 02:03:43.000]  SLBS current, the LpGBT clock polarity
[02:03:43.000 --> 02:03:45.000]  and the LpGBT clock strength
[02:03:45.000 --> 02:03:47.000]  so you have really a lot of them
[02:03:47.000 --> 02:03:49.000]  you don't need to look
[02:03:49.000 --> 02:03:51.000]  all of them
[02:03:51.000 --> 02:03:53.000]  potato will do the job
[02:03:53.000 --> 02:03:55.000]  but this is the typical plot that you
[02:03:55.000 --> 02:03:57.000]  want to do
[02:03:57.000 --> 02:03:59.000]  once you see that
[02:03:59.000 --> 02:04:01.000]  you have problem with alignment
[02:04:01.000 --> 02:04:03.000]  of these kind of things that
[02:04:03.000 --> 02:04:05.000]  it prevents you to have a stable communication
[02:04:05.000 --> 02:04:07.000]  a lot of errors in communicating
[02:04:07.000 --> 02:04:09.000]  in a
[02:04:09.000 --> 02:04:11.000]  decoding the events
[02:04:11.000 --> 02:04:13.000]  these kind of things may indicate
[02:04:13.000 --> 02:04:15.000]  that your working range is
[02:04:15.000 --> 02:04:17.000]  small and
[02:04:17.000 --> 02:04:19.000]  even if the LpGBT can align
[02:04:19.000 --> 02:04:21.000]  the C-C
[02:04:21.000 --> 02:04:23.000]  it might not be a good alignment
[02:04:23.000 --> 02:04:25.000]  you just found a phase but then
[02:04:25.000 --> 02:04:27.000]  that phase is not really stable
[02:04:29.000 --> 02:04:31.000]  so
[02:04:31.000 --> 02:04:33.000]  let me open just another one
[02:04:33.000 --> 02:04:35.000]  so for example this one is a pretty good one
[02:04:35.000 --> 02:04:37.000]  you see that you have a wide range
[02:04:37.000 --> 02:04:39.000]  of areas in which things are working fine
[02:04:43.000 --> 02:04:45.000]  any question on this
[02:04:45.000 --> 02:04:47.000]  because basically all the electric chain validation
[02:04:47.000 --> 02:04:49.000]  will look like these
[02:04:49.000 --> 02:04:51.000]  so you see
[02:04:51.000 --> 02:04:53.000]  how we are plotting this
[02:04:53.000 --> 02:04:55.000]  basically you understand how we are plotting all the electric chain validation
[02:05:07.000 --> 02:05:09.000]  then next step
[02:05:09.000 --> 02:05:11.000]  of the electric chain validation
[02:05:11.000 --> 02:05:13.000]  is actually an auxiliary step
[02:05:15.000 --> 02:05:17.000]  you kind of remember before
[02:05:17.000 --> 02:05:19.000]  I mentioned briefly
[02:05:19.000 --> 02:05:21.000]  in the plots
[02:05:21.000 --> 02:05:23.000]  at the beginning
[02:05:23.000 --> 02:05:25.000]  let me open one again
[02:05:31.000 --> 02:05:33.000]  yes, okay in this one
[02:05:33.000 --> 02:05:35.000]  you kind of remember
[02:05:35.000 --> 02:05:37.000]  I was telling you so here
[02:05:37.000 --> 02:05:39.000]  despite the fact that we have five lines
[02:05:39.000 --> 02:05:41.000]  we are just going to show one of them
[02:05:41.000 --> 02:05:43.000]  because we cannot really distinguish
[02:05:43.000 --> 02:05:45.000]  since the C-C does a risk rambling
[02:05:45.000 --> 02:05:47.000]  we cannot really distinguish
[02:05:47.000 --> 02:05:49.000]  between
[02:05:49.000 --> 02:05:51.000]  in which line an error occurs
[02:05:53.000 --> 02:05:55.000]  this is
[02:05:55.000 --> 02:05:57.000]  partially true
[02:05:57.000 --> 02:05:59.000]  we can actually set the C-C
[02:05:59.000 --> 02:06:01.000]  in bypass mode
[02:06:01.000 --> 02:06:03.000]  so basically you can
[02:06:03.000 --> 02:06:05.000]  ask the C-C
[02:06:05.000 --> 02:06:07.000]  just to forward the incoming
[02:06:07.000 --> 02:06:09.000]  data
[02:06:09.000 --> 02:06:11.000]  to the LpGBT
[02:06:11.000 --> 02:06:13.000]  without any processing
[02:06:13.000 --> 02:06:15.000]  this is
[02:06:15.000 --> 02:06:17.000]  a bit more complicated
[02:06:17.000 --> 02:06:19.000]  for a few reasons
[02:06:19.000 --> 02:06:21.000]  the first one is that
[02:06:21.000 --> 02:06:23.000]  each CBC has
[02:06:23.000 --> 02:06:25.000]  five lines
[02:06:25.000 --> 02:06:27.000]  actually six lines
[02:06:27.000 --> 02:06:29.000]  in input to the C-C
[02:06:29.000 --> 02:06:31.000]  so there are six times eight
[02:06:31.000 --> 02:06:33.000]  so 48 lines going to the C-C
[02:06:33.000 --> 02:06:35.000]  but only
[02:06:35.000 --> 02:06:37.000]  six coming out from the C-C
[02:06:37.000 --> 02:06:39.000]  to LpGBT
[02:06:39.000 --> 02:06:41.000]  so you cannot forward all of them in one shot
[02:06:41.000 --> 02:06:43.000]  so you have to choose what you
[02:06:43.000 --> 02:06:45.000]  want to forward
[02:06:45.000 --> 02:06:47.000]  and then on top of that
[02:06:49.000 --> 02:06:51.000]  you cannot forward
[02:06:51.000 --> 02:06:53.000]  out by construction
[02:06:53.000 --> 02:06:55.000]  all the lines
[02:06:55.000 --> 02:06:57.000]  for the one CBC
[02:06:57.000 --> 02:06:59.000]  but you can forward only
[02:06:59.000 --> 02:07:01.000]  four
[02:07:01.000 --> 02:07:03.000]  and these four
[02:07:03.000 --> 02:07:05.000]  are
[02:07:05.000 --> 02:07:07.000]  what is called a
[02:07:07.000 --> 02:07:09.000]  five port
[02:07:09.000 --> 02:07:11.000]  and here in the stable you see the grouping
[02:07:11.000 --> 02:07:13.000]  so
[02:07:13.000 --> 02:07:15.000]  there are 12 five ports
[02:07:15.000 --> 02:07:17.000]  and each one of these have four lines
[02:07:17.000 --> 02:07:19.000]  and these four lines
[02:07:19.000 --> 02:07:21.000]  are basically
[02:07:21.000 --> 02:07:23.000]  values information that are coming
[02:07:23.000 --> 02:07:25.000]  from the various chips
[02:07:25.000 --> 02:07:27.000]  so basically the first ten five port
[02:07:27.000 --> 02:07:29.000]  are only the
[02:07:29.000 --> 02:07:31.000]  trigger information
[02:07:31.000 --> 02:07:33.000]  and then the last two are
[02:07:33.000 --> 02:07:35.000]  for the level ones
[02:07:35.000 --> 02:07:37.000]  and
[02:07:37.000 --> 02:07:39.000]  to do the mapping
[02:07:39.000 --> 02:07:41.000]  is even more complicated
[02:07:41.000 --> 02:07:43.000]  because
[02:07:43.000 --> 02:07:45.000]  the C-I-C
[02:07:45.000 --> 02:07:47.000]  has
[02:07:47.000 --> 02:07:49.000]  used a front-end ID
[02:07:49.000 --> 02:07:51.000]  that is the one
[02:07:51.000 --> 02:07:53.000]  indicating the second column
[02:07:53.000 --> 02:07:55.000]  that actually doesn't match
[02:07:55.000 --> 02:07:57.000]  the one that is used
[02:07:57.000 --> 02:07:59.000]  in the I-Sql-C
[02:07:59.000 --> 02:08:01.000]  that is also the one
[02:08:01.000 --> 02:08:03.000]  that we used for indicating the modules
[02:08:03.000 --> 02:08:05.000]  in the
[02:08:05.000 --> 02:08:07.000]  chips into PH2-ACF
[02:08:07.000 --> 02:08:09.000]  so there are basically two IDs
[02:08:09.000 --> 02:08:11.000]  one that is used for the I-Sql-C
[02:08:11.000 --> 02:08:13.000]  and one is used for the C-I-C
[02:08:13.000 --> 02:08:15.000]  to identify which system is connected
[02:08:15.000 --> 02:08:17.000]  they don't match
[02:08:17.000 --> 02:08:19.000]  and for historical reason
[02:08:19.000 --> 02:08:21.000]  it was chosen to PH2-ACF to use
[02:08:21.000 --> 02:08:23.000]  the I-Sql-C1
[02:08:23.000 --> 02:08:25.000]  so these are the numbers that you see in the PH2-ACF
[02:08:25.000 --> 02:08:27.000]  but then there is a bit of mapping
[02:08:27.000 --> 02:08:29.000]  so all the way down to the port
[02:08:29.000 --> 02:08:31.000]  the five port and the port line
[02:08:31.000 --> 02:08:33.000]  to actually understand which
[02:08:33.000 --> 02:08:35.000]  lines you are
[02:08:35.000 --> 02:08:37.000]  you are expecting
[02:08:37.000 --> 02:08:39.000]  so you see is a bit complicated
[02:08:41.000 --> 02:08:43.000]  but this is how we need to do it
[02:08:43.000 --> 02:08:45.000]  in the PH2-ACF
[02:08:45.000 --> 02:08:47.000]  I don't think you need to understand really the details
[02:08:47.000 --> 02:08:49.000]  because the PH2-ACF then does
[02:08:49.000 --> 02:08:51.000]  a little bit of
[02:08:51.000 --> 02:08:53.000]  remapping
[02:08:53.000 --> 02:08:55.000]  for you
[02:08:55.000 --> 02:08:57.000]  I don't worry too much about what is happening
[02:08:57.000 --> 02:08:59.000]  into this step
[02:08:59.000 --> 02:09:01.000]  ok so
[02:09:01.000 --> 02:09:03.000]  for the
[02:09:03.000 --> 02:09:05.000]  the second problem
[02:09:05.000 --> 02:09:07.000]  that I will mention is that
[02:09:07.000 --> 02:09:09.000]  once running by pass mode
[02:09:09.000 --> 02:09:11.000]  the LpGBT
[02:09:11.000 --> 02:09:13.000]  so
[02:09:13.000 --> 02:09:15.000]  the data that are coming out from the C-I-C
[02:09:15.000 --> 02:09:17.000]  are not any more clock
[02:09:17.000 --> 02:09:19.000]  to the usual clock in which we
[02:09:19.000 --> 02:09:21.000]  did the phase alignment and so on
[02:09:21.000 --> 02:09:23.000]  so we need
[02:09:23.000 --> 02:09:25.000]  to re-align the LpGBT
[02:09:25.000 --> 02:09:27.000]  but by construction
[02:09:27.000 --> 02:09:29.000]  this cannot be done automatically
[02:09:29.000 --> 02:09:31.000]  it needs to be done
[02:09:31.000 --> 02:09:33.000]  manually
[02:09:33.000 --> 02:09:35.000]  so we need to do a manual strain of the LpGBT phases
[02:09:35.000 --> 02:09:37.000]  in order to align
[02:09:37.000 --> 02:09:39.000]  the data
[02:09:39.000 --> 02:09:41.000]  coming from the C-I-C when the C-I-C
[02:09:41.000 --> 02:09:43.000]  is in bypass mode
[02:09:43.000 --> 02:09:45.000]  so it's an extra complication
[02:09:45.000 --> 02:09:47.000]  so
[02:09:47.000 --> 02:09:49.000]  that's why we need to do
[02:09:49.000 --> 02:09:51.000]  one five port at a time
[02:09:51.000 --> 02:09:53.000]  to scan the LpGBT phase
[02:09:53.000 --> 02:09:55.000]  and find the best phase
[02:09:55.000 --> 02:09:57.000]  and this is what is happening
[02:09:57.000 --> 02:09:59.000]  into all these plots
[02:09:59.000 --> 02:10:01.000]  that I'm showing you here
[02:10:01.000 --> 02:10:03.000]  that has this
[02:10:03.000 --> 02:10:05.000]  LpGBT for C-I-C by pass
[02:10:05.000 --> 02:10:07.000]  and there are
[02:10:07.000 --> 02:10:09.000]  for each of the
[02:10:09.000 --> 02:10:11.000]  five port
[02:10:11.000 --> 02:10:13.000]  four plots
[02:10:13.000 --> 02:10:15.000]  so let's start
[02:10:15.000 --> 02:10:17.000]  from
[02:10:17.000 --> 02:10:19.000]  the
[02:10:19.000 --> 02:10:21.000]  from the
[02:10:21.000 --> 02:10:23.000]  test base
[02:10:25.000 --> 02:10:27.000]  okay
[02:10:27.000 --> 02:10:29.000]  so here you kind of recognize the same idea
[02:10:29.000 --> 02:10:31.000]  so
[02:10:31.000 --> 02:10:33.000]  you have the phase on the x-axis
[02:10:33.000 --> 02:10:35.000]  and the line on the y-axis
[02:10:35.000 --> 02:10:37.000]  and you are going to see this
[02:10:37.000 --> 02:10:39.000]  always stub one, stub two, stub three
[02:10:39.000 --> 02:10:41.000]  and stub four
[02:10:41.000 --> 02:10:43.000]  because
[02:10:43.000 --> 02:10:45.000]  the forwarding
[02:10:45.000 --> 02:10:47.000]  always goes through these four lines
[02:10:47.000 --> 02:10:49.000]  every time
[02:10:49.000 --> 02:10:51.000]  so we just need to provide a name
[02:10:51.000 --> 02:10:53.000]  there is no correlation
[02:10:53.000 --> 02:10:55.000]  between these and what's
[02:10:55.000 --> 02:10:57.000]  happening to the CBC
[02:10:57.000 --> 02:10:59.000]  so
[02:10:59.000 --> 02:11:01.000]  it's a bit annoying but
[02:11:01.000 --> 02:11:03.000]  this is how we do it
[02:11:03.000 --> 02:11:05.000]  and then for the same plot
[02:11:05.000 --> 02:11:07.000]  we have the
[02:11:09.000 --> 02:11:11.000]  it is
[02:11:11.000 --> 02:11:13.000]  the error rate
[02:11:17.000 --> 02:11:19.000]  here you cannot see slightly better
[02:11:19.000 --> 02:11:21.000]  so yeah
[02:11:21.000 --> 02:11:23.000]  the phase scan again
[02:11:23.000 --> 02:11:25.000]  the line
[02:11:25.000 --> 02:11:27.000]  and then
[02:11:27.000 --> 02:11:29.000]  the error rate
[02:11:29.000 --> 02:11:31.000]  in percentage, in percentage from zero to one
[02:11:31.000 --> 02:11:33.000]  and we need to identify
[02:11:33.000 --> 02:11:35.000]  the working area so you see that
[02:11:35.000 --> 02:11:37.000]  there is a nice area in which we don't have any error
[02:11:37.000 --> 02:11:39.000]  so we set
[02:11:39.000 --> 02:11:41.000]  our working point
[02:11:41.000 --> 02:11:43.000]  in the middle of the widest
[02:11:43.000 --> 02:11:45.000]  area and we know from that point
[02:11:45.000 --> 02:11:47.000]  onward is that very likely
[02:11:49.000 --> 02:11:51.000]  the data that from the CIC
[02:11:51.000 --> 02:11:53.000]  sorry from the CBC
[02:11:53.000 --> 02:11:55.000]  are bypassed by the CIC
[02:11:55.000 --> 02:11:57.000]  goes to the LpGBT and arrive to the board
[02:11:57.000 --> 02:11:59.000]  are properly identified
[02:11:59.000 --> 02:12:01.000]  so it is important to align
[02:12:01.000 --> 02:12:03.000]  properly the LpGBT
[02:12:03.000 --> 02:12:05.000]  and unfortunately we need to repeat this for every
[02:12:05.000 --> 02:12:07.000]  five-port
[02:12:07.000 --> 02:12:09.000]  because these every five-port
[02:12:09.000 --> 02:12:11.000]  are different
[02:12:11.000 --> 02:12:13.000]  are different working points
[02:12:13.000 --> 02:12:15.000]  so from one to another you see that there are variations
[02:12:15.000 --> 02:12:17.000]  so we need to repeat it all
[02:12:17.000 --> 02:12:19.000]  so all these steps
[02:12:19.000 --> 02:12:21.000]  I think you can safely ignore all these
[02:12:21.000 --> 02:12:23.000]  plots
[02:12:23.000 --> 02:12:25.000]  as long as everything works
[02:12:25.000 --> 02:12:27.000]  because it is pure auxiliary calibration
[02:12:27.000 --> 02:12:29.000]  to make the next trans-tap
[02:12:29.000 --> 02:12:31.000]  which is the electric chain validation
[02:12:31.000 --> 02:12:33.000]  between in this case the CBC
[02:12:33.000 --> 02:12:35.000]  and the CIC to work
[02:12:35.000 --> 02:12:37.000]  so all of these
[02:12:37.000 --> 02:12:39.000]  it just made that identify
[02:12:39.000 --> 02:12:41.000]  the best phase which is also
[02:12:41.000 --> 02:12:43.000]  included
[02:12:47.000 --> 02:12:49.000]  somewhere
[02:12:49.000 --> 02:12:51.000]  best phase
[02:12:51.000 --> 02:12:53.000]  so here I am telling you that
[02:12:53.000 --> 02:12:55.000]  the five-port is the best
[02:12:55.000 --> 02:12:57.000]  the five-port is over here
[02:12:57.000 --> 02:12:59.000]  so you see that it is in the wide
[02:12:59.000 --> 02:13:01.000]  area
[02:13:01.000 --> 02:13:03.000]  and it is kind of in the center
[02:13:03.000 --> 02:13:05.000]  the next one is a sixth
[02:13:05.000 --> 02:13:07.000]  which is over here
[02:13:07.000 --> 02:13:09.000]  that is between these two points
[02:13:09.000 --> 02:13:11.000]  and so
[02:13:13.000 --> 02:13:15.000]  after we have done all these
[02:13:15.000 --> 02:13:17.000]  then we can actually set this phase
[02:13:17.000 --> 02:13:19.000]  and run
[02:13:21.000 --> 02:13:23.000]  the electric chain validation
[02:13:23.000 --> 02:13:25.000]  between the CBC
[02:13:25.000 --> 02:13:27.000]  and the CIC
[02:13:27.000 --> 02:13:29.000]  which is basically our last
[02:13:29.000 --> 02:13:31.000]  step of the electric chain validation
[02:13:33.000 --> 02:13:35.000]  so for these
[02:13:35.000 --> 02:13:37.000]  we have plot all the way down
[02:13:37.000 --> 02:13:39.000]  here
[02:13:39.000 --> 02:13:41.000]  and
[02:13:41.000 --> 02:13:43.000]  as for
[02:13:43.000 --> 02:13:45.000]  before
[02:13:45.000 --> 02:13:47.000]  we have two plots
[02:13:47.000 --> 02:13:49.000]  for each one of the points
[02:13:49.000 --> 02:13:51.000]  and again is
[02:13:51.000 --> 02:13:53.000]  we are going to guess it now
[02:13:53.000 --> 02:13:55.000]  is error rate
[02:13:55.000 --> 02:13:57.000]  and
[02:13:57.000 --> 02:13:59.000]  number of tests
[02:13:59.000 --> 02:14:01.000]  and
[02:14:01.000 --> 02:14:03.000]  more or less as for the
[02:14:03.000 --> 02:14:05.000]  electric chain validation
[02:14:05.000 --> 02:14:07.000]  we set three different
[02:14:07.000 --> 02:14:09.000]  values in the current
[02:14:09.000 --> 02:14:11.000]  of the CBC
[02:14:11.000 --> 02:14:13.000]  used for driving the lines
[02:14:13.000 --> 02:14:15.000]  between the CBC
[02:14:15.000 --> 02:14:17.000]  and
[02:14:17.000 --> 02:14:19.000]  CIC
[02:14:19.000 --> 02:14:21.000]  so you can set the current use here
[02:14:21.000 --> 02:14:23.000]  and you can imagine a bit more current
[02:14:23.000 --> 02:14:25.000]  actually rather than taking
[02:14:25.000 --> 02:14:27.000]  current zero
[02:14:27.000 --> 02:14:29.000]  sorry
[02:14:29.000 --> 02:14:31.000]  the other way around 14 is the lowest
[02:14:31.000 --> 02:14:33.000]  current
[02:14:33.000 --> 02:14:35.000]  zero
[02:14:35.000 --> 02:14:37.000]  is the highest
[02:14:37.000 --> 02:14:39.000]  and
[02:14:39.000 --> 02:14:41.000]  eight is coming
[02:14:41.000 --> 02:14:43.000]  okay
[02:14:43.000 --> 02:14:45.000]  so you can recognize
[02:14:45.000 --> 02:14:47.000]  the same plot as before
[02:14:47.000 --> 02:14:49.000]  so here we have the phases
[02:14:49.000 --> 02:14:51.000]  and the x axis
[02:14:51.000 --> 02:14:53.000]  and the y axis you have the various lines
[02:14:53.000 --> 02:14:55.000]  you see that there are all the CBCs connected
[02:14:55.000 --> 02:14:57.000]  for each one you have all the lines
[02:14:57.000 --> 02:14:59.000]  of the CBC
[02:14:59.000 --> 02:15:01.000]  line you see that
[02:15:01.000 --> 02:15:03.000]  there is no phase two and three
[02:15:03.000 --> 02:15:05.000]  because these phases do not work
[02:15:05.000 --> 02:15:07.000]  on the CIC
[02:15:07.000 --> 02:15:09.000]  so we simply skip that
[02:15:09.000 --> 02:15:11.000]  and as usual
[02:15:11.000 --> 02:15:13.000]  you see the number of tests
[02:15:13.000 --> 02:15:15.000]  for the level one is lower
[02:15:15.000 --> 02:15:17.000]  because they need to be done on the software
[02:15:17.000 --> 02:15:19.000]  it's still 10 to the 5 so it's not super low
[02:15:19.000 --> 02:15:21.000]  but
[02:15:21.000 --> 02:15:23.000]  it's a bit lower in order to
[02:15:23.000 --> 02:15:25.000]  not take too much time
[02:15:25.000 --> 02:15:27.000]  and here is
[02:15:27.000 --> 02:15:29.000]  what we get for
[02:15:29.000 --> 02:15:31.000]  the
[02:15:31.000 --> 02:15:33.000]  for the error rate test
[02:15:33.000 --> 02:15:35.000]  so you see that most
[02:15:35.000 --> 02:15:37.000]  of the phases
[02:15:37.000 --> 02:15:39.000]  most of the area have a wide phase
[02:15:39.000 --> 02:15:41.000]  so let's say for example this one
[02:15:41.000 --> 02:15:43.000]  you see there is a wide area
[02:15:43.000 --> 02:15:45.000]  in which any of these phases
[02:15:45.000 --> 02:15:47.000]  should work
[02:15:47.000 --> 02:15:49.000]  and means that is pretty good
[02:15:49.000 --> 02:15:51.000]  level ones are the ones that
[02:15:51.000 --> 02:15:53.000]  sometimes are affected by some
[02:15:53.000 --> 02:15:55.000]  issues in the pattern matching
[02:15:55.000 --> 02:15:57.000]  and this is something that is happening
[02:15:57.000 --> 02:15:59.000]  where not always the data
[02:15:59.000 --> 02:16:01.000]  are properly sampled
[02:16:01.000 --> 02:16:03.000]  and then we do the matching
[02:16:03.000 --> 02:16:05.000]  via software by reading those data
[02:16:05.000 --> 02:16:07.000]  and sometimes we get some
[02:16:07.000 --> 02:16:09.000]  where we don't get 100%
[02:16:09.000 --> 02:16:11.000]  because some of it are misread
[02:16:11.000 --> 02:16:13.000]  not really something concerning
[02:16:13.000 --> 02:16:15.000]  we are trying to
[02:16:15.000 --> 02:16:17.000]  improve it but it's going to be a little bit more challenging
[02:16:17.000 --> 02:16:19.000]  because it's really the mechanism
[02:16:19.000 --> 02:16:21.000]  for how these data are sampled
[02:16:21.000 --> 02:16:23.000]  and so we are going to probably leave
[02:16:23.000 --> 02:16:25.000]  with that but you clearly see
[02:16:25.000 --> 02:16:27.000]  phases which they don't work there
[02:16:27.000 --> 02:16:29.000]  the error rate are much much much higher
[02:16:29.000 --> 02:16:31.000]  so this is 45%
[02:16:31.000 --> 02:16:33.000]  compared to
[02:16:33.000 --> 02:16:35.000]  two per mil even less than two per mil
[02:16:35.000 --> 02:16:37.000]  okay
[02:16:37.000 --> 02:16:39.000]  and as for before we had the four different
[02:16:39.000 --> 02:16:41.000]  currents but
[02:16:41.000 --> 02:16:43.000]  the amount
[02:16:43.000 --> 02:16:45.000]  the plots are always the same
[02:16:49.000 --> 02:16:51.000]  okay
[02:16:51.000 --> 02:16:53.000]  so we are at the end
[02:16:53.000 --> 02:16:55.000]  of the electric chain validation
[02:16:57.000 --> 02:16:59.000]  before moving on
[02:16:59.000 --> 02:17:01.000]  do you have any questions
[02:17:01.000 --> 02:17:03.000]  for this?
[02:17:11.000 --> 02:17:13.000]  okay and now
[02:17:13.000 --> 02:17:15.000]  the last
[02:17:15.000 --> 02:17:17.000]  two tests
[02:17:17.000 --> 02:17:19.000]  so a bit error rate
[02:17:19.000 --> 02:17:21.000]  this one is the newest one
[02:17:21.000 --> 02:17:23.000]  so the idea
[02:17:23.000 --> 02:17:25.000]  here
[02:17:25.000 --> 02:17:27.000]  is that we want to
[02:17:27.000 --> 02:17:29.000]  check
[02:17:29.000 --> 02:17:31.000]  the stability of the link between the
[02:17:31.000 --> 02:17:33.000]  basically the LPGPT
[02:17:33.000 --> 02:17:35.000]  and FPGA going through
[02:17:35.000 --> 02:17:37.000]  the VTRX
[02:17:37.000 --> 02:17:39.000]  so this will allow you to check
[02:17:39.000 --> 02:17:41.000]  if there are problems between
[02:17:41.000 --> 02:17:43.000]  the LPGPT and VTRX
[02:17:43.000 --> 02:17:45.000]  or between the VTRX
[02:17:45.000 --> 02:17:47.000]  and the board
[02:17:47.000 --> 02:17:49.000]  and
[02:17:49.000 --> 02:17:51.000]  so
[02:17:51.000 --> 02:17:53.000]  the LPGPT has some functionality
[02:17:53.000 --> 02:17:55.000]  to create a PRBS
[02:17:55.000 --> 02:17:57.000]  which is
[02:17:57.000 --> 02:17:59.000]  a pseudo random
[02:17:59.000 --> 02:18:01.000]  bit
[02:18:01.000 --> 02:18:03.000]  something
[02:18:03.000 --> 02:18:05.000]  I don't know exactly but it generates
[02:18:05.000 --> 02:18:07.000]  pseudo random bits
[02:18:07.000 --> 02:18:09.000]  in a precise
[02:18:09.000 --> 02:18:11.000]  pattern that
[02:18:11.000 --> 02:18:13.000]  is known and
[02:18:13.000 --> 02:18:15.000]  we can set the
[02:18:15.000 --> 02:18:17.000]  same generator into the firmware
[02:18:17.000 --> 02:18:19.000]  and then at that point
[02:18:19.000 --> 02:18:21.000]  we can see
[02:18:21.000 --> 02:18:23.000]  if every pattern matching the expected pattern
[02:18:23.000 --> 02:18:25.000]  sent by the LPGPT
[02:18:25.000 --> 02:18:27.000]  matches the expected pattern
[02:18:27.000 --> 02:18:29.000]  received by the FC7
[02:18:29.000 --> 02:18:31.000]  and this will allow us to
[02:18:31.000 --> 02:18:33.000]  determine if there were any bits
[02:18:33.000 --> 02:18:35.000]  that got corrupted
[02:18:35.000 --> 02:18:37.000]  during the transmission
[02:18:37.000 --> 02:18:39.000]  so
[02:18:43.000 --> 02:18:45.000]  so the
[02:18:45.000 --> 02:18:47.000]  LPGPT has different
[02:18:47.000 --> 02:18:49.000]  procedure that can be used
[02:18:49.000 --> 02:18:51.000]  for generating this pattern
[02:18:51.000 --> 02:18:53.000]  you can check them into the
[02:18:53.000 --> 02:18:55.000]  LPGPT manual
[02:18:55.000 --> 02:18:57.000]  the one that we are using
[02:18:57.000 --> 02:18:59.000]  basically emulates one PLBS
[02:18:59.000 --> 02:19:01.000]  for every one of these lines
[02:19:01.000 --> 02:19:03.000]  it's just a technicality
[02:19:03.000 --> 02:19:05.000]  because then we can still
[02:19:05.000 --> 02:19:07.000]  split them
[02:19:07.000 --> 02:19:09.000]  at the level of the
[02:19:09.000 --> 02:19:11.000]  of the firmware
[02:19:11.000 --> 02:19:13.000]  and then handle them
[02:19:13.000 --> 02:19:15.000]  as we handle normally the lines
[02:19:15.000 --> 02:19:17.000]  that are
[02:19:17.000 --> 02:19:19.000]  encoded by the LPGPT
[02:19:19.000 --> 02:19:21.000]  and then decoded back
[02:19:21.000 --> 02:19:23.000]  so you're gonna see even if the lines
[02:19:23.000 --> 02:19:25.000]  between the LPGPT and VTRX
[02:19:25.000 --> 02:19:27.000]  and between the VTRX
[02:19:27.000 --> 02:19:29.000]  and the FGA
[02:19:29.000 --> 02:19:31.000]  is a single line
[02:19:31.000 --> 02:19:33.000]  you're gonna see
[02:19:33.000 --> 02:19:35.000]  results split by the various lines
[02:19:35.000 --> 02:19:37.000]  and the reason for that
[02:19:37.000 --> 02:19:39.000]  is
[02:19:41.000 --> 02:19:43.000]  is that
[02:19:43.000 --> 02:19:45.000]  we don't have enough
[02:19:45.000 --> 02:19:47.000]  resources to do all of this in parallel
[02:19:47.000 --> 02:19:49.000]  so each of the lines
[02:19:49.000 --> 02:19:51.000]  is done one at a time
[02:19:51.000 --> 02:19:53.000]  and just because it's quite
[02:19:53.000 --> 02:19:55.000]  a new
[02:19:55.000 --> 02:19:57.000]  a new test
[02:19:57.000 --> 02:19:59.000]  we keep also the results
[02:19:59.000 --> 02:20:01.000]  separated for each line
[02:20:01.000 --> 02:20:03.000]  if we have
[02:20:03.000 --> 02:20:05.000]  we see any issues popping up
[02:20:05.000 --> 02:20:07.000]  we can understand where it's coming from
[02:20:07.000 --> 02:20:09.000]  and do some debugging
[02:20:09.000 --> 02:20:11.000]  but basically you are just testing
[02:20:11.000 --> 02:20:13.000]  one single line
[02:20:13.000 --> 02:20:15.000]  okay so all the results
[02:20:15.000 --> 02:20:17.000]  since it's done at the level of the LPGPT
[02:20:17.000 --> 02:20:19.000]  are stored
[02:20:19.000 --> 02:20:21.000]  in the optical view
[02:20:21.000 --> 02:20:23.000]  and
[02:20:23.000 --> 02:20:25.000]  we have a few plots
[02:20:25.000 --> 02:20:27.000]  okay so the really first one
[02:20:27.000 --> 02:20:29.000]  is
[02:20:29.000 --> 02:20:31.000]  a bit error rate
[02:20:31.000 --> 02:20:33.000]  phase scan
[02:20:33.000 --> 02:20:35.000]  because
[02:20:35.000 --> 02:20:37.000]  so this is basically a technicality
[02:20:37.000 --> 02:20:39.000]  but
[02:20:41.000 --> 02:20:43.000]  the LPGPT
[02:20:43.000 --> 02:20:45.000]  generates this bit error rate
[02:20:45.000 --> 02:20:47.000]  pattern
[02:20:47.000 --> 02:20:49.000]  from
[02:20:49.000 --> 02:20:51.000]  clock source
[02:20:51.000 --> 02:20:53.000]  that you can change the phase
[02:20:53.000 --> 02:20:55.000]  and there are some phases in which the LPGPT
[02:20:55.000 --> 02:20:57.000]  will not work because it will not
[02:20:57.000 --> 02:20:59.000]  understand its own pattern
[02:20:59.000 --> 02:21:01.000]  so we need to do a quick scan
[02:21:01.000 --> 02:21:03.000]  to understand what are the working phases
[02:21:03.000 --> 02:21:05.000]  we see that there is plenty of space
[02:21:05.000 --> 02:21:07.000]  we just need to identify one
[02:21:07.000 --> 02:21:09.000]  and basically the outcome phase
[02:21:09.000 --> 02:21:11.000]  is stored
[02:21:11.000 --> 02:21:13.000]  here into the best phase
[02:21:15.000 --> 02:21:17.000]  these are basically a technicality
[02:21:17.000 --> 02:21:19.000]  so I just wanted to mention it for completeness
[02:21:21.000 --> 02:21:23.000]  but I don't think
[02:21:23.000 --> 02:21:25.000]  I mean it doesn't tell you anything
[02:21:25.000 --> 02:21:27.000]  about the code in order to
[02:21:27.000 --> 02:21:29.000]  avoid generating bits
[02:21:29.000 --> 02:21:31.000]  where the LPGPT
[02:21:31.000 --> 02:21:33.000]  is in a condition that is not ready to transmit
[02:21:33.000 --> 02:21:35.000]  and create a fake
[02:21:35.000 --> 02:21:37.000]  bit error rate that has nothing to do
[02:21:37.000 --> 02:21:39.000]  with the link
[02:21:39.000 --> 02:21:41.000]  between your module
[02:21:41.000 --> 02:21:43.000]  and the FCSL
[02:21:43.000 --> 02:21:45.000]  then once this is fixed
[02:21:45.000 --> 02:21:47.000]  we can really do
[02:21:47.000 --> 02:21:49.000]  the real bit error rate test
[02:21:49.000 --> 02:21:51.000]  and as
[02:21:51.000 --> 02:21:53.000]  for all the other plots
[02:21:53.000 --> 02:21:55.000]  we have two level
[02:21:55.000 --> 02:21:57.000]  two plots
[02:21:57.000 --> 02:21:59.000]  one that contains the number of tested bits
[02:22:01.000 --> 02:22:03.000]  and you see that we do
[02:22:03.000 --> 02:22:05.000]  10 to the
[02:22:05.000 --> 02:22:07.000]  10 bit
[02:22:07.000 --> 02:22:09.000]  is kind of uniform
[02:22:09.000 --> 02:22:11.000]  you are going to notice that
[02:22:11.000 --> 02:22:13.000]  it's kind of symmetric because
[02:22:13.000 --> 02:22:15.000]  the two hybrids are done in parallel
[02:22:15.000 --> 02:22:17.000]  so once
[02:22:17.000 --> 02:22:19.000]  we do the star number two or the right hybrid
[02:22:19.000 --> 02:22:21.000]  we also do the star number two
[02:22:21.000 --> 02:22:23.000]  the left hybrid and this will
[02:22:23.000 --> 02:22:25.000]  call out the same amount
[02:22:25.000 --> 02:22:27.000]  it's not going to be super precise
[02:22:27.000 --> 02:22:29.000]  as all the bits
[02:22:29.000 --> 02:22:31.000]  number of bits that I test
[02:22:31.000 --> 02:22:33.000]  that I showed you before
[02:22:33.000 --> 02:22:35.000]  because we just
[02:22:35.000 --> 02:22:37.000]  set at least a certain number of bits
[02:22:37.000 --> 02:22:39.000]  and then we have to wait and there is no way
[02:22:39.000 --> 02:22:41.000]  we can really control exactly how many
[02:22:41.000 --> 02:22:43.000]  we just control that we get
[02:22:43.000 --> 02:22:45.000]  at least the amount that we request
[02:22:45.000 --> 02:22:47.000]  so nothing to worry about that
[02:22:47.000 --> 02:22:49.000]  but you see that it's kind of more or less the same
[02:22:49.000 --> 02:22:51.000]  or different of you
[02:22:51.000 --> 02:22:53.000]  less than upon me
[02:22:53.000 --> 02:22:55.000]  and then
[02:22:55.000 --> 02:22:57.000]  the real important one is the
[02:22:57.000 --> 02:22:59.000]  bit error rate
[02:22:59.000 --> 02:23:01.000]  that as I was mentioning
[02:23:01.000 --> 02:23:03.000]  is the measurement of just
[02:23:03.000 --> 02:23:05.000]  the link stability but it's still split
[02:23:05.000 --> 02:23:07.000]  for every
[02:23:07.000 --> 02:23:09.000]  for every line and here
[02:23:09.000 --> 02:23:11.000]  for what I see so far
[02:23:11.000 --> 02:23:13.000]  you should expect always
[02:23:13.000 --> 02:23:15.000]  a zero bit error rate
[02:23:15.000 --> 02:23:17.000]  so
[02:23:17.000 --> 02:23:19.000]  if you sum all of these
[02:23:19.000 --> 02:23:21.000]  you get 10 to the
[02:23:23.000 --> 02:23:25.000]  12
[02:23:25.000 --> 02:23:27.000]  10 to the 11
[02:23:29.000 --> 02:23:31.000]  which is a lot of bits
[02:23:31.000 --> 02:23:33.000]  and we expect less than
[02:23:33.000 --> 02:23:35.000]  10 to the 13
[02:23:35.000 --> 02:23:37.000]  10 to the 12, 10 to the 13
[02:23:37.000 --> 02:23:39.000]  that's why your show always gets zero
[02:23:39.000 --> 02:23:41.000]  why we don't do 10 to the 13
[02:23:41.000 --> 02:23:43.000]  is simply because
[02:23:43.000 --> 02:23:45.000]  this it's really
[02:23:45.000 --> 02:23:47.000]  all the time that is needed
[02:23:47.000 --> 02:23:49.000]  for doing this test is waiting to
[02:23:49.000 --> 02:23:51.000]  collect enough bits and 10 to the
[02:23:51.000 --> 02:23:53.000]  13 are a few hours
[02:23:53.000 --> 02:23:55.000]  of testing which we cannot
[02:23:55.000 --> 02:23:57.000]  really afford
[02:23:57.000 --> 02:23:59.000]  we did it with a couple models here while
[02:23:59.000 --> 02:24:01.000]  developing and we didn't see any
[02:24:01.000 --> 02:24:03.000]  error with the 10 to the 13
[02:24:03.000 --> 02:24:05.000]  we needed to set a reasonable
[02:24:05.000 --> 02:24:07.000]  number for the testing procedure
[02:24:07.000 --> 02:24:09.000]  and that's why we stick to
[02:24:09.000 --> 02:24:11.000]  the 10 to the
[02:24:11.000 --> 02:24:13.000]  12 I think
[02:24:13.000 --> 02:24:15.000]  10 to the 11
[02:24:15.000 --> 02:24:17.000]  10,000
[02:24:17.000 --> 02:24:19.000]  now it's 10 to the 12
[02:24:19.000 --> 02:24:21.000]  sorry 10 to the 11
[02:24:23.000 --> 02:24:25.000]  because you can
[02:24:25.000 --> 02:24:27.000]  imagine some of these bits you have
[02:24:27.000 --> 02:24:29.000]  for 12
[02:24:29.000 --> 02:24:31.000]  even if you collect for each one of these
[02:24:31.000 --> 02:24:33.000]  10 to the 10 then the total one
[02:24:33.000 --> 02:24:35.000]  is going to be 10 to the 11 because
[02:24:35.000 --> 02:24:37.000]  it's a factor of 12
[02:24:37.000 --> 02:24:39.000]  and this
[02:24:39.000 --> 02:24:41.000]  is honestly
[02:24:41.000 --> 02:24:43.000]  just one part of the bit error rate test
[02:24:43.000 --> 02:24:45.000]  because the
[02:24:45.000 --> 02:24:47.000]  other important part
[02:24:47.000 --> 02:24:49.000]  is the
[02:24:49.000 --> 02:24:51.000]  factor counter so I think
[02:24:51.000 --> 02:24:53.000]  I mentioned before
[02:24:53.000 --> 02:24:55.000]  this modular operator with the
[02:24:55.000 --> 02:24:57.000]  factor 5 which means that
[02:24:57.000 --> 02:24:59.000]  we can correct up to 5 bits
[02:24:59.000 --> 02:25:01.000]  that were flipped
[02:25:01.000 --> 02:25:03.000]  in the communication
[02:25:03.000 --> 02:25:05.000]  so that
[02:25:05.000 --> 02:25:07.000]  it might happen that you don't see
[02:25:07.000 --> 02:25:09.000]  any error rate here
[02:25:09.000 --> 02:25:11.000]  but you still have bits that are flipped
[02:25:11.000 --> 02:25:13.000]  in the communication
[02:25:13.000 --> 02:25:15.000]  but they were automatically corrected
[02:25:15.000 --> 02:25:17.000]  which is good but at the same time
[02:25:17.000 --> 02:25:19.000]  it means that you might have some
[02:25:19.000 --> 02:25:21.000]  instabilities because you shouldn't
[02:25:21.000 --> 02:25:23.000]  have bits that are flipped
[02:25:23.000 --> 02:25:25.000]  so
[02:25:25.000 --> 02:25:27.000]  for doing this the
[02:25:27.000 --> 02:25:29.000]  firmware is able to count
[02:25:29.000 --> 02:25:31.000]  many bits and corrected and we store
[02:25:31.000 --> 02:25:33.000]  that information in the factor count
[02:25:33.000 --> 02:25:35.000]  so
[02:25:35.000 --> 02:25:37.000]  the difference between the other products
[02:25:37.000 --> 02:25:39.000]  here was divided by left and right hybrid
[02:25:39.000 --> 02:25:41.000]  because here we are
[02:25:41.000 --> 02:25:43.000]  injecting really lines
[02:25:43.000 --> 02:25:45.000]  or pretending to inject lines
[02:25:45.000 --> 02:25:47.000]  both in the left and right hybrid
[02:25:47.000 --> 02:25:49.000]  when instead the fact
[02:25:49.000 --> 02:25:51.000]  is a cumulative
[02:25:51.000 --> 02:25:53.000]  information for the overall package
[02:25:53.000 --> 02:25:55.000]  so the all information
[02:25:55.000 --> 02:25:57.000]  from that is received by
[02:25:57.000 --> 02:25:59.000]  the LpGBT
[02:25:59.000 --> 02:26:01.000]  and that's why we have a single number
[02:26:01.000 --> 02:26:03.000]  for the two hybrids
[02:26:03.000 --> 02:26:05.000]  you still have separated by lines
[02:26:05.000 --> 02:26:07.000]  because again here we are doing
[02:26:07.000 --> 02:26:09.000]  one line at a time
[02:26:09.000 --> 02:26:11.000]  because of resource in the firmware
[02:26:11.000 --> 02:26:13.000]  that's why you have different
[02:26:17.000 --> 02:26:19.000]  bins for each one of the lines
[02:26:19.000 --> 02:26:21.000]  but again you can imagine
[02:26:21.000 --> 02:26:23.000]  to sum them all together
[02:26:23.000 --> 02:26:25.000]  again this is just for us to understand
[02:26:25.000 --> 02:26:27.000]  if there was a failure, if the failure is coming
[02:26:27.000 --> 02:26:29.000]  is more
[02:26:29.000 --> 02:26:31.000]  or less uniform is that it's
[02:26:31.000 --> 02:26:33.000]  modulated just one
[02:26:33.000 --> 02:26:35.000]  bit suspicious
[02:26:35.000 --> 02:26:37.000]  we add a bug in the field
[02:26:39.000 --> 02:26:41.000]  and in this case it's not really a percentage
[02:26:41.000 --> 02:26:43.000]  but it's rather
[02:26:43.000 --> 02:26:45.000]  really an encounter
[02:26:45.000 --> 02:26:47.000]  because we don't really have
[02:26:47.000 --> 02:26:49.000]  a way to completely
[02:26:49.000 --> 02:26:51.000]  read how many
[02:26:51.000 --> 02:26:53.000]  packages were read
[02:26:53.000 --> 02:26:55.000]  we just
[02:26:55.000 --> 02:26:57.000]  read at the end of the bit error rate test
[02:26:57.000 --> 02:26:59.000]  but it's done via software
[02:26:59.000 --> 02:27:01.000]  so we don't really have a perfect denominator
[02:27:01.000 --> 02:27:03.000]  but in first
[02:27:03.000 --> 02:27:05.000]  we can assume that the denominator
[02:27:05.000 --> 02:27:07.000]  is the same
[02:27:07.000 --> 02:27:09.000]  we're collecting a lot of bits
[02:27:09.000 --> 02:27:11.000]  even if
[02:27:11.000 --> 02:27:13.000]  a millisecond later
[02:27:13.000 --> 02:27:15.000]  we read that the
[02:27:15.000 --> 02:27:17.000]  number of encounters is still
[02:27:17.000 --> 02:27:19.000]  more or less the same amount
[02:27:19.000 --> 02:27:21.000]  of bits we read before
[02:27:25.000 --> 02:27:27.000]  so this is for the bit error rate test
[02:27:27.000 --> 02:27:29.000]  and
[02:27:29.000 --> 02:27:31.000]  as you just told me
[02:27:31.000 --> 02:27:33.000]  if you have any questions
[02:27:33.000 --> 02:27:35.000]  and then the
[02:27:35.000 --> 02:27:37.000]  OT register tester
[02:27:37.000 --> 02:27:39.000]  so this is really the last one
[02:27:39.000 --> 02:27:41.000]  and here we want to test
[02:27:41.000 --> 02:27:43.000]  the stability of the S4C
[02:27:43.000 --> 02:27:45.000]  so here
[02:27:45.000 --> 02:27:47.000]  we are
[02:27:47.000 --> 02:27:49.000]  writing and reading registers
[02:27:49.000 --> 02:27:51.000]  into both the CBCs
[02:27:51.000 --> 02:27:53.000]  and the CICs
[02:27:53.000 --> 02:27:55.000]  we select a few registers
[02:27:55.000 --> 02:27:57.000]  we're actually writing
[02:27:57.000 --> 02:27:59.000]  the pattern and reading it back
[02:27:59.000 --> 02:28:01.000]  and then writing the inverse pattern
[02:28:01.000 --> 02:28:03.000]  and reading it back and so on and forth
[02:28:03.000 --> 02:28:05.000]  for
[02:28:05.000 --> 02:28:07.000]  1,000 times or something like that
[02:28:07.000 --> 02:28:09.000]  and then we store everything
[02:28:09.000 --> 02:28:11.000]  into a single
[02:28:11.000 --> 02:28:13.000]  register
[02:28:13.000 --> 02:28:15.000]  into a single plot
[02:28:15.000 --> 02:28:17.000]  where for every of these
[02:28:17.000 --> 02:28:19.000]  of the chip
[02:28:19.000 --> 02:28:21.000]  so the HCBC and the CIC
[02:28:21.000 --> 02:28:23.000]  and you have one plot for each one of the hybrid
[02:28:23.000 --> 02:28:25.000]  we store
[02:28:25.000 --> 02:28:27.000]  the efficiency in reading and writing
[02:28:27.000 --> 02:28:29.000]  and here you should
[02:28:29.000 --> 02:28:31.000]  see always 100%
[02:28:31.000 --> 02:28:33.000]  because so far
[02:28:33.000 --> 02:28:35.000]  so CIC
[02:28:35.000 --> 02:28:37.000]  S4C is very stable
[02:28:37.000 --> 02:28:39.000]  for the CBC
[02:28:39.000 --> 02:28:41.000]  we avoid using
[02:28:41.000 --> 02:28:43.000]  a change of the page that can
[02:28:43.000 --> 02:28:45.000]  create some stability because we don't
[02:28:45.000 --> 02:28:47.000]  exist so we don't want to
[02:28:47.000 --> 02:28:49.000]  create
[02:28:49.000 --> 02:28:51.000]  so here we want really to see
[02:28:51.000 --> 02:28:53.000]  not really the overall behavior of the chip
[02:28:53.000 --> 02:28:55.000]  but the particular behavior
[02:28:55.000 --> 02:28:57.000]  of the chip on your module
[02:28:57.000 --> 02:28:59.000]  if there is any stability on the S4C lines
[02:28:59.000 --> 02:29:01.000]  since the S4C line goes
[02:29:01.000 --> 02:29:03.000]  through the
[02:29:03.000 --> 02:29:05.000]  through the
[02:29:05.000 --> 02:29:07.000]  connectors between the
[02:29:07.000 --> 02:29:09.000]  FEH and SEH
[02:29:09.000 --> 02:29:11.000]  you, if you see some stability
[02:29:11.000 --> 02:29:13.000]  you might check that
[02:29:13.000 --> 02:29:15.000]  and
[02:29:15.000 --> 02:29:17.000]  okay so
[02:29:17.000 --> 02:29:19.000]  these are
[02:29:19.000 --> 02:29:21.000]  all the tests
[02:29:21.000 --> 02:29:23.000]  and all the plots that
[02:29:23.000 --> 02:29:25.000]  we run from the result
[02:29:25.000 --> 02:29:27.000]  file
[02:29:27.000 --> 02:29:29.000]  and after this I will move to
[02:29:29.000 --> 02:29:31.000]  the monitor
[02:29:31.000 --> 02:29:33.000]  the QM file
[02:29:33.000 --> 02:29:35.000]  but I think we should stub
[02:29:35.000 --> 02:29:37.000]  a bit to see if you
[02:29:37.000 --> 02:29:39.000]  have any question or comments
[02:29:41.000 --> 02:29:43.000]  on what was discussed
[02:29:49.000 --> 02:29:51.000]  okay
[02:29:53.000 --> 02:29:55.000]  okay
[02:29:57.000 --> 02:29:59.000]  can you just
[02:29:59.000 --> 02:30:01.000]  confirm you can still hear me
[02:30:01.000 --> 02:30:03.000]  just to be sure
[02:30:03.000 --> 02:30:05.000]  yes
[02:30:05.000 --> 02:30:07.000]  okay one yes
[02:30:07.000 --> 02:30:09.000]  go ahead
[02:30:09.000 --> 02:30:11.000]  I just have a doubt that I was speaking
[02:30:11.000 --> 02:30:13.000]  to myself because it was not the
[02:30:13.000 --> 02:30:15.000]  first time it happened
[02:30:15.000 --> 02:30:17.000]  okay
[02:30:17.000 --> 02:30:19.000]  so moving on
[02:30:19.000 --> 02:30:21.000]  to the monitor
[02:30:21.000 --> 02:30:23.000]  so the monitor is a separate file
[02:30:23.000 --> 02:30:25.000]  it's gonna
[02:30:25.000 --> 02:30:27.000]  store it into
[02:30:27.000 --> 02:30:29.000]  the
[02:30:29.000 --> 02:30:31.000]  into a separate folder
[02:30:31.000 --> 02:30:33.000]  for most of the cases
[02:30:33.000 --> 02:30:35.000]  when you're running manually it's all of the case
[02:30:35.000 --> 02:30:37.000]  GIFT actually moved it already into
[02:30:37.000 --> 02:30:39.000]  result file folder
[02:30:39.000 --> 02:30:41.000]  and the reason why it's a separate file
[02:30:41.000 --> 02:30:43.000]  because the result
[02:30:43.000 --> 02:30:45.000]  they go from the start to the stub
[02:30:45.000 --> 02:30:47.000]  when you start the monitor
[02:30:47.000 --> 02:30:49.000]  it goes from the configure to the
[02:30:49.000 --> 02:30:51.000]  halt or the destroy
[02:30:51.000 --> 02:30:53.000]  the reason is that for example in the Balmina
[02:30:53.000 --> 02:30:55.000]  we do start and stub during
[02:30:55.000 --> 02:30:57.000]  the test that is usually one
[02:30:57.000 --> 02:30:59.000]  of the plateau of the temperature
[02:30:59.000 --> 02:31:01.000]  but then
[02:31:01.000 --> 02:31:03.000]  we are into a stub
[02:31:03.000 --> 02:31:05.000]  state
[02:31:05.000 --> 02:31:07.000]  during the changing current
[02:31:07.000 --> 02:31:09.000]  and we want to keep monitoring even
[02:31:09.000 --> 02:31:11.000]  if you don't do any run and that's why
[02:31:11.000 --> 02:31:13.000]  you have two separate files because
[02:31:13.000 --> 02:31:15.000]  there are different times
[02:31:17.000 --> 02:31:19.000]  then
[02:31:21.000 --> 02:31:23.000]  people are working on
[02:31:23.000 --> 02:31:25.000]  creating
[02:31:25.000 --> 02:31:27.000]  a business script that merge
[02:31:27.000 --> 02:31:29.000]  all the information together also including
[02:31:29.000 --> 02:31:31.000]  an information like power supply and so on
[02:31:31.000 --> 02:31:33.000]  so what actually potato receives
[02:31:33.000 --> 02:31:35.000]  is a combination of the two
[02:31:35.000 --> 02:31:37.000]  files together
[02:31:37.000 --> 02:31:39.000]  this is still
[02:31:39.000 --> 02:31:41.000]  being developed
[02:31:41.000 --> 02:31:43.000]  so I just wanted to show you
[02:31:43.000 --> 02:31:45.000]  exactly what
[02:31:47.000 --> 02:31:49.000]  all this information will be used
[02:31:49.000 --> 02:31:51.000]  for
[02:31:51.000 --> 02:31:53.000]  the
[02:31:53.000 --> 02:31:55.000]  qualification
[02:31:55.000 --> 02:31:57.000]  so the monitor
[02:31:57.000 --> 02:31:59.000]  is also set into the XML
[02:31:59.000 --> 02:32:01.000]  file
[02:32:01.000 --> 02:32:03.000]  and there is a list of the parameter
[02:32:03.000 --> 02:32:05.000]  that we are testing and for each one of these
[02:32:05.000 --> 02:32:07.000]  there is a plot as a function
[02:32:07.000 --> 02:32:09.000]  of time or the values that we are monitoring
[02:32:09.000 --> 02:32:11.000]  so the first
[02:32:11.000 --> 02:32:13.000]  leveler which we start monitoring
[02:32:13.000 --> 02:32:15.000]  is at the level of the optical group
[02:32:15.000 --> 02:32:17.000]  and all the information
[02:32:17.000 --> 02:32:19.000]  that are extracted from the
[02:32:19.000 --> 02:32:21.000]  LpGBT which has an ADC
[02:32:21.000 --> 02:32:23.000]  and the ADC
[02:32:23.000 --> 02:32:25.000]  is both connected to values
[02:32:25.000 --> 02:32:27.000]  that are inside the chip
[02:32:27.000 --> 02:32:29.000]  or
[02:32:29.000 --> 02:32:31.000]  are coming from the lines
[02:32:31.000 --> 02:32:33.000]  that are connected to the chip
[02:32:33.000 --> 02:32:35.000]  and there are all listed here
[02:32:35.000 --> 02:32:37.000]  so here are the full list of values
[02:32:37.000 --> 02:32:39.000]  that we register
[02:32:39.000 --> 02:32:41.000]  and the name of the value is always
[02:32:41.000 --> 02:32:43.000]  stored into the name or the plot
[02:32:43.000 --> 02:32:45.000]  as well as
[02:32:45.000 --> 02:32:47.000]  into
[02:32:47.000 --> 02:32:49.000]  the plot
[02:32:49.000 --> 02:32:51.000]  title itself
[02:32:51.000 --> 02:32:53.000]  so the first one
[02:32:53.000 --> 02:32:55.000]  it is
[02:32:55.000 --> 02:32:57.000]  the
[02:32:57.000 --> 02:32:59.000]  VDD
[02:32:59.000 --> 02:33:01.000]  so
[02:33:01.000 --> 02:33:03.000]  the LpGBT
[02:33:03.000 --> 02:33:05.000]  uses a few
[02:33:05.000 --> 02:33:07.000]  digital voltages
[02:33:07.000 --> 02:33:09.000]  that are used to make it work
[02:33:09.000 --> 02:33:11.000]  and these allow you to
[02:33:11.000 --> 02:33:13.000]  monitor their values
[02:33:13.000 --> 02:33:15.000]  in particular they should be around 1.2
[02:33:15.000 --> 02:33:17.000]  and you see that this table over
[02:33:17.000 --> 02:33:19.000]  the run
[02:33:19.000 --> 02:33:21.000]  these were taken
[02:33:21.000 --> 02:33:23.000]  at the same time of the result file
[02:33:23.000 --> 02:33:25.000]  I will show you before
[02:33:25.000 --> 02:33:27.000]  not that it matters too much
[02:33:27.000 --> 02:33:29.000]  but just to show you that even if you run
[02:33:29.000 --> 02:33:31.000]  you don't see too many instabilities
[02:33:31.000 --> 02:33:33.000]  okay
[02:33:33.000 --> 02:33:35.000]  so this is one
[02:33:35.000 --> 02:33:37.000]  I'm just going to open the mall
[02:33:37.000 --> 02:33:39.000]  I will say in most of the cases
[02:33:39.000 --> 02:33:41.000]  these
[02:33:41.000 --> 02:33:43.000]  should give you a bit more
[02:33:43.000 --> 02:33:45.000]  immediate feedback if something
[02:33:45.000 --> 02:33:47.000]  structurally bad
[02:33:47.000 --> 02:33:49.000]  is happening to your module
[02:33:49.000 --> 02:33:51.000]  so these should be around
[02:33:51.000 --> 02:33:53.000]  1.2
[02:33:53.000 --> 02:33:55.000]  so if you see something really
[02:33:55.000 --> 02:33:57.000]  low, really high
[02:33:57.000 --> 02:33:59.000]  mind the case some major issues
[02:33:59.000 --> 02:34:01.000]  don't worry too much about
[02:34:01.000 --> 02:34:03.000]  these wings because the ADC
[02:34:03.000 --> 02:34:05.000]  is not perfect sometimes has a
[02:34:05.000 --> 02:34:07.000]  longer
[02:34:07.000 --> 02:34:09.000]  readout and
[02:34:09.000 --> 02:34:11.000]  we don't really have feedback when something is wrong
[02:34:11.000 --> 02:34:13.000]  you just have a real number so you can
[02:34:13.000 --> 02:34:15.000]  just ignore the smallest wings
[02:34:15.000 --> 02:34:17.000]  if the thing stays
[02:34:17.000 --> 02:34:19.000]  up or down for quite a long time
[02:34:19.000 --> 02:34:21.000]  then it might indicate something
[02:34:21.000 --> 02:34:23.000]  a single point is never an issue
[02:34:23.000 --> 02:34:25.000]  okay
[02:34:25.000 --> 02:34:27.000]  then
[02:34:27.000 --> 02:34:29.000]  the other
[02:34:29.000 --> 02:34:31.000]  another voltage that we are monitoring
[02:34:31.000 --> 02:34:33.000]  is this one
[02:34:33.000 --> 02:34:35.000]  this is basically the same voltage
[02:34:35.000 --> 02:34:37.000]  so I'm not really completely sure
[02:34:37.000 --> 02:34:39.000]  what is the difference between these two
[02:34:39.000 --> 02:34:41.000]  I will just guess there are two different
[02:34:41.000 --> 02:34:43.000]  blocks of the
[02:34:43.000 --> 02:34:45.000]  of the activity that takes that
[02:34:45.000 --> 02:34:47.000]  two different voltages
[02:34:47.000 --> 02:34:49.000]  in order to work
[02:34:49.000 --> 02:34:51.000]  different instances of two
[02:34:51.000 --> 02:34:53.000]  different voltage in order to work
[02:34:53.000 --> 02:34:55.000]  and then we have the temperature
[02:34:55.000 --> 02:34:57.000]  measurement
[02:34:57.000 --> 02:34:59.000]  so this
[02:34:59.000 --> 02:35:01.000]  really the measurement
[02:35:01.000 --> 02:35:03.000]  of the temperature sensor
[02:35:03.000 --> 02:35:05.000]  inside the HPT
[02:35:05.000 --> 02:35:07.000]  you see that it's largely
[02:35:07.000 --> 02:35:09.000]  warm up this was done into the KT
[02:35:09.000 --> 02:35:11.000]  box probably we don't tell you
[02:35:11.000 --> 02:35:13.000]  too much of a control of a temperature
[02:35:13.000 --> 02:35:15.000]  here
[02:35:15.000 --> 02:35:17.000]  these temperature are already
[02:35:17.000 --> 02:35:19.000]  calibrated they come
[02:35:19.000 --> 02:35:21.000]  the information
[02:35:21.000 --> 02:35:23.000]  comes from this big file
[02:35:23.000 --> 02:35:25.000]  that I've also mentioned
[02:35:25.000 --> 02:35:27.000]  at the beginning and the LpGBT group
[02:35:27.000 --> 02:35:29.000]  is providing to us
[02:35:29.000 --> 02:35:31.000]  and contains
[02:35:31.000 --> 02:35:33.000]  a few information
[02:35:33.000 --> 02:35:35.000]  which also the calibration
[02:35:35.000 --> 02:35:37.000]  calibration for the
[02:35:37.000 --> 02:35:39.000]  internal temperature sensor
[02:35:39.000 --> 02:35:41.000]  the LpGBT so this should be quite
[02:35:41.000 --> 02:35:43.000]  reliable
[02:35:43.000 --> 02:35:45.000]  then there are a few extra
[02:35:45.000 --> 02:35:47.000]  that
[02:35:47.000 --> 02:35:49.000]  at the moment we are not using them
[02:35:49.000 --> 02:35:51.000]  and we are just keeping them
[02:35:51.000 --> 02:35:53.000]  available
[02:35:53.000 --> 02:35:55.000]  let me just open
[02:35:55.000 --> 02:35:57.000]  both of them ADC0 and AC3
[02:35:57.000 --> 02:35:59.000]  these are inputs into
[02:35:59.000 --> 02:36:01.000]  the
[02:36:01.000 --> 02:36:03.000]  LpGBT that are coming
[02:36:03.000 --> 02:36:05.000]  from
[02:36:05.000 --> 02:36:07.000]  I think the two hybrids 0 and 3
[02:36:07.000 --> 02:36:09.000]  I think are the two different hybrids
[02:36:09.000 --> 02:36:11.000]  and these are values that
[02:36:11.000 --> 02:36:13.000]  are being
[02:36:13.000 --> 02:36:15.000]  controlled
[02:36:15.000 --> 02:36:17.000]  by the
[02:36:17.000 --> 02:36:19.000]  the CIC so the CIC has the possibility
[02:36:19.000 --> 02:36:21.000]  to output
[02:36:21.000 --> 02:36:23.000]  an analog value that
[02:36:23.000 --> 02:36:25.000]  can monitor
[02:36:25.000 --> 02:36:27.000]  some information internal
[02:36:27.000 --> 02:36:29.000]  to the
[02:36:29.000 --> 02:36:31.000]  CBC
[02:36:31.000 --> 02:36:33.000]  we are not really setting anything
[02:36:33.000 --> 02:36:35.000]  in particular also because it's a little
[02:36:35.000 --> 02:36:37.000]  more complicated because it's the same
[02:36:37.000 --> 02:36:39.000]  line for all the CBC so you
[02:36:39.000 --> 02:36:41.000]  need to enable one CBC at a time
[02:36:41.000 --> 02:36:43.000]  so we just
[02:36:43.000 --> 02:36:45.000]  include them here
[02:36:45.000 --> 02:36:47.000]  just for completeness
[02:36:47.000 --> 02:36:49.000]  but you can safely
[02:36:49.000 --> 02:36:51.000]  ignore them and we can use them
[02:36:51.000 --> 02:36:53.000]  in the future if something comes up
[02:36:53.000 --> 02:36:55.000]  that we need to monitor
[02:36:55.000 --> 02:36:57.000]  for the time being we don't
[02:36:57.000 --> 02:36:59.000]  think there was anything particular
[02:36:59.000 --> 02:37:01.000]  so we just keep them and in this moment
[02:37:01.000 --> 02:37:03.000]  they attach us to something that I
[02:37:03.000 --> 02:37:05.000]  don't even know so
[02:37:05.000 --> 02:37:07.000]  they look cool because they change
[02:37:07.000 --> 02:37:09.000]  but we don't really use them
[02:37:09.000 --> 02:37:11.000]  okay
[02:37:11.000 --> 02:37:13.000]  then
[02:37:13.000 --> 02:37:15.000]  so we have
[02:37:15.000 --> 02:37:17.000]  another
[02:37:17.000 --> 02:37:19.000]  plot so
[02:37:19.000 --> 02:37:21.000]  these
[02:37:21.000 --> 02:37:23.000]  is the monitor on the left
[02:37:25.000 --> 02:37:27.000]  voltage that goes
[02:37:27.000 --> 02:37:29.000]  on the left hybrid is 1.25 volts
[02:37:29.000 --> 02:37:31.000]  I don't think we have
[02:37:31.000 --> 02:37:33.000]  anything about the right hybrid
[02:37:33.000 --> 02:37:35.000]  we are limited input so we are just
[02:37:35.000 --> 02:37:37.000]  one I guess the assumption is that
[02:37:37.000 --> 02:37:39.000]  since everything comes from the DC-DC converter
[02:37:39.000 --> 02:37:41.000]  there is no particular reason
[02:37:41.000 --> 02:37:43.000]  why the left hybrid and the right hybrid
[02:37:43.000 --> 02:37:45.000]  should save a different voltage
[02:37:45.000 --> 02:37:47.000]  and this should be around 1.25
[02:37:47.000 --> 02:37:49.000]  in reality it's like lower
[02:37:49.000 --> 02:37:51.000]  so far I saw this
[02:37:51.000 --> 02:37:53.000]  for every single module we
[02:37:53.000 --> 02:37:55.000]  tested so I think
[02:37:55.000 --> 02:37:57.000]  is as good
[02:37:57.000 --> 02:37:59.000]  as one can expect
[02:37:59.000 --> 02:38:01.000]  as usual quick swing
[02:38:01.000 --> 02:38:03.000]  you can just simply ignore
[02:38:03.000 --> 02:38:05.000]  then
[02:38:05.000 --> 02:38:07.000]  this is the input voltage
[02:38:07.000 --> 02:38:09.000]  that you are providing from the power supply
[02:38:09.000 --> 02:38:11.000]  you usually use
[02:38:11.000 --> 02:38:13.000]  10.5
[02:38:13.000 --> 02:38:15.000]  slightly lower
[02:38:15.000 --> 02:38:17.000]  I'm not sure if this due to some
[02:38:17.000 --> 02:38:19.000]  dropping the cable
[02:38:19.000 --> 02:38:21.000]  or is really that in reality
[02:38:21.000 --> 02:38:23.000]  you have some
[02:38:23.000 --> 02:38:25.000]  some filter on something that's slightly lower
[02:38:25.000 --> 02:38:27.000]  on it is
[02:38:27.000 --> 02:38:29.000]  not super well calibrated
[02:38:29.000 --> 02:38:31.000]  because this goes through a voltage divider
[02:38:31.000 --> 02:38:33.000]  so there might be uncertainty
[02:38:33.000 --> 02:38:35.000]  in the car
[02:38:35.000 --> 02:38:37.000]  in the resistors
[02:38:37.000 --> 02:38:39.000]  that are used in the voltage
[02:38:39.000 --> 02:38:41.000]  divider in my previous
[02:38:41.000 --> 02:38:43.000]  life-different value from that
[02:38:43.000 --> 02:38:45.000]  again the module is very
[02:38:45.000 --> 02:38:47.000]  resilient so
[02:38:47.000 --> 02:38:49.000]  here you will really see something
[02:38:49.000 --> 02:38:51.000]  when you have something
[02:38:51.000 --> 02:38:53.000]  quite big
[02:38:53.000 --> 02:38:55.000]  I think the module can be powerful
[02:38:55.000 --> 02:38:57.000]  8 volts or even something like that
[02:38:57.000 --> 02:38:59.000]  so it is quite resilient
[02:38:59.000 --> 02:39:01.000]  resilient
[02:39:01.000 --> 02:39:03.000]  ADC I already mentioned it
[02:39:03.000 --> 02:39:05.000]  and then sensor
[02:39:05.000 --> 02:39:07.000]  temperature
[02:39:07.000 --> 02:39:09.000]  so this is the temperature
[02:39:09.000 --> 02:39:11.000]  without on the sensor
[02:39:11.000 --> 02:39:13.000]  there is an NTC
[02:39:13.000 --> 02:39:15.000]  negative
[02:39:15.000 --> 02:39:17.000]  power and
[02:39:17.000 --> 02:39:19.000]  negative temperature
[02:39:19.000 --> 02:39:21.000]  for efficient resistor
[02:39:21.000 --> 02:39:23.000]  so it means that
[02:39:23.000 --> 02:39:25.000]  higher is the
[02:39:25.000 --> 02:39:27.000]  lower is the voltage
[02:39:27.000 --> 02:39:29.000]  lower is the
[02:39:29.000 --> 02:39:31.000]  lower is the temperature
[02:39:31.000 --> 02:39:33.000]  lower is the resistance
[02:39:33.000 --> 02:39:35.000]  I think it works like that
[02:39:35.000 --> 02:39:37.000]  but anyway what we do
[02:39:37.000 --> 02:39:39.000]  is that we inject a certain amount of current
[02:39:39.000 --> 02:39:41.000]  and we
[02:39:41.000 --> 02:39:43.000]  in that resistor we read
[02:39:43.000 --> 02:39:45.000]  the voltage
[02:39:45.000 --> 02:39:47.000]  and here is this resistor is the one
[02:39:47.000 --> 02:39:49.000]  that is on the top sensor
[02:39:49.000 --> 02:39:51.000]  in the peak
[02:39:51.000 --> 02:39:53.000]  the high voltage state that has two connections
[02:39:53.000 --> 02:39:55.000]  one of the two is the temperature sensor
[02:39:55.000 --> 02:39:57.000]  and this is the one that we are reading
[02:39:57.000 --> 02:39:59.000]  and this therefore is the temperature of the top sensor
[02:40:03.000 --> 02:40:05.000]  then leakage current
[02:40:05.000 --> 02:40:07.000]  so this is the measurement
[02:40:07.000 --> 02:40:09.000]  of the current
[02:40:09.000 --> 02:40:11.000]  that
[02:40:11.000 --> 02:40:13.000]  the
[02:40:13.000 --> 02:40:15.000]  the VTRX needs to be called incoming
[02:40:19.000 --> 02:40:21.000]  light
[02:40:21.000 --> 02:40:23.000]  and therefore there is a diode that
[02:40:23.000 --> 02:40:25.000]  connects
[02:40:25.000 --> 02:40:27.000]  to the fiber
[02:40:27.000 --> 02:40:29.000]  and the diode transforms
[02:40:29.000 --> 02:40:31.000]  the signal
[02:40:31.000 --> 02:40:33.000]  in the optical signal to a current
[02:40:33.000 --> 02:40:35.000]  that is used for
[02:40:35.000 --> 02:40:37.000]  the data
[02:40:37.000 --> 02:40:39.000]  transfer and then
[02:40:39.000 --> 02:40:41.000]  you can measure the leakage current
[02:40:41.000 --> 02:40:43.000]  of these
[02:40:43.000 --> 02:40:45.000]  of these
[02:40:45.000 --> 02:40:47.000]  diodes
[02:40:47.000 --> 02:40:49.000]  so I think we have a quick discussion
[02:40:49.000 --> 02:40:51.000]  few weeks ago
[02:40:51.000 --> 02:40:53.000]  with Dana
[02:40:53.000 --> 02:40:55.000]  I
[02:40:55.000 --> 02:40:57.000]  understood the first time
[02:40:57.000 --> 02:40:59.000]  I think this indicates really the average light
[02:40:59.000 --> 02:41:01.000]  that is collected
[02:41:01.000 --> 02:41:03.000]  by this diode
[02:41:03.000 --> 02:41:05.000]  for the time being
[02:41:05.000 --> 02:41:07.000]  I don't think it's going to tell us too much
[02:41:07.000 --> 02:41:09.000]  but in the future with the radiation
[02:41:09.000 --> 02:41:11.000]  this will probably go
[02:41:11.000 --> 02:41:13.000]  down I guess
[02:41:13.000 --> 02:41:15.000]  because the conversion factor
[02:41:15.000 --> 02:41:17.000]  from a photon collector will go down
[02:41:17.000 --> 02:41:19.000]  so this can be used for monitoring
[02:41:19.000 --> 02:41:21.000]  radiation damage
[02:41:21.000 --> 02:41:23.000]  of course we are not
[02:41:23.000 --> 02:41:25.000]  in this case for production
[02:41:25.000 --> 02:41:27.000]  really the
[02:41:27.000 --> 02:41:29.000]  last two
[02:41:29.000 --> 02:41:31.000]  so these are two
[02:41:31.000 --> 02:41:33.000]  temperature sensor
[02:41:33.000 --> 02:41:35.000]  I'm going to open them both
[02:41:35.000 --> 02:41:37.000]  let's close a few things
[02:41:37.000 --> 02:41:39.000]  zoom
[02:41:39.000 --> 02:41:41.000]  zoom
[02:41:41.000 --> 02:41:43.000]  zoom
[02:41:43.000 --> 02:41:45.000]  zoom
[02:41:45.000 --> 02:41:47.000]  zoom
[02:41:47.000 --> 02:41:49.000]  so the B pole
[02:41:49.000 --> 02:41:51.000]  is the
[02:41:51.000 --> 02:41:53.000]  the
[02:41:53.000 --> 02:41:55.000]  chip
[02:41:55.000 --> 02:41:57.000]  that allow you to convert
[02:41:57.000 --> 02:41:59.000]  the
[02:41:59.000 --> 02:42:01.000]  in the
[02:42:01.000 --> 02:42:03.000]  in the
[02:42:03.000 --> 02:42:05.000]  DC DC convert
[02:42:05.000 --> 02:42:07.000]  convert the
[02:42:07.000 --> 02:42:09.000]  voltage
[02:42:09.000 --> 02:42:11.000]  that you are providing to the module
[02:42:11.000 --> 02:42:13.000]  to the needed voltages for the chip
[02:42:13.000 --> 02:42:15.000]  for the chips into the module
[02:42:15.000 --> 02:42:17.000]  and there are two
[02:42:17.000 --> 02:42:19.000]  the B pole 12
[02:42:19.000 --> 02:42:21.000]  that can convert
[02:42:21.000 --> 02:42:23.000]  12 because it can convert 12 volts
[02:42:23.000 --> 02:42:25.000]  in reality we use only 10.5 volts
[02:42:25.000 --> 02:42:27.000]  into
[02:42:27.000 --> 02:42:29.000]  2.5 volts
[02:42:29.000 --> 02:42:31.000]  and then a second stage
[02:42:31.000 --> 02:42:33.000]  that converts from 12
[02:42:33.000 --> 02:42:35.000]  so is the B pole
[02:42:35.000 --> 02:42:37.000]  2B5
[02:42:37.000 --> 02:42:39.000]  from
[02:42:39.000 --> 02:42:41.000]  2.5 to 1.2 volts
[02:42:41.000 --> 02:42:43.000]  something like that
[02:42:43.000 --> 02:42:45.000]  we have two stages because
[02:42:45.000 --> 02:42:47.000]  the VTRAX needs 2.5 volts
[02:42:47.000 --> 02:42:49.000]  so that's why we need
[02:42:49.000 --> 02:42:51.000]  2
[02:42:51.000 --> 02:42:53.000]  and that's why
[02:42:53.000 --> 02:42:55.000]  there are these two steps
[02:42:55.000 --> 02:42:57.000]  so each one of these
[02:42:57.000 --> 02:42:59.000]  they have
[02:42:59.000 --> 02:43:01.000]  temperature sensor
[02:43:01.000 --> 02:43:03.000]  that we that are connected also
[02:43:03.000 --> 02:43:05.000]  to the
[02:43:05.000 --> 02:43:07.000]  VT
[02:43:07.000 --> 02:43:09.000]  however
[02:43:09.000 --> 02:43:11.000]  these two sensor
[02:43:11.000 --> 02:43:13.000]  temperature sensor are not calibrated
[02:43:13.000 --> 02:43:15.000]  in particular is not calibrated
[02:43:15.000 --> 02:43:17.000]  the offset
[02:43:17.000 --> 02:43:19.000]  the
[02:43:19.000 --> 02:43:21.000]  the slope is quite
[02:43:21.000 --> 02:43:23.000]  precise as for all the
[02:43:23.000 --> 02:43:25.000]  temperature sensor we have available
[02:43:25.000 --> 02:43:27.000]  in the chips in this example
[02:43:27.000 --> 02:43:29.000]  but the offset is not calibrated
[02:43:29.000 --> 02:43:31.000]  I don't think we have anything to correct
[02:43:31.000 --> 02:43:33.000]  for that
[02:43:33.000 --> 02:43:35.000]  so don't use to match the absolute value
[02:43:35.000 --> 02:43:37.000]  but you can use the variations to see
[02:43:37.000 --> 02:43:39.000]  for example if it's not well connected
[02:43:39.000 --> 02:43:41.000]  you must see that this value might go
[02:43:41.000 --> 02:43:43.000]  much higher than the other one
[02:43:43.000 --> 02:43:45.000]  in the offset
[02:43:45.000 --> 02:43:47.000]  I just took the more or less the
[02:43:47.000 --> 02:43:49.000]  average that I see in the model
[02:43:49.000 --> 02:43:51.000]  they had a bit of a study
[02:43:51.000 --> 02:43:53.000]  what is the offset variation to the center point
[02:43:53.000 --> 02:43:55.000]  just to have something reasonable
[02:43:55.000 --> 02:43:57.000]  but again the absolute
[02:43:57.000 --> 02:43:59.000]  value is not really
[02:43:59.000 --> 02:44:01.000]  alive
[02:44:01.000 --> 02:44:03.000]  and I think if I'm not mistaken
[02:44:03.000 --> 02:44:05.000]  that's all because at the other level
[02:44:05.000 --> 02:44:07.000]  we don't have any other
[02:44:07.000 --> 02:44:09.000]  monitoring capabilities
[02:44:09.000 --> 02:44:11.000]  for the 2S
[02:44:11.000 --> 02:44:13.000]  or at least we don't
[02:44:13.000 --> 02:44:15.000]  monitor anything else
[02:44:15.000 --> 02:44:17.000]  so
[02:44:17.000 --> 02:44:19.000]  this is all for the monitoring part
[02:44:19.000 --> 02:44:21.000]  for the 2S
[02:44:21.000 --> 02:44:23.000]  any
[02:44:23.000 --> 02:44:25.000]  questions comments
[02:44:25.000 --> 02:44:27.000]  on this
[02:44:35.000 --> 02:44:37.000]  anything
[02:44:37.000 --> 02:44:39.000]  you would like to ask
[02:44:39.000 --> 02:44:41.000]  discuss
[02:44:43.000 --> 02:44:45.000]  sorry for
[02:44:45.000 --> 02:44:47.000]  you
[02:44:47.000 --> 02:44:49.000]  I just have one question
[02:44:49.000 --> 02:44:51.000]  the leakage current of the
[02:44:51.000 --> 02:44:53.000]  VTRX plus
[02:44:53.000 --> 02:44:55.000]  the RSSI signal
[02:44:55.000 --> 02:44:57.000]  for its
[02:44:59.000 --> 02:45:01.000]  it is exactly that one
[02:45:01.000 --> 02:45:03.000]  and I just
[02:45:03.000 --> 02:45:05.000]  use this one because
[02:45:05.000 --> 02:45:07.000]  okay
[02:45:07.000 --> 02:45:09.000]  not that I understand much better
[02:45:09.000 --> 02:45:11.000]  this leakage current but I thought was
[02:45:11.000 --> 02:45:13.000]  slightly more
[02:45:13.000 --> 02:45:15.000]  comprehensible than the RSSI
[02:45:15.000 --> 02:45:17.000]  it is exactly that thing
[02:45:17.000 --> 02:45:19.000]  thank you
[02:45:23.000 --> 02:45:25.000]  okay
[02:45:25.000 --> 02:45:27.000]  okay
[02:45:27.000 --> 02:45:29.000]  I'm gonna
[02:45:29.000 --> 02:45:31.000]  stub
[02:45:31.000 --> 02:45:33.000]  sharing
[02:45:35.000 --> 02:45:37.000]  I'm gonna also
[02:45:37.000 --> 02:45:39.000]  stub recording
