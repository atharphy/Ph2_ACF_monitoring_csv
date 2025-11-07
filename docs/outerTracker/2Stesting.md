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

##### Alignemnt -> establish proper communication for all chips on a module & FPGA

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
This is the last step of the CBCICIC alignment. It is more relevant for PS modules where the stub info is sent over 2words but it is performed also for 2S ones even if the stub info is sent into a single word.

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


![CBCtoCIC_PatternMatchingErrorRate_Hybrid](../images/OTtesting/2S/CBCtoCIC_PatternMatchingErrorRate_Hybrid.png)

This concludes the alignment section.

##### PedestalEqualization (also known as Trimming) - Chip
By design, the comparators that set the threshold for each channel have some unavoidable production variations. To compensate, each CBC chip allows an offset to be applied to the amplifier signal. This adjusts the pedestal up or down so that the threshold is uniform across all channels.
During pedestal equalization, these offsets are set channel by channel to ensure consistent thresholds. 

The results of this calibration are stored at the chip level.

The first plot shows for every channel in the CBC the offset that was chosen.
If something unusual appears later, it is possible to check whether any channel offsets have reached their extremes—0 or 255—which could indicate a failure. The CBC has so far demonstrated very stable behavior, with no occurrences of this issue.
![ChannelOffsetValues_Chip](../images/OTtesting/2S/ChannelOffsetValues_Chip.png)


In the plot below, the x-axis represents the channel, while the y-axis shows occupancy. The goal is to achieve approximately 50% occupancy, which corresponds to the expected value after calibration. If any channel shows unusually high or low occupancy, it may indicate an issue with that channel. Some variation is expected due to the discrete adjustment steps, but overall the distribution should be roughly uniform.
![ChannelOccupancyAfterOffsetEqualization_Chip](../images/OTtesting/2S/ChannelOccupancyAfterOffsetEqualization_Chip.png)

##### PedeNoise - Hybrid, Chip, Channel

A scan of the applied threshold is performed, measuring the occupancy (without injection) for each threshold value. Ideally, this would produce a perfect step function. In reality, the presence of noise modifies the response, effectively convoluting the step function with a Gaussian. The Gaussian represents the noise in the system, and the resulting curve takes an S shape. This S-shaped curve is referred to as the “S curve.”

For this test, many plots are saved at different levels.

The S-Curve distribution is saved for each channel and shown below for one example channel.
On the X-axis there is the applied threshold in VcTh units (1 VcTh unit =  156 electrons). Higher VcTh correspond to lower thresholds. On the Y-axis there is the occpuncy. Each S-curve is fitted individually. From this fit, the Gaussian component of the convolution allows extraction of the noise, represented by the width of the Gaussian. The underlying step function from the convolution corresponds to the pedestal, which is measured at around 50% occupancy. This measurement is performed without injection, so the pedestal obtained reflects the actual baseline of the system.
![SCurve_1channel](../images/OTtesting/2S/SCurve_1channel.png)

At the chip level, a 2D summary plot is stored summarizing the S-curve of all channels. Each plot shows channels on the x-axis, thresholds on the y-axis, and occupancy on the z-axis, with each line representing a single channel.

![SCurve_Chip](../images/OTtesting/2S/SCurve_Chip.png)

<details>
  <summary>Known issues</summary>

Broken wirebond or disconnected bump bond can show up as a compressed S-curve for a specific channel. If the issues appears in cold and disappears at room temperature it may be the CBC known issue of the corrupted offset register.

![SCurve_Chip_buggy](../images/OTtesting/2S/SCurve_Chip_buggy.png)

Another know issues is when horizontal stripes are present. This is a communication issue affecting the whole module. Example will be added when found again.


</details>


From the S-Curve, the pedestal for every channel can be extracted. The cumulative distribution of the pedestal should appear very sharp, while failures would show as long tails or outliers.
![PedestalDistribution_Chip](../images/OTtesting/2S/PedestalDistribution_Chip.png)

The channel pedestal plot shows the pedestal for each channel, revealing a very uniform distribution across all channels within a few VcTh, corresponding to a width of a few hundred electrons.
![ChannelPedestal_Chip](../images/OTtesting/2S/ChannelPedestal_Chip.png)

