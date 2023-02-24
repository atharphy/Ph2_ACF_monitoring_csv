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


## =x= Middleware for the Inner-Tracker (IT) system  =x=
```diff
+ Last change made to this section: 21/11/2022
```

### Suggested software and firmware versions:
- Software git branch / tag : `Dev` / `v4-10`
- Firmware tag: `4.5`

### Important webpages:
- Mattermost forum: [`cms-it-daq`](https://mattermost.web.cern.ch/cms-it-daq/)
- DAQ web page: https://cms-tracker-daq.web.cern.ch/cms-tracker-daq/
- Detailed description of the various calibrations: https://cernbox.cern.ch/index.php/s/O07UiVaX3wKiZ78
- Program to generate enable/injection patterns for x-talk studies: `pyUtilsIT/ManipulateITchipMask.py`
- Mask converter from `Ph2_ACF` to `Alki's` code: `pyUtilsIT/ConvertPh2ACFMask2Alkis.py`

**FC7 setup:**
1. Install `wireshark` in order to figure out which is the MAC address of your FC7 board (`sudo yum install wireshark`, then run `sudo tshark -i ethernet_card`, where `ethernet_card` is the name of the ethernet card of your PC to which the FC7 is connected to)
2. In `/etc/ethers` put `mac_address fc7.board.1` and in `/etc/hosts` put `192.168.1.80 fc7.board.1`
3. Restart the network: `sudo /etc/init.d/network restart`
4. Install the rarpd daemon (version for CENTOS6 should work just fine even for CENTOS7): `sudo yum install rarp_file_name.rpm` from [here](https://archives.fedoraproject.org/pub/archive/epel/6/x86_64/Packages/r/rarpd-ss981107-42.el6.x86_64.rpm)
5. Start the rarpd daemon: `sudo systemctl start rarpd` or `sudo rarp -e -A` (to start rarpd automatically after bootstrap: `sudo systemctl enable rarpd`)

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
3. `cp settings/RD53Files/CMSIT_RD53A/B.txt choose_a_name`
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

The program `CMSITminiDAQ` is the portal for all calibrations and for data taking.
Through `CMSITminiDAQ`, and with the right command line option, you can run the following scans/ calibrations/ operation mode:
```
1. Latency scan
2. PixelAlive
3. Noise scan
4. SCurve scan
5. Gain scan
6. Threshold equalization
7. Gain optimization
8. Threshold minimization
9. Threshold adjustment
10. Injection delay scan
11. Clock delay scan
12. Bit Error Rate test
13. Data read back optimisation
14. Chip internal voltage tuning
15. Generic DAC-DAC scan
16. Physics
```

It might be useful to create one `CMSIT.xml` file for each "set" of calibrations, for instance `noise`, `gain`, and "the rest".
### =x= End of Inner-Tracker section =x=


### Setup
Firmware for the FC7 can be found in /firmware. Since the "old" FMC flavour is deprecated, only new FMCs (both connectors on the same side) are supported.
You'll need Xilinx Vivado and a Xilinx Platform Cable USB II (http://uk.farnell.com/xilinx/hw-usb-ii-g/platform-cable-configuration-prog/dp/1649384).
For more information on the firmware, please check the doc directory of https://gitlab.cern.ch/cms_tk_ph2/d19c-firmware


### Gitlab CI setup for Developers (required to submit merge requests!!!)
1. Make sure you are subscribed to the cms-tracker-phase2-DAQ e-group

2. Add predefined variables

    i. from your fork go to `Ph2_ACF > settings > CI/CD`

    ii. expand the `Variables` section

    iii. click the `Add variable` button
        - add key: USER_NAME and value: <your CERN user name>

    iv. click the `Add variable` button
        - select the flag `Mask variable`
        - add key: USER_PASS and value: <your CERN password encoded to base64>
          e.g encode "thisword": printf "thisword" | base64

3. Enable shared Runners (if not enabled)
    i. from `settings > CI/CD` expand the `Runners` section
    ii. click the `Allow shared Runners` button

### Setup on CentOs7
1. Install devtoolset 10
```bash
sudo yum install -y centos-release-scl-rh
sudo yum install -y devtoolset-10
```

2. On CC7 you also need to install boost v1.53 headers (default on this system) and pugixml as they don't ship with uHAL any more:

```bash
sudo yum install -y boost-devel pugixml-devel json-devel
```

2. Install uHAL. SW tested with uHAL version up to 2.7.1

    Follow instructions from
    https://ipbus.web.cern.ch/ipbus/doc/user/html/software/install/yum.html

3. Install CERN ROOT
```bash
sudo yum install -y root
sudo yum install -y root-net-http root-net-httpsniff  root-graf3d-gl root-physics root-montecarlo-eg root-graf3d-eve root-geom libusb-devel xorg-x11-xauth.x86_64
```

5. Install CMAKE3 > 3.0:

```bash
sudo yum install -y cmake3
```

6. Install python3

```bash
sudo yum install -y python3 python3-devel
```

7. Install protobuf:

   Follow instructions from
   https://gitlab.cern.ch/cms_tk_ph2/MessageUtils/-/blob/master/README.md

8. Install pybind11 (if installed in the same directoory when you plan to install the Ph2_ACF, the setup.sh will point to the correct location)
```bash
wget https://github.com/pybind/pybind11/archive/refs/tags/v2.9.2.tar.gz
tar zxvf v2.9.2.tar.gz
```


### Run in docker container
    Docker container are provided to facilitate users and developers in setting up the framework.

All docker containers can be found here:
https://gitlab.cern.ch/cms_tk_ph2/docker_exploration/container_registry

Do run using one of the container, use the command:
```bash
docker run --rm -ti -v $PWD:$PWD -w $PWD <image>
```

Suggested images are:
  -  For users (comes with Ph2_ACF of Dev branch installed): `gitlab-registry.cern.ch/cms_tk_ph2/docker_exploration/cmstkph2_user_c7:latest`
  -  For developers (no Ph2_ACF, just environment and libraries): `gitlab-registry.cern.ch/cms_tk_ph2/docker_exploration/cmstkph2_udaq_c7:latest`

  Specific tags can be pulled substituting `latest` with `ph2_acf_<Ph2_ACF tag>` (i.e. `ph2_acf_v4-05`)


### clang-format (required to submit merge requests!!!)
1. install 7.0 llvm toolset:

```bash
yum install centos-release-scl
yum install llvm-toolset-7.0
```

2. if you already sourced the environment, you should be able to run the command to format the Ph2_ACF (to be done before each merge request!!!):

```bash
formatAll
```


### The Ph2_ACF software
Follow these instructions to install and compile the libraries (provided you installed the latest version of gcc, µHal,  mentioned above):

1. Clone the GitHub repo and run cmake

```bash
git clone --recurse-submodules https://gitlab.cern.ch/cms_tk_ph2/Ph2_ACF.git # N.B. to syncrhonize only the submodule: `git submodule sync; git submodule update --init --recursive --remote`
cd Ph2_ACF
source setup.sh
mkdir build
cd build
cmake .. # add -D CMAKE_BUILD_TYPE=Debug if you plan to use gdb for debugging, if you yum-instanlled `cmake3`, you might need to tall it `cmake3 ..`
```

2. Do a `make -jN` in the build/ directory or alternatively do `make -C build/ -jN` in the Ph2_ACF root directory.

3. Don't forget to `source setup.sh` to set all the environment variables correctly.

4. Launch

```bash
systemtest --help
```
to test the parsing of the HWDescription.xml file.

5. Launch

```bash
datatest --help
```
to test if you can correctly read data

6. Launch

```bash
calibrate --help
```
to calibrate a hybrid,

```bash
hybridtest --help
```
to test a hybird's I2C registers and input channel connectivity

```bash
cmtest --help
```
to run the CM noise study

```bash
pulseshape --help
```
to measure the analog pulseshape of the cbc

```bash
configure --help
```
to apply a configuration to the CBCs

7. Launch

```bash
commission --help
```
to do latency & threshold scans

8. Launch

```bash
fpgaconfig --help
```
to upload a new FW image to the GLIB

9. Launch

```bash
miniDAQ --help
```
to save binary data from the GLIB to file

10. Launch

```bash
miniDQM --help
```
to run the DQM code from the June '15 beamtest


### Setup on CentOs8 (deprecated)
The following procedure will install (in order):
1. the `boost` and `pugixml` libraries
2. the `cactus` libraries for ipBus (using [these instructions](https://ipbus.web.cern.ch/doc/user/html/software/install/yum.html))
3. `root` with all its needed libraries
4. `cmake`, tools for clang, including `clang-format` and `git-extras`

# Libraries needed by Ph2_ACF
```bash
sudo yum install -y boost-devel pugixml-devel json-devel
```

# uHAL libraries (cactus)
```bash
sudo curl https://ipbus.web.cern.ch/doc/user/html/_downloads/ipbus-sw.centos8.x86_64.repo \
  -o /etc/yum.repos.d/ipbus-sw.repo
sudo yum-config-manager --enable powertools
sudo yum clean all
sudo yum groupinstall uhal
```

# ROOT
```bash
sudo yum install -y root root-net-http root-net-httpsniff root-graf3d-gl root-physics \
  root-montecarlo-eg root-graf3d-eve root-geom libusb-devel xorg-x11-xauth.x86_64
```

# Build tools and some nice git extras
```bash
sudo yum install -y cmake3
sudo yum install -y clang-tools-extra
sudo yum install -y git-extras
```

Install devtoolset 10
```bash
sudo yum makecache --refresh
sudo yum -y install gcc-toolset-10-gcc
```

Install python3

```bash
sudo yum install -y python3 python3-devel
```

Install protobuf:

Follow instructions to install protobuf from (Just install section is needed)
https://gitlab.cern.ch/cms_tk_ph2/MessageUtils/-/blob/master/README.md

Install `pybind11` (if installed in the same directoory when you plan to install the Ph2_ACF, the setup.sh will point to the correct location)

```bash
wget https://github.com/pybind/pybind11/archive/refs/tags/v2.9.2.tar.gz
tar zxvf v2.9.2.tar.gz
```

### Setup on RHEL 9.1 or AlmaLinux 9.1
The following procedure will install (in order):
0. complete the `cern` installation
1. the `boost` and `pugixml` libraries
2. `erlang` (using [these instructions](https://www.rabbitmq.com/install-rpm.html))
3. the `cactus` libraries for ipBus (using [these instructions](https://ipbus.web.cern.ch/doc/user/html/software/install/yum.html))
4. `root` with all its needed libraries
5. `cmake`, tools for clang, including `clang-format` and `git-extras`

#### Complete the CERN installation
Make sure that the CERN installation is complete by running
```bash
sudo dnf --repofrompath=cern9el,http://linuxsoft.cern.ch/internal/repos/cern9el-stable/x86_64/os --repo=cern9el install cern-release
```

#### Libraries needed by Ph2_ACF
```bash
sudo yum install -y boost-devel pugixml-devel json-devel
```

#### Erlang (needed by uHAL)
This installs `erlang` from a specific rpm. It would be nice if in the future, the correct version
of `erlang` could be made available via the CERN repository
```bash
wget https://github.com/rabbitmq/erlang-rpm/releases/download/v25.1.2/erlang-25.1.2-1.el9.x86_64.rpm
sudo yum -y install erlang-25.1.2-1.el9.x86_64.rpm
```

#### uHAL libraries (cactus)
```bash
sudo curl https://ipbus.web.cern.ch/doc/user/html/_downloads/ipbus-sw.el9.repo -o /etc/yum.repos.d/ipbus-sw.repo
sudo yum clean all
sudo yum groupinstall -y uhal controlhub
```

#### ROOT
```bash
sudo yum install -y root root-net-http root-net-httpsniff root-graf3d-gl root-physics \
  root-montecarlo-eg root-graf3d-eve root-geom libusb-devel xorg-x11-xauth.x86_64
```

#### Build tools and some nice git extras
```bash
sudo yum install -y cmake3 clang-tools-extra git-extras
```

**devtoolset 12**
```bash
sudo yum makecache --refresh
sudo yum -y install gcc-toolset-12-gcc
```

**python3**
```bash
sudo yum install -y python3 python3-devel
```

**protobuf**
Follow instructions to install protobuf from (Just install section is needed)
https://gitlab.cern.ch/cms_tk_ph2/MessageUtils/-/blob/master/README.md

**pybind11** (if installed in the same directoory when you plan to install the Ph2_ACF, the setup.sh will point to the correct location)
```bash
wget https://github.com/pybind/pybind11/archive/refs/tags/v2.9.2.tar.gz
tar zxvf v2.9.2.tar.gz
```

### Nota Bene
When you write a register in the Glib or the Cbc, the corresponding map of the HWDescription object in memory is also updated, so that you always have an exact replica of the HW Status in the memory.

Register values are:
  - 8-bit unsigend integers for the CBCs that should be edited in hex notation, i.e. '0xFF'
  - 32-bit unsigned integers for the GLIB: decimal values

For debugging purpose, you can activate DEV_FLAG in the sources or in the Makefile and also activate the uHal log in RegManager.cc.


### External clock and trigger
Please see the D19C FW  [documentation](https://gitlab.cern.ch/cms_tk_ph2/d19c-firmware/blob/master/doc/Middleware_Short_Guide.md) for instructions on how to use external clock and trigger with the various FMCs (DIO5 and CBC3 FMC)


### Known issues
uHAL exceptions and UDP timeouts when reading larger packet sizes from the GLIB board: this can happen for some users (cause not yet identified) but can be circumvented by changing the line

`ipbusudp-2.0://192.168.000.175:50001`

in the connections.xml file to

`chtcp-2.0://localhost:10203?target=192.168.000.175:50001`

and then launching the CACTUS control hub by the command:

`/opt/cactus/bin/controlhub_start`

This uses TCP protocol instead of UDP which accounts for packet loss but decreases the performance.


### Support, suggestions?
For any support/suggestions, mail to fabio.raveraSPAMNOT@cern.ch, mauro.dinardoSPAMNOT@cern.ch


### Firmware repository for OT tracker
https://udtc-ot-firmware.web.cern.ch/
