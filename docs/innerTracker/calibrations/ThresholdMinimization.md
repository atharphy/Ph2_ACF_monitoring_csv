# Threshold minimization scan

**Scan command:** `thrmin`

|Untuned thresholds: `Vth = 400` | Threhold minimization: `Vth = 380` |
| ------------------------------ | ---------------------------------- |
|![Setup](images/thremin_1.png){width=300 } | ![Setup](images/thremin_2.png){width=300 } | 
|**Equalized thresholds: `Vth = 380`** | **Threhold minimization: `Vth = 357`** |
|![Setup](images/thremin_3.png){width=300 } | ![Setup](images/thremin_4.png){width=300 } |


Threshold minimisation algorithm is based on:

* trying to minimise the threshold until the average occupancy is close to the `TargetOcc`
* the algorithm also takes into account the percentage of masked pixels which should not exceed `MaxMaskedPixels` (the pixels are masked if they have an occupancy greater than `OccPerPixel`, as in the noise scan)

* Analog injection
* `1` bunch crossing
* `1e7` triggers
* `8` steps for the binary search (in this example)

## Configuration parameters

| Name             | Typical value | Description |
| ---------------- | ------------- | ----------- |
| `nEvents`        | 1e7           |             |
| `nEvtsBurst`     | 1e4           |             |
| `INJtype`        | 0             |             |
| `TargetOcc`      | 1e-6          |             |
| `MaxMaskedPixel` | 1             | percentage  |
| `ThrStart`       | 340           |             |
| `ThrStop`        | 440           |             |
| `nClkDelays`     | 10            |             |
