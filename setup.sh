#!/bin/bash
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

#########
# BOOST #
#########
export KERNELRELEASE=$(uname -r)
if [[ $KERNELRELEASE == *"el6"* ]]; then
    export BOOST_LIB=/opt/cactus/lib
    export BOOST_INCLUDE=/opt/cactus/include
elif [[ $KERNELRELEASE == *"el8"* ]]; then
    export BOOST_LIB=/opt/cactus/lib
    export BOOST_INCLUDE=/opt/cactus/include
elif [[ $KERNELRELEASE == "5."*"-generic" ]]; then
    export BOOST_INCLUDE=/usr/include
    export BOOST_LIB=/usr/lib/x86_64-linux-gnu
else
    export BOOST_INCLUDE=/usr/include
    export BOOST_LIB=/usr/lib64
fi

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

#######
# ZMQ #
#######
export ZMQ_HEADER_PATH=/usr/include/zmq.hpp

####################
# External Plugins #
####################
export EXTERNAL_TOOLS_BASE_DIR=${PH2ACF_BASE_DIR%/*}
# if in the docker container I want to do this .. need to figure out how to make sure that this is set-up correctly
export AMC13DIR=$CACTUSINCLUDE/amc13
export ANTENNADIR=$EXTERNAL_TOOLS_BASE_DIR/CMSPh2_AntennaDriver
export USBINSTDIR=$EXTERNAL_TOOLS_BASE_DIR/Ph2_USBInstDriver
export EUDAQDIR=$EXTERNAL_TOOLS_BASE_DIR/eudaq
export POWERSUPPLYDIR=$EXTERNAL_TOOLS_BASE_DIR/power_supply

# These are git references for the dependencies that are included via CMake ExternalProjects
export PH2_TCUSB_REF=e28b79c090b35d9fc686b5d56a4ebab0c36a1a22
export EUDAQ_REF=ac59b87fca12806d775e95df2d253c3bf96420ee

###########
# ANTENNA #
###########
export ANTENNALIB=$ANTENNADIR/lib

###########
# HMP4040 #
###########
export USBINSTLIB=$USBINSTDIR/lib

##########
# EUDAQ #
##########
export EUDAQLIB=$EUDAQDIR/lib

##########
# System #
##########
export PATH=$PH2ACF_BASE_DIR/bin:$PH2ACF_BASE_DIR/ProductionTools/LDACLINCalibration:$PATH
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
export TCUSBTcpServerFlag='-D__TCP_SERVER__'
export AntennaFlag='-D__ANTENNA__'
export UseRootFlag='-D__USE_ROOT__'
export MultiplexingFlag='-D__MULTIPLEXING__'
export EuDaqFlag='-D__EUDAQ__'

#####################
# Compilation flags #
#####################

# Stand-alone application, without data streaming
export CompileForHerd=false
export CompileForShep=false

# Stand-alone application, with data streaming
# export CompileForHerd=true
# export CompileForShep=true

# Herd application
# export CompileForHerd=true
# export CompileForShep=false

# Shep application
# export CompileForHerd=false
# export CompileForShep=true

# Compile with EUDAQ libraries
export CompileWithEUDAQ=false

# Compile with TC_USB library
export CompileWithTCUSB=false
export UseTCUSBforROH=false
export UseTCUSBTcpServer=false

# Clang-format command
if command -v clang-format &> /dev/null; then
 clang_command="clang-format"
else
  clang_command="/opt/rh/llvm-toolset-7.0/root/usr/bin/clang-format"
fi

alias formatAll="find ${PH2ACF_BASE_DIR} -iname *.h -o -iname *.cc | xargs ${clang_command} -i"

echo "=== DONE ==="