Similar distribution are also shown for the noise.
![NoiseDistribution_Chip](../images/OTtesting/2S/NoiseDistribution_Chip.png)
![ChannelNoise_Chip](../images/OTtesting/2S/ChannelNoise_Chip.png)

The bottom sensor has a higher noise compared to the top because of longer traces in the foldover hybrid.
![ChannelNoiseTop_Chip](../images/OTtesting/2S/ChannelNoiseTop_Chip.png)
![ChannelNoiseBottom_Chip](../images/OTtesting/2S/ChannelNoiseBottom_Chip.png)

There are there very similar distributions summarizing the performance at the hybrid level.

![NoiseDistribution](../images/OTtesting/2S/NoiseDistribution.png)
![StripChannelNoise](../images/OTtesting/2S/StripChannelNoise.png)

The bottom sensor has a higher noise compared to the top because of longer traces (extra capacitance) in the foldover hybrid.
![StripChannelNoiseTop](../images/OTtesting/2S/StripChannelNoiseTop.png)
![StripChannelNoiseBottom](../images/OTtesting/2S/StripChannelNoiseBottom.png)

<details>
  <summary>Known issues</summary>

A group of channel with high noise may indicate a scratch on the sensor.
![StripChannelNoise_Scratch](../images/OTtesting/2S/StripChannelNoise_Scratch.png)

A channel with low noise could indicate a broken wirebond. Below 2 a broken bumpbond.
![StripChannelNoise_brokenBons](../images/OTtesting/2S/StripChannelNoise_brokenBonds.png)

Groups of broken channels in the center of CBC can indicate that sparking occurred.
![StripChannelNoise_Sparking](../images/OTtesting/2S/StripChannelNoise_Sparking.png)

If the HV is not applied, a very high noise is shown over both hybrids and sensors.
</details>

##### OTinjectionDelayOptimization - Chip

After pedestal tests, injection tests are performed by scanning the injection delay and measuring, for each delay, the threshold corresponding to 50% occupancy—where the input signal equals the comparator threshold—allowing reconstruction of the full signal distribution as a function of time.

![ThresholdVsDelayScan_Chip](../images/OTtesting/2S/ThresholdVsDelayScan_Chip.png)

At this stage, the working point can be determined by setting the injection delay at the signal peak, which in first approximation is independent of the injected charge, ensuring that the signal always crosses the threshold at the same instant; the distance from the pedestal is then adjusted—typically five times the measured pedestal noise from the previous calibration—to suppress pedestal-induced noise and precisely define the working point.

![BestThresholdAndDelay_Chip](../images/OTtesting/2S/BestThresholdAndDelay_Chip.png)

From this point onward, each injection measurement is performed using the identified injection delay and threshold, ensuring that subsequent tests are properly configured so that any injected charge is read at the correct value.

##### OTinjectionOccupancyScan - Chip

The occupancy is measured for different injection charges to establish a reference: first by recording the occupancy without injection to quantify the contribution from noise alone, and then by measuring the response for known injected charges, allowing evaluation of the detection efficiency for signals corresponding to specific charge amounts.

Five different measurements are performed, corresponding to five different plots. Three examples are shown, for no injection and one for some injected charge.

The one below is without injection.
![ChannelOccupancy_Injection_0.000_MIP_Chip](../images/OTtesting/2S/ChannelOccupancy_Injection_0.000_MIP_Chip_Chip.png)

The occupancy is measured for each chip, and although some slight activity may appear, the threshold is set to five times the noise, so almost no signal is expected except for very small fluctuations (that may be more visible in log scale).

Then, injections are performed at different charge levels — for example, a quarter of a MIP, which corresponds to roughly the same level as the threshold set at five times the noise, yielding about 50% efficiency. This value indicates that a signal equivalent to a quarter of a MIP produces a 50% detection probability. Measuring lower charges is important to study cluster size and improve spatial resolution. Subsequent plots show the occupancy for each channel at 0.25, 0.5, 1, and 2 MIPs, allowing identification of potential issues. The inspection of these results is automated by potato, which flags problematic modules; manual inspection is mainly needed for those flagged as bad. Since each CBC chip behaves slightly differently, occupancy maps are produced per CBC and per channel, enabling the identification of noisy or inefficient channels, such as those with damaged comparators, although new modules typically show very few such cases.

