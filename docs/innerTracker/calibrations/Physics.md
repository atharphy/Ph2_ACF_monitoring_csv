# Physics

**Scan command:** `physics`

The “calibration” called physics simply tests a new mode of operation which is meant to be used in testbeams (data are sent periodically to a DQM process for online data integrity verification).

In case of a need to integrate the Ph2_ACF in an existing framework’ state machine to be used in a testbeam, we strongly recommend to follow the same structure which is present in CMSITminiDAQ related to option “-s” (have a look at the code)

Available plots are:

* Occupancy 2D distribution over the pixel matrix
* Pulse-height 2D distribution and also the 1D distribution (eventually mimicking a Landau)
* Trigger 1D distribution
* Bunch-Crossing 1D distribution
* Readout Error distribution for debugging

Though the “physics” mode is meant to be used within an external state machine that gives the commands to the calibration (start, stop, pause, etc …) the CMSITminiDAQ can run a “physics”, mainly for code testing purposes What you need to know when you run:

`CMSITminiDAQ -f CMSIT.xml -c physics -t time_in_seconds`

* number of triggers set by the parameter trigger_to_accept in the XML file
* in order to mimic the external state machine the CMSITminiDAQ sends a “start” and then after time_in_seconds a “stop”, the process terminates after all trigger_to_accept have been collected (if it takes less than time_in_seconds, then it will wait for time_in_seconds, otherwise it will wait until all triggers have been collected)

In the `CMSIT.xml` config. file the user can specify the output directory of binary data.
