# Noise occupancy

**Scan command:** `noise`

![Setup](images/noise_scan.png){ align=right width=600 }

The noise scan allows to detect noisy pixels.

* Analog (or Digiral) injection.
* 1 bunch crossing
* 1e7 triggers per pixel

The configuration for the noise scan will mask the pixels that have occupancy > 1e-6 computed with respect to the total number of triggers.

It is possible to make the scan run faster changing the `nTRIGxEvent`. For example given the two following configurations:

* `nEvents  = 1e7`, `nTRIGxEvent = 10`
* `nEvents  = 1e8`, `nTRIGxEvent = 1`

the first one runs faster even though they send the same amount of triggers.

## Configuration parameters

| Name             | Typical value | Notes       |
| ---------------- | ------------- | ----------- |
| `nEvents`        | 1e7           | Usually 1/`TargetOcc` * 10 |
| `nEvtsBurst`     | 1e4           |             |
| `nTRIGxEvent`    | 10            |             |
| `INJtype`       | 0             |             |
| `TargetOcc`      | 1e-6          |             |
| `OccPerPixel`    | 2e-5          |             |
