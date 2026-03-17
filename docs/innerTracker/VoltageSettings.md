# Current and voltage settings

Although it’s not strictly needed for a first test, in principle you need to tune the current and voltage of the readout chips to the following values:

## RD53A

* Analog voltage to **1.3 V** through the register `VOLTAGE_TRIM_ANA` in XML file
* Digital voltage to **1.3 V** through the register `VOLTAGE_TRIM_DIG` in XML file
* $I_{ref}$ to **4 μA** through external jumpers and measuring the voltage drop across a
resistors (typically **R45** , 10 kΩ)
* $V_{ref}$ to **0.9 V** through the register `MONITOR_CONFIG_BG` in `XML` file

## CROC

* Analog voltage to **1.2 V** through the register `VOLTAGE_TRIM_ANA` in `XML` file
* Digital voltage to **1.2 V** through the register `VOLTAGE_TRIM_DIG` in `XML` file
* $I_{ref}$ to **4 μA** through external jumpers by measuring the voltage $V_{offs}$ to be **0.5 V**
* Measure $V_{ref}$ in order to have the right `VCal` to electron conversion factor parameter
