# Description of PS test results

This documentation is adapted from the [PS testing with Ph2_ACF tutorial](https://indico.cern.ch/event/1540158/). This focuses on the tests performed during module production with the goal of qualify the modules.

**For production testing, official releases and tools as GIPHT should be used.**

For other testing the main commands after installing the software are:

```
cd Ph2_ACF  # change the directory accordingly
source setup.sh 
```

If not already done, insert the correct machine and IP address in the
xml file located in the settings directory that you are going to use. The recommended XML is [settings/PS_Module_v2p1.xml](../../settings/PS_Module_v2p1.xml)

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
The description below is for the standard `PSfullTest` that is a very comprehensive set of measurements. The `PSquickTest` is a subset of that.

When running, two files are created:
- The `Results.root` that contains all the calibration results and the metadata
- The monitoring that contains the variables that we  would like to monitor, for example temperatures, voltages, in some cases also currents

### Results description

#### Metadata
When you open the file, the main folder inside is the `Detector` folder, containing metadata.

Metadata are storing what in root is called the object string and the reason for that is that if you store anything else that is not an object you need to create a dictionary. With a string we can basically store everything that we need.


For instance, `Username_Detector` contains the user name of the computer where the tests is carried out and `HostName_Detector` is the name of the computer.

`GitCommitHash_Detector` and `GitTag_Detector` are used verify that the user is in the right tag and commit for production. These fields will be used by the Potato grading software.

The `CalibrationName_Detector` is the name of the calibration, for instance `PSfullTest` to track which type of test you run.

The `InitialDetectorConfiguration_Detector` is basically a copy of the `HWDescription` section of the ` settings/MyXML.xml` used to run the test. The `FinalDetectorConfiguration_Detector` is similar to the `Initial` one with possible updated values due to the performed calibrations.

In the `CalibrationStartTimestamp_Detector` and `SubCalibrationNameAndType_Detecor` there is information about the time of each calibration. The calibration start time timestamp is stored as soon as you start configuring the module. Then since multiple steps are done, the different start of the steps (SubCalibration) are listed next to the steps. This is not really needed for any particular information for the QA but   it's really to keep track of the time for developers. The `CalibrationstubTimeStamp_Detector` is the time stamp for when the calibration is finished.

 
There is a minor difference of a few seconds in what you see in the metadata and in the printout on the terminal because in the beginning time stamp in the metadata the time used to read the configurations is not considered.

After the `Detector` folder we have the `Board`. Here we have three metadata:
- `D_NameID_Board` is the IP address
- `D_InitialBoardConfiguration_Board` is the XML for the board configuration, typically what you have in [settings/BeBoardFiles/uDTC_registers_PS.xml](../../settings/BeBoardFiles/uDTC_registers_PS.xml)
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

Every hybrid contains 8 SSA chips and is connected to 8 MPAs. There is a directory for each of them containg the same metadata `NameId` with the fuse ID, `InitialReadoutChipConfiguration` and `FinalReadoutChipConfiguration` with the registers.
Similar to the LpGBT case, the `IsReadoutChipCalibrated` is set to 1 if the calibration data is found and used for that chip. Currently calibrations are available for MPAs but not SSAs.

#### Calibrations 

The first step is the configuration where we just load all the registers into the various chips. Basically we just take the information that are stored in the values configuration file and we just load them into the chips.

The other steps are more elaborate and produce result plots described below.

##### TuneLpGBTVref - OpticalGroup

Vref is basically the reference voltage for the LpGBT ADC converter. It's needed for converting into meaningful values the ADC that are read by the LpGBT. This step is  basically the loading (not really a tuning) of a value that the LpGBT group gave us and is stored in [the calibration file](../../settings/lpGBTFiles/lpgbt_calibration.csv). The information for a specific LpGBT can be found by the fuse ID. In this step we retrieve the value from the file and we store it in the chip. No plots are produced.


##### OTPSADCCalibration - Chip
This calibration step is not included in the `PSquickTest` because it is relatively long. Both the SSA and the MPA contain an ADC that requires calibration. A band-gap is present in each chip. A precise calibration is available from the chip developers obatined at wafer testing for MPAs but not SSAs.
The ADC calibration is still executed because several internal biases depend on specific voltages that determine the correct amount of charge injected during tests, and these parameters must be optimized. 

The calibration is performed in two main steps. During wafer testing at about 25 °C, the true band-gap value of each chip is measured in millivolts, and the corresponding ADC_VREF register value is programmed so that the ADC range spans 850 mV. This VREF setting is saved in the chip fuses. When the chip is mounted on a module, the first task is to verify that the fused VREF still produces an effective 850 mV range when compared to the band-gap value, which should remain stable. If the range has shifted, VREF is re-optimized. Once VREF is confirmed or corrected, all other ADC-related registers are tuned accordingly.

Two plots are stored. The first shows the ADC_VREF register giving a range of 850mV. 
![VREF_DACtoV_Chip](../images/OTtesting/PS/VREF_DACtoV_Chip.png)

The second one is the ADC calibration curve obtained during the procedure.
![ADC_Slope_MPA](../images/OTtesting/PS/ADC_Slope_MPA.png)


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

For a more detailed explanation check the video tutorial around minutes 27-32. This is not reported here as this should not be problematic anymore with LpGBTv2.


##### Alignemnt -> establish proper communication for all chips on a module & FPGA

##### OTalignLpGBTinputs - OpticalGroup

To better understand the alignment steps, please refer to the [PS module communication scheme on slide 4](https://indico.cern.ch/event/1540158/contributions/6481542/attachments/3057153/5408250/FRavera_2025_04_28_PSschool.pdf) where we see the 8 SSAs and 8 MPAs per side. Each SSA is paired with an MPA and the MPAs communicate with the CIC. The CIC (one per side) communicate with the LpGBT and the LpGBT with the FPGA.

For the data rate that can be handled in the FPGA, the signal coming from the module is then split in separate components for the two FEHs and then in L1 (red line) and stub data (blue lines).

This test is used to make sure that the LpGBT understands what the CIC sends.
Basically the LpGBT samples the data received and this test finds the correct sampling phase. If the phase is not correct, sometimes a one can be interpreted as a zero, or vice versa, and therefore the communication will not work. 

This is an automatic procedure done by the LpGBT that has this automatic phase alignment. The test sets the CIC in a state that sends a specific pattern and ask the LpGBT to align, ie to find the best phase for sampling the incoming data from the CIC.

There are three plots associated with this test, identified with `CICtoLpGBT` string. 

![CICtoLpGBT_PhaseAlignmentEfficiency](../images/OTtesting/PS/CICtoLpGBT_PhaseAlignmentEfficiency.png)

The *Phase Alignment Efficiency* is obtained repeting the automic phase alignment 100 times (default value that can be configured in the XML) and counting how many times the alignment succeeded. If there are no troubles you should see 100% efficiency. This is telling us how well the automatic procedure on the LpGBT works.

Then we want also to extract the best phase that allow the LpGBT to properly sample the data. This is shown in the next two plots.

![CICtoLpGBT_FoundPhaseDistribution](../images/OTtesting/PS/CICtoLpGBT_FoundPhaseDistribution.png)

On the X axis the various lines between CIC and LpGBT are shown: 1 L1, 5 stub lines for the right (R) and left (L) FEHs. On the Y axis the phase. The LpGBT scans phases between 0 to 14, covering two clock cycles. The 15 is an error code that is used by the LpGBT. When repeating the measurement 100 times, we store basically the frequency for which one phase is chosen and this is shown on the Z-axis.

Usually you see bins with roughly 1 (yellow) on one phase and in some cases two phases are picked with a similar frequency. 
If a vertical line of bins with some frequency  is seen, it means that LpGBT was not able to choose any particular phase and that something is going on  with that particular line. The line goes to the connector between the service hybrid and the front end hybrid so something may be wrong with that connector, since the hybrids were already tested and it would be quite unlikely that LpGBT or the CIC are the problem or their connection with the hybrid is the problem.

The best phase (the one with the highest frequency) is chosen from the previous plot, used by the LpGBT and shown in the plot below, one for each line.
![CICtoLpGBT_BestPhase](../images/OTtesting/PS/CICtoLpGBT_BestPhase.png)


##### OTalignBoardDataWord - Hybrid
After we have done the alignment of the LpGBT, we align the data word into the FC7. 
Once more, we set the CIC such that it keeps sending data through the lines described in the [OTalignLpGBTinputs - OpticalGroup](#otalignlpgbtinputs---opticalgroup). This time we want to identify the data in the FPGA.

For the PS, we have two data transmission speeds, 10G and 5G, corresponding to the CIC sending packets of 16 and 8 bits respectively. These bits are properly identified into the FPGA, by identifying the first bit.
For instance, if the CIC sends a sequence of 101010 then you need to know that the first one has to be a 1 and then the second one has to be a 0.

To do the alignment, the test tells the FPGA which is the expected pattern.

Once the FPGA receives a packet, it is checked the delay that is needed  such that the first bit of the packet is the first bit of the expected pattern. The delay is called FPGA *bitslips*. The chosen bitslip for each line is shown in the plot below. There is one plot per hybrid. This plots have the purpose to store the found value.
![Board_WordAlignmentBitSlipValues_Hybrid](../images/OTtesting/PS/Board_WordAlignmentBitSlipValues_Hybrid.png)

The plot below stores the number of retries. The alignment procedure is tried for a maximum of 10 times in case of failures. Retries can indicate instabilities. The retry number is stored per each line since each line is handled separately.

![Board_WordAlignmentRetryNumbers_Hybrid](../images/OTtesting/PS/Board_WordAlignmentRetryNumbers_Hybrid.png)


It is not uncommon to have one or two retries as there are some instabilities when writing some particular registers into the board. That's why we try multiple times. If the test retries 10 times, very likely means that it never manages to align and it would be good to check the connections between the CIC and the LpGBT.

##### OTverifyBoardDataWord - Hybrid

This test is designed to verify that the alignment performed in the previous steps properly succeeded. Once again the CIC is set in the same configuration to send a pattern through each line. The test checks if the pattern sent by the CIC matches the pattern received by the LpGBT.

Two plots (per hybrid) are stored for this test.

One plot contains the number of bits used for the test. For every line, it shows how many bits were tested. The difference between the number of bits we are testing between the stubs and the Level-1 is only for timing purposes.  

The stub line can be implemented directly in the firmware, which is very fast.  

In contrast, the Level-1 implementation would require a major firmware update, and we currently don’t have the resources for that. That’s why you see fewer bits for Level-1.  

However, it’s still on the order of 10⁶ bits — not a small number — but lower than the number of stub bits. This corresponds to the number of tester bits.

![CICtoLpGBT_PatternMatchingTestedBits_Hybrid](../images/OTtesting/PS/CICtoLpGBT_PatternMatchingTestedBits_Hybrid.png)

We also have the error rate.  
This value ranges from 0 to 1, where 1 means 100% errors and 0 means no errors.  

From what we have observed so far, this part of the test is very stable.  

An error rate around 10⁻⁶ or 10⁻⁷ might just be a glitch.  
If the error rate is higher than that, check the connections between the hybrids and the connectors, as that might be the cause.


![CICtoLpGBT_PatternMatchingErrorRate_Hybrid](../images/OTtesting/PS/CICtoLpGBT_PatternMatchingErrorRate_Hybrid.png)


Now, we are sure that the communication between the CIC and the board works fine.


##### OTCICphaseAlignment - Hybrid
Now, we can begin aligning the MPA with the CIC.  
This step is conceptually similar to the alignment between the CIC and the LpGBT, but it’s a bit more complicated. The goal is to synchronize all the lines between CIC and MPA.

The MPA is set in a state that is sending a specific pattern to the CIC.
The CIC then uses this pattern to adjust its phase and achieve proper alignment.

The phase alignment logic in the CIC is a simplified version of that used in the LpGBT, since the same block was copied into the CIC design.  
The process is therefore similar:  
- Send a known pattern.  
- The CIC scans phases to find the one that decodes the pattern correctly.

To check stability, the CIC is asked to align 100 times.  
This ensures that the alignment procedure is reliable and repeatable; if the result is consistent across all runs, the alignment is stable.  
At the end of each alignment, we can query whether the procedure suCICeeded or not.

The resulting plots below resemble those used for the CIC-LpGBT phase alignment and are produced at the hybrid level.

We can plot the efficiency of alignment.  
Each vertical bin represents different elements: one line for the Level-1, 5 lines for the stubs. On the X-Axis we have one column for every MPA.  
On the z-axis, we display the efficiency, ranging from 0 to 1.  
If there are any issues, the efficiency will appear noticeably below 1.  

![MPAtoCIC_LockingEfficiency_Hybrid](../images/OTtesting/PS/MPAtoCIC_LockingEfficiency_Hybrid.png)


We also plot, for each line, the phase and the frequency at which each phase was chosen.  

As for the LpGBT, you will often see cases where two phases are essentially equivalent. In these cases, we choose the phase with the highest probability.

![MPAtoCIC_InputPhaseDistribution_Hybrid](../images/OTtesting/PS/MPAtoCIC_InputPhaseDistribution_Hybrid.png)

This phase scan above is performed over two clock cycles.  
For example, if one phase is around 5, the equivalent phase on the next cycle would be 5 + 8 = 13.  
So effectively, there are two working points for each line, and the system will pick one of the two.  

Looking at five phases individually might be misleading, since some of them are equivalent due to the two-cycle scan.  

The error code is represented by the number 15.  
If a value of 15 appears, it means the alignment failed.  
However, we rarely see this because the hybrids and modules we receive are generally good, and the MPAs within the same hybrid have already been tested.  

If an issue does occur, the lock efficiency should reflect it — for example, a 15 in the phase scan would likely correspond to a low or failing lock efficiency.

Then we show the best input phase in a 2D plot.  
On the y-axis, we have the different lines, and on the x-axis, the different MPAs.  
The z-axis represents the best phase — basically the most probable phase for each line and MPA combination.

![MPAtoCIC_BestInputPhases_Hybrid](../images/OTtesting/PS/MPAtoCIC_BestInputPhases_Hybrid.png)

##### OTCICwordAlignment - Hybrid
The next step to be addressed is the MPA's processing of stubs. The goal is to align all lines with the 40MHz clock. 

Now, the CIC can correctly identify ones and zeros coming from the MPA, but the CIC also needs to process the stub information.  
Each MPA sends stubs in a specific format, with the information of the number of stubs, their address and bending divided on multiple lines [(See slide 7)](https://indico.cern.ch/event/1540158/contributions/6481542/attachments/3057153/5408250/FRavera_2025_04_28_PSschool.pdf).  

An additional step is required because the CIC does not simply forward the information coming from the MPA; it processes it. The CIC must understand these bits and decide which stubs to actually send, because it cannot send all stubs at once. Each CIC can handle only a limited number of stubs.  
In particular, for the stubs, the CIC must correctly identify the stub packets. Each stub packet sent by the MPA contains up to five stubs, encoded in two 8-bit words. As before, we must determine the correct starting bit of these 8-bit words, so another word-alignment procedure is performed.
To do this, the MPA is configured to send a known stub pattern. The CIC is then told which pattern to expect. Using this reference, the CIC automatically scans the possible bit shifts and determines the correct alignment needed to reconstruct the stub pattern accurately.

This creates a single plot showing the delay applied on each line of the MPA.  
Since the lines are very similar in length, values should be roughly identical.  
If any line shows a value drastically different from the average, it may indicate a problem.  
![MPAtoCIC_WordAlignmentDelay_Hybrid](../images/OTtesting/PS/MPAtoCIC_WordAlignmentDelay_Hybrid.png)

These plots are not used for debugging or QA; they are mainly to store the values chosen. Unlike previous scans, this one only scans a single phase, so there is just one working point for each line.

##### OTCICBX0Alignment - Hybrid
This is the last step of the MPA - CIC alignment. It is more relevant for PS modules where the stub info is sent over 2 words but it is performed also for 2S ones even if the stub info is sent into a single word.

Since all MPAs and lines are synchronized, only one of the chip and lines is set to send a pattern and used for the measurement of the BX0 delay. The BX0 delay is measured between a Resync and the reception of the pattern in the CIC. This basically aligns the lines with the 20MHz clock.

![CICBX0AlignmentDelay_Hybrid](../images/OTtesting/PS/CICBX0AlignmentDelay_Hybrid.png)

An empty plot shows that the alignment fails. This could be due to a problem on the MPA/MPA line chosen for the alignment or on a problem in the CIC.

##### OTalignStubPackage - OpticalGroup

This is the last alignment step of the stub package. There are 6 stub lines between the CIC and the LpGBT and the stub info is sent following [the scheme on slide 12](https://indico.cern.ch/event/1540158/contributions/6481542/attachments/3057153/5408250/FRavera_2025_04_28_PSschool.pdf).

There is 1 bit that indicates if the pattern is coming from the CBC or the MPA.
We consider the MPA case (bit = 1). Then we have status bits that indicate errors.  
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

![Board_BestStubPackageDelay_OpticalGroup](../images/OTtesting/common/Board_BestStubPackageDelay_OpticalGroup.png)

On the y-axis, we show the right hybrid and the left hybrid.  
For each, we display a single number that indicates which stub package delay was chosen.  

You generally don’t need to check anything specific here.  

From the module QA point of view, there isn’t much to check here.  
If you do see an issue in this plot, it most likely indicates a problem elsewhere in the setup, for instance a failed BXO alignment.

This concludes the alignment section.

##### OTverifyCICdataWord - Hybrid

At this point, all chips are aligned: the CIC is synced with the LpGBT, the MPA data are correctly decoded by both the FPGA and the CIC, and all chips are communicating. The final step is to check the connection quality between the MPA and CIC.
We set the MPA to send a specific pattern and check if the received data matches. The resulting plots show cumulative errors for all stub lines and Level-1 lines together. At this stage, we cannot pinpoint which line caused an issue without additional, more time-consuming steps, so we just look at a combined value.

Stub tested bits, showed below, are higher because their pattern matching is done in firmware, while Level-1 errors are computed in software, which takes longer. 
![MPAtoCIC_PatternMatchingTestedBits_Hybrid](../images/OTtesting/PS/MPAtoCIC_PatternMatchingTestedBits_Hybrid.png)

Small error rates (around 0.01–0.1%) are normal and not a concern. Large errors, e.g., 20%, would indicate a real problem.


![MPAtoCIC_PatternMatchingErrorRate_Hybrid](../images/OTtesting/PS/MPAtoCIC_PatternMatchingErrorRate_Hybrid.png)



Both plots show data as a function of the MPA hybrid on the x-axis, while on the y-axis we separately display the Level-1 line and the stub lines.
At this stage, we cannot yet determine which of the stubs lines is responsible for a potential issue, if one is observed. Distinguishing between them requires a relatively involved procedure, which is performed during the electrical chain validation.
You will see later how we can identify the specific source of a problem. For now, we can only conclude that one of these connections is faulty.

One important point is that for the MPA, <span style="color:red;font-weight:bold;"> all these communications go through a wirebond pair. So if you observe errors (100% error rate) on these lines, you should inspect the wirebonds</span>.
This is crucial because, on the strip side, you can simply monitor the noise, whereas on the pixel side you are communicating with a chip. Therefore, <span style="color:red;font-weight:bold;">you need to run all the verification steps that help identify issues in the wirebonds between the MPA and the hybrid</span>.

##### OTverifyMPASSAdataWord - Hybrid

The final step of this alignment procedure is the verification of the communication between the SSA and the MPA. These lines are also wirebonded, which is why it is crucial to check them.
All of these alignment procedures are included in the quick test performed before encapsulation, specifically because they help detect missing wirebonds.
The idea is similar to what we described earlier. However, it is slightly more technical because the MPA cannot be bypassed. The only way to validate these connections is to inject real data.
In practice, you need to inject signals into the SSA on the channels that correspond to the MPA inputs so that these lines are actually used.
This becomes even more involved because the eight SSA–MPA lines used for cluster communication each transmit clusters from specific strips. This means you must inject eight clusters each time to ensure all lines are exercised; otherwise, you would never be able to reach or test the higher cluster-number lines.
These are just some of the technical details involved.


Stub tested bits, showed below as clusters since they will be interpreted as stubs byt the MPA, are higher because their pattern matching is done in firmware, while Level-1 errors are computed in software, which takes longer. 

![SSAtoMPA_PatternMatchingTestedBits_Hybrid](../images/OTtesting/PS/SSAtoMPA_PatternMatchingTestedBits_Hybrid.png)


Small error rates (around 0.01–0.1%) are normal and not a concern. Large errors, e.g., 20%, would indicate a real problem. Examples of known issues are shown below.
![SSAtoMPA_PatternMatchingErrorRate_Hybrid](../images/OTtesting/PS/SSAtoMPA_PatternMatchingErrorRate_Hybrid.png)

As before, <span style="color:red;font-weight:bold;"> all these communications go through a wirebond pair. So if you observe errors (100% error rate) on these lines, you should inspect the wirebonds</span>.
This is crucial because, on the strip side, you can simply monitor the noise, whereas on the pixel side you are communicating with a chip. Therefore, <span style="color:red;font-weight:bold;">you need to run all the verification steps that help identify issues in the wirebonds between the MPA and the hybrid</span>.

<details>
  <summary>Known issues</summary>

Example of a missing wirebond, 100% error rate for one chip and line. [This debug script](../../pythonUtils/ModuleNoiseAnalyzer.py) will help you identify the wirebond number. When inspecting the wirebonds, remember that wirebond 12 for the bandgap is never connected.

![SSAtoMPA_PatternMatchingErrorRate_Hybrid_missingWirebond](../images/OTtesting/PS/SSAtoMPA_PatternMatchingErrorRate_missingWirebond.png)

The one below is a known issue not yet understood. Please report if you observe it!
![SSAtoMPA_PatternMatchingErrorRate_Hybrid_clusterProblem](../images/OTtesting/PS/SSAtoMPA_PatternMatchingErrorRate_Hybrid_clusterProblem.png)

</details>

##### OTPSringOscillatorTest - Hybrid

Now we perform the ring oscillator test. This test was included at the request of the chip developers. The chips contain simple oscillators that continuously toggle, and the system counts how many oscillations occur within a fixed measurement window. The number of oscillations depends mainly on temperature, power distribution, and—in principle—radiation damage, although the latter is not relevant for the production module tests.
The idea is that abnormal oscillator counts could indicate problems such as poor power distribution. However, after discussions with the chip designers, we agreed that so far this test has not revealed any issues that were not already visible with other, more direct tests. Since the test is extremely fast, we still run it, and the acceptance limits used in Potato come from wafer-testing statistics.
At this point, we do not have any clear failure cases that would show up distinctly in the ring-oscillator plots. For this reason, these plots are generally not very informative when looking for module-level problems, and you can safely ignore them during visual inspection.
Each chip implements two families of ring oscillators:
- Delay-based oscillators, mainly sensitive to voltage and process variations.
- Inverter-based oscillators, more sensitive to radiation damage.

These two types provide complementary information, but only at a very qualitative level for our purposes.
There is also an important difference between the strip side (SSA) and the pixel side (MPA):
- MPA: Ring oscillators are present both in the periphery and in every pixel row, so each MPA has several oscillators distributed across the chip.
- SSA: Fewer oscillators are implemented; each SSA typically has four ring oscillator locations (e.g., bottom-left, bottom-center, bottom-right, and top-right).

All oscillators are controlled and read through I²C registers. A configuration register defines the measurement duration, and the read-only registers report the number of oscillations during that interval. Starting a new measurement simply requires toggling the start bit.
In summary, the ring oscillator test is included for completeness and historical reasons, but it is not currently used to diagnose module failures.

Here two example plots for MPA
![MPA_RingOscillatorDelayCounts_Hybrid](../images/OTtesting/PS/MPA_RingOscillatorDelayCounts_Hybrid.png)
![MPA_RingOscillatorInverterCounts_Hybrid](../images/OTtesting/PS/MPA_RingOscillatorInverterCounts_Hybrid.png)

And here for SSA
![SSA_RingOscillatorDelayCounts_Hybrid](../images/OTtesting/PS/SSA_RingOscillatorDelayCounts_Hybrid.png)
![SSA_RingOscillatorInverterCounts_Hybrid](../images/OTtesting/PS/SSA_RingOscillatorInverterCounts_Hybrid.png)

##### PedestalEqualization, "Trimming" (quick test) - Chip

Every comparator has an intrinsic offset due to transistor mismatch—this is unavoidable in the chip fabrication process. To compensate for these variations, both the MPA and the SSA implement two thresholds:
- A global threshold, common to all channels.
- A local threshold, adjustable independently per channel.
By tuning the local threshold for each channel, we can equalize the effective threshold across the chip so that all channels respond uniformly.

You’ll notice the naming of the procedures differs between the quick test and the full test.
**Quick test**: uses a binary scan.
Since we use the fast synchronous counters, the occupancy curve is not monotonic. Near the pedestal, noise makes the comparator toggle up and down rapidly, so the counter sees repeated threshold crossings. For this reason, we must inject a large signal, far from the pedestal, otherwise the result becomes dominated by noise oscillations.
This approach is fine for spotting clearly noisy or clearly quiet channels (which, on the strip side, often correlates with missing wirebonds), but the pedestal trimming itself is not very precise.

For each chip, two plots are produced. SSA results are shown as 1D distributions; all channels are in one block. MPA are displayed as a 2D map, which makes the spatial distribution easier to inspect.

Offset (local threshold) that equalizes the channel to the target. Valid offset values range from 0 to 31. Channels stuck at 0 or 31 indicate the allowed range was not sufficient to correct the channel. 
![ChannelOffsetValues_SSA_quick](../images/OTtesting/PS/ChannelOffsetValues_SSA_quick.png)
![2DChannelOffsetValues_MPA_quick](../images/OTtesting/PS/2DChannelOffsetValues_MPA_quick.png)


Occupancy at the chosen threshold (target ≈ 50%)
Ideally, every channel reaches ~50% occupancy once properly trimmed.
In the occupancy plot, channels far from 50% (close to 0% or close to 100%) indicate the trimming failed.

![ChannelOccupancyAfterOffsetEqualization_SSA_quick](../images/OTtesting/PS/ChannelOccupancyAfterOffsetEqualization_SSA_quick.png)
![2DChannelOccupancyAfterOffsetEqualization_MPA_quick](../images/OTtesting/PS/2DChannelOccupancyAfterOffsetEqualization_MPA_quick.png)



##### PedestalEqualizationPSatPedestal, "Trimming" (fullTest) - Chip

This calibration is done without pulse injection and is more precise in determining the pedestal compared to the one performed during the quickTest.

First, tune “Vtrim” per each chip, `C[0, 6]` for MPAs and `Bias_D5DAC8` for SSAs.
“Vtrim” = 31 gives the maximum range for the trim bits for MPAs and the minimum one for SSAs.

Then, tune trim bits for each channel. 

Calibration steps for each chip:
1. Set trim bits to max and Vtrim to 0. Find the channels that have the lowest (LOW) and
highest (HIGH) thresholds giving the max occupancy.
2. Set trim bits of HIGH to the minimum value and find the target threshold giving the
maximum occupancy (TARGET).
3. Do a binary scan on Vtrim (inspired by https://gitlab.cern.ch/gzevi/NewSoftware/-/blob/main/src/StateSetters/ModulePedestalNoisePS.cpp?ref_type=heads#L80) until
LOW has TARGET as the threshold giving the maximum occupancy.
4. Set the threshold to TARGET for all channels. Scan trimbits and set as the final trim
bit for each channel the one giving the maximum occupancy.

Three plots are saved per each chip type.
Two plots show the pedestal (per channel and as a cumulative distribution) to evaluate the uniformity of the trimming procedure. If the trimming succeeded the distribution is very narrow.

SSA plots:
![ChannelPedestal_SSA_full](../images/OTtesting/PS/ChannelPedestal_SSA_full.png)
![PedestalDistribution_SSA_full](../images/OTtesting/PS/PedestalDistribution_SSA_full.png)



MPA plots:
![ChannelPedestal_MPA_full](../images/OTtesting/PS/ChannelPedestal_MPA_full.png)
![PedestalDistribution_MPA_full](../images/OTtesting/PS/PedestalDistribution_MPA_full.png)

Offset (local threshold) that equalizes the channel to the target. Valid offset values range from 0 to 31. Channels stuck at 0 or 31 indicate the allowed range was not sufficient to correct the channel.
SSA: 
![ChannelTrimBit_SSA_full](../images/OTtesting/PS/ChannelTrimBit_SSA_full.png)
MPA:
![ChannelTrimBit_MPA_full](../images/OTtesting/PS/ChannelTrimBit_MPA_full.png)

##### PedeNoise - Hybrid, Chip, Channel

A scan of the applied threshold is performed, measuring the occupancy injecting some pulse for each threshold value. Ideally, this would produce a perfect step function. In reality, the presence of noise modifies the response, effectively convoluting the step function with a Gaussian. The Gaussian represents the noise in the system, and the resulting curve takes an S shape. This S-shaped curve is referred to as the “S curve.”

For this test, many plots are saved at different levels.

The S-Curve distribution is saved for each channel and shown below for one example channel.
On the X-axis there is the applied threshold in VcTh units (1 VcTh unit =  94 electrons for MPAs and 250 electrons for SSAs). Lower VcTh correspond to lower thresholds. On the Y-axis there is the occpuncy. Each S-curve is fitted individually. From this fit, the Gaussian component of the convolution allows extraction of the noise, represented by the width of the Gaussian. The underlying step function from the convolution corresponds to the pedestal, which is measured at around 50% occupancy. This measurement is performed without injection, so the pedestal obtained reflects the actual baseline of the system.

SSA:
![SCurve_1channel_SSA](../images/OTtesting/PS/SCurve_1channel_SSA.png)
MPA:
![SCurve_1channel_MPA](../images/OTtesting/PS/SCurve_1channel_MPA.png)

At the chip level, a 2D summary plot is stored summarizing the S-curve of all channels. Each plot shows channels on the x-axis, thresholds on the y-axis, and occupancy on the z-axis, with each line representing a single channel.

SSA:
![SCurve_SSA](../images/OTtesting/PS/SCurve_SSA.png)

MPA:
For the 2D plots, you have both columns and rows. The 1D representation is simply obtained by taking each row and placing it after the previous one. In other words, you read the pixels from left to right across a row, then move to the next row and again go from left to right, and so on.
![SCurve_MPA](../images/OTtesting/PS/SCurve_MPA.png)

<details>
  <summary>Known issues</summary>

For the strip sensor only (plots taken from 2S modules):

Broken wirebond or disconnected bump bond can show up as a compressed S-curve for a specific channel.

![SCurve_Chip_buggy](../images/OTtesting/2S/SCurve_Chip_buggy.png)

Another know issues is when horizontal stripes are present. This is a communication issue affecting the whole module. Example will be added when found again.


</details>


From the S-Curve, the pulse height for every channel can be extracted. The cumulative distribution of the pulse height should appear very sharp, while failures would show as long tails or outliers.

SSA:
![PulseHeightDistribution_SSA](../images/OTtesting/PS/PulseHeightDistribution_SSA.png)

MPA:
![PulseHeightDistribution_MPA](../images/OTtesting/PS/PulseHeightDistribution_MPA.png)

The channel pedestal plot shows the pedestal for each channel, revealing a very uniform distribution across all channels within a few VcTh, corresponding to a width of a few hundred electrons.

SSA:
![ChannelPulseHeight_SSA](../images/OTtesting/PS/ChannelPulseHeight_SSA.png)

MPA:
![ChannelPulseHeight_MPA](../images/OTtesting/PS/ChannelPulseHeight_MPA.png)


Similar distribution are also shown for the noise.

SSA:

![NoiseDistribution_SSA](../images/OTtesting/PS/NoiseDistribution_SSA.png)
![ChannelNoise_SSA](../images/OTtesting/PS/ChannelNoise_SSA.png)

MPA:
![NoiseDistribution_MPA](../images/OTtesting/PS/NoiseDistribution_MPA.png)
![ChannelNoise_MPA](../images/OTtesting/PS/ChannelNoise_MPA.png)

In this case, we also have the 2D distribution.
This view is more informative because you can immediately see that the noise is larger at the edges. The reason is that the pixels near the edges are physically larger.
Another detail you may notice is that the two leftmost columns always have the same value.
This is because the leftmost and rightmost channels do not actually exist—they are duplicated entries. The duplication is done to simplify the matching to the strip side.
In reality, the MPA does not have 120 distinct pixel columns. It has only 118 real columns, but the edge columns are duplicated so that the data structure still appears to have 120 columns. That is why you see 120 columns in the plots even though the number of physical columns is smaller.
![2DChannelNoise_MPA](../images/OTtesting/PS/2DChannelNoise_MPA.png)

There are there very similar distributions summarizing the performance at the hybrid level.

Strips:
![StripNoiseDistribution_Hybrid](../images/OTtesting/PS/StripNoiseDistribution_Hybrid.png)
Higher noise toward the POH (SSA0 on right hybrid and SSA7 on left hybrid) is expected.
![StripChannelNoise_Hybrid](../images/OTtesting/PS/StripChannelNoise_Hybrid.png)



Pixels:
![PixelNoiseDistribution_Hybrid](../images/OTtesting/PS/PixelNoiseDistribution_Hybrid.png)
![PIxelChannelNoise_Hybrid](../images/OTtesting/PS/PixelChannelNoise_Hybrid.png)



<details>
  <summary>Known issues</summary>

A group of channel with high noise may indicate a scratch on the sensor.
![StripChannelNoise_Scratch](../images/OTtesting/2S/StripChannelNoise_Scratch.png)

A channel with low noise could indicate a broken wirebond. Below 2 a broken bumpbond.
![StripChannelNoise_brokenBonds](../images/OTtesting/2S/StripChannelNoise_brokenBonds.png)

Groups of broken channels in the center of CBC can indicate that sparking occurred.
![StripChannelNoise_Sparking](../images/OTtesting/2S/StripChannelNoise_Sparking.png)

If the HV is not applied, a very high noise is shown over both hybrids and sensors.
</details>

##### OTinjectionDelayOptimization - Chip
Up to this point, all our tests used asynchronous injection, so we didn’t need to worry about injection delay or latency. You simply inject a charge, and the counter registers whenever the signal crosses the comparator threshold.
However, when we want to measure efficiencies by injecting real events, asynchronous counters become unreliable—especially at low thresholds—because they can count multiple times within the same bunch crossing. With synchronous counters, on the other hand, you always know exactly how many events were injected, so the denominator of the efficiency is well defined.
To use synchronous counting correctly, we must ensure that the sampling occurs at the right point on the pulse shape. For this reason, we perform a scan of the injection delay.


Injection tests are performed by scanning the injection delay and measuring, for each delay, the threshold corresponding to 50% occupancy—where the input signal equals the comparator threshold—allowing reconstruction of the full signal distribution as a function of time.

SSA:
![ThresholdVsDelayScan_SSA](../images/OTtesting/PS/ThresholdVsDelayScan_SSA.png)
MPA:
![ThresholdVsDelayScan_MPA](../images/OTtesting/PS/ThresholdVsDelayScan_MPA.png)
At this stage, the working point can be determined by setting the injection delay at the signal peak, which in first approximation is independent of the injected charge, ensuring that the signal always crosses the threshold at the same instant; the distance from the pedestal is then adjusted—typically five times the measured pedestal noise from the previous calibration—to suppress pedestal-induced noise and precisely define the working point.

![BestThresholdAndDelay_SSA](../images/OTtesting/PS/BestThresholdAndDelay_SSA.png)

![BestThresholdAndDelay_MPA](../images/OTtesting/PS/BestThresholdAndDelay_MPA.png)

From this point onward, each injection measurement is performed using the identified injection delay and threshold, ensuring that subsequent tests are properly configured so that any injected charge is read at the correct value.

##### OTinjectionOccupancyScan - Chip

The occupancy is measured for different injection charges to establish a reference: first by recording the occupancy without injection to quantify the contribution from noise alone, and then by measuring the response for known injected charges, allowing evaluation of the detection efficiency for signals corresponding to specific charge amounts.

Five different measurements are performed, corresponding to five different plots. Three examples are shown, for no injection and one for some injected charge.

The one below is without injection.
SSA:
![ChannelOccupancy_Injection_0.000_MIP_SSA](../images/OTtesting/PS/ChannelOccupancy_Injection_0.000_MIP_SSA.png)
MPA:
![ChannelOccupancy_Injection_0.000_MIP_MPA](../images/OTtesting/PS/ChannelOccupancy_Injection_0.000_MIP_MPA.png)

The occupancy is measured for each chip, and although some slight activity may appear, the threshold is set to five times the noise, so almost no signal is expected except for very small fluctuations (that may be more visible in log scale).

Then, injections are performed at different charge levels — for example, a quarter of a MIP, which corresponds to roughly the same level as the threshold set at five times the noise, yielding about 50% efficiency. This value indicates that a signal equivalent to a quarter of a MIP produces a 50% detection probability. Measuring lower charges is important to study cluster size and improve spatial resolution. Subsequent plots show the occupancy for each channel at 0.25, 0.5, 1, and 2 MIPs, allowing identification of potential issues. The inspection of these results is automated by potato, which flags problematic modules; manual inspection is mainly needed for those flagged as bad. Since each CBC chip behaves slightly differently, occupancy maps are produced per CBC and per channel, enabling the identification of noisy or inefficient channels, such as those with damaged comparators, although new modules typically show very few such cases.

SSA:
![ChannelOccupancy_Injection_0.250_MIP_SSA](../images/OTtesting/PS/ChannelOccupancy_Injection_0.250_MIP_SSA.png)
![ChannelOccupancy_Injection_1.000_MIP_SSA](../images/OTtesting/PS/ChannelOccupancy_Injection_1.000_MIP_SSA.png)
MPA:
The inefficiency in the edge pixels are not real. They are a result of the large double pixels.
![ChannelOccupancy_Injection_0.250_MIP_MPA](../images/OTtesting/PS/ChannelOccupancy_Injection_0.250_MIP_MPA.png)
![ChannelOccupancy_Injection_1.000_MIP_SSA](../images/OTtesting/PS/ChannelOccupancy_Injection_1.000_MIP_MPA.png)


##### OTPScommonNoise - OpticalGroup, Hybrid, Chip
The goal of the common-mode noise measurement is to check whether channels behave in a correlated way. Ideally each channel would be completely independent, but in reality channels on the same chip and hybrid share ground, power, and other circuitry, so some correlation is expected.
We perform two measurements at two different thresholds. This came after discussions with Giovanni, who suggested adding an additional point for integration studies. Since the calibration is fast (about 16 seconds), adding a second threshold has minimal impact on total time.
Occupancy-driven approach
The original idea—taken from the 2S procedure—was to maximize the occupancy.
For the 2S this is straightforward because the 2S sends unsparsified data: we get one bit per strip, and the CIC can bypass sparsification. This makes it possible to reach 100% occupancy.
For the PS, however, the output is limited by the maximum number of clusters that can be transmitted:
- up to 127 strip clusters per hybrid
- up to 127 pixel clusters per hybrid
So we cannot reach full occupancy. To approximate the 2S-style approach, we tune the threshold to achieve about 50% of the available clusters—roughly 60 clusters for the whole hybrid. Because the threshold DAC has limited resolution, the occupancy cannot be tuned very precisely, but we aim to center the distribution as much as possible. These results are shown in the occupancy-driven plots.
The idea is to examine both the low- and high-occupancy tails to look for signs of common-mode effects, since you do not expect all channels to fire—or not fire—simultaneously.
A possible alternative would be to enable one SSA or one MPA at a time. That would allow a slightly higher relative occupancy per chip (e.g., 16 clusters out of 32 instead of 8 out of 127), but the gain is modest and the procedure would become significantly longer, since it would need to be repeated for all chips.
In practice we might only need to look at one side of the distribution. Correlated noise is not expected to affect the low and high tails differently. Therefore, in addition to the occupancy-driven plot, we also produce a 3σ noise plot, which focuses on only one tail.

For now we keep both methods—occupancy-driven and 3σ noise—just as we do for the 2S. Since the scans are fast, having both gives more flexibility, and later we can choose the one that provides the best discrimination or simply keep both if they remain useful.


- Below the common noise distribution for one chip is shown. 

Here the hits on one chip are shown. It shows the distribution of events vs. number of hits on that chip.
Occupancy is low by construction because the PS cannot reach high hit multiplicity.
SSA:
![CommonNoiseHits_OccupancyDriven_SSA](../images/OTtesting/PS/CommonNoiseHits_OccupancyDriven_Chip_SSA.png)
MPA:
![CommonNoiseHits_OccupancyDriven_MPA](../images/OTtesting/PS/CommonNoiseHits_OccupancyDriven_Chip_MPA.png)

- Now we show distributions at the hybrid level


Here we look at the distribution of the number of hits per event across the entire hybrid for strips and pixels. The hybrid-level 1D hit distributions show the number of events as a function of the number of hits for pixels and strips separately. Both occupancy-driven and 3σ versions exist. The pixel side often shows a slightly higher tail, while the strip side falls off quickly at low hit counts due to cluster-size limits.
![CommonNoiseHitsStrip_OccupancyDriven_Hybrid](../images/OTtesting/PS/CommonNoiseHitsStrip_OccupancyDriven_Hybrid.png)
![CommonNoiseHitsPixel_OccupancyDriven_Hybrid](../images/OTtesting/PS/CommonNoiseHitsPixel_OccupancyDriven_Hybrid.png)

We also have common noise correlation plots for each SSA-MPA pair. The chip-to-chip correlation plots display the number of hits in the SSA versus the number of hits in the MPA for each event, with the z-axis showing how many events populate each combination. These plots allow you to see correlated behaviour between the pixel and strip chips within the same module.
![SSAtoMPA_CommonNoiseCorrelation_OccupancyDriven_Hybrid](../images/OTtesting/PS/SSAtoMPA_CommonNoiseCorrelation_OccupancyDriven_Hybrid.png)

And the correlation at the hybrid level. At the hybrid level, the 2D correlation plots use the number of strip hits on the x-axis and the number of pixel hits on the y-axis, with the number of events shown on the z-axis. These give a global view of correlations across the entire hybrid
![CommonNoiseStripPixelCorrelation_OccupancyDriven_Hybrid](../images/OTtesting/PS/CommonNoiseStripPixelCorrelation_OccupancyDriven_Hybrid.png)


- OpticalGroup/Module level distributions

![CommonNoiseHitsStrip_OccupancyDriven_OG](../images/OTtesting/PS/CommonNoiseHitsStrip_OccupancyDriven_OG.png)
![CommonNoiseHitsPixel_OccupancyDriven_OG](../images/OTtesting/PS/CommonNoiseHitsPixel_OccupancyDriven_OG.png)


And the correlation at the hybrid level:
![CommonNoiseStripPixelCorrelation_OccupancyDriven_OG](../images/OTtesting/PS/CommonNoiseStripPixelCorrelation_OccupancyDriven_OG.png)


These results are harder to interpret, reflecting the true behavior of the module, so if any unusual tails or unexpected noise appear in the pedenoise results, these plots should be checked to identify possible anomalies by comparing them with reference common noise plots from other modules to confirm that the noise distribution matches expectations.

##### Electric Chain Validation (ECV)
The electric chain validation focuses on evaluating the width of the working area of the communication phases within the module. Unlike the verification step, which uses the phase identified by the CIC or the LpGBT as optimal, the validation manually scans different phases to determine the range over which the chain remains operational. A broad working area indicates a stable configuration, while a narrow one suggests that the system operates close to its limits and may become unstable once installed in the detector.


##### OTSSAtoMPAecv - Hybrid

We force the system into all possible operating points, including regions outside the normal working range of the chip. Starting with the SSA-to-MPA communication, these plots are stored at the hybrid level. Because everything is based on pattern matching, we produce two plots: one showing the error rate and one showing the number of tested bits. You will notice several SSA→MPA plots, because we also scan the current that drives the lines between the SSA and MPA. This current is set by a register on the SSA, and by scanning it we check how the MPA responds to different drive strengths, since the optimal value can vary.

Between the SSA and MPA there is no real fine phase control: only two possible phases exist, corresponding to the rising or falling edge of the clock on which the MPA samples the data arriving from the SSA. Changing the sampling phase may effectively switch to the opposite edge of the clock, but doing so can also shift the data by one bit to the left or right. To account for this, the plot contains three bins for the rising edge and three for the falling edge. For each edge we test the nominal bit, the bit before, and the bit after. This compensates for the possible bit shift introduced when the sampling edge changes. With more experience we may eventually reduce the number of tested offsets, but for now we include all three to guarantee that the first bit is correctly sampled.

The first plot shows the number of tested bits for every configuration, and each line corresponds to a different MPA. 

![SSAtoMPA_SamplingEdgeTestedBits_SSA_SLVScurrent_4_Hybrid](../images/OTtesting/PS/SSAtoMPA_SamplingEdgeTestedBits_SSA_SLVScurrent_4_Hybrid.png)

The second plot shows the error rate for the same configurations—again grouped by rising edge, falling edge, and the three possible bit offsets for each. For a good module you expect that, for each horizontal line (each line of each MPA), at least one configuration yields zero errors.

![SSAtoMPA_SamplingEdgeErrorRate_SSA_SLVScurrent_4_Hybrid](../images/OTtesting/PS/SSAtoMPA_SamplingEdgeErrorRate_SSA_SLVScurrent_4_Hybrid.png)

Each configuration is independent, so the MPAs do not all need to use the same clock edge or offset; as long as every MPA has at least one valid setting, the module is considered good.


##### OTSSAtoSSAecv - Hybrid

The next step is to check the phases between one SSA and the neighboring SSA. Each SSA exchanges information about the leftmost and rightmost strips with the adjacent chip. This is necessary because hits can span across two SSAs and their corresponding MPAs, and the system must still be able to form correct stubs. The same concept exists for the CBC, but in that case it is implemented differently: the CBC uses a fake channel connected to the neighboring chip, whereas the SSA uses an actual line that transfers the pattern shown here. If this line is not interpreted correctly for any reason, the communication between the two SSAs is lost. Since this is a purely bump-bond–level connection, it is not something that can be fixed during the quick test; it can only be evaluated during the full test.
The plots we save are similar to the SSA→MPA communication plots and are also stored at the hybrid level. For each condition we again save two plots: one with the error rate and one showing the number of tested bits. As before, we scan three different drive-current settings for these lines by adjusting the SSA register that controls the current.
As with the previous case, there is no fine phase control between the SSAs—only the choice of sampling on the rising or the falling edge of the clock. Changing the sampling edge can produce a one-bit shift in either direction, so we test three bit positions (–1, 0, +1) for each of the two edges to ensure that a working configuration is not missed simply because the sampling edge moves the data boundary.
In the plots, the y-axis shows the direction of the communication: one set of lines corresponds to SSA-A sending to SSA-B, and the other corresponds to SSA-B sending back to SSA-A. Just as before, the second plot shows the corresponding error rate for all tested configurations.
This is also the reason why we must include the bit-shift tests. When switching from rising-edge sampling to falling-edge sampling, some SSAs still work correctly, but the valid working point is shifted by one bit. For each communication direction, at least one valid configuration is required, and in practice there are often two. This is likely because the two SSAs are physically close, making the communication relatively forgiving.
Overall, this procedure verifies the lateral communication between neighboring SSAs and ensures that every pair has at least one reliable operating point.

![SSAtoSSA_SamplingEdgeTestedBits_SSA_SLVScurrent_4_Hybrid](../images/OTtesting/PS/SSAtoSSA_SamplingEdgeTestedBits_SSA_SLVScurrent_4_Hybrid.png)
![SSAtoSSA_SamplingEdgeErrorRate_SSA_SLVScurrent_4_Hybrid](../images/OTtesting/PS/SSAtoSSA_SamplingEdgeErrorRate_SSA_SLVScurrent_4_Hybrid.png)


##### OTCICtoLpGBTecv - Hybrid
The CIC-to-LpGBT ECV studies the transmission between the CIC and LpGBT by varying the LpGBT sampling phase and checking whether the data patterns sent by the CIC are correctly reconstructed in the FPGA. For each phase setting, the number of tested bits and corresponding error rate are recorded in hybrid-level plots labeled as CIC-to-LpGBT pattern matching. The test also explores the impact of varying the current used by the CIC to drive the data (SLVS strenght, from 1 to 5), the LpGBT clock polarity, and the CIC clock drive strength (from 1 to 7). The LpGBT provides the clock to the hybrid, and changing its polarity effectively shifts the clock phase by 50%. These variations help evaluate how different transmission parameters affect data reconstruction and identify the range of stable operating conditions where several phases ensure reliable communication between components.


This plot shows, on the Y axis, the line ID corresponding to each transmission line, and on the X axis, the manually selected LpGBT sampling phase, which ranges from 0 to 14. The phase is varied manually rather than letting the LpGBT automatically adjust it, in order to explore also regions where the LpGBT cannot properly sample the incoming data. The Z axis represents the number of tested bits, with the stub pattern matched in firmware for speed, while the Level-1 pattern matching is performed in the software, resulting in longer scan times and fewer tested bits.
![CICtoLpGBT_PatternMatchingTestedBits_CIC_SLVScurrent_5_LpGBT_Clock_Polarity_0_Clock_Strength_7_Hybrid](../images/OTtesting/PS/CICtoLpGBT_PatternMatchingTestedBits_CIC_SLVScurrent_5_LpGBT_Clock_Polarity_0_Clock_Strength_7_Hybrid.png)


In the plot below the Z axis represents the number of errors. Some lines are expected not to work properly, since sampling may occur when the incoming data from the CIC are transitioning, leading to bit misinterpretation. The quality of a module is therefore evaluated by the width of the phase range over which correct data transmission is achieved.
![CICtoLpGBT_PatternMatchingErrorRate_CIC_SLVScurrent_5_LpGBT_Clock_Polarity_0_Clock_Strength_7_Hybrid](../images/OTtesting/PS/CICtoLpGBT_PatternMatchingErrorRate_CIC_SLVScurrent_5_LpGBT_Clock_Polarity_0_Clock_Strength_7_Hybrid.png)

Many versions of the above plots are stored for the various values and combinations of the current used by the CIC to drive the data (SLVS strenght, from 1 to 5), the LpGBT clock polarity, and the CIC clock drive strength (from 1 to 7).

##### OTalignLpGBTinputsForBypass - Hybrid
Earlier I mentioned that, at that stage of [MPAtoCIC_PatternMatcing](#otverifycicdataword---hybrid)  testing, we cannot yet determine which specific stub line is causing an error in the stub pattern matching. The plot we looked at only shows a single bin indicating that some error occurred, but it does not identify which stub line is responsible. To obtain that level of detail, the CIC must be put into bypass mode.
This introduces several complications. First, when the CIC is in bypass mode, the phase of the data lines changes. Because of this, the LpGBT must be realigned. However, we cannot use the automatic alignment, since the bypassed data does not contain the pattern expected by the LpGBT, so we are forced to perform a manual phase scan. This is what the next step is about.
The second complication is that the CIC receives 48 input lines from each group of eight MPAs, but can output at most seven lines at a time. In practice, the lines are grouped into “phyports,” and only one phyport can be bypassed at a time. This means we can only test four lines at once. We therefore need to loop over all phyports to cover all lines.
[Mapping](https://fnal-outer-tracker.docs.cern.ch/documents/PhyPortMap.pdf) the CIC lines back to the identifiers used in Ph2_ACF is non-trivial, because the front-end IDs used inside the CIC do not match the I²C addresses assigned to the MPAs. The full mapping between CIC front-end IDs, I²C IDs, and the identifiers used in Ph2_ACF is given in the table shown here. To make things more confusing, the mapping differs between the left and right branches of the hybrid. Fortunately, Ph2_ACF handles this internally, so the plots you will see are already expressed in terms of the familiar IDs used in your XML configuration and the numbers printed on the hybrid.
For each phyport, we configure the MPAs to inject a known pattern, set the CIC into bypass mode, and then scan the LpGBT phase to find a working region. The resulting plots are stored in the directory for “LpGBT for CIC bypass” and include both the number of tested bits and the bit-error rate. We also store the “best phase,” defined as the central value of the largest continuous phase region with zero errors.
The test-bit plots show, for each phase of the LpGBT, how many bits were received, and the error-rate plots indicate where errors occur. 

![LpGBTforCICbypass_PhaseScanTestedBits_phyPort0_Hybrid](../images/OTtesting/PS/LpGBTforCICbypass_PhaseScanTestedBits_phyPort0_Hybrid.png)

In the error-rate plot, the regions with no errors mark the usable phases, and we select the nominal operating phase in the center of the widest such region. 

![LpGBTforCICbypass_PhaseScanBitErrorRate_phyPort1_Hybrid](../images/OTtesting/PS/LpGBTforCICbypass_PhaseScanBitErrorRate_phyPort1_Hybrid.png)

These plots are mainly auxiliary: they allow you to diagnose problems later on by checking whether any anomaly was already visible at the bypass-mode stage.
Since each phyport has a different optimal phase, this entire procedure must be repeated for all phyports. After completing this scan, we have a reliable phase setting for each group of lines. At that point, we can safely run the full link-validation tests between the MPAs and the CIC in bypass mode, using injected patterns from the MPAs and verifying their integrity through the CIC.

##### OTChipToCICecv - Hybrid

The final step of the electrical chain validation focuses on the link between the MPA and the CIC. For this stage, we again produce two plots per scan point: the error rate and the number of tests. The procedure is similar to the previous validation steps, but here we vary the MPA output drive current that controls the signal strength on the lines between the MPA and the CIC. Three current settings are typically used to study the behavior of the link. In this configuration, a higher drive strength corresponds to a lower numerical value—so current setting 0 gives the highest current, 14 the lowest, and 8 an intermediate value. 

The plots follow the same format as before, with the phase on the x-axis and the line ID on the y-axis, showing all MPAs and their corresponding lines. Phases 2 and 3 are absent because they are not functional on the CIC and are therefore skipped. As usual, the number of tests is smaller for the Level-1 data since those checks are performed in software—still around 10⁵ to ensure sufficient statistics without excessive runtime. 

![MPAtoCIC_PhaseScanTestedBits_MPA_SLVScurrent_1_Hybrid](../images/OTtesting/PS/MPAtoCIC_PhaseScanTestedBits_MPA_SLVScurrent_1_Hybrid.png)

The corresponding error-rate plots show that most channels exhibit a broad phase region with zero errors, indicating a stable and well-aligned communication between the MPA and the CIC.

![MPAtoCIC_PhaseScanErrorRate_MPA_SLVScurrent_1_Hybrid](../images/OTtesting/PS/MPAtoCIC_PhaseScanErrorRate_MPA_SLVScurrent_1_Hybrid.png)
The Level-1 channels occasionally show issues in the pattern matching due to imperfect data sampling, leading to rare misreads and preventing a 100% match rate. While this is not a major concern, improvements are being explored, though the underlying sampling mechanism makes it difficult to fully eliminate. The plots clearly show that non-working phases have much higher error rates—around 45% compared to below 0.2% in well-aligned regions. As before, the results are shown for four different current settings, all displaying similar behavior.


##### OTBitErrorRateTest - OpticalGroup
The bit error rate (BER) test is designed to verify the stability of the optical link between the LpGBT and the FPGA, passing through the VTRx. This allows checking for transmission issues either between the LpGBT and VTRx or between the VTRx and the board.
The LpGBT includes a built-in PRBS (Pseudo-Random Bit Sequence) generator, which produces a known pseudo-random bit pattern. The same pattern is generated in the firmware, and by comparing the sent and received sequences, any bit mismatches can be detected—indicating corrupted bits during transmission.
The LpGBT offers multiple PRBS modes (listed in its manual), and in this test, one PRBS is emulated per line. Although the data link is a single physical channel between the LpGBT, VTRx, and FPGA, the results are split by line in the firmware for analysis.
Because of limited FPGA resources, the test is run sequentially, one line at a time, and results are kept separate to help identify and debug potential issues. Since the test runs at the LpGBT level, all results are stored in the optical view.
The first plot shows the bit error rate phase scan. This step is mostly a technical procedure, as the LpGBT generates the bit error rate pattern from a clock source whose phase can be adjusted. Certain phases prevent the LpGBT from correctly interpreting its own pattern, so a quick scan is performed to identify the valid working phases. 

![BERTerrorRatePhaseScan_OpticalGroup](../images/OTtesting/PS/BERTerrorRatePhaseScan_OpticalGroup.png)
![BERTtestedBitsCounterPhaseScan_OpticalGroup](../images/OTtesting/PS/BERTtesteBitCounterPhaseScan_OpticalGroup.png)

Once a stable phase is found, it is stored as the best phase. This step ensures that the LpGBT is in a proper transmission state and avoids generating fake bit errors unrelated to the actual link between the module and the FC7. After determining the correct phase, the real bit error rate test can be performed.

![BERTbestPhase_OpticalGroup](../images/OTtesting/PS/BERTbestPhase_OpticalGroup.png)

As for the other plots, there are two levels of information: one plot shows the number of tested bits, which reaches approximately 10¹⁰ bits and appears fairly uniform. The distribution is roughly symmetric because the two hybrids are tested in parallel, meaning that when the stub number two on the right hybrid is tested, the corresponding stub number two on the left hybrid is tested as well, resulting in similar counts. The exact number of tested bits is not strictly controlled, since only a minimum threshold is set and the system runs until that is exceeded, so small variations are expected and not concerning.
![BERTtestedBitsCounter_OpticalGroup](../images/OTtesting/PS/BERTtesteBitCounter_OpticalGroup.png)

The key plot is the bit error rate, which measures the stability of the link while remaining split by line. Under normal conditions, the bit error rate is expected to be zero. Summing across all lines corresponds to about 10¹¹–10¹² tested bits, and the expected bit error rate is below 10⁻¹²–10⁻¹³. Testing up to 10¹³ bits would require several hours, so the procedure uses a lower value for practicality. During development, modules tested up to 10¹³ bits showed no errors, confirming the link stability. Therefore, the standard validation relies on about 10¹¹ tested bits per run, which provides sufficient confidence in link performance while keeping testing time reasonable.
![BERTerrorRate_OpticalGroup](../images/OTtesting/PS/BERTerrorRate_OpticalGroup.png)

The forward error correction (FEC) counter provides complementary information to the bit error rate by tracking how many bits were flipped during transmission but successfully corrected by the FEC mechanism, which can fix up to five flipped bits per packet. Although a zero bit error rate indicates no uncorrected errors, nonzero FEC counts can still reveal link instabilities. The firmware records the number of corrected bits, and this information is stored cumulatively for the entire LpGBT packet, resulting in a single value for both hybrids. The results remain separated by line since each line is tested independently due to firmware resource limits, but they can be summed to assess overall behavior. The plot reports counts rather than percentages because the total number of transmitted packets is not precisely known, though it can be approximated from the test duration and total bits processed. Consistent factor counts across lines suggest stable communication, while localized or irregular counts may indicate transient link issues.
![FECerrorCounter_OpticalGroup](../images/OTtesting/PS/FECerrorCounter_OpticalGroup.png)

##### OTRegisterTester - Hybrid
This test checks the stability of the I2C communication by repeatedly writing and reading specific registers on both the CBCs and the CIC. A known pattern is written and read back, then its inverse is written and read back, and this cycle is repeated about a thousand times. The results are summarized in a single plot showing the read and write efficiency for each of the eight CBCs and the CIC, with one plot per hybrid. The expected outcome is a consistent 100% efficiency, as the CIC I2C communication is typically very stable. For the CBCs, register page flipping that is known to cause instabilities is avoided to ensure meaningful results. The goal is not to test the general chip performance but to identify possible I2C instabilities specific to the module, which could originate from issues in the connectors between the FEH and SEH if deviations are observed.

![RegisterMatchingEfficiency_Hybrid](../images/OTtesting/PS/RegisterMatchingEfficiency_Hybrid.png)

[02:20:40.000 --> 02:20:42.000]  Then monitor BQM plot.
[02:20:42.000 --> 02:20:55.000]  So all these information that we're storing are contained into the XML file just for you one as a reference.
[02:20:55.000 --> 02:20:58.000]  They are located over here.
[02:20:58.000 --> 02:21:04.000]  At the end of the XML file and it contains the list of information that we are storing.
[02:21:04.000 --> 02:21:13.000]  For the PS we're storing both information read by the LPGT and information read by the SSA and the MPA.
[02:21:13.000 --> 02:21:16.000]  So I'm going to go to all of these plots.
[02:21:16.000 --> 02:21:28.000]  So these are T-graph and as I was saying are stored into a separate plot such that you can monitor also when you are not in a running condition.
[02:21:28.000 --> 02:21:34.000]  So starting, just closing everything.
[02:21:34.000 --> 02:21:41.000]  So the first, so it's going to look like the same structure of the Resalphi.
[02:21:41.000 --> 02:21:50.000]  And then at the optical group, we start to see the monitor of the LPGT.
[02:21:50.000 --> 02:21:57.000]  So these are in particular first three are measurement of some values that are read inside the LPGT.
[02:21:57.000 --> 02:22:01.000]  And in particular, this one is one voltage.
[02:22:01.000 --> 02:22:05.000]  They call VDD that should be around 1.2 volts.
[02:22:05.000 --> 02:22:07.000]  It's more or less stable.
[02:22:07.000 --> 02:22:12.000]  Just please ignore if you see some large swing like that.
[02:22:12.000 --> 02:22:16.000]  The ADC readout sometimes fails.
[02:22:16.000 --> 02:22:18.000]  So you might see some sort of swing.
[02:22:18.000 --> 02:22:21.000]  So these are clearly not real.
[02:22:21.000 --> 02:22:23.000]  So don't be alarmed when you have something like that.
[02:22:23.000 --> 02:22:30.000]  These are the beginning will sometimes happen at any time.
[02:22:30.000 --> 02:22:33.000]  And the same for these other voltage.
[02:22:33.000 --> 02:22:37.000]  I honestly, not 100% sure what are these values.
[02:22:37.000 --> 02:22:39.000]  So why we have these two values?
[02:22:39.000 --> 02:22:48.000]  I would guess there are two blocks that have two different voltages to avoid first talk or something like that in basic.
[02:22:48.000 --> 02:22:49.000]  Those are available.
[02:22:49.000 --> 02:22:51.000]  We monitor them both.
[02:22:51.000 --> 02:22:53.000]  And they are both more or less to the same value.
[02:22:53.000 --> 02:22:57.000]  So far we haven't seen surprises.
[02:22:57.000 --> 02:23:01.000]  Then the temperature.
[02:23:01.000 --> 02:23:05.000]  So you see here there are some swings that we can sort of ignore.
[02:23:05.000 --> 02:23:12.000]  That means you mean this is the temperature of the sensor that is within the LPGT.
[02:23:12.000 --> 02:23:17.000]  This should be quite accurate because we already have the calibration of all the LPGT.
[02:23:17.000 --> 02:23:21.000]  So this value should be quite realistic.
[02:23:21.000 --> 02:23:35.000]  I don't know exactly in terms of degrees because I don't remember what the LPGT group said, but it should be the absolute value should be quite realistic.
[02:23:35.000 --> 02:23:43.000]  Just open to the flag.
[02:23:43.000 --> 02:23:59.000]  And the other information are read out by input that are provided to the LPGT via some connector of the data that then go into the internal ADC of the LPGT.
[02:23:59.000 --> 02:24:03.000]  The first one is the sensor temperature.
[02:24:03.000 --> 02:24:12.000]  And with the sensor temperature, I mean the one that sticks out from the ROH and is in contact with the sensor.
[02:24:12.000 --> 02:24:20.000]  So this is basically equivalent of what we have for the 2S.
[02:24:20.000 --> 02:24:38.000]  So also this one, the measurement of the temperature of the resistor should be quite accurate because we have the calibration of the LPGT as well as the nominal NTC calibration.
[02:24:38.000 --> 02:24:44.000]  This is the NCT, so it is the name of the resistor.
[02:24:44.000 --> 02:25:02.000]  So then there is the question of how accurate is the measurement of the sensor temperature because of not great contact with the sensor itself and some power that is leaked from the ROH.
[02:25:02.000 --> 02:25:11.000]  But the measurement should be quite accurate to the temperature that is seen by the resistor.
[02:25:11.000 --> 02:25:26.000]  Then we have the measurement of the leakage current on the diode that converts the optical signal into an electrical signal.
[02:25:26.000 --> 02:25:41.000]  As far as I understood, this is the measurement basically of the, so it's going to be used during data taking to monitor the radiation damage of this diode.
[02:25:41.000 --> 02:25:51.000]  This we have to have an idea of the yield of the signal that is collected by the diode.
[02:25:51.000 --> 02:25:58.000]  So this should be the average current generated by the light that is being received.
[02:25:58.000 --> 02:26:02.000]  So for the time being, we don't really have a plan to use it.
[02:26:02.000 --> 02:26:17.000]  I would say that if we have problems with this diode, we will see this in effect from other behavior of the plot.
[02:26:17.000 --> 02:26:22.000]  Sorry, now I have also to open to the dog.
[02:26:22.000 --> 02:26:28.000]  Okay, and then we also monitor for the VTRX the temperature.
[02:26:28.000 --> 02:26:34.000]  And as you can see already without an zoom, we are having some issue over here.
[02:26:34.000 --> 02:27:00.000]  So, first of all, this is the temperature that is a temperature sensor that we read from the VTRX itself is not going to be super accurate in terms of absolute value because we don't have a calibration value for that.
[02:27:00.000 --> 02:27:09.000]  But as all the temperature sensor that we have, those are quite released in terms of relative variations.
[02:27:09.000 --> 02:27:29.000]  Now, while we have these, okay, this was due to a recent thing that was added and we actually, Stefan found out that we were setting the, so you need to set up a current to be provided to the resistors.
[02:27:29.000 --> 02:27:41.000]  In order to measure the temperature and what we were doing is that we were setting the current given the expected resistors in order to have a good resolution.
[02:27:41.000 --> 02:27:51.000]  The problem with that is that when you go down in temperature, the value of the resistor changes drastically by the factor of 10 or even more.
[02:27:51.000 --> 02:27:55.000]  And therefore, if you use the nominal value of room temperature will not work anymore.
[02:27:55.000 --> 02:28:06.000]  So what was added is that instead of using the previous values, but then when you have that the readout of the ADC fails once that sometimes happened, then it got stuck.
[02:28:06.000 --> 02:28:16.000]  So I tried to fix it a little bit better and it works as we've seen for the sensor temperature before the VTRX is still not working fine.
[02:28:16.000 --> 02:28:21.000]  So that's why you see this ring, but we're addressing that.
[02:28:22.000 --> 02:28:32.000]  And finally, with the PS, we also monitor the LPG, the 2.5 volts.
[02:28:32.000 --> 02:28:39.000]  That is the one that is applied to the LpGBT server to the VTRX that requires 2.5 volts.
[02:28:39.000 --> 02:28:43.000]  So here we can monitor the voltage.
[02:28:43.000 --> 02:28:50.000]  Okay, so these are the information that we store at the level of the LpGBT.
[02:28:50.000 --> 02:28:55.000]  And then the next information that we can store for the SSN and PA.
[02:28:55.000 --> 02:29:10.000]  So we have two voltages, the analog voltage that is around 1.2, 1.3.
[02:29:10.000 --> 02:29:18.000]  And as well as the digital voltage that it should be around 1 is slightly higher than 1.
[02:29:18.000 --> 02:29:23.000]  So here, so far, I don't think we saw anything, but it might be an indication.
[02:29:23.000 --> 02:29:27.000]  Maybe if a certain amount something fails, you can see a drastic drop or something like that.
[02:29:27.000 --> 02:29:32.000]  So I would guess we'll be using failure cases to do a bit of debugging.
[02:29:32.000 --> 02:29:40.000]  But so far, if you don't have enough power, you will see clearly some other problems.
[02:29:40.000 --> 02:29:43.000]  And finally, we have the temperature measurement.
[02:29:43.000 --> 02:29:46.000]  This was briefly discussed at the beginning.
[02:29:46.000 --> 02:29:49.000]  This is the part that at the moment we still don't have the calibration.
[02:29:49.000 --> 02:29:56.000]  So the relative variation should be quite accurate.
[02:29:56.000 --> 02:30:04.000]  But the absolute value is not.
[02:30:04.000 --> 02:30:13.000]  So this was probably higher than what we're showing here because we were running long calibration without cooling.
[02:30:13.000 --> 02:30:22.000]  So these don't use them as absolute values until we get the real calibration.
[02:30:22.000 --> 02:30:25.000]  And for the MPA is the same.
[02:30:25.000 --> 02:30:34.000]  We store the two measurements of the digital and analog current voltage as well as the temperature.
[02:30:34.000 --> 02:30:46.000]  And as for the SSA, also this is not really accurate in terms of absolute value.
[02:30:46.000 --> 02:30:59.000]  And those are all the parameters that were currently monitored during all the measurements that we do with the PH2SEL.
[02:30:59.000 --> 02:31:03.000]  Any questions or comments on this?
[02:31:03.000 --> 02:31:12.000]  Okay.
[02:31:12.000 --> 02:31:24.000]  So we are at the end of this discussion.
[02:31:24.000 --> 02:31:50.000]  So I think we can, so I'm going to just ask you if there is anything that you would like to better understand what was discussed today.
[02:31:50.000 --> 02:31:55.000]  Sorry, I have a question.
[02:31:55.000 --> 02:31:57.000]  Maybe I missed something.
[02:31:57.000 --> 02:32:03.000]  At some point you said that the first and the last three are duplicated.
[02:32:03.000 --> 02:32:07.000]  There's a lot of slides so I don't remember exactly where it was.
[02:32:07.000 --> 02:32:11.000]  But I didn't really understand what was the reason.
[02:32:11.000 --> 02:32:26.000]  Yeah, it's simply because when you have, I'm going to open in the meantime, when you have to do the bombarding of the two asyps,
[02:32:26.000 --> 02:32:34.000]  it is quite difficult to have a very small space in between the two chips.
[02:32:34.000 --> 02:32:42.000]  So what they usually do, and this is done also for the inner tracker for the current and I think also the future.
[02:32:42.000 --> 02:33:00.000]  What they do is that they make the picture at the edge between the two chips a bit wider such that you have a bit more room to compensate for the fact that you cannot cut the basic super precise.
[02:33:00.000 --> 02:33:03.000]  So this is usually the case.
[02:33:03.000 --> 02:33:09.000]  In our case, there is an extra complication that you need to reconstruct the steps.
[02:33:09.000 --> 02:33:16.000]  So you need to have the capability to match the pixel with the strips.
[02:33:16.000 --> 02:33:30.000]  And since you have 120 columns in the strips,
[02:33:30.000 --> 02:33:37.000]  it is much easier if you can do 120 pixel 2.
[02:33:37.000 --> 02:33:49.000]  Simply because then you don't need to think, okay, I have to shift one strip by one because I need to match with the picture that is actually shifted.
[02:33:49.000 --> 02:34:02.000]  It's just a small trick in which they just duplicate everything they pretend that the pixel are actually 16 by 120.
[02:34:02.000 --> 02:34:15.000]  And then you just feed everything to the cluster mechanism that reconstruct the clusters that that point is completely agnostic about what is going on before just going to receive every time a two hits from the corner pixel and that's it.
[02:34:15.000 --> 02:34:19.000]  And then we'll assume that that is a pixel of cluster size too.
[02:34:19.000 --> 02:34:23.000]  And it will be much easier to match it with the strips.
[02:34:23.000 --> 02:34:34.000]  As you saw in practice actually the pixels are actually 118 on the side.
[02:34:34.000 --> 02:34:52.000]  Yes, exactly 118 on the sensor 118 and on the pixel are 120 but the two at the edge are fake are just duplicating whatever they see in the in the previous picture.
[02:34:52.000 --> 02:34:54.000]  I see. Okay, thank you.
[02:34:54.000 --> 02:34:56.000]  No problem.
[02:35:05.000 --> 02:35:18.000]  Okay, anything else that you want to chat about.
[02:35:18.000 --> 02:35:26.000]  Okay, then what I'm gonna do is that I'm gonna stop the recording.
[02:35:26.000 --> 02:35:29.000]  I'm sharing.
