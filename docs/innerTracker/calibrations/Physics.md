# Physics

## Purpose

The purpose of the Physics scan is actual physics data-taking during test beams, tests with radioactive sources, etc. It can mimic the operation of the final experimental system, receiving control, trigger, and clock commands from external equipment, and sending out periodic data to a Data Quality Monitoring (DQM) process. For testing purposes, it is also possible to run Physics without external equipment, with start and stop commands being issued by the Ph2_ACF software, and sending triggers periodically from FSM, or making use of the chip self-trigger.

## Method

In principle, the Physics scan is one of the simplest procedures in Ph2_ACF. It simply collects the data the chip sends out when it gets triggered and fills the histograms. If there is no external machine to send the start and stop signals, it can be managed by Ph2_ACF by setting the running time and the number of triggers one wants to accept before stopping.

From the collected data, the Physics scan produces occupancy and ToT histograms. There is also a possibility to output the binary data received from the chip by setting `SaveBinaryData`=1. The binary data file can then be converted into a ROOT file to analyze the data event by event. In that file, the pixel row, column, and [ToT](TermExplanations.md#time-over-threshold-tot) is provided for each hit, along with the event number, Trigger ID, and bunch crossing ID (if enabled in the settings file).

**Scan command:** `physics`

* Normally no injection, real particle hits are used

## Typical output

Below we proviee an example of the histograms produced by the Physics scan using a radioactive source placed on top of an SCC with HitOr self-trigger enabled.

### Occupancy

Picture below shows the 2D [occupancy](TermExplanations.md#pixel-occupancy) map, where a "blob" of hits produced by particles from a radioactive source can be observed.

![Occupancy](images/physics/Occupancy.png){width=400}

### ToT

Pictures below show a 1D distribution and a 2D map of total collected signal strength ([ToT](TermExplanations.md#time-over-threshold-tot)) produced by the hits during the data-taking.

![ToT 1D](images/physics/ToT1D.png){width=350}![ToT 2D](images/physics/ToT2D.png){width=350}

## Operation

Many different modes of operation are possible for the Physics scan, depending on the desired measurement equipment in use. Here, we will only outline the relevant settings for running a Physics scan with no external control. The command to run the Physics scan is as follows:
```
CMSITminiDAQ -f your_hw_description_file.xml -c physics -t time_in_seconds
```
where `time_in_seconds` is the time in seconds for which the calibration will run. If the running time is not given after the `-t` flag, the calibration will run until `triggers_to_accept` triggers get sent.

Table below contains the most general relevant parameters for the Physics scan together with a description of what each parameter does.

|Name                |Typical value|Description|
|--------------------|-------------|-----------|
|`triggers_to_accept`|10000        |Number of triggers to accept before stopping the scan|
|`nTRIGxEvent`       |10           |Number of subsequent triggers sent for each triggering event|
|`SaveBinaryData`    |1            |Whether to save all the binary event information in a separate file|
|`DataOutputDir`     |             |Location to save the binary files|
|`-t`                |60           |Terminal command flag to set the running time in seconds|

There are other relevant parameters that depend on the setup and the type of measurement. Two important examples are given in corresponding tables below.

### HitOr self-trigger

You can read more about running with self-trigger [here](../SelfTrigger.md).

Table below contains the relevant parameters for the Physics scan **using HitOr self-trigger functionality**.

|Name                    |Typical value|Description|
|------------------------|-------------|-----------|
|`SelfTriggerEn`         | 1           |Whether to enable the self-trigger|
|`SelfTriggerDelay`      | 30          |Delay applied to the HitOr pulse (in bunch crossings)|
|`SelfTriggerMultiplier` | 10          |Similar to `nTRIGxEvent`, a number of triggers sent for each HitOr|
|`SelfTriggerDeadTime`   | 0           |How many bunch crossings to wait after triggering before accepting new HitOr triggers|
|`TriggerConfig`         | 42          |Trigger latency (in bunch crossings), should be $\approx$`SelfTriggerDelay`+12|
|`EnOutputDataChipId`    | SCC$\rightarrow$0, Module$\rightarrow$1|Whether to output the chip ID number in the Aurora 64-bit block|
|`HitOrPatternLUT`       | 0xFFFE      |Each bit represents a unique combination of the four HitOr lanes, 0xFFFE = OR of all combinations|
|`HIT_SAMPLE_MODE`       | 0           |0 – asynchronous, 1 – synchronous sampling mode|
|`HITOR_MASK_0`          | 0           |16-bit number to **disable** core columns 15:0|
|`HITOR_MASK_1`          | 0           |16-bit number to **disable** core columns 31:16|
|`HITOR_MASK_2`          | 0           |16-bit number to **disable** core columns 47:32|
|`HITOR_MASK_3`          | 0           |6-bit number to **disable** core columns 53:48|
|`trigger_source`        | 2           |Choosing the trigger source (2 – an OR of self-trigger and FSM)|
|`tp_fsm_trigger_en`     | 0           |Enable (1) or disable (0) triggering from FSM (in `fast_cmd_reg_2`)|
|`self_trigger_en`       | 1           |Whether to enable the self-trigger (in `Aurora_block`)|

### External HitOr trigger (from FMC)

!!! warning This mode works only on Single-Chip Cards (SCCs).

You can read more about running with external HitOr [here](../ExternalTriggers.md#triggering-on-the-croc-HitOr).

Table below contains the relevant parameters for the Physics scan **using external HitOr trigger**. 

|Name                |Typical value|Description|
|--------------------|-------------|-----------|
|`SelfTriggerEn`     |0            |Whether to enable the self-trigger|
|`TriggerConfig`     |39           |Trigger latency (in bunch crossings)|
|`GP_LVDS_ROUTE_0`   |1821         |Information to direct through GP LVDS lanes 1:0|
|`GP_LVDS_ROUTE_1`   |1951         |Information to direct through GP LVDS lanes 3:2|
|`EnOutputDataChipId`|1            |Whether to output the chip ID number in the Aurora 64-bit block|
|`HitOrPatternLUT`   |0xFFFE       |Each bit represents a unique combination of the four HitOr lanes, 0xFFFE = OR of all combinations|
|`HIT_SAMPLE_MODE`   |0            |0 – asynchronous, 1 – synchronous sampling mode|
|`HITOR_MASK_0`      |0            |16-bit number to **disable** core columns 15:0|
|`HITOR_MASK_1`      |0            |16-bit number to **disable** core columns 31:16|
|`HITOR_MASK_2`      |0            |16-bit number to **disable** core columns 47:32|
|`HITOR_MASK_3`      |0            |6-bit number to **disable** core columns 53:48|
|`trigger_source`    |6            |Choosing the trigger source (6 – HitOr)|
|`HitOr_enable_l12`  |0            |Set the miniDP slot used for HitOr (0b0001 = leftmost slot)|