# Injection delay scan

**Scan command:** `injdelay`

!!! note "Update of .txt files"
    In `v4-13` and older versions of `Ph2_ACF`, the received optimal values from this scan are not yet written into the .txt file describing the ROC configurations. This for instance also prevents `dirigent` from being able to update these values to the .xml file. This should be fixed in newer versions

!!! note "`CAL_EDGE` step size"
    To be checked: `CAL_EDGE_FINE_DELAY` in RD53B should be of the size of about 0.7 ns.

![Setup](images/injection_delay_scan.png){ align=right width=600 }

This calibration scans the fine delay (~1.5 ns step) of the charge injection looking for the best value
Preliminary settings in `XML`:

* Set, in `XML` file, number of triggers to sent "in association to one event" to 1
* Set, in `XML` file, `VCal` the particle Most Probable Value (MPV)

The calibration performs the following sequence of steps:

* Set injection delay to zero
* Scan latency register looking for best value (e.g. found best latency = N)
* Scan injection delay in latency `N-1` and `N`
* Search for best injection delay value

* Register to fill in the result from latency scan:
    * RD53B: `TriggerConfig`
    * RD53A: `LATENCY_CONFIG`
* Register to fill in the result from Injection delay:
    * `CAL_EDGE_FINE_DELAY`

After running the injection delay scan you should run [`SCurve`](SCurve.md) to measure the in-time threshold.

* Analog (or Digital) injection
* **RD53A:** Only Linear FE / Fully configurable from config file

## Configuration parameters

| Name             | Typical value | Description |
| ---------------- | ------------- | ----------- |
| `nEvents`        | 100           |             |
| `nEvtsBurst`     | =`nEvents`    |             |
| `nTRIGxEvent`    | 10            |             |
| `INJtype`        | 1             |             |
| `LatencyStart`   | 110           |             |
| `LatencyStop`    | 150           |             |
| `DoOnlyNGroups`  | 1             |             |
