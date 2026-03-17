# How to test all FMC readout lanes

These are the instructions on how to test all FMC’s readout lanes with a Single Chip Card:

1. Download the **quad module FW** from the [usual repository](https://gitlab.cern.ch/cmstkph2-IT/d19c-firmware/-/releases)
2. Copy these lines in the XML file:
```xml
<Register name="gtx_rx_polarity">
    <Register name="fmc_l12"> 4095 </Register>
    <Register name="fmc_l8">     0 </Register>
    <Register name="cmd_strobe"> 1 </Register>
</Register>
```
3. You should now be able to sequentially test the lanes with the following settings:
   * **miniDP 1 ➜ HybridID 0:**  
     `Lane = 0, OutputLanes = "1000"`
     `Lane = 1, OutputLanes = "0100"`
     `Lane = 2, OutputLanes = "0010"`
     `Lane = 3, OutputLanes = "0001"`
   * **miniDP 2 ➜ HybridID 1:**  
     `Lane = 0, OutputLanes = "1000"`
     `Lane = 1, OutputLanes = "0100"`
     `Lane = 2, OutputLanes = "0010"`
     `Lane = 3, OutputLanes = "0001"`
   * **miniDP 3 ➜ HybridID 2:**  
     `Lane = 0, OutputLanes = "1000"`
     `Lane = 1, OutputLanes = "0100"`
   * **miniDP 4 ➜ HybridID 2:**  
     `Lane = 2, OutputLanes = "1000"`
     `Lane = 3, OutputLanes = "0100"`
