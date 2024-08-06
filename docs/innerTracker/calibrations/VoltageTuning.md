# Chip internal voltage tuning

**Scan command:** `voltagetuning`

## RD53A

Although it is not strictly needed for a first test, in principle you need to tune:

| Name      | Target value | Description |
| --------- | ------------ | ----------- |
| $I_{ref}$ | $4 \mu A$ | through external jumpers and measuring the voltage drop across a resistor (typically R45, 10kOhm)|
    | $V_{ref}$ | $0.9 V$   | through the register `MONITOR_CONFIG_BG` in the `XML`

## RD53B

Although it is not strictly needed for a first test, in principle you need to tune:

| Name      | Target value | Description |
| --------- | ------------ | ----------- |
| $I_{ref}$ | $4 \mu A$ | through external jumpers and measuring the voltage $V_{offs}$ to be $0.5 V$|
| $V_{ref}$ | $0.9 V$   | to have the right conversion factor for `VCal` to electrons |

Also, the following two registers have to be trimmed such that the voltage on the read-out chips is the correct one:

| Name              | Target RD53A | Target RD53B |
| ----------------- | ------------ | ------------ |
| `VOLTAGE_TRIM_ANA`| 1.3 V        | 1.2 V        |
| `VOLTAGE_TRIM_DIG`| 1.3 V        | 1.2 V        |

## Configuration parameters

| Name             | Typical value | Description |
| ---------------- | ------------- | ----------- |
| `VDDDTrimTarget` | RD53B: 1.2, RD53A: 1.3|     |
| `VDDATrimTarget` | RD53B: 1.2, RD53A: 1.3|     |
| `VDDDTrimTolerance` | 0.01       |             |
| `VDDATrimTolerance` | 0.01       |             |

## Expected output

![Setup](images/voltage_tuning.png)
