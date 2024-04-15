# CMS Ph2 ACF (Acquisition & Control Framework)


### Contains:
- A middleware API layer, implemented in C++, which wraps the firmware calls and handshakes into abstracted functions
- A C++ object-based library describing the system components (CBCs, RD53, Hybrids, Boards) and their properties (values, status)


###  A short guide to write the GoldenImage to the SD card
1. Connect the SD card
2. Download the golden firmware from the [cms-tracker-daq webpage](https://cms-tracker-daq.web.cern.ch/cms-tracker-daq/Downloads/sdgoldenimage.img)
3. `sudo fdisk -l` - find the name of the SD card (for example, /dev/mmcblk0)
4. `sudo chmod 744 /dev/sd_card_name` - to be able to play with it
5. Go to the folder were you saved the sdgoldenimage.img file
6. `dd if=sdgoldenimage.img of=/dev/sd_card_name bs=512` - to write the image to the SD card.
If the SD card is partitioned (formatted), pay attention to write on the block device (e.g. `/dev/mmcblk0`) and not inside the partition (e.g. `/dev/mmcblk0p1`)
7. Once the previous command is done, you can list the SD card: `./imgtool /dev/sd_card_name list` - there should be a GoldenImage.bin, with 20MB block size
8. Insert the SD card into the FC7

Alternatively, instead of the `dd` command above, to only copy the needed bytes you can do:
```bash
imageName=sdgoldenimage.img
dd if=$imageName bs=512 iflag=count_bytes of=somefile_or_device count=$(ls -s --block-size=1 $imageName | awk '{print $1}')
```

If you installed the command `pv` (`sudo yum install -y pv`), then the best way is the following (replacing `/dev/mmcblk0` with your target device):
```bash
pv sdgoldenimage.img | sudo dd of=/dev/mmcblk0
```


## 
### =x= Middleware for the Inner-Tracker (IT) system  =x=
```diff
+ Last change made to this section: 27/02/2024
```

#### Suggested software and firmware versions:
- Software git branch / tag : `Dev` / `v4-22`
- Firmware tag: `v4-08`

#### Important webpages and information:
- Mattermost forum: [`cms-it-daq`](https://mattermost.web.cern.ch/cms-it-daq/)
- DAQ web page: https://cms-tracker-daq.web.cern.ch/cms-tracker-daq/
- Detailed description of the various calibrations: https://cernbox.cern.ch/s/uSezc8ErG7F4tJ0
- ROC tuning sequence: https://www.overleaf.com/read/ffpkqnjjjscd
- Latest IT-DAQ school: https://indico.cern.ch/event/1374747/
- Program to generate enable/injection patterns for x-talk studies: `pyUtilsIT/ManipulateITchipMask.py`
- Mask converter from `Ph2_ACF` to `Alki's` code: `pyUtilsIT/ConvertPh2ACFMask2Alkis.py`

**FC7 setup:**
1. Install `wireshark` in order to figure out which is the MAC address of your FC7 board (`sudo yum install wireshark`, then run `sudo tshark -i ethernet_card`, where `ethernet_card` is the name of the ethernet card of your PC to which the FC7 is connected to)
2. In `/etc/ethers` put `mac_address fc7-1` and in `/etc/hosts` put `192.168.1.80 fc7-1` (increase these numbers for additional FC7 boards)
3. Restart the network: `sudo /etc/init.d/network restart`
4. Install the rarpd daemon: `sudo yum install rarp_file_name.rpm`
5. Start the rarpd daemon: `sudo systemctl start rarpd` (to start rarpd automatically after bootstrap: `sudo systemctl enable rarpd`)

More details on the hardware needed to setup the system can be found [here](https://indico.cern.ch/event/1014295/contributions/4257334/attachments/2200045/3728440/Low-resoution%202021_02%20DAQ%20School.pdf)

**Firmware setup:**
1. Check whether the DIP switches on FC7 board are setup for the use of a microSD card (`out-in-in-in-out-in-in-in`)
2. Insert a microSD card in the PC and run `/sbin/fdisk -l` to understand to which dev it's attached to (`/dev/sd_card_name`)
3. Upload a golden firmware on the microSD card (read FC7 manual or run `dd if=sdgoldenimage.img of=/dev/sd_card_name bs=512`)
4. Download the proper IT firmware version from [here](https://gitlab.cern.ch/cmstkph2-IT/d19c-firmware/-/releases)
5. Plug the microSD card in the FC7
6. From Ph2_ACF use the command `fpgaconfig` to upload the proper IT firmware (see instructions: `IT-DAQ setup and run` before running this command)

**N.B.:** a golden firmware is any stable firmware either from IT or OT, and it's needed just to initialize the `IPbus` communication at bootstrap (in order to create and image of the microSD card you can use the command: `dd if=/dev/sd_card_name conv=sync,noerror bs=128K | gzip -c > sdgoldenimage.img.gz`) <br />
A golden firmware can be downloaded from the [cms-tracker-daq webpage](https://cms-tracker-daq.web.cern.ch/cms-tracker-daq/Downloads/sdgoldenimage.img) <br />
A detailed manual about the firmware can be found [here](https://gitlab.cern.ch/cmstkph2-IT/d19c-firmware/blob/master/doc/IT-uDTC_fw_manual_v1.0.pdf)

**IT-DAQ setup and run:**
1. Folow instructions below to install all needed software packages (like `pugixml`, `boost`, `python`. etc ...)
2. `mkdir choose_a_name`
3. `cp settings/RD53Files/CMSIT_RD53A/B.txt choose_a_name` (if you are using an optical readout you need also: `cp settings/lpGBTFiles/CMSIT_LpGBT-v0(1).txt choose_a_name`)
4. `cp settings/CMSIT_RD53A/B.xml choose_a_name`
5. `cd choose_a_name`
6. Edit the file `CMSIT_RD53A/B.xml` in case you want to change some parameters needed for the calibrations or for configuring the chip
7. Run the command: `CMSITminiDAQ -f CMSIT_RD53A/B.xml -r` to reset the FC7 (just once)
8. Run the command: `CMSITminiDAQ -f CMSIT_RD53A/B.xml -c name_of_the_calibration` (or `CMSITminiDAQ --help` for help)

**N.B.:** to speed up the `IPbus` communication you can implement [this](https://ipbus.web.cern.ch/doc/user/html/performance.html) trick

**Basic list of commands for the `fpgaconfig` program (run from the `choose_a_name` directory):**
- Run the command: `fpgaconfig -c CMSIT_RD53A/B.xml -l` to check which firmware is on the microSD card
- Run the command: `fpgaconfig -c CMSIT_RD53A/B.xml -f firmware_file_name_on_the_PC -i firmware_file_name_on_the_microSD` to upload a new firmware to the microSD card
- Run the command: `fpgaconfig -c CMSIT_RD53A/B.xml -i firmware_file_name_on_the_microSD` to load a new firmware from the microSD card to the FPGA
- Run the command: `fpgaconfig --help` for help
### =x= End of Inner-Tracker section =x=


##
### The `Ph2_ACF` software

Installation of the software is documented at <https://ph2acf.docs.cern.ch/general/ph2acf_install/>


##
### Run in docker container - Deprecated: update of docker registry to alma9 is required
Docker container are provided to facilitate users and developers in setting up the framework.
All docker containers can be found here: `https://gitlab.cern.ch/cms_tk_ph2/docker_exploration/container_registry`

Do run using one of the container, use the command:
```bash
docker run --rm -ti -v $PWD:$PWD -w $PWD <image>
```

Suggested images are:
-  For users (comes with Ph2_ACF of Dev branch installed): `gitlab-registry.cern.ch/cms_tk_ph2/docker_exploration/cmstkph2_user_c7:latest`
-  For developers (no Ph2_ACF, just environment and libraries): `gitlab-registry.cern.ch/cms_tk_ph2/docker_exploration/cmstkph2_udaq_c7:latest`

Specific tags can be pulled substituting `latest` with `ph2_acf_<Ph2_ACF tag>` (i.e. `ph2_acf_v4-05`)


##
### Gitlab CI setup for Developers (required to submit merge requests)
Enable shared Runners (if not enabled)
- From `settings > CI/CD` expand the `Runners` section
- Click the `Allow shared Runners` button


##
### Setup on RHEL 9 or AlmaLinux 9

See <https://ph2acf.docs.cern.ch/general/required_install/> for instructions on installing required libraries and tools on RHEL/AlmaLinux 9.

##
### To pull large files
Install `git lfs`
```bash
sudo yum install git-lfs
git lfs install
```
Go to your main `Ph2_ACF` folder and run
```bash
git config lfs.https://gitlab.cern.ch/cms_tk_ph2/Ph2_ACF.git/info/lfs.locksverify true # or your username instead of cms_tk_ph2
```

For example `lpGBT` calibration data for a different source repository (e.g. `cmsinnertracker`)
```bash
git remote add cms_tk_ph2 https://gitlab.cern.ch/cms_tk_ph2/Ph2_ACF.git
git fetch cms_tk_ph2
git lfs fetch cms_tk_ph2
git lfs pull cms_tk_ph2
```


##
### Known issues
uHAL exceptions and UDP timeouts when reading larger packet sizes from the FC7 board: this can happen for some users (cause not yet identified) but can be circumvented by changing the line

`ipbusudp-2.0://192.168.000.175:50001`

in the connections.xml file to

`chtcp-2.0://localhost:10203?target=192.168.000.175:50001`

and then launching the CACTUS control hub by the command:

`/opt/cactus/bin/controlhub_start`

This uses TCP protocol instead of UDP which accounts for packet loss but decreases the performance


##
### Support, suggestions
For any support/suggestions, send an email to fabio.raveraSPAMNOT@cern.ch, mauro.dinardoSPAMNOT@cern.ch


##
### Firmware repository for OT tracker
`https://udtc-ot-firmware.web.cern.ch/`