![ChannelOccupancy_Injection_0.250_MIP_Chip](../images/OTtesting/2S/ChannelOccupancy_Injection_0.250_MIP_Chip.png)
![ChannelOccupancy_Injection_1.000_MIP_Chip](../images/OTtesting/2S/ChannelOccupancy_Injection_1.000_MIP_Chip.png)

##### OTCMNoise (Common Noise) - OpticalGroup, Hybrid, Chip
The common mode noise test checks whether there is any correlation in the noise across different channels of the same chip.
Ideally, each channel should behave independently, meaning that noise fluctuations in one channel should not affect others. However, in reality, the channels share common elements — such as the same ground, power supply, and piece of silicon — which can lead to correlated noise.
This test is used to quantify that correlation. Two types of measurements are performed, Occupancy-driven common noise, where the threshold is set such that each channel has about 50% occupancy and one with the threshold at 3 sigma from the pedestal. Since the pedestal has already been tuned, all channels should exhibit similar occupancy. Plots will be shown only for the first case.
In a perfectly uncorrelated system, the distribution of the number of hits per event would follow a binomial shape centered at half the number of channels (e.g., 128 hits for a 256-channel chip like the CBC).
In practice, however, the distribution shows tails on both sides — events with unusually high or low numbers of hits. The width of this distribution reflects the level of correlation between channels: the broader it is, the stronger the common mode noise.
The inspection of this test is typically automated by potato, but manual checks can be done if a module shows anomalous behavior.

- Below the common noise distribution for one CBC is shown. 

Here the hits on one chip are shown. The same plot exist divided for top and bottom sensors. Since all channels are connected to the same sensor, slight differences in behavior can occur between the top and bottom sensors, leading to small variations in the observed common mode noise.
![CommonNoiseHits_OccupancyDriven_Chip](../images/OTtesting/2S/CommonNoiseHits_OccupancyDriven_Chip.png)

A top–bottom correlation is expected since the channels belong to the same chip; this is visualized by plotting the two distributions together in a correlation plot, where the presence of a diagonal indicates some correlation, though the effect is minor and not concerning given that all elements share the same sensor and electronics.
![CommonNoiseTopBottomCorrelation_OccupancyDriven_Chip](../images/OTtesting/2S/CommonNoiseTopBottomCorrelation_OccupancyDriven_Chip.png)

Here we show the correlaton of the chip with the rest of the hybrid.
![CommonNoiseHybridCorrelation_OccupancyDriven_Chip](../images/OTtesting/2S/CommonNoiseHybridCorrelation_OccupancyDriven_Chip.png)

Here we have the correlation of the channels within one chip, showing which other channels are firing when one channel is firing. The diagonal is comparing one channel with itself.
![2DChipHits_OccupancyDriven_Chip](../images/OTtesting/2S/2DChipHits_OccupancyDriven_Chip.png)

- Now we show distributions at the hybrid level


Here we look at the distribution of the number of hits per event across the entire hybrid. In this case, the range extends from 0 up to 2036 channels (corresponding to 254 × 8).The same plot exist divided for top and bottom sensors.
![CommonNoiseHits_OccupancyDriven_Hybrid](../images/OTtesting/2S/CommonNoiseHits_OccupancyDriven_Hybrid.png)

As before, the correlation between the top and bottom strips is observed, showing a significant degree of correlation. This behavior reflects the real conditions of the system and is not a major concern.
![CommonNoiseTopBottomCorrelation_OccupancyDriven_Hybrid](../images/OTtesting/2S/CommonNoiseTopBottomCorrelation_OccupancyDriven_Hybrid.png)

- OpticalGroup/Module level distributions

Here we have more than 4000 channels. As before we have also plots for top and bottom divided and for their correlations. The correlations between top and bottom is lower as we are now combining two hybrids.
![CommonNoiseHits_OccupancyDriven_OpticalGroup](../images/OTtesting/2S/CommonNoiseHits_OccupancyDriven_OpticalGroupd.png)

