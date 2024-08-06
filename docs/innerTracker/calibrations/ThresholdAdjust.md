# Threshold adjustment

**Scan command:** `thradj`

Moves the mean threshold of the pixel matrix to `TargetThreshold` defined in electrons.

!!! warning "You have to update the config manually in the xml file"
        - RD53B: `DAC_GDAC_{L,R,M}_LIN`
        - RD53A: `Vthreshold_LIN`

## Configuration parameters

| Name             | Typical value | Description |
| ---------------- | ------------- | ----------- |
| `nEvents`        | 100           |             |
| `nEvtsBurst`     | =`nEvents`    |             |
| `nTRIGxEvent`    | 10            |             |
| `INJtype`        | 1             |             |
| `VCalHStart`     | 100           |             |
| `VCalHStop`      | 600           |             |
| `TargetThreshold`|              |  Desired charge threshold in electrons |
| `ThrStart`       | 340           |             |
| `ThrStop`        | 440           |             |

## Expected output

### "Default"

![Default thresholds around 300 `DeltaVCal`](images/threshold_adjust_1.png){ width=400 }

### "Tuned to 2ke"

![Adjusted threshold with target 2000 electrons](images/threshold_adjust_2.png){ width=400 }
