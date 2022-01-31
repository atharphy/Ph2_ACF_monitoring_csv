#!/bin/bash

##########
# CACTUS #
##########
export CACTUSROOT=/opt/cactus_Ph2_ACF
export CACTUSBIN=$CACTUSROOT/bin
export CACTUSLIB=$CACTUSROOT/lib
export CACTUSINCLUDE=$CACTUSROOT/include

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

###########
# Ph2_ACF #
###########
export PH2ACF_BASE_DIR=$(pwd)

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

################
# Compilations #
################

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