The two hybrids below appear uncorrelated. 
![CommonNoiseCrossHybridCorrelation_OccupancyDriven_OpticalGroup](../images/OTtesting/2S/CommonNoiseCrossHybridCorrelation_OccupancyDriven_OpticalGroup.png)


These results are harder to interpret, reflecting the true behavior of the module, so if any unusual tails or unexpected noise appear in the pedenoise results, these plots should be checked to identify possible anomalies by comparing them with reference common noise plots from other modules to confirm that the noise distribution matches expectations.


An additional measurement was included using the same plot evaluated above but with the threshold at three sigma from the pedestal, providing an alternative way to visualize the noise effect. At zero sigma (shown above), where the expected occupancy is 50%, both left and right tails can be inspected since not all channels are expected to fire simultaneously, while at three sigma, the focus is on the right tail to highlight deviations. This offers the same information from a different perspective, emphasizing one side of the distribution. Given the current dataset, there is no clear advantage to using one representation over the other, so both are included since the acquisition time is minimal. The three-sigma plot is generally more straightforward to interpret because it isolates one tail, while the zero-sigma version requires considering both sides. For occupancy-driven tests, the zero- and three-sigma plots contain equivalent information, except that the three-sigma plots are shifted left due to lower average occupancy.

##### Electric Chain Validation (ECV)
The electric chain validation focuses on evaluating the width of the working area of the communication phases within the module. Unlike the verification step, which uses the phase identified by the CIC or the LpGBT as optimal, the validation manually scans different phases to determine the range over which the chain remains operational. A broad working area indicates a stable configuration, while a narrow one suggests that the system operates close to its limits and may become unstable once installed in the detector.

##### OTCICtoLpGBTecv - Hybrid
The CIC-to-LpGBT ECV studies the transmission between the CIC and LpGBT by varying the LpGBT sampling phase and checking whether the data patterns sent by the CIC are correctly reconstructed in the FPGA. For each phase setting, the number of tested bits and corresponding error rate are recorded in hybrid-level plots labeled as CIC-to-LpGBT pattern matching. The test also explores the impact of varying the current used by the CIC to drive the data (SLVS strenght, from 1 to 5), the LpGBT clock polarity, and the CIC clock drive strength (from 1 to 7). The LpGBT provides the clock to the hybrid, and changing its polarity effectively shifts the clock phase by 50%. These variations help evaluate how different transmission parameters affect data reconstruction and identify the range of stable operating conditions where several phases ensure reliable communication between components.


This plot shows, on the Y axis, the line ID corresponding to each transmission line, and on the X axis, the manually selected LpGBT sampling phase, which ranges from 0 to 14. The phase is varied manually rather than letting the LpGBT automatically adjust it, in order to explore also regions where the LpGBT cannot properly sample the incoming data. The Z axis represents the number of tested bits, with the stub pattern matched in firmware for speed, while the Level-1 pattern matching is performed in the software, resulting in longer scan times and fewer tested bits.
![CICtoLpGBT_PatternMatchingTestedBits_CIC_SLVScurrent_5_LpGBT_Clock_Polarity_0_Clock_Strength_7_Hybrid](../images/OTtesting/2S/CICtoLpGBT_PatternMatchingTestedBits_CIC_SLVScurrent_5_LpGBT_Clock_Polarity_0_Clock_Strength_7_Hybrid.png)


In the plot below the Z axis represents the number of errors. Some lines are expected not to work properly, since sampling may occur when the incoming data from the CIC are transitioning, leading to bit misinterpretation. The quality of a module is therefore evaluated by the width of the phase range over which correct data transmission is achieved.
![CICtoLpGBT_PatternMatchingErrorRate_CIC_SLVScurrent_5_LpGBT_Clock_Polarity_0_Clock_Strength_7_Hybrid](../images/OTtesting/2S/CICtoLpGBT_PatternMatchingErrorRate_CIC_SLVScurrent_5_LpGBT_Clock_Polarity_0_Clock_Strength_7_Hybrid.png)

