# Basic list of commands for the `fpgaconfig` program

Make sure you have the FC7 communication [properly set up](FC7setup.md).
Make sure the FC7 has the golden firmware image loaded as described [here](FWsetup.md).

Then, run the following from the `choose_a_name` directory:

- Download the proper IT firmware version from [here](https://gitlab.cern.ch/cmstkph2-IT/d19c-firmware/-/releases)
- Unzip the downloaded firmware image: `tar -xvzf firmware_file_name_on_the_PC.tar.gz`
- Run the command: `fpgaconfig -c CMSIT_RD53A/B.xml -l` to check which firmware is already available on the microSD card
- Run the command: `fpgaconfig -c CMSIT_RD53A/B.xml -f firmware_file_name_on_the_PC.bit -i firmware_file_name_on_the_microSD` to upload a new firmware to the microSD card
- Run the command: `fpgaconfig -c CMSIT_RD53A/B.xml -i firmware_file_name_on_the_microSD` to load a new firmware from the microSD card to the FPGA
- Run the command: `fpgaconfig --help` for help
