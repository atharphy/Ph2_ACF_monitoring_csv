# Data Read Back Optimisation

**Scan command:** `datarbopt`

## Configuration Parameters

|Name | Typical Value | Description |
|--|--|--|
|`TAP0Start`|0|0 = `BE-FE`, 1 = `BE-LPGBT`, 2 = `LPGBT-FE`|
|`TAP0Stop`|1023| |
|`TAP1Start`|0| |
|`TAP1Stop|511| |
|`TAP2Start|0||
|`TAP2Stop|511||
|`framesORtime`| |0= time in [s] or number of frames|

The number of steps is `10`or less depending whether `TAPxStop - TAPxStart <10`

## Expected Output

![Setup](images/DataReadBackOptimisation.png){width=600}

## Failure Modes

Below `100` there is a high error rate
![Setup](images/DataReadBackOptimisation_Error.png){width=600}
