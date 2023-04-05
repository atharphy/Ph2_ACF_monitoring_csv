#!/bin/bash

###################################
# Enable devtools-10 for C++ > 14 #
###################################
[ -f /etc/centos-release ] && majorRelease=$(cat /etc/centos-release | tr -dc '0-9.'|cut -d \. -f1)
[ -f /etc/redhat-release ] && majorRelease=$(cat /etc/redhat-release | tr -dc '0-9.'|cut -d \. -f1)

if [[ $majorRelease == "7" ]]; then
  source scl_source enable devtoolset-10 || true # This might cause a nonzero exit code in the CI for some reason, so let's ignore it
elif [[ $majorRelease == "8" ]]; then
  source scl_source enable gcc-toolset-10
elif [[ $majorRelease == "9" ]]; then
  source scl_source enable gcc-toolset-12
else
  echo OS Release not supported
fi

###########
# Ph2_ACF #
###########
export PH2ACF_BASE_DIR=$(pwd)

##########
# CACTUS #
##########
export CACTUSROOT=/opt/cactus
export CACTUSBIN=$CACTUSROOT/bin
export CACTUSLIB=$CACTUSROOT/lib
export CACTUSINCLUDE=$CACTUSROOT/include

##########
# PYTHON #
##########
alias PythonController.py="python3 ${PH2ACF_BASE_DIR}/pythonUtils/PythonController.py"
alias fpgaconfig.py="python3 ${PH2ACF_BASE_DIR}/pythonUtils/fpgaconfig.py"

########
# ROOT #
########
THISROOTSH=${ROOTSYS}/bin/thisroot.sh
[ ! -f ${THISROOTSH} ] || source ${THISROOTSH}
unset THISROOTSH

if ! command -v root &> /dev/null; then
  printf "%s\n" ">> ERROR -- CERN ROOT is not available; please install it before using Ph2_ACF (see README)"
  return 1
fi

####################
# External Plugins #
####################
export EXTERNAL_TOOLS_BASE_DIR=${PH2ACF_BASE_DIR%/*}
export AMC13DIR=$CACTUSINCLUDE/amc13
export POWERSUPPLYDIR=$EXTERNAL_TOOLS_BASE_DIR/power_supply

# These are git references for the dependencies that are included via CMake ExternalProjects
export PH2_TCUSB_REF=889b673e9d9582dab64cfeb60990800970ee47af
export EUDAQ_REF=ac59b87fca12806d775e95df2d253c3bf96420ee
export PYBIND11_REF=v2.9.2

#######
# ZMQ #
#######
export ZMQ_HEADER_PATH=/usr/include/zmq.hpp

###########
# ANTENNA #
###########
export ANTENNADIR=$EXTERNAL_TOOLS_BASE_DIR/CMSPh2_AntennaDriver
export ANTENNALIB=$ANTENNADIR/lib

###########
# HMP4040 #
###########
export USBINSTDIR=$EXTERNAL_TOOLS_BASE_DIR/Ph2_USBInstDriver
export USBINSTLIB=$USBINSTDIR/lib

#########
# EUDAQ #
#########
export EUDAQLIB=$EUDAQDIR/lib

############
# Pybind11 #
############
#export PYBIND11=$PH2ACF_BASE_DIR/../pybind11-2.9.2/
#export PYBIND11INCLUDE=$PYBIND11/include
export PYTHONINCLUDE=/usr/include/python3.6m/

##########
# System #
##########
export PATH=$PH2ACF_BASE_DIR/bin:$PH2ACF_BASE_DIR/ProductionToolsIT/LDACLINCalibration:$PATH
export LD_LIBRARY_PATH=$USBINSTLIB:$ANTENNALIB:$PH2ACF_BASE_DIR/RootWeb/lib:$CACTUSLIB:$PH2ACF_BASE_DIR/lib:$EUDAQLIB:/opt/rh/llvm-toolset-7.0/root/usr/lib64:$LD_LIBRARY_PATH

#########
# Flags #
#########
export HttpFlag='-D__HTTP__'
export ZmqFlag='-D__ZMQ__'
export USBINSTFlag='-D__USBINST__'
export Amc13Flag='-D__AMC13__'
export TCUSBFlag='-D__TCUSB__'
export TCUSBforROHFlag='-D__ROH_USB__'
export TCUSBforSEHFlag='-D__SEH_USB__'
export AntennaFlag='-D__ANTENNA__'
export UseRootFlag='-D__USE_ROOT__'
export MultiplexingFlag='-D__MULTIPLEXING__'
export EuDaqFlag='-D__EUDAQ__'

#####################
# Compilation flags #
#####################

# C++ standard
if [[ $majorRelease == "9" ]]; then
  export STDCXX="17"
else
  export STDCXX="14"
fi

# Stand-alone application, without data streaming
export CompileForHerd=false
export CompileForShep=false

# Stand-alone application, with data streaming
# export CompileForHerd=true
# export CompileForShep=true

####################
# Herd application #
####################
# export CompileForHerd=true
# export CompileForShep=false

####################
# Shep application #
####################
# export CompileForHerd=false
# export CompileForShep=true

################################
# Compile with EUDAQ libraries #
################################
export CompileWithEUDAQ=false

###############################
# Compile with TC_USB library #
###############################
export CompileWithTCUSB=false

########################################
# OTHybridTester for either ROH or SEH #
########################################
export UseTCUSBforROH=false
export UseTCUSBforSEH=false


########################
# Clang-format command #
########################
if command -v clang-format &> /dev/null; then
 clang_command="clang-format"
else
  clang_command="/opt/rh/llvm-toolset-7.0/root/usr/bin/clang-format"
fi

alias formatAll="find ${PH2ACF_BASE_DIR} -type f \\( -name \"*.cc\" -o -name \"*.h\" \\) ! -path \"${PH2ACF_BASE_DIR}/MessageUtils/*\" ! -path \"${PH2ACF_BASE_DIR}/*/_deps/*\" | xargs ${clang_command} -i"

if [[ $1 == "ci" ]]; then
    export CompileForHerd=false
    export CompileForShep=false
    export CompileWithEUDAQ=false
    export CompileWithTCUSB=false
    export UseTCUSBforROH=false
    export UseTCUSBforSEH=false
fi

echo "=== DONE: you can now run cmake ==="
