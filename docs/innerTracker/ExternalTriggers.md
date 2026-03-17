# External triggers and clock

## DIO5 and TLU

The relevant settings in the XML file are given [here](ConfigFile.md#dio5-and-tlu-settings).

External triggers must be provided through a lemo cable to the DIO5 FMC board, input number 2 (TTL standard, 50 Ω impedance).

External clock must be provided through a lemo cable to the DIO5 FMC board, input number 5 (TTL standard, 50 Ω impedance).

`trigger_to_accept` used in [`physics`](calibrations/Physics.md) scan: total number of triggers to readout (0 = no limit).

If using `trigger_source = 4` ➜ terminate with 50 Ω: busy to TLU (DIO5 FMC board output number 3) and clk to TLU (DIO5 FMC output number 1) and use same lemo cables with same length.

![ExternalTriggers](images/ExternalTriggers.png){width=800}

## Triggering on the CROC HitOr

!!! info "This works only with SCCs as modules don't have the HitOr connected"

### Hardware

Connect the 2nd DP plug on the SCC (DP2/rightmost connector on Bonn/Zurich SCCs) to one of the first two (from the left) connectors on the lower row of the KSU FMC (1st=J5=HitOr_1, 2nd=J8=HitOr_2), as shown in the pictures below (the HitOr cable is shown in orange).
    
![HitOr Cable SCC](images/HitOR_SCC_colored.jpg){width=350}![HitOr Cable FMC](images/HitOR_FMC_colored.jpg){width=350}

### Configuration

1. **Set trigger source to HitOr and turn on the correct input DP on the KSU FMC**  
    Set in the `CMSIT_RD53B.xml` file

    ```xml
    <Register name="fast_cmd_reg_2">
            <Register name="trigger_source">   6 </Register> <!-- 6 = Hitor input -->
            <Register name="HitOr_enable_l12"> 1 </Register> <!-- 1 or 2 = turn on the first(HitOr_1, see above)
                                                                  and/or the second (HitOr_2) connector -->
    </Register>
    ```

   **warning:** BUG: Specifying the input in binary as suggested in the XML file does not work.

2. **Route the HitOr signals to the 2nd DP connector**  
    This is specified in `GP_LVDS_ROUTE_{0,1}`, which are compound registers (two times two 6-bit registers).
    The assignment table is in the [CROCv1 manual](https://cds.cern.ch/record/2665301) in Table 28 (Page 114):  HitOr[3] = 28 ... HitOr[0] = 31.
    So to turn on all HitOr's, you have to set the following in the chip configuration:

    ```xml
    GP_LVDS_ROUTE_0 = "1821" <!-- ((28<<6) + 29) -->
    GP_LVDS_ROUTE_1 = "1951" <!-- ((30<<6) + 31) -->
    ```
    Other combinations are also possible.
    A deeper explanation about the LVDS routing is given [further down in this page](ExternalTriggers.md#general-purpose-lvds-options).

3. **Turn on the HitOr per pixel and column**  
    * `CMSIT_RD53B.txt`: Set the `HITOR` bit for all relevant pixels to 1.
    * `CMSIT_RD53B.xml`: Set the `HITOR_MASK_{0..3}` to **0** for all core columns that you want to **activate**:
    ```xml
    HITOR_MASK_0 = "0"
    HITOR_MASK_1 = "0"
    HITOR_MASK_2 = "0"
    HITOR_MASK_3 = "0"
    ```

4. **Set the hit sampling mode to asynchronous**
    ```xml
    HIT_SAMPLE_MODE = "0"
    ```

5. **Find and adjust the latency (`TriggerConfig`)**  
    * A typical value is `TriggerConfig = "36"`

!!! info "The HitOr is by definition **asynchronous**."
    Noise masking/Physics should only done in the asynchronous mode (`HIT_SAMPLE_MODE = "0"`).

### General-Purpose LVDS Options

There are 4 general-purpose LVDS lanes through which the HitOr data can be sent. The HitOr information is itself transferred in 4 lanes. To direct the information from the 4 HitOr lanes into the 4 LVDS lanes, one can set `GP_LVDS_ROUTE_0` = 1821 and `GP_LVDS_ROUTE_1` = 1951. However, other numeric combinations also allow the HitOr information to be sent through the LVDS lanes.

`GP_LVDS_ROUTE_0` is a stack of two 6-bit numbers: `GP_LVDS(1)` and `GP_LVDS(0)`. Similarly, `GP_LVDS_ROUTE_1` consists of `GP_LVDS(3)` and `GP_LVDS(2)`. Each of the four `GP_LVDS(*)` (where `*` is `0`, `1`, `2`, or `3`) registers can take a value from 0 to 63 to select a signal source for each LVDS output lane. Numbers 28..31 correspond to HitOr lanes 3..0, and 32 corresponds to a HitOr combination from self-trigger. Thus, one can direct each HitOr lane separately to a different general-purpose LVDS output lane or send the combined value through a single lane.

If we want to send the HitOr lane no. 3 to `GP_LVDS(1)` and HitOr lane no. 2 to `GP_LVDS(0)`, we need to combine the two 6-bit numbers corresponding to 28 and 29 into a single 12-bit number. 28 in binary is `0b011100`, and 29 is `0b011101`. Combining them is `0b011100011101`, which is 1821 in decimal. Similarly, combining 30 (`0b011110`, corresponds to HitOr lane no. 1) and 31 (`0b011111`, corresponds to HitOr lane no. 0) gives 1951 (`0b011110011111`).

Another simple way to find the correct number to enter into `GP_LVDS_ROUTE_{0,1}` would be multiplying the first number by 64 ($2^6$) and adding the second number: `(28<<6)+29`=28$\times$64+29=1821, `(30<<6)+31`=30$\times$64+31=1951. The default value for `GP_LVDS_ROUTE_{0,1}` in Ph2_ACF is 1495, or a number 23 repeated twice. As explained in the [CROCv1 manual](https://cds.cern.ch/record/2665301), this mode corresponds to "Goes low when low power mode is enabled". This number should be used in case some of the HitOr lanes are not intended to be used or if one decides to use the combination of all HitOr lanes (32) in a single LVDS lane. In that case, one can set, e.g., `GP_LVDS_ROUTE_0`=1504 (23$\times$64+32) and `GP_LVDS_ROUTE_1`=1495 (23$\times$64+23).

A summary of this is given in the tables below.

|Selected signal  |`GP_LVDS(x)` value|
|-----------------|------------------|
|HitOr [0]        |31                |
|HitOr [1]        |30                |
|HitOr [2]        |29                |
|HitOr [3]        |28                |
|HitOr combination|32                |

|Register name    |Explanation                        | |
|-----------------|-----------------------------------|-|
|`GP_LVDS_ROUTE_0`|64$\times$`GP_LVDS(1)`+`GP_LVDS(0)`|![LVDS ROUTE 0](images/LVDS_ROUTE_0.png){width=150}|
|`GP_LVDS_ROUTE_1`|64$\times$`GP_LVDS(3)`+`GP_LVDS(2)`|![LVDS ROUTE 1](images/LVDS_ROUTE_1.png){width=150}|