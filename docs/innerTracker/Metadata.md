# Metadata

The ROOT files produced by Ph2_ACF also have metadata attached to them:

* They are at the level of `Board`
* They can be read out as `TString`s

**Three values are saved:**

* Boolean `ITBeginOfCalib`: `true` = all requested lanes are "Up" (`true` = OK)
* Integer `ITEndOfCalib`:
    * number of re-transmitted packets (0 = OK)
    * number of lost packets (0 = OK)

![The aforementioned values in TBrowser](images/Metadata.png){width=700}

## Number of lost packets
Some data may get corrupted during a scan.
E.g., running [`pixelalive`](calibrations/PixelAlive.md), sending 100 injections per injection-pattern.
The 100 injections is a "packet" of data because they are readout from the FPGA memory banks at once.

* If there is data corruption, the DAQ tries to request a new packet (i.e. the number of packets re-transmission is increased by one)
* If the new packet is OK, then there is no corrupted data
* If the new packet is instead still corrupted then another packet is requested
* The DAQ tries 10 times
* If after 10 times the packet is still corrupted then the number of lost packets is increased by one

![Example of terminal output when the data is corrupted](images/LostPackets.png){width=600}