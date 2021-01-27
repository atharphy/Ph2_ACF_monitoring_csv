#!/bin/bash

##########
# CACTUS #
##########
export CACTUSBIN=/opt/cactus/bin
export CACTUSLIB=/opt/cactus/lib
export CACTUSINCLUDE=/opt/cactus/include

#########
# BOOST #
#########
export KERNELRELEASE=$(uname -r)
if [[ $KERNELRELEASE == *"el6"* ]]; then
    export BOOST_LIB=/opt/cactus/lib
    export BOOST_INCLUDE=/opt/cactus/include
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

###########
# Ph2_ACF #
###########
export PH2ACF_BASE_DIR=$(pwd)

####################
# External Plugins #
####################
export AMC13DIR=$CACTUSINCLUDE/amc13
export ANTENNADIR=$PH2ACF_BASE_DIR/../CMSPh2_AntennaDriver
export USBINSTDIR=$PH2ACF_BASE_DIR/../Ph2_USBInstDriver
export EUDAQDIR=$PH2ACF_BASE_DIR/../eudaq
export POWERSUPPLYDIR=$PH2ACF_BASE_DIR/../power_supply

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
export PATH=$PH2ACF_BASE_DIR/bin:$PATH
export LD_LIBRARY_PATH=$USBINSTLIB:$ANTENNALIB:$PH2ACF_BASE_DIR/RootWeb/lib:$CACTUSLIB:$PH2ACF_BASE_DIR/lib:$EUDAQLIB:/opt/rh/llvm-toolset-7.0/root/usr/lib64:$LD_LIBRARY_PATH

#########
# Flags #
#########
export HttpFlag='-D__HTTP__'
export ZmqFlag='-D__ZMQ__'
export USBINSTFlag='-D__USBINST__'
export Amc13Flag='-D__AMC13__'
export TCUSBFlag='-D__TCUSB__'
export AntennaFlag='-D__ANTENNA__'
export UseRootFlag='-D__USE_ROOT__'
export MultiplexingFlag='-D__MULTIPLEXING__'
export EuDaqFlag='-D__EUDAQ__'
export PowerSupplyFlag='-D__POWERSUPPLY__'

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

# Clang-format command
alias formatAll="find ${PH2ACF_BASE_DIR} -iname *.h -o -iname *.cc | xargs /opt/rh/llvm-toolset-7.0/root/usr/bin/clang-format -i"

echo "=== DONE ==="
