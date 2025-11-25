# Description of 2S test results

[TOC maxLevel=3]

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

![VTRx_LightYieldScan](OTtesting/common/VTRx_LightYieldScan.png)

The bias and modulation, shown on the x and y axes of the plot, are two registers of the VTRX that control the laser driver. On the Z axis we have the power in microWatt that is measured by the the SFP connector, the receiver on the FC7.

We obtain the distribution measuring the power varying the bias and modulation.
Moving from the left to the right, the power increases.
For the modulation, when you increase it, you decrease the power. The reason is that basically the bias sets the high level and instead the modulation controls the swing down.

If the power in the x and y direction is not changing, something is going on with the driver controller. Moreover, the optical power that you receive might be lower in case you have some damage on the fibers - less obvious and this will be handled by potato. Very likely you will see other problems.

<details>
  <summary>Known issues</summary>

A distribution like the one below may be due to a problematic SFP connector on the FC7 or dirt in the fibers.

![VTRx_LightYieldScan_buggy](OTtesting/common/VTRx_LightYieldScan_buggy.png)
</details>


##### OTLpGBTEyeOpeningTest - OpticalGroup

The calibration is performed for three different values of the electrical attenuation that the LpGBT applies to the VTRx signal that is proportional to the optical power that is received. The signal can be attenuated to 1/3, 2/3 or not attenuated. Here we show and explain the result for one of the attenuations.

![LpGBT_EyeOpeningScan](OTtesting/common/LpGBT_EyeOpeningScan.png)

The eye opening is a capability of the LpGBT and more info can be found in the manual. A high count rate (z-axis) corresponds to the center of the eye, above the lower part of the signal (voltage) but below the high part of the signal (voltage) - the large yellow area. Outside this range of the signal there is a lower count. The absolute numbers are not easy to interpret. The relevant part is the transition region between the two yellow areas. 
This plot was especially relevant for LpGBT v1 that had known issues with the transistion part moving up and down with the power.

For a more detailed explanation check the video tutorial around minutes 27-32. This is not reported here as this should not be problematic anymore with LpGBTv2.


##### Alignemnt -> establish proper communication for all chips on a module & FPGA

##### OTalignLpGBTinputs - OpticalGroup

