# Module testing

When dealing with modules you need to configure the `XML` file in a way to map the actual hardware you have.
In particular you need to set the **hybrid ID** for every module, and **chip ID** with **lane** for every chip.

### Hybrid ID configuration

Every FC7 board has two FMC connectors: L8 and L12 (telling the total number of GTX readout lanes the connector has).
Every FMC card has four mini Display Port (miniDP) connectors to connect the modules.
If the FMC card is connected to the L12 connector of the FC7 board, then the first two miniDP ports will have 4 GTX lanes each, while the last two miniDP ports will have 2 GTX lanes each.
If the FMC card is connected to the L8 connector of the FC7 board, then all miniDP ports will have 2 GTX lanes each.
A typical modules uses 4 GTX lanes for readout, therefore the maximum amount of modules that can be connected to a single FC7 is 5.
In order to do this, 3 of the 5 modules must use double miniDP adapters.

Each module connected to the FC7 board is identified by a **Hybrid ID**.
**Hybrid ID** depends on which miniDP port(s) on the FMC card are used to connect the module and must be set in the `XML` configuration file, as shown in the image below.
Note that a double Display Port adapter is required to connect the modules to hybrid IDs 2 and above.

![DP adapter](images/DoubleDPadapter.png){width=400}

## Lane mapping

!!! note "What do we mean by **lane**?"
    In Ph2_ACF, **lane** specifies the index of the chip within a module (0 to 3) - and specifically for electrical link, it is associated to the GTX receiver of the FPGA that this chip is connected to.
    4 GTX receivers of the FPGA are associated to 4 chips, and this is hardcoded in the firmware.
    For the optical link, **lane** is not associated with a specific receiver of the FPGA (mapping of chips to e-links is specified by RxGroups), so it just specifies the index of the chip within a module.

When running any calibration, the `Number of active data lanes` printed out on screen during the initialization process indicates which chips are sending the default signal pattern to the FC7.
It doesn’t necessarily mean that you properly matched **chip ID** and **lane**.
I.e. you might have set a wrong **chip ID** and yet you might still get that all **lanes** are active.
The **chip ID** is only needed to address the chip in the programming process.

If you don’t know the correspondence between **chip ID** and lane of your module you might want to proceed in the following way:

1. Set the **chip ID** to 16 (8) for RD53C (RD53A), which is the `"broadcast"` address
2. Check if all **lanes** are active. If not, then you might need to adjust the internal voltage by
playing with the `VOLTAGE_TRIM` register
3. Once you get that all lanes are active then you can manually scan the **chip ID** from 0 to 15 (7) for RD53C (RD53A) in order to find out which is the right address (it is suggested to do so one chip at a time)

### Typical mapping for CROCv2 quad modules

| TBPX	             | TFPX                | TEPX                |
| ------------------ | ------------------- | ------------------- |
| chip ID 0 ↔ lane 0 | chip ID 14 ↔ lane 0 | chip ID 15 ↔ lane 0 |
| chip ID 1 ↔ lane 1 | chip ID 13 ↔ lane 1 | chip ID 14 ↔ lane 1 |
| chip ID 2 ↔ lane 2 | chip ID 12 ↔ lane 2 | chip ID 13 ↔ lane 2 |
| chip ID 3 ↔ lane 3 | chip ID 15 ↔ lane 3 | chip ID 12 ↔ lane 3 |

**Lane mapping for TBPX quad modules**

![TBPX](images/LaneMappingTBPX4.png){width=400}

**Lane mapping for TFPX quad modules**

![TFPX](images/LaneMappingTFPX4.png){width=400}

**Lane mapping for TEPX quad modules**

![TEPX](images/LaneMappingTEPX.png){width=400}

### Typical mapping for CROCv2 dual modules

| TBPX               | TFPX                    |
| ------------------ | ----------------------- |
| chip ID 0 ↔ lane 0 | chip ID 12 ↔ lanes 3, 0 |
| chip ID 1 ↔ lane 3 | chip ID 13 ↔ lane 1     |

### Special lane mapping options for TBPX dual modules

TBPX dual modules can output data through 6 different GTX lanes simulatenously (3 per chip), meaning each chip is connected to a different hybrid in this case.
This makes many different lane mappings possible.
The ones reported in the table above are correct only when chip ID 0 is connected to hybrid ID 0 or 1.
Other possibilities are listed below:

| Hybrid(s)	| Chip ID | Lane | `outputLanes` |
| --------- | ------- | ---- | ------------- |
| 0+1       | 0       | 0    | 0100          |
| 0+1       | 0       | 1    | 0010          |
| 0+1       | 0       | 2    | 0001          |
| 0+1       | 1       | 3    | 0100          |
| 3         | 0       | 0    | 0100          |
| 3         | 0       | 1    | 0010          |
| 3         | 1       | 2    | 0010          |
| 3         | 1       | 3    | 0001          |

For hybrid IDs 2 and 4, the same mappings as hybrid ID 3 apply.
However, it is currently recommended not to use it as some modification to the FMC or the firmware would be required for optimal performance.
In any of these cases, a double-DP adapter should be used to connect the module to the FMC.

**Lane mapping for TBPX dual z+ modules**

![TBPX](images/LaneMappingTBPX2z+.png){width=400}

**Lane mapping for TBPX dual z- modules**

![TBPX](images/LaneMappingTBPX2z-.png){width=400}

**Lane mapping for TFPX dual modules**

![TFPX](images/LaneMappingTFPX2.png){width=400}

### Typical mapping for RD53A quad modules

| TBPX	             | TEPX               |
| ------------------ | ------------------ |
| chip ID 4 ↔ lane 0 | chip ID 0 ↔ lane 0 |
| chip ID 5 ↔ lane 1 | chip ID 1 ↔ lane 1 |
| chip ID 6 ↔ lane 2 | chip ID 2 ↔ lane 2 |
| chip ID 7 ↔ lane 3 | chip ID 3 ↔ lane 3 |