# IT-DAQ setup and run

1. Folow the instructions [here](../general/required_install.md) to install all needed software packages (like `pugixml`, `boost`, `python`. etc ...)
2. Install and build the Ph2_ACF software, as described [here](../general/ph2acf_install.md)
3. `mkdir choose_a_name`
4. `cp settings/RD53Files/CMSIT_RD53{A,Bv1,Bv2}.txt choose_a_name` (if you are using an [optical readout](OpticalReadout.md) you need also: `cp settings/lpGBTFiles/CMSIT_LpGBT-v0(1).txt choose_a_name`)
5. `cp settings/CMSIT_RD53A/B.xml choose_a_name`
6. `cd choose_a_name`
7. Edit the file `CMSIT_RD53A/B.xml` in case you want to change some parameters needed for the calibrations or for configuring the chip
8. Run the command: `CMSITminiDAQ -f CMSIT_RD53A/B.xml -r` to reset the FC7 (just once)
9. Run the command: `CMSITminiDAQ -f CMSIT_RD53A/B.xml -c name_of_the_calibration` (or `CMSITminiDAQ --help` for help)

**N.B.:** to speed up the `IPbus` communication you can implement [this](https://ipbus.web.cern.ch/doc/user/html/performance.html) trick