Many versions of the above plots are stored for the various values and combinations of the current used by the CIC to drive the data (SLVS strenght, from 1 to 5), the LpGBT clock polarity, and the CIC clock drive strength (from 1 to 7).

##### OTalignLpGBTinputsForBypass - Hybrid
The next step in the electrical chain validation is an auxiliary procedure that allows studying data transmission with the CIC in bypass mode. Normally, the CIC scrambles the data coming from the readout chip, making it impossible to determine on which line an error occurs, but in bypass mode the CIC simply forwards the incoming data to the LpGBT without processing. Since each CBC sends six lines to the CIC, and only six lines total go from the CIC to the LpGBT, not all can be forwarded simultaneously, so a subset must be selected. The CIC allows forwarding only four lines at a time, grouped into so-called “five ports.” There are twelve five ports in total: the first ten carry trigger information, and the last two carry Level-1 data. 
[Mapping](https://fnal-outer-tracker.docs.cern.ch/documents/PhyPortMap.pdf) these correctly is complicated by the fact that the CIC and I2C systems use different front-end IDs to identify connected chips, and these IDs do not match. PH2_ACF uses the I2C IDs and automatically performs the internal remapping between the CIC front-end IDs, five ports, and line indices, so this does not need to be handled manually.
A further complication arises because, in bypass mode, the CIC output is no longer synchronized to the standard clock that the LpGBT uses for phase alignment. As a result, the phase alignment of the LpGBT must be re-tuned manually. The procedure consists of scanning the LpGBT phases for each five port individually to identify the correct phase alignment for data transmission, as shown in the “LpGBT for CIC bypass” plots.

In these plots, the x-axis shows the LpGBT phase, and the y-axis shows the line index. You will always see four lines—stub 1 through stub 4—since the forwarding in bypass mode always goes through four lines, regardless of which CBC they originate from. These labels are therefore arbitrary and not correlated with specific CBC lines, but they are needed for display. Here we see again the tested bits.
![LpGBTforCICbypass_PhaseScanTestedBits_phyPort0_Hybrid](../images/OTtesting/2S/LpGBTforCICbypass_PhaseScanTestedBits_phyPort0_Hybrid.png)


The corresponding plot below shows the error rate (from 0 to 1) as a function of phase and line. The goal is to identify the working region where no transmission errors occur. The optimal working point is chosen at the center of the widest error-free region, ensuring that the data bypassed from the CIC to the LpGBT and sent to the board are correctly received. Because each five-port behaves differently and has its own optimal phase alignment, this scan must be repeated for every five-port. 

![LpGBTforCICbypass_PhaseScanBitErrorRate_phyPort0_Hybrid](../images/OTtesting/2S/LpGBTforCICbypass_PhaseScanBitErrorRate_phyPort0_Hybrid.png)

All these scans are auxiliary calibration steps—if everything works properly, the details of these plots can be ignored, since their purpose is simply to enable the final validation of the electrical chain between the CBC and the CIC.

##### OTChipToCICecv - Hybrid
The final step of the electrical chain validation focuses on the link between the CBC and the CIC. For this stage, we again produce two plots per scan point: the error rate and the number of tests. The procedure is similar to the previous validation steps, but here we vary the CBC output drive current that controls the signal strength on the lines between the CBC and the CIC. Three current settings are typically used to study the behavior of the link. In this configuration, a higher drive strength corresponds to a lower numerical value—so current setting 0 gives the highest current, 14 the lowest, and 8 an intermediate value.

The plots follow the same format as before, with the phase on the x-axis and the line ID on the y-axis, showing all CBCs and their corresponding lines. Phases 2 and 3 are absent because they are not functional on the CIC and are therefore skipped. As usual, the number of tests is smaller for the Level-1 data since those checks are performed in software—still around 10⁵ to ensure sufficient statistics without excessive runtime. 

![CBCtoCIC_PhaseScanTestedBits_CBC_SLVScurrent_0_Hybrid](../images/OTtesting/2S/CBCtoCIC_PhaseScanTestedBits_CBC_SLVScurrent_0_Hybrid.png)

The corresponding error-rate plots show that most channels exhibit a broad phase region with zero errors, indicating a stable and well-aligned communication between the CBC and the CIC.

![CBCtoCIC_PhaseScanErrorRate_CBC_SLVScurrent_0_Hybrid](../images/OTtesting/2S/CBCtoCIC_PhaseScanErrorRate_CBC_SLVScurrent_0_Hybrid.png)
The Level-1 channels occasionally show issues in the pattern matching due to imperfect data sampling, leading to rare misreads and preventing a 100% match rate. While this is not a major concern, improvements are being explored, though the underlying sampling mechanism makes it difficult to fully eliminate. The plots clearly show that non-working phases have much higher error rates—around 45% compared to below 0.2% in well-aligned regions. As before, the results are shown for four different current settings, all displaying similar behavior.

This is the conclusion of the electric chain validation.


[02:17:17.000 --> 02:17:19.000]  so a bit error rate
[02:17:19.000 --> 02:17:21.000]  this one is the newest one
[02:17:21.000 --> 02:17:23.000]  so the idea
[02:17:23.000 --> 02:17:25.000]  here
[02:17:25.000 --> 02:17:27.000]  is that we want to
[02:17:27.000 --> 02:17:29.000]  check
[02:17:29.000 --> 02:17:31.000]  the stability of the link between the
[02:17:31.000 --> 02:17:33.000]  basically the LpGBT
[02:17:33.000 --> 02:17:35.000]  and FPGA going through
[02:17:35.000 --> 02:17:37.000]  the VTRX
[02:17:37.000 --> 02:17:39.000]  so this will allow you to check
[02:17:39.000 --> 02:17:41.000]  if there are problems between
[02:17:41.000 --> 02:17:43.000]  the LpGBT and VTRX
[02:17:43.000 --> 02:17:45.000]  or between the VTRX
[02:17:45.000 --> 02:17:47.000]  and the board
[02:17:47.000 --> 02:17:49.000]  and
[02:17:49.000 --> 02:17:51.000]  so
[02:17:51.000 --> 02:17:53.000]  the LpGBT has some functionality
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
[02:18:23.000 --> 02:18:25.000]  sent by the LpGBT
[02:18:25.000 --> 02:18:27.000]  matches the expected pattern
[02:18:27.000 --> 02:18:29.000]  received by the FC7
[02:18:29.000 --> 02:18:31.000]  and this will allow us to
[02:18:31.000 --> 02:18:33.000]  determine if there were any bits
[02:18:33.000 --> 02:18:35.000]  that got corrupted
[02:18:35.000 --> 02:18:37.000]  during the transmission
[02:18:37.000 --> 02:18:39.000]  so
[02:18:43.000 --> 02:18:45.000]  so the
[02:18:45.000 --> 02:18:47.000]  LpGBT has different
[02:18:47.000 --> 02:18:49.000]  procedure that can be used
[02:18:49.000 --> 02:18:51.000]  for generating this pattern
[02:18:51.000 --> 02:18:53.000]  you can check them into the
[02:18:53.000 --> 02:18:55.000]  LpGBT manual
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
[02:19:17.000 --> 02:19:19.000]  encoded by the LpGBT
[02:19:19.000 --> 02:19:21.000]  and then decoded back
[02:19:21.000 --> 02:19:23.000]  so you're gonna see even if the lines
[02:19:23.000 --> 02:19:25.000]  between the LpGBT and VTRX
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
[02:20:15.000 --> 02:20:17.000]  since it's done at the level of the LpGBT
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
[02:20:41.000 --> 02:20:43.000]  the LpGBT
[02:20:43.000 --> 02:20:45.000]  generates this bit error rate
[02:20:45.000 --> 02:20:47.000]  pattern
[02:20:47.000 --> 02:20:49.000]  from
[02:20:49.000 --> 02:20:51.000]  clock source
[02:20:51.000 --> 02:20:53.000]  that you can change the phase
[02:20:53.000 --> 02:20:55.000]  and there are some phases in which the LpGBT
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
[02:21:29.000 --> 02:21:31.000]  where the LpGBT
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
