# Gain Optimization

**Scan command:** `gainopt`

!!! note "TODO: Configuration to be checked"
!!! note "TODO: Needs explanation"

* `20` points along `VCal`
* `10` steps for binary search (in this example)

## Configuration Parameters

|Name           | Typical Value | Description |
|---------------|---------------|-------------|
|`nEvents`      |100            |             |
|`nEvtsBurst`   | = `nEvents`   |             |
|`nTRIGxEvent`  | 10            |             |
|`INJtype`      |1              |             |
|`TargetCharge` |20000          |             |
|`KrumCurrStart`|0              |             |
|`KrumCurrStop` |127            |             |

## Expected Output

### "ToT vs DelVCal"

![Setup](images/GainOptimization.png){ width=600}

### "Gain 1D"

![Setup](images/GainOptimization_Gain1D.png){ width=600}

### "Gain map"

![Setup](images/GainOptimization_Gain2D.png){ width=600}

### "Offset 1D"

![Setup](images/GainOptimization_Intercept1D.png){ width=600}

### "Offset map"

![Setup](images/GainOptimization_Intercept2D.png){ width=600}

## Failure modes
