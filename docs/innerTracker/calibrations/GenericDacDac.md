# DAC-DAC scan

**Scan command:** `gendacdac`

![Setup](images/dacdac_1.png){ width=600 }

The calibration scans two registers, they can be either firmware or frontend chip registers or both. The user can specify the name of the registers, the starting and ending values, and the step size.

Also returns the optimum value for both chosen registers where the received occupancy is the highest

In default settings scans:

* `VCAL_HIGH` from 250 to 500 with step 50
* `user.ctrl_regs.fast_cmd_reg_5.delay_after_inject_pulse` from 25 to 50 with step 1

## Configuration parameters

| Name             | Typical value | Description |
| ---------------- | ------------- | ----------- |
| `RegNameDAC1`    | `user.ctrl_regs.fast_cmd_reg_5.delay_after_inject_pulse`|             |
| `StartValueDAC1` |               |             |
| `StopValueDAC1`  |               |             |
| `StepDAC1`       | 1             |             |
| `RegNameDAC1`    | `VCAL_HIGH`   |             |
| `StartValueDAC1` |               |             |
| `StopValueDAC1`  |               |             |
| `StepDAC1`       | 20            |             |

## Tornado plot

![Tornado plot](images/dacdac_2.png){ width=600 }

The generic DAC-DAC scan also allows to produce
the “tornado plot”:

* Run [`latency`](LatencyScan.md) ➜ gives proper latency to start with
* `CAL_EDGE_FINE_DELAY` from 0 to 61 with step 1
* `VCAL_HIGH` from 300 to 1300 with step 20
Returns the optimal value as the highest efficiency with lowest injection delay and `VCAl`
