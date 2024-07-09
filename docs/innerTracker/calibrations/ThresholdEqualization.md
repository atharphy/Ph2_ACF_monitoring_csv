# Threshold equalization

**Scan command:** `threqu`

<!-- ![Setup](images/threqu1.png){ align=right width=600 } -->

The threshold equalization should return a more narrow threshold distribution and scurve.


## Configuration parameters

| Name             | Typical value | Description |
| ---------------- | ------------- | ----------- |
| `nEvents`        | 100           |             |
| `nEvtsBurst`     | =`nEvents`    |             |
| `nTRIGxEvent`    | 10            |             |
| `INJtype`        | 1             |             |
| `VCalHStart`     | 100           |             |
| `VCalHStop`      | 600           |             |
| `VCalHnsteps`    | 50            |             |
| `TDACGainStart`  |               |  Starting value of `LDAC_LIN`           |
| `TDACGainStop`   |               |  Stopping value of `LDAC_LIN`           |
| `TDACGaiNSteps`  |               |  Number of steps fot the `LDAC_LIN` scan           |
| `DoNSteps`       | 0             |  `0`: use at high `GDAC`, `1 or 2`: use at low `GDAC`  |
| `ResetTDAC`      | -1 or 16      |  `16` for starting from scratch, `-1` for `DoNSteps` != 0  |

![Setup](images/threqu2.png){ align=right width=400 }

If `ResetTDAC = -1` then as starting values of TDAC are used those in the config file, the TDAC are reset to the value.In case the resulting TDAC distribution is highly asymmetric, then you might want to re-run the scan by resetting the TDAC, or when the thr. vs TDAC becomes non linear as after irradiation.

The threshold equalization can be run for 

* high global threshold (`DoNSteps = 0`) to equalize the pixels around the same threshold value
* low global threshold (`DoNSteps = 1`) to improve the width of the threshold 1d distribution

The plots below show the improvement of the standard deviation in the threshold distribution after re-running the threshold equalization with `DoNSteps = 1` at low threshold.

![Setup](images/threqu6.png){ width=600 }
