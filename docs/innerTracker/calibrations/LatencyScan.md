# Latency scan

**Scan command:** `latency`

Finds the optimal latency for scans and measurements.
If used with TLU or external trigger source, make sure to have a relatively high hit occupancy in order to locate the maximum.

!!! warning "You have to update the config manually in the xml file"
	* RD53B: `TriggerConfig`
	* RD53A: `LATENCY_CONFIG`

## Configuration parameters

| Name             | Typical value | Description |
| ---------------- | ------------- | ----------- |
| `nEvents`        | 100           |             |
| `nEvtsBurst`     | =`nEvents`    |             |
| `nTRIGxEvent`    | 10            |             |
| `INJtype`        | 1             |             |
| `LatencyStart`   | 110           |             |
| `LatencyStop`    | 150           |             |
| `DoOnlyNGroups` | 1             |             |

## Expected output

### "With injections"

![Setup](images/latency_scan.png){ width=600 }
