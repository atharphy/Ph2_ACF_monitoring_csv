# Gain scan

**Scan command:** `gain`

![Setup](images/gain_scan.png){ width=30% }

This scan is measuring the ToT response of each pixel to a range of injected charges and calculates the calibration constants for \( q(\textrm{ToT}) = \textrm{gain} \cdot \textrm{ToT} + \textrm{offset} \;\; [q] = 1 e^- \).

## Configuration parameters

| Name             | Typical value | Description |
| ---------------- | ------------- | ----------- |
| `nEvents`        | 100           |             |
| `nEvtsBurst`     | =`nEvents`    |             |
| `nTRIGxEvent`    | 10            |             |
| `VCalHStart`     | 100           |             |
| `VCalHStop`      | 4000          | The maximum value is 4096. Use a large range to have enough lever for the fit.</br>Adjust according to the resulting `Gain` plot. |
| `VCalnsteps`     | 20            |             |
| `LatencyStart`   | 110           |             |
| `LatencyStop`    | 150           |             |
| `DoOnlyNGroups`  | 1             |             |
| `SEL_CAL_RANGE`  |               | RD53B/C chip register! |

## Typical output

!!! note "Todo"

    - add typical values for RD53A/B

### "Gain scan"

![Setup](images/gain_scan.png){ align=center width=600 }

### "Gain 1D"

![Setup](images/gain_scan_gain1D.png){ align=center width=600 }

### "Offset 1D"

![Setup](images/gain_scan_offset1D.png){ align=center width=600 }
