# Module testing

When dealing with modules you need to configure the `XML` file in a way to map the actual
hardware you have.
In particular you need to set the **chip ID** and the **lane** for every chip.
The `Number of active data` printed out on screen during the initialisation
process indicates which chips are sending the default signal pattern to the FC7. It doesn’t
necessary mean that you properly matched **chip ID** and **lane**, i.e. you might have set a
wrong **chip ID** and yet you might still get that all **lanes** are active.
The **chip ID** is only needed to address the chip in the programming process

## Lane mapping

If you don’t know the correspondence between **chip ID** and lane of your module you might
want to proceed in the following way:

1. Set the **chip ID** to 16 (8) for RD53B (RD53A), which is the `"broadcast"` address
2. Check if all **lanes** are active. If not, then you might need to adjust the internal voltage by
playing with the `VOLTAGE_TRIM` register
3. Once you get that all lanes are active then you can manually scan the **chip ID** from 0 to 15 (7) for RD53B (RD53A) in order to find out which is the right address (it is suggested to do so one chip at a time)

### Typical mapping for CROCv2 quad modules

| TBPX	                | TFPX                  |	TEPX                |
| --------------------- | --------------------- | --------------------- |
| chip ID 0 <-> lane 0  | chip ID 14 <-> lane 0	| chip ID 15 <-> lane 0 |
| chip ID 1 <-> lane 1	| chip ID 13 <-> lane 1	| chip ID 14 <-> lane 1 |
| chip ID 2 <-> lane 2	| chip ID 12 <-> lane 2	| chip ID 13 <-> lane 2 |
| chip ID 3 <-> lane 3	| chip ID 15 <-> lane 3	| chip ID 12 <-> lane 3 |

**Lane mapping for TBPX quad modules**

![TBPX](images/LaneMappingTBPX4.png){width=400}

**Lane mapping for TFPX quad modules**

![TFPX](images/LaneMappingTFPX4.png){width=400}

**Lane mapping for TEPX quad modules**

![TEPX](images/LaneMappingTEPX.png){width=400}

### Typical mapping for CROCv2 dual modules

| TBPX	                | TFPX                      |
| --------------------- | ------------------------- |
| chip ID 0 <-> lane 0  | chip ID 12 <-> lanes 3, 0 |
| chip ID 1 <-> lane 3	| chip ID 13 <-> lane 1	    |

### Lane mapping options for TBPX dual modules

TBPX dual modules can output data through 6 different GTX lanes simulatenously (3 per chip).
This makes many different lane mappings possible.
The ones reported in the table above are correct only when chip ID 0 is connected to hybrid ID 0 or 1.
Other possibilities are listed below:

| Hybrid(s)	| Chip ID | Lane | `outputLanes` |
| --------- | ------- | ---- | ----------- |
| 0,1       | 0       | 0    | 0100        |
| 0,1       | 0       | 1    | 0010        |
| 0,1       | 0       | 2    | 0001        |
| 0,1       | 1       | 3    | 0100        |
| 3         | 0       | 0    | 0100        |
| 3         | 0       | 1    | 0010        |
| 3         | 1       | 2    | 0010        |
| 3         | 1       | 3    | 0001        |

For hybrid IDs 2 and 4, the same mappings as hybrid ID 3 apply.
However, it is currently recommended not to use it as some modification to the FMC or the firmware would be required for optimal performance.

!!! note "Note that a double Display Port adapter is required to connect the modules to hybrid IDs 2 and above"
    ![DP adapter](images/DoubleDPadapter.png){width=400}

**Lane mapping for TBPX dual z+ modules**

![TBPX](images/LaneMappingTBPX2z+.png){width=400}

**Lane mapping for TBPX dual z- modules**

![TBPX](images/LaneMappingTBPX2z-.png){width=400}

**Lane mapping for TFPX dual modules**

![TFPX](images/LaneMappingTFPX2.png){width=400}

### Typical mapping for RD53A quad modules

| TBPX	               | TEPX                 |
| -------------------- | -------------------- |
| chip ID 4 <-> lane 0 | chip ID 0 <-> lane 0 |
| chip ID 5 <-> lane 1 | chip ID 1 <-> lane 1 |
| chip ID 6 <-> lane 2 | chip ID 2 <-> lane 2 |
| chip ID 7 <-> lane 3 | chip ID 3 <-> lane 3 |