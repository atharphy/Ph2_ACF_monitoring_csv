# Clock delay scan

**Scan command:** `clkdelay`

!!! note "`CAL_EDGE`"
    To be verified, if 1.5ns per `CAL_EDGE_FINE_DELAY` step is still correct for RD53B

<!--
No image for this scan yet
![Setup](/media/injection_delay_scan.png){ align=right width=600 }
-->

!!! warning "This calibration is supposed to run with external triggers together with real particles"

This scan is very much like the injection delay scan, but in this case the scan is performed on the fine clock delay (`CAL_EDGE_FINE_DELAY` ~1.5 ns step) looking for the best value
Preliminary settings in `XML` file:

* Set, in XML file, number of triggers to sent “in association to one event" to 1
* Set, in XML file, VCal to the particle Most Probable Value

The calibration performs the following sequence of steps:

* Set clock delay to zero
* Scan latency register looking for best value (e.g. found best latency = N)
* Scan clock delay in latency N-1 and N
* Search for beast clock delay value

* It needs real particles together with an external trigger

* `1` bunch crossing
* `100` triggers per pixel
* `50` latency steps
* `32` steps in injection delay
* Only Linear FE / Fully configurable from config file

## Configuration parameters

| Name             | Typical value | Description |
| ---------------- | ------------- | ----------- |
| `nEvents`        | 100           |             |
| `nEvtsBurst`     | =`nEvents`    |             |
| `nTRIGxEvent`    | 10            |             |
| `INJtype`        | 1             |             |
| `DoOnlyNGroups`  | 1             |             |
