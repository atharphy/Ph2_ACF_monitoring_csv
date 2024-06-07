# IT middleware setup and use

## Recommended software and firmware versions

- Software git branch / tag : `Dev` / `v5-01`
- Firmware tag: `v4-09`

## Important webpages and information

- Mattermost forum: [`cms-it-daq`](https://mattermost.web.cern.ch/cms-it-daq/)
- DAQ web page: <https://cms-tracker-daq.web.cern.ch/cms-tracker-daq/>
- Detailed description of the various calibrations: <https://cernbox.cern.ch/s/uSezc8ErG7F4tJ0>
- ROC tuning sequence: <https://www.overleaf.com/read/ffpkqnjjjscd>
- Latest IT-DAQ school: <https://indico.cern.ch/event/1374747/>
- Program to generate enable/injection patterns for x-talk studies: `pyUtilsIT/ManipulateITchipMask.py`
- Mask converter from `Ph2_ACF` to `Alki's` code: `pyUtilsIT/ConvertPh2ACFMask2Alkis.py`

## FC7 setup

1. Install `wireshark` in order to figure out which is the MAC address of your FC7 board (`sudo yum install wireshark`, then run `sudo tshark -i ethernet_card`, where `ethernet_card` is the name of the ethernet card of your PC to which the FC7 is connected to)
2. In `/etc/ethers` put `mac_address fc7-1` and in `/etc/hosts` put `192.168.1.80 fc7-1` (increase these numbers for additional FC7 boards)
3. Restart the network: `sudo /etc/init.d/network restart`
4. Install the rarpd daemon: `sudo yum install rarp_file_name.rpm`
5. Start the rarpd daemon: `sudo systemctl start rarpd` (to start rarpd automatically after bootstrap: `sudo systemctl enable rarpd`)

More details on the hardware needed to setup the system can be found [here](https://indico.cern.ch/event/1014295/contributions/4257334/attachments/2200045/3728440/Low-resoution%202021_02%20DAQ%20School.pdf)

## Firmware setup

1. Check whether the DIP switches on FC7 board are setup for the use of a microSD card (`out-in-in-in-out-in-in-in`)
2. Insert a microSD card in the PC and run `/sbin/fdisk -l` to understand to which dev it's attached to (`/dev/sd_card_name`)
3. Upload a golden firmware on the microSD card (read FC7 manual or run `dd if=sdgoldenimage.img of=/dev/sd_card_name bs=512`)
4. Download the proper IT firmware version from [here](https://gitlab.cern.ch/cmstkph2-IT/d19c-firmware/-/releases)
5. Plug the microSD card in the FC7
6. From Ph2_ACF use the command `fpgaconfig` to upload the proper IT firmware (see instructions: `IT-DAQ setup and run` before running this command)

**N.B.:** a golden firmware is any stable firmware either from IT or OT, and it's needed just to initialize the `IPbus` communication at bootstrap (in order to create and image of the microSD card you can use the command: `dd if=/dev/sd_card_name conv=sync,noerror bs=128K | gzip -c > sdgoldenimage.img.gz`) <br />
A golden firmware can be downloaded from the [cms-tracker-daq webpage](https://cms-tracker-daq.web.cern.ch/cms-tracker-daq/Downloads/sdgoldenimage.img) <br />
A detailed manual about the firmware can be found [here](https://gitlab.cern.ch/cmstkph2-IT/d19c-firmware/blob/master/doc/IT-uDTC_fw_manual_v1.0.pdf)

## IT-DAQ setup and run

1. Folow instructions below to install all needed software packages (like `pugixml`, `boost`, `python`. etc ...)
2. `mkdir choose_a_name`
3. `cp settings/RD53Files/CMSIT_RD53A/B.txt choose_a_name` (if you are using an optical readout you need also: `cp settings/lpGBTFiles/CMSIT_LpGBT-v0(1).txt choose_a_name`)
4. `cp settings/CMSIT_RD53A/B.xml choose_a_name`
5. `cd choose_a_name`
6. Edit the file `CMSIT_RD53A/B.xml` in case you want to change some parameters needed for the calibrations or for configuring the chip
7. Run the command: `CMSITminiDAQ -f CMSIT_RD53A/B.xml -r` to reset the FC7 (just once)
8. Run the command: `CMSITminiDAQ -f CMSIT_RD53A/B.xml -c name_of_the_calibration` (or `CMSITminiDAQ --help` for help)

**N.B.:** to speed up the `IPbus` communication you can implement [this](https://ipbus.web.cern.ch/doc/user/html/performance.html) trick

## Basic list of commands for the `fpgaconfig` program

 (run from the `choose_a_name` directory)

- Run the command: `fpgaconfig -c CMSIT_RD53A/B.xml -l` to check which firmware is on the microSD card
- Run the command: `fpgaconfig -c CMSIT_RD53A/B.xml -f firmware_file_name_on_the_PC -i firmware_file_name_on_the_microSD` to upload a new firmware to the microSD card
- Run the command: `fpgaconfig -c CMSIT_RD53A/B.xml -i firmware_file_name_on_the_microSD` to load a new firmware from the microSD card to the FPGA
- Run the command: `fpgaconfig --help` for help
