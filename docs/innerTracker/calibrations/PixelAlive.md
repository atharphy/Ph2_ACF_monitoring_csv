# PixelAlive

**Scan command:** `pixelalive`

* analog or digital injection

## Configuration Parameters

| Name          | Typical Value | Description |
|---------------|---------------|-------------|
|`nEvents`      |100            |             |
|`nTRIGxEvent`  |10             |             |
|`INJtype`      |1              |             |
|`OccPerPixel`  |2e-5           |             |
|`UnstuckPixels`|0              | if `0` then the scan will mask all pixels which have an occupancy smaller than `OccPerPixel`, instead if `1` then the scan will set `TDAC`to `0`(increased threshold) for those which have an occupancy smaller than `OccPerPixel`|

## Expected Output

### "Occupancy"

![Occupancy](images/PixelAlive.png){ width=400}

### "ToT"

![ToT](images/PixelAlive_ToT.png){ width=400}

### "TrigId"

!!! warning "As of v4-13, this plot does not work with CROC"
![TrigId](images/PixelAlive_TrigID.png){ width=400}

### "BCID"

!!! warning "As of v4-13, this plot does not work with CROC"
![BCID](images/PixelAlive_BCID.png){ width=400}

<br>

## Failure Modes