To better understand the alignment steps, please refer to the [2S module communication scheme on slide 5](https://indico.cern.ch/event/1540157/contributions/6481541/attachments/3057152/5426570/FRavera_2025_04_28_2Sschool.pdf) where we see the 8 CBCs per side that communicate with one CIC. The CIC (one per side) communicate with the LpGBT and the LpGBT with the FPGA.

For the data rate that can be handled in the FPGA, the signal coming from the module is then split in separate components for the two FEHs and then in L1 (red line) and stub data (blue lines).

This test is used to make sure that the LpGBT understands what the CIC sends.
Basically the LpGBT samples the data received and this test finds the correct sampling phase. If the phase is not correct, sometimes a one can be interpreted as a zero, or vice versa, and therefore the communication will not work. 

This is an automatic procedure done by the LpGBT that has this automatic phase alignment. The test sets the CIC in a state that sends a specific pattern and ask the LpGBT to align, ie to find the best phase for sampling the incoming data from the CIC.

There are three plots associated with this test, identified with `CICtoLpGBT` string. 

![CICtoLpGBT_PhaseAlignmentEfficiency](OTtesting/2S/CICtoLpGBT_PhaseAlignmentEfficiency.png)

The *Phase Alignment Efficiency* is obtained repeting the automic phase alignment 100 times (default value that can be configured in the XML) and counting how many times the alignment succeeded. If there are no troubles you should see 100% efficiency. This is telling us how well the automatic procedure on the LpGBT works.

Then we want also to extract the best phase that allow the LpGBT to properly sample the data. This is shown in the next two plots.

![CICtoLpGBT_FoundPhaseDistribution](OTtesting/2S/CICtoLpGBT_FoundPhaseDistribution.png)

On the X axis the various lines between CIC and LpGBT are shown: 1 L1, 5 stub lines for the right (R) and left (L) FEHs. On the Y axis the phase. The LpGBT scans phases between 0 to 14, covering two clock cycles. The 15 is an error code that is used by the LpGBT. When repeating the measurement 100 times, we store basically the frequency for which one phase is chosen and this is shown on the Z-axis.

Usually you see bins with roughly 1 (yellow) on one phase and in some cases two phases are picked with a similar frequency. 
If a vertical line of bins with some frequency  is seen, it means that LpGBT was not able to choose any particular phase and that something is going on  with that particular line. The line goes to the connector between the service hybrid and the front end hybrid so something may be wrong with that connector, since the hybrids were already tested and it would be quite unlikely that LpGBT or the CIC are the problem or their connection with the hybrid is the problem.

The best phase (the one with the highest frequency) is chosen from the previous plot, used by the LpGBT and shown in the plot below, one for each line.

![CICtoLpGBT_BestPhase](OTtesting/2S/CICtoLpGBT_BestPhase.png)

##### OTalignBoardDataWord - Hybrid
After we have done the alignment of the LpGBT, we align the data word into the FC7. 
Once more, we set the CIC such that it keeps sending data through the lines described in the [OTalignLpGBTinputs - OpticalGroup](#otalignlpgbtinputs---opticalgroup). This time we want to identify the data in the FPGA.

The CIC sends a packet of 8 bits and we need to verify that the 8 bits are properly identified into the FPGA, by identifying the first of the 8 bits.
For instance, if the CIC sends a sequence of 101010 then you need to know that the first one has to be a 1 and then the second one has to be a 0.

To do the alignment, the test tells the FPGA which is the expected pattern.

Once the FPGA receives a packet, it is checked the delay that is needed  such that the first bit of the packet is the first bit of the expected pattern. The delay is called FPGA *bitslips*. The chosen bitslip for each line is shown in the plot below. There is one plot per hybrid. This plots have the purpose to store the found value.

![Board_WordAlignmentBitSlipValues_Hybrid](OTtesting/2S/Board_WordAlignmentBitSlipValues_Hybrid.png)

The plot below stores the number of retries. The alignment procedure is tried for a maximum of 10 times in case of failures. Retries can indicate instabilities. The retry number is stored per each line since each line is handled separately.

![Board_WordAlignmentRetryNumbers_Hybrid](OTtesting/2S/Board_WordAlignmentRetryNumbers_Hybrid.png)


It is not uncommon to have one or two retries as there are some instabilities when writing some particular registers into the board. That's why we try multiple times. If the test retries 10 times, very likely means that it never manages to align and it would be good to check the connections between the CIC and the LpGBT.

##### OTverifyBoardDataWord - Hybrid

This test is designed to verify that the alignment performed in the previous steps properly succeeded. Once again the CIC is set in the same configuration to send a pattern through each line. The test checks if the pattern sent by the CIC matches the pattern received by the LpGBT.

Two plots (per hybrid) are stored for this test.

One plot contains the number of bits used for the test. For every line, it shows how many bits were tested. The difference between the number of bits we are testing between the stubs and the Level-1 is only for timing purposes.  

The stub line can be implemented directly in the firmware, which is very fast.  

In contrast, the Level-1 implementation would require a major firmware update, and we currently don’t have the resources for that. That’s why you see fewer bits for Level-1.  

However, it’s still on the order of 10⁶ bits — not a small number — but lower than the number of stub bits. This corresponds to the number of tester bits.

![CICtoLpGBT_PatternMatchingTestedBits_Hybrid](OTtesting/2S/CICtoLpGBT_PatternMatchingTestedBits_Hybrid.png)

We also have the error rate.  
This value ranges from 0 to 1, where 1 means 100% errors and 0 means no errors.  

From what we have observed so far, this part of the test is very stable.  

An error rate around 10⁻⁶ or 10⁻⁷ might just be a glitch.  
If the error rate is higher than that, check the connections between the hybrids and the connectors, as that might be the cause.


![CICtoLpGBT_PatternMatchingErrorRate_Hybrid](OTtesting/2S/CICtoLpGBT_PatternMatchingErrorRate_Hybrid.png)


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

![CBCtoCIC_LockingEfficiency_Hybrid](OTtesting/2S/CBCtoCIC_LockingEfficiency_Hybrid.png)


We also plot, for each line, the phase and the frequency at which each phase was chosen.  
If you zoom in on the X-axis, you can see, for every CBC, the stub lines and the Level-1 lines all in the same plot. We combined them into a single plot to avoid creating too many separate plots.  

As for the LpGBT, you will often see cases where two phases are essentially equivalent. In these cases, we choose the phase with the highest probability.

![CBCtoCIC_InputPhaseDistribution_Hybrid](OTtesting/2S/CBCtoCIC_InputPhaseDistribution_Hybrid.png)

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

![CBCtoCIC_BestInputPhases_Hybrid](OTtesting/2S/CBCtoCIC_BestInputPhases_Hybrid.png)


##### OTCICwordAlignment - Hybrid
The next step to be addressed is the CBC's processing of stubs. The goal is to align all lines with the 40MHz clock. 

Now, the CIC can correctly identify ones and zeros coming from the CBC, but the CIC also needs to process the stub information.  
Each CBC sends stubs in a specific format: three lines for the stub address, one and a half lines for the bending, and some bits on the last line for the error code [(See slide 8)](https://indico.cern.ch/event/1540157/contributions/6481541/attachments/3057152/5426570/FRavera_2025_04_28_2Sschool.pdf).  

The CIC must understand these bits and decide which stubs to actually send, because it cannot send all stubs at once. Each CIC can handle only a limited number of stubs.  

This step is called word alignment because the CIC needs to identify not only the first bit but also its position in the full data stream.  
The procedure is similar to what is done in the FC7 for bit identification. The CBC is set to send a specific pattern and we tell the CIC what to expect. 

This creates a single plot showing the delay applied on each line of the CBC.  
Since the lines are very similar in length, values should be roughly identical.  
If any line shows a value drastically different from the average, it may indicate a problem.

![CBCtoCIC_WordAlignmentDelay_Hybrid](OTtesting/2S/CBCtoCIC_WordAlignmentDelay_Hybrid.png)

These plots are not used for debugging or QA; they are mainly to store the values chosen. Unlike previous scans, this one only scans a single phase, so there is just one working point for each line.


##### OTCICBX0Alignment - Hybrid
This is the last step of the CBC - CIC alignment. It is more relevant for PS modules where the stub info is sent over 2 words but it is performed also for 2S ones even if the stub info is sent into a single word.

Since all CBCs and lines are synchronized, only one of the chip and lines is set to send a pattern and used for the measurement of the BX0 delay. The BX0 delay is measured between a Resync and the reception of the pattern in the CIC.

![CICBX0AlignmentDelay_Hybrid](OTtesting/2S/CICBX0AlignmentDelay_Hybrid.png)

An empty plot shows that the alignment fails. This could be due to a problem on the CBC/CBC line chosen for the alignment or on a problem in the CIC.

##### OTalignStubPackage - OpticalGroup

This is the last alignment step of the stub package. There are 5 stub lines between the CIC and the LpGBT and the stub info is sent following [the scheme on slide 10](https://indico.cern.ch/event/1540157/contributions/6481541/attachments/3057152/5426570/FRavera_2025_04_28_2Sschool.pdf).

There is 1 bit that indicates if the pattern is coming from the CBC or the MPA.
We consider the CBC case (bit = 0). Then we have status bits that indicate errors.  
Next, we have the bunch crossing IDs, which tell you the bunch crossing at which the pattern or packet was sent.  

We also include the number of stubs, indicating how many stubs the packet contains, followed by all the stub data.  

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

![Board_BestStubPackageDelay_OpticalGroup](OTtesting/common/Board_BestStubPackageDelay_OpticalGroup.png)

On the y-axis, we show the right hybrid and the left hybrid.  
For each, we display a single number that indicates which stub package delay was chosen.  

You generally don’t need to check anything specific here.  

From the module QA point of view, there isn’t much to check here.  
If you do see an issue in this plot, it most likely indicates a problem elsewhere in the setup, for instance a failed BXO alignment.

##### OTverifyCICdataWord - Hybrid


At this point, all chips are aligned: the CIC is synced with the LpGBT, the CBC data are correctly decoded by both the FPGA and the CIC, and all chips are communicating. The final step is to check the connection quality between the CBC and CIC.
We set the CBC to send a specific pattern and check if the received data matches. The resulting plots show cumulative errors for all stub lines and Level-1 lines together. At this stage, we cannot pinpoint which line caused an issue without additional, more time-consuming steps, so we just look at a combined value.


Stub tested bits, showed below, are higher because their pattern matching is done in firmware, while Level-1 errors are computed in software, which takes longer.

![CBCtoCIC_PatternMatchingTestedBits_Hybrid](OTtesting/2S/CBCtoCIC_PatternMatchingTestedBits_Hybrid.png)

Small error rates (around 0.01–0.1%) are normal and not a concern. Large errors, e.g., 20%, would indicate a real problem.


![CBCtoCIC_PatternMatchingErrorRate_Hybrid](OTtesting/2S/CBCtoCIC_PatternMatchingErrorRate_Hybrid.png)

This concludes the alignment section.

##### PedestalEqualization (also known as Trimming) - Chip
By design, the comparators that set the threshold for each channel have some unavoidable production variations. To compensate, each CBC chip allows an offset to be applied to the amplifier signal. This adjusts the pedestal up or down so that the threshold is uniform across all channels.
During pedestal equalization, these offsets are set channel by channel to ensure consistent thresholds. 

The results of this calibration are stored at the chip level.

The first plot shows for every channel in the CBC the offset that was chosen.
If something unusual appears later, it is possible to check whether any channel offsets have reached their extremes—0 or 255—which could indicate a failure. The CBC has so far demonstrated very stable behavior, with no occurrences of this issue.

![ChannelOffsetValues_Chip](OTtesting/2S/ChannelOffsetValues_Chip.png)


In the plot below, the x-axis represents the channel, while the y-axis shows occupancy. The goal is to achieve approximately 50% occupancy, which corresponds to the expected value after calibration. If any channel shows unusually high or low occupancy, it may indicate an issue with that channel. Some variation is expected due to the discrete adjustment steps, but overall the distribution should be roughly uniform.

![ChannelOccupancyAfterOffsetEqualization_Chip](OTtesting/2S/ChannelOccupancyAfterOffsetEqualization_Chip.png)

##### PedeNoise - Hybrid, Chip, Channel

A scan of the applied threshold is performed, measuring the occupancy (without injection) for each threshold value. Ideally, this would produce a perfect step function. In reality, the presence of noise modifies the response, effectively convoluting the step function with a Gaussian. The Gaussian represents the noise in the system, and the resulting curve takes an S shape. This S-shaped curve is referred to as the “S curve.”

For this test, many plots are saved at different levels.

The S-Curve distribution is saved for each channel and shown below for one example channel.
On the X-axis there is the applied threshold in VcTh units (1 VcTh unit =  156 electrons). Higher VcTh correspond to lower thresholds. On the Y-axis there is the occpuncy. Each S-curve is fitted individually. From this fit, the Gaussian component of the convolution allows extraction of the noise, represented by the width of the Gaussian. The underlying step function from the convolution corresponds to the pedestal, which is measured at around 50% occupancy. This measurement is performed without injection, so the pedestal obtained reflects the actual baseline of the system.

![SCurve_1channel](OTtesting/2S/SCurve_1channel.png)

At the chip level, a 2D summary plot is stored summarizing the S-curve of all channels. Each plot shows channels on the x-axis, thresholds on the y-axis, and occupancy on the z-axis, with each line representing a single channel.

![SCurve_Chip](OTtesting/2S/SCurve_Chip.png)

<details>
  <summary>Known issues</summary>

Broken wirebond or disconnected bump bond can show up as a compressed S-curve for a specific channel. If the issues appears in cold and disappears at room temperature it may be the CBC known issue of the corrupted offset register.

![SCurve_Chip_buggy](OTtesting/2S/SCurve_Chip_buggy.png)

Another know issues is when horizontal stripes are present. This is a communication issue affecting the whole module. Example will be added when found again.


</details>


From the S-Curve, the pedestal for every channel can be extracted. The cumulative distribution of the pedestal should appear very sharp, while failures would show as long tails or outliers.

![PedestalDistribution_Chip](OTtesting/2S/PedestalDistribution_Chip.png)

The channel pedestal plot shows the pedestal for each channel, revealing a very uniform distribution across all channels within a few VcTh, corresponding to a width of a few hundred electrons.

![ChannelPedestal_Chip](OTtesting/2S/ChannelPedestal_Chip.png)

Similar distribution are also shown for the noise.

![NoiseDistribution_Chip](OTtesting/2S/NoiseDistribution_Chip.png)

![ChannelNoise_Chip](OTtesting/2S/ChannelNoise_Chip.png)

The bottom sensor has a higher noise compared to the top because of longer traces in the foldover hybrid.

![ChannelNoiseTop_Chip](OTtesting/2S/ChannelNoiseTop_Chip.png)

![ChannelNoiseBottom_Chip](OTtesting/2S/ChannelNoiseBottom_Chip.png)

There are there very similar distributions summarizing the performance at the hybrid level.

![NoiseDistribution](OTtesting/2S/NoiseDistribution.png)

![StripChannelNoise](OTtesting/2S/StripChannelNoise.png)

The bottom sensor has a higher noise compared to the top because of longer traces (extra capacitance) in the foldover hybrid.

![StripChannelNoiseTop](OTtesting/2S/StripChannelNoiseTop.png)

![StripChannelNoiseBottom](OTtesting/2S/StripChannelNoiseBottom.png)

<details>
  <summary>Known issues</summary>

A group of channel with high noise may indicate a scratch on the sensor.

![StripChannelNoise_Scratch](OTtesting/2S/StripChannelNoise_Scratch.png)

A channel with low noise could indicate a broken wirebond. Below 2 a broken bumpbond.

![StripChannelNoise_brokenBons](OTtesting/2S/StripChannelNoise_brokenBonds.png)

Groups of broken channels in the center of CBC can indicate that sparking occurred.

![StripChannelNoise_Sparking](OTtesting/2S/StripChannelNoise_Sparking.png)

If the HV is not applied, a very high noise is shown over both hybrids and sensors.
</details>

##### OTinjectionDelayOptimization - Chip

After pedestal tests, injection tests are performed by scanning the injection delay and measuring, for each delay, the threshold corresponding to 50% occupancy—where the input signal equals the comparator threshold—allowing reconstruction of the full signal distribution as a function of time.

![ThresholdVsDelayScan_Chip](OTtesting/2S/ThresholdVsDelayScan_Chip.png)

At this stage, the working point can be determined by setting the injection delay at the signal peak, which in first approximation is independent of the injected charge, ensuring that the signal always crosses the threshold at the same instant; the distance from the pedestal is then adjusted—typically five times the measured pedestal noise from the previous calibration—to suppress pedestal-induced noise and precisely define the working point.

![BestThresholdAndDelay_Chip](OTtesting/2S/BestThresholdAndDelay_Chip.png)

From this point onward, each injection measurement is performed using the identified injection delay and threshold, ensuring that subsequent tests are properly configured so that any injected charge is read at the correct value.

##### OTinjectionOccupancyScan - Chip

The occupancy is measured for different injection charges to establish a reference: first by recording the occupancy without injection to quantify the contribution from noise alone, and then by measuring the response for known injected charges, allowing evaluation of the detection efficiency for signals corresponding to specific charge amounts.

Five different measurements are performed, corresponding to five different plots. Three examples are shown, for no injection and one for some injected charge.

The one below is without injection.

![ChannelOccupancy_Injection_0.000_MIP_Chip](OTtesting/2S/ChannelOccupancy_Injection_0.000_MIP_Chip_Chip.png)

The occupancy is measured for each chip, and although some slight activity may appear, the threshold is set to five times the noise, so almost no signal is expected except for very small fluctuations (that may be more visible in log scale).

Then, injections are performed at different charge levels — for example, a quarter of a MIP, which corresponds to roughly the same level as the threshold set at five times the noise, yielding about 50% efficiency. This value indicates that a signal equivalent to a quarter of a MIP produces a 50% detection probability. Measuring lower charges is important to study cluster size and improve spatial resolution. Subsequent plots show the occupancy for each channel at 0.25, 0.5, 1, and 2 MIPs, allowing identification of potential issues. The inspection of these results is automated by potato, which flags problematic modules; manual inspection is mainly needed for those flagged as bad. Since each CBC chip behaves slightly differently, occupancy maps are produced per CBC and per channel, enabling the identification of noisy or inefficient channels, such as those with damaged comparators, although new modules typically show very few such cases.

![ChannelOccupancy_Injection_0.250_MIP_Chip](OTtesting/2S/ChannelOccupancy_Injection_0.250_MIP_Chip.png)

![ChannelOccupancy_Injection_1.000_MIP_Chip](OTtesting/2S/ChannelOccupancy_Injection_1.000_MIP_Chip.png)

##### OTCMNoise (Common Noise) - OpticalGroup, Hybrid, Chip
The common mode noise test checks whether there is any correlation in the noise across different channels of the same chip.
Ideally, each channel should behave independently, meaning that noise fluctuations in one channel should not affect others. However, in reality, the channels share common elements — such as the same ground, power supply, and piece of silicon — which can lead to correlated noise.
This test is used to quantify that correlation. Two types of measurements are performed, Occupancy-driven common noise, where the threshold is set such that each channel has about 50% occupancy and one with the threshold at 3 sigma from the pedestal. Since the pedestal has already been tuned, all channels should exhibit similar occupancy. Plots will be shown only for the first case.
In a perfectly uncorrelated system, the distribution of the number of hits per event would follow a binomial shape centered at half the number of channels (e.g., 128 hits for a 256-channel chip like the CBC).
In practice, however, the distribution shows tails on both sides — events with unusually high or low numbers of hits. The width of this distribution reflects the level of correlation between channels: the broader it is, the stronger the common mode noise.
The inspection of this test is typically automated by potato, but manual checks can be done if a module shows anomalous behavior.

- Below the common noise distribution for one CBC is shown. 

Here the hits on one chip are shown. The same plot exist divided for top and bottom sensors. Since all channels are connected to the same sensor, slight differences in behavior can occur between the top and bottom sensors, leading to small variations in the observed common mode noise.

![CommonNoiseHits_OccupancyDriven_Chip](OTtesting/2S/CommonNoiseHits_OccupancyDriven_Chip.png)

A top–bottom correlation is expected since the channels belong to the same chip; this is visualized by plotting the two distributions together in a correlation plot, where the presence of a diagonal indicates some correlation, though the effect is minor and not concerning given that all elements share the same sensor and electronics.

![CommonNoiseTopBottomCorrelation_OccupancyDriven_Chip](OTtesting/2S/CommonNoiseTopBottomCorrelation_OccupancyDriven_Chip.png)

Here we show the correlaton of the chip with the rest of the hybrid.

![CommonNoiseHybridCorrelation_OccupancyDriven_Chip](OTtesting/2S/CommonNoiseHybridCorrelation_OccupancyDriven_Chip.png)

Here we have the correlation of the channels within one chip, showing which other channels are firing when one channel is firing. The diagonal is comparing one channel with itself.

![2DChipHits_OccupancyDriven_Chip](OTtesting/2S/2DChipHits_OccupancyDriven_Chip.png)

- Now we show distributions at the hybrid level


Here we look at the distribution of the number of hits per event across the entire hybrid. In this case, the range extends from 0 up to 2036 channels (corresponding to 254 × 8).The same plot exist divided for top and bottom sensors.

![CommonNoiseHits_OccupancyDriven_Hybrid](OTtesting/2S/CommonNoiseHits_OccupancyDriven_Hybrid.png)

As before, the correlation between the top and bottom strips is observed, showing a significant degree of correlation. This behavior reflects the real conditions of the system and is not a major concern.

![CommonNoiseTopBottomCorrelation_OccupancyDriven_Hybrid](OTtesting/2S/CommonNoiseTopBottomCorrelation_OccupancyDriven_Hybrid.png)

- OpticalGroup/Module level distributions

Here we have more than 4000 channels. As before we have also plots for top and bottom divided and for their correlations. The correlations between top and bottom is lower as we are now combining two hybrids.

![CommonNoiseHits_OccupancyDriven_OpticalGroup](OTtesting/2S/CommonNoiseHits_OccupancyDriven_OpticalGroupd.png)

The two hybrids below appear uncorrelated.

![CommonNoiseCrossHybridCorrelation_OccupancyDriven_OpticalGroup](OTtesting/2S/CommonNoiseCrossHybridCorrelation_OccupancyDriven_OpticalGroup.png)


These results are harder to interpret, reflecting the true behavior of the module, so if any unusual tails or unexpected noise appear in the pedenoise results, these plots should be checked to identify possible anomalies by comparing them with reference common noise plots from other modules to confirm that the noise distribution matches expectations.


An additional measurement was included using the same plot evaluated above but with the threshold at three sigma from the pedestal, providing an alternative way to visualize the noise effect. At zero sigma (shown above), where the expected occupancy is 50%, both left and right tails can be inspected since not all channels are expected to fire simultaneously, while at three sigma, the focus is on the right tail to highlight deviations. This offers the same information from a different perspective, emphasizing one side of the distribution. Given the current dataset, there is no clear advantage to using one representation over the other, so both are included since the acquisition time is minimal. The three-sigma plot is generally more straightforward to interpret because it isolates one tail, while the zero-sigma version requires considering both sides. For occupancy-driven tests, the zero- and three-sigma plots contain equivalent information, except that the three-sigma plots are shifted left due to lower average occupancy.

##### Electric Chain Validation (ECV)
The electric chain validation focuses on evaluating the width of the working area of the communication phases within the module. Unlike the verification step, which uses the phase identified by the CIC or the LpGBT as optimal, the validation manually scans different phases to determine the range over which the chain remains operational. A broad working area indicates a stable configuration, while a narrow one suggests that the system operates close to its limits and may become unstable once installed in the detector.

##### OTCICtoLpGBTecv - Hybrid
The CIC-to-LpGBT ECV studies the transmission between the CIC and LpGBT by varying the LpGBT sampling phase and checking whether the data patterns sent by the CIC are correctly reconstructed in the FPGA. For each phase setting, the number of tested bits and corresponding error rate are recorded in hybrid-level plots labeled as CIC-to-LpGBT pattern matching. The test also explores the impact of varying the current used by the CIC to drive the data (SLVS strenght, from 1 to 5), the LpGBT clock polarity, and the CIC clock drive strength (from 1 to 7). The LpGBT provides the clock to the hybrid, and changing its polarity effectively shifts the clock phase by 50%. These variations help evaluate how different transmission parameters affect data reconstruction and identify the range of stable operating conditions where several phases ensure reliable communication between components.


This plot shows, on the Y axis, the line ID corresponding to each transmission line, and on the X axis, the manually selected LpGBT sampling phase, which ranges from 0 to 14. The phase is varied manually rather than letting the LpGBT automatically adjust it, in order to explore also regions where the LpGBT cannot properly sample the incoming data. The Z axis represents the number of tested bits, with the stub pattern matched in firmware for speed, while the Level-1 pattern matching is performed in the software, resulting in longer scan times and fewer tested bits.

![CICtoLpGBT_PatternMatchingTestedBits_CIC_SLVScurrent_5_LpGBT_Clock_Polarity_0_Clock_Strength_7_Hybrid](OTtesting/2S/CICtoLpGBT_PatternMatchingTestedBits_CIC_SLVScurrent_5_LpGBT_Clock_Polarity_0_Clock_Strength_7_Hybrid.png)


In the plot below the Z axis represents the number of errors. Some lines are expected not to work properly, since sampling may occur when the incoming data from the CIC are transitioning, leading to bit misinterpretation. The quality of a module is therefore evaluated by the width of the phase range over which correct data transmission is achieved.

![CICtoLpGBT_PatternMatchingErrorRate_CIC_SLVScurrent_5_LpGBT_Clock_Polarity_0_Clock_Strength_7_Hybrid](OTtesting/2S/CICtoLpGBT_PatternMatchingErrorRate_CIC_SLVScurrent_5_LpGBT_Clock_Polarity_0_Clock_Strength_7_Hybrid.png)

Many versions of the above plots are stored for the various values and combinations of the current used by the CIC to drive the data (SLVS strenght, from 1 to 5), the LpGBT clock polarity, and the CIC clock drive strength (from 1 to 7).

##### OTalignLpGBTinputsForBypass - Hybrid
The next step in the electrical chain validation is an auxiliary procedure that allows studying data transmission with the CIC in bypass mode. Normally, the CIC scrambles the data coming from the readout chip, making it impossible to determine on which line an error occurs, but in bypass mode the CIC simply forwards the incoming data to the LpGBT without processing. Since each CBC sends six lines to the CIC, and only six lines total go from the CIC to the LpGBT, not all can be forwarded simultaneously, so a subset must be selected. The CIC allows forwarding only four lines at a time, grouped into so-called “five ports.” There are twelve five ports in total: the first ten carry trigger information, and the last two carry Level-1 data. 
[Mapping](https://fnal-outer-tracker.docs.cern.ch/documents/PhyPortMap.pdf) these correctly is complicated by the fact that the CIC and I2C systems use different front-end IDs to identify connected chips, and these IDs do not match. PH2_ACF uses the I2C IDs and automatically performs the internal remapping between the CIC front-end IDs, five ports, and line indices, so this does not need to be handled manually.
A further complication arises because, in bypass mode, the CIC output is no longer synchronized to the standard clock that the LpGBT uses for phase alignment. As a result, the phase alignment of the LpGBT must be re-tuned manually. The procedure consists of scanning the LpGBT phases for each five port individually to identify the correct phase alignment for data transmission, as shown in the “LpGBT for CIC bypass” plots.

In these plots, the x-axis shows the LpGBT phase, and the y-axis shows the line index. You will always see four lines—stub 1 through stub 4—since the forwarding in bypass mode always goes through four lines, regardless of which CBC they originate from. These labels are therefore arbitrary and not correlated with specific CBC lines, but they are needed for display. Here we see again the tested bits.

![LpGBTforCICbypass_PhaseScanTestedBits_phyPort0_Hybrid](OTtesting/2S/LpGBTforCICbypass_PhaseScanTestedBits_phyPort0_Hybrid.png)


The corresponding plot below shows the error rate (from 0 to 1) as a function of phase and line. The goal is to identify the working region where no transmission errors occur. The optimal working point is chosen at the center of the widest error-free region, ensuring that the data bypassed from the CIC to the LpGBT and sent to the board are correctly received. Because each five-port behaves differently and has its own optimal phase alignment, this scan must be repeated for every five-port. 

![LpGBTforCICbypass_PhaseScanBitErrorRate_phyPort0_Hybrid](OTtesting/2S/LpGBTforCICbypass_PhaseScanBitErrorRate_phyPort0_Hybrid.png)

All these scans are auxiliary calibration steps—if everything works properly, the details of these plots can be ignored, since their purpose is simply to enable the final validation of the electrical chain between the CBC and the CIC.

##### OTChipToCICecv - Hybrid
The final step of the electrical chain validation focuses on the link between the CBC and the CIC. For this stage, we again produce two plots per scan point: the error rate and the number of tests. The procedure is similar to the previous validation steps, but here we vary the CBC output drive current that controls the signal strength on the lines between the CBC and the CIC. Three current settings are typically used to study the behavior of the link. In this configuration, a higher drive strength corresponds to a lower numerical value—so current setting 0 gives the highest current, 14 the lowest, and 8 an intermediate value. There is no need for a CBC to CBC ECV because the CBC uses a fake channel connected to the neighboring chip.

The plots follow the same format as before, with the phase on the x-axis and the line ID on the y-axis, showing all CBCs and their corresponding lines. Phases 2 and 3 are absent because they are not functional on the CIC and are therefore skipped. As usual, the number of tests is smaller for the Level-1 data since those checks are performed in software—still around 10⁵ to ensure sufficient statistics without excessive runtime. 

![CBCtoCIC_PhaseScanTestedBits_CBC_SLVScurrent_0_Hybrid](OTtesting/2S/CBCtoCIC_PhaseScanTestedBits_CBC_SLVScurrent_0_Hybrid.png)

The corresponding error-rate plots show that most channels exhibit a broad phase region with zero errors, indicating a stable and well-aligned communication between the CBC and the CIC.

![CBCtoCIC_PhaseScanErrorRate_CBC_SLVScurrent_0_Hybrid](OTtesting/2S/CBCtoCIC_PhaseScanErrorRate_CBC_SLVScurrent_0_Hybrid.png)

The Level-1 channels occasionally show issues in the pattern matching due to imperfect data sampling, leading to rare misreads and preventing a 100% match rate. While this is not a major concern, improvements are being explored, though the underlying sampling mechanism makes it difficult to fully eliminate. The plots clearly show that non-working phases have much higher error rates—around 45% compared to below 0.2% in well-aligned regions. As before, the results are shown for four different current settings, all displaying similar behavior.

This is the conclusion of the electric chain validation.

##### OTBitErrorRateTest - OpticalGroup
The bit error rate (BER) test is designed to verify the stability of the optical link between the LpGBT and the FPGA, passing through the VTRx. This allows checking for transmission issues either between the LpGBT and VTRx or between the VTRx and the board.
The LpGBT includes a built-in PRBS (Pseudo-Random Bit Sequence) generator, which produces a known pseudo-random bit pattern. The same pattern is generated in the firmware, and by comparing the sent and received sequences, any bit mismatches can be detected—indicating corrupted bits during transmission.
The LpGBT offers multiple PRBS modes (listed in its manual), and in this test, one PRBS is emulated per line. Although the data link is a single physical channel between the LpGBT, VTRx, and FPGA, the results are split by line in the firmware for analysis.
Because of limited FPGA resources, the test is run sequentially, one line at a time, and results are kept separate to help identify and debug potential issues. Since the test runs at the LpGBT level, all results are stored in the optical view.
The first plot shows the bit error rate phase scan. This step is mostly a technical procedure, as the LpGBT generates the bit error rate pattern from a clock source whose phase can be adjusted. Certain phases prevent the LpGBT from correctly interpreting its own pattern, so a quick scan is performed to identify the valid working phases. 

![BERTerrorRatePhaseScan_OpticalGroup](OTtesting/2S/BERTerrorRatePhaseScan_OpticalGroup.png)

![BERTtestedBitsCounterPhaseScan_OpticalGroup](OTtesting/2S/BERTtestedBitsCounterPhaseScan_OpticalGroup.png)

Once a stable phase is found, it is stored as the best phase. This step ensures that the LpGBT is in a proper transmission state and avoids generating fake bit errors unrelated to the actual link between the module and the FC7. After determining the correct phase, the real bit error rate test can be performed.

![BERTbestPhase_OpticalGroup](OTtesting/2S/BERTbestPhase_OpticalGroup.png)

As for the other plots, there are two levels of information: one plot shows the number of tested bits, which reaches approximately 10¹⁰ bits and appears fairly uniform. The distribution is roughly symmetric because the two hybrids are tested in parallel, meaning that when the stub number two on the right hybrid is tested, the corresponding stub number two on the left hybrid is tested as well, resulting in similar counts. The exact number of tested bits is not strictly controlled, since only a minimum threshold is set and the system runs until that is exceeded, so small variations are expected and not concerning.

![BERTtestedBitsCounter_OpticalGroup](OTtesting/2S/BERTtestedBitsCounter_OpticalGroup.png)

The key plot is the bit error rate, which measures the stability of the link while remaining split by line. Under normal conditions, the bit error rate is expected to be zero. Summing across all lines corresponds to about 10¹¹–10¹² tested bits, and the expected bit error rate is below 10⁻¹²–10⁻¹³. Testing up to 10¹³ bits would require several hours, so the procedure uses a lower value for practicality. During development, modules tested up to 10¹³ bits showed no errors, confirming the link stability. Therefore, the standard validation relies on about 10¹¹ tested bits per run, which provides sufficient confidence in link performance while keeping testing time reasonable.

![BERTerrorRate_OpticalGroup](OTtesting/2S/BERTerrorRate_OpticalGroup.png)

The forward error correction (FEC) counter provides complementary information to the bit error rate by tracking how many bits were flipped during transmission but successfully corrected by the FEC mechanism, which can fix up to five flipped bits per packet. Although a zero bit error rate indicates no uncorrected errors, nonzero FEC counts can still reveal link instabilities. The firmware records the number of corrected bits, and this information is stored cumulatively for the entire LpGBT packet, resulting in a single value for both hybrids. The results remain separated by line since each line is tested independently due to firmware resource limits, but they can be summed to assess overall behavior. The plot reports counts rather than percentages because the total number of transmitted packets is not precisely known, though it can be approximated from the test duration and total bits processed. Consistent factor counts across lines suggest stable communication, while localized or irregular counts may indicate transient link issues.

![FECerrorCounter_OpticalGroup](OTtesting/2S/FECerrorCounter_OpticalGroup.png)

##### OTRegisterTester - Hybrid
This test checks the stability of the I2C communication by repeatedly writing and reading specific registers on both the CBCs and the CIC. A known pattern is written and read back, then its inverse is written and read back, and this cycle is repeated about a thousand times. The results are summarized in a single plot showing the read and write efficiency for each of the eight CBCs and the CIC, with one plot per hybrid. The expected outcome is a consistent 100% efficiency, as the CIC I2C communication is typically very stable. For the CBCs, register page flipping that is known to cause instabilities is avoided to ensure meaningful results. The goal is not to test the general chip performance but to identify possible I2C instabilities specific to the module, which could originate from issues in the connectors between the FEH and SEH if deviations are observed.

![RegisterMatchingEfficiency_Hybrid](OTtesting/2S/RegisterMatchingEfficiency_Hybrid.png)

### Monitoring  

The monitor produces a separate file stored in its own folder (`MonitorDQM`) and the file name contains a time stamp for identification typically within the result file directory when running manually. It is separated from the result file because the monitor records data from the configure to the halt or destroy state, while the result file only covers the calibration start-to-stop interval. For instance, during Burn-in tests, monitoring continues even when the system is in a stop state between temperature plateaus or to track changes during current adjustments. Since these periods occur at different times, two distinct files are generated. A dedicated script  merges the monitor and result files along with additional data, such as power supply information, into a single combined dataset used by Potato for module qualification.


The monitor is configured in the XML file, which defines the list of parameters to be measured. For each parameter, a plot is produced showing its value as a function of time or the evolution of the monitored quantity during the run.

#### LpGBT - OpticalGroup
The first level of monitoring is performed at the optical group level. All information is extracted from the LpGBT, which includes an ADC. This ADC can measure both internal quantities within the chip and external signals from lines connected to it. All these monitored quantities are listed in the XML, and the name of each value is included both in the plot title and in the plot file name for clarity.


The first monitored quantity is VDD, one of the digital supply voltages used by the LpGBT. These measurements allow you to check that the power remain stable during operation. Typically, the voltage should be around 1.2 V.
The plots shown here were taken at the same time as the result files from before, although the exact timing isn’t critical — the key point is that even during data taking, you shouldn’t see large fluctuations.
In most cases, these plots provide an immediate indication of any structural problem with the module. If you notice values significantly higher or lower than 1.2 V, it could point to a major issue. Small spikes in the distribution aren’t a concern — the ADC isn’t perfect and sometimes gives slightly delayed readings. These single-point deviations can be ignored, but if the voltage stays persistently high or low over time, that’s a sign of a potential problem.

![D_B(0)_LpGBT_DQM_VDD_OpticalGroup(0)](OTtesting/common/D_B(0)_LpGBT_DQM_VDD_OpticalGroup(0).png)


Another monitored voltage is shown here, which is essentially the same supply as the previous one. The difference sgould be that they correspond to two separate internal blocks of the LpGBT that each require their own instance of the same voltage to operate.

![D_B(0)_LpGBT_DQM_VDDA_OpticalGroup(0)](OTtesting/common/D_B(0)_LpGBT_DQM_VDDA_OpticalGroup(0).png)

Then we have the temperature measurement, which comes from the internal temperature sensor of the LpGBT. You can see a gradual warm-up, as this test was performed in the KIT box where the temperature is not tightly controlled. These temperature values are already calibrated using information provided by the LpGBT group in the configuration file mentioned earlier, which includes the calibration constants for the internal sensor, so the measurement should be quite reliable.

![D_B(0)_LpGBT_DQM_LpGBTtemp_OpticalGroup(0)](OTtesting/common/D_B(0)_LpGBT_DQM_LpGBTtemp_OpticalGroup(0).png)


There are also a few additional monitored quantities, ADC0 and ADC3, which are inputs to the LpGBT coming from the two hybrids. These values are controlled by the CIC, which can output an analog signal to monitor internal information. At the moment, nothing specific is configured on these channels since enabling them would require activating one CBC at a time, so they are included mainly for completeness. They can be safely ignored for now, although they may become useful in future studies; currently, they are simply connected to undefined signals, so while their values may fluctuate, they do not carry meaningful information.

![D_B(0)_LpGBT_DQM_ADC0_OpticalGroup(0)](OTtesting/common/D_B(0)_LpGBT_DQM_ADC0_OpticalGroup(0).png) 

![D_B(0)_LpGBT_DQM_ADC3_OpticalGroup(0)](OTtesting/common/D_B(0)_LpGBT_DQM_ADC3_OpticalGroup(0).png) 

There is another monitored quantity showing the voltage on the left hybrid, which should be around 1.25 V. There isn’t a separate measurement for the right hybrid due to limited inputs, but the assumption is that both hybrids get the same voltage from the DC-DC converter. In practice, the measured voltage is slightly lower, which has been consistent across all tested modules. Minor fluctuations can be ignored.

![D_B(0)_LpGBT_DQM_1V25_Left_OpticalGroup(0)](OTtesting/common/D_B(0)_LpGBT_DQM_1V25_Left_OpticalGroup(0).png) 

This is the input voltage coming from the power supply, which is usually set around 10.5 V. The measured voltage may be slightly lower, possibly due to cable drops, filtering, or uncertainties in the voltage divider used for measurement. The module itself is quite resilient, so only significant deviations—well below the nominal voltage, e.g., down to 8 V—would cause concern. Minor differences are not critical.

![D_B(0)_LpGBT_DQM_VIN_OpticalGroup(0)](OTtesting/common/D_B(0)_LpGBT_DQM_VIN_OpticalGroup(0).png) 

The next monitored parameter is the sensor temperature.
Each sensor includes an NTC (Negative Temperature Coefficient) resistor, meaning its resistance decreases as temperature increases. In other words, higher temperatures correspond to lower resistances and therefore to higher measured voltages (since a fixed current is injected through the resistor).
The system injects a known current through the NTC and reads the resulting voltage drop to estimate the temperature. The specific resistor monitored here is the one placed on the top sensor — on the high-voltage side of the module, where two connections are available, one of which is the temperature sensor. Thus, this measurement represents the temperature of the top sensor.

![D_B(0)_LpGBT_DQM_SensorTemp_OpticalGroup(0)](OTtesting/common/D_B(0)_LpGBT_DQM_SensorTemp_OpticalGroup(0).png) 

Then there is the measurement of the current related to the VTRx+ receiver. The VTRx+ receives incoming light through a fiber, and a photodiode inside converts this optical signal into an electrical current used for data transmission. The measured current corresponds to the diode’s response — sometimes referred to as the RSSI (received signal strength indicator) — which represents the amount of light collected by the diode.
For now, this measurement is mostly stable and not particularly informative, but it can become useful in the future. With radiation damage, the photodiode’s efficiency is expected to decrease, meaning the same amount of incoming light would generate a smaller current. Monitoring this evolution can therefore help track radiation effects on the VTRx+ performance.
During production testing, this parameter is not critical, but it is kept for completeness and possible long-term monitoring.

![D_B(0)_LpGBT_DQM_VTRxLeakageCurr_OpticalGroup(0)](OTtesting/common/D_B(0)_LpGBT_DQM_VTRxLeakageCurr_OpticalGroup(0).png)

The last two quantities monitored are the temperature sensors from the bPOLs.
The bPOL is the chip that in the DC-DC converter converts the voltage provided to the module to the needed voltages for the chips on the module. There are two bPOL one for the conversion 12 (in reality we supply 10.5) -> 2.5 and one for the conversion 2.5 -> 1.2. We have two stages because the VTRX needs 2.5 volts.
Each bPOL has a temperature sensor but they are not calibrated. The slope is precise but the offset is not. So the absulute value is not correct but the variations are accurate.
For example, if one is not well connected, one of the sensors may show a quite different value.

![D_B(0)_LpGBT_DQM_BPOL12Vtemp_OpticalGroup(0)](OTtesting/common/D_B(0)_LpGBT_DQM_VTRxLeakageCurr_OpticalGroup(0).png)

![D_B(0)_LpGBT_DQM_BPOL2V5temp_OpticalGroup(0)](OTtesting/common/D_B(0)_LpGBT_DQM_VTRxLeakageCurr_OpticalGroup(0).png)

For 2S modules, we are not monitoring any other level.