if(NOT DEFINED ENV{OTSDAQ_CMSOUTERTRACKER_DIR}) 

MESSAGE(STATUS " ")
MESSAGE(STATUS "    ${BoldYellow}========================================================================================================${Reset}")
MESSAGE(STATUS "    ${BoldYellow}MIDDLEWARE${Reset} [stand-alone/middleware]: [${BoldCyan}Ph2_ACF/miniDAQ/CMakeLists.txt${Reset}]. ${BoldRed}Begin...${Reset}")
MESSAGE(STATUS " ")

# Includes
include_directories(${UHAL_UHAL_INCLUDE_PREFIX})
include_directories(${UHAL_GRAMMARS_INCLUDE_PREFIX})
include_directories(${UHAL_LOG_INCLUDE_PREFIX})
include_directories(${PROJECT_SOURCE_DIR}/miniDAQ)
include_directories(${PROJECT_SOURCE_DIR}/HWDescription)
include_directories(${PROJECT_SOURCE_DIR}/HWInterface)
include_directories(${PROJECT_SOURCE_DIR}/Utils)
include_directories(${PROJECT_SOURCE_DIR}/System)
include_directories(${PROJECT_SOURCE_DIR}/tools)
include_directories(${PROJECT_SOURCE_DIR}/NetworkUtils)
include_directories(${PROJECT_SOURCE_DIR})
include_directories(${CMAKE_CURRENT_SOURCE_DIR})

# Library dirs
link_directories(${UHAL_UHAL_LIB_PREFIX})
link_directories(${PROJECT_SOURCE_DIR/lib})

# Initial set of libraries
set(LIBS ${LIBS} Ph2_Description Ph2_Interface Ph2_Utils Ph2_System NetworkUtils)

# Check for ZMQ installed
if(ZMQ_FOUND)
    #here, now check for UsbInstLib
    if(PH2_USBINSTLIB_FOUND)

        #add include directoreis for ZMQ and USBINSTLIB
        include_directories(${PH2_USBINSTLIB_INCLUDE_DIRS})
        link_directories(${PH2_USBINSTLIB_LIBRARY_DIRS})
        include_directories(${ZMQ_INCLUDE_DIRS})

        #and link against the libs
        set(LIBS ${LIBS} ${ZMQ_LIBRARIES} ${PH2_USBINSTLIB_LIBRARIES})
        set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} $ENV{ZmqFlag} $ENV{USBINSTFlag}")
    endif()
endif()

# Check for AMC13 libraries
if(${CACTUS_AMC13_FOUND})
    include_directories(${PROJECT_SOURCE_DIR}/AMC13)
    include_directories(${UHAL_AMC13_INCLUDE_PREFIX})
    link_directories(${UHAL_AMC13_LIB_PREFIX})
    set(LIBS ${LIBS} cactus_amc13_amc13 Ph2_Amc13)
    set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} $ENV{Amc13Flag}")
endif()

# Check for AntennaDriver
if(${PH2_ANTENNA_FOUND})
    include_directories(${PH2_ANTENNA_INCLUDE_DIRS})
    link_directories(${PH2_ANTENNA_LIBRARY_DIRS})
    set(LIBS ${LIBS} usb ${PH2_ANTENNA_LIBRARIES})
    set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} $ENV{AntennaFlag}")
endif()

# Boost also needs to be linked
if(Boost_FOUND)
    set(LIBS ${LIBS} ${Boost_LIBRARIES})
    set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -lboost_date_time -lboost_iostreams")
endif()

####################################
## EXECUTABLES
####################################

# # CMSITminiDAQ
# add_executable(CMSITminiDAQ CMSITminiDAQ.cc)
# target_link_libraries(CMSITminiDAQ ${LIBS} Ph2_Tools RootWeb)

# # miniDAQ
# add_executable(miniDAQ miniDAQ.cc publisher.cc publisher.h ../DQMUtils/DQMEvent.h ../DQMUtils/DQMEvent.cc ../DQMUtils/SLinkDQMHistogrammer.cc ../DQMUtils/SLinkDQMHistogrammer.cc)
# target_link_libraries(miniDAQ ${LIBS} RootWeb)

# # miniDQM
#add_executable(miniDQM miniDQM.cc publisher.cc publisher.h ../DQMUtils/DQMHistogrammer.cc ../DQMUtils/DQMHistogrammer.h)
#target_link_libraries(miniDQM ${LIBS} Ph2_Tools RootWeb)

# # miniSLinkDQM
# add_executable(miniSLinkDQM miniSLinkDQM.cc publisher.cc publisher.h ../DQMUtils/DQMEvent.h ../DQMUtils/DQMEvent.cc ../DQMUtils/SLinkDQMHistogrammer.cc ../DQMUtils/SLinkDQMHistogrammer.h)
# target_link_libraries(miniSLinkDQM ${LIBS} RootWeb)

# RunController
add_executable(RunController RunController.cc MiddlewareController.cc)
target_link_libraries(RunController ${LIBS} Ph2_Tools)

# file(GLOB BINARIES RELATIVE ${PROJECT_SOURCE_DIR}/miniDAQ *.cc)

# message("--     ${BoldCyan}#### Building the following executables: ####${Reset}")
# foreach( sourcefile ${BINARIES} )
#     string(REPLACE ".cc" "" name ${sourcefile})
#     message(STATUS "    ${name}")
# endforeach(sourcefile ${BINARIES})
# message("--     ${BoldCyan}#### End ####${Reset}")

MESSAGE(STATUS " ")
MESSAGE(STATUS "    ${BoldYellow}MIDDLEWARE${Reset} [stand-alone/middleware]: [${BoldCyan}Ph2_ACF/miniDAQ/CMakeLists.txt${Reset}]. ${BoldGreen}DONE!${Reset}")
MESSAGE(STATUS "    ${BoldBlue}========================================================================================================${Reset}")
MESSAGE(STATUS " ")

else() # ------------------------------- Compilation in the otsdaq environment ---------------------------------------------

    MESSAGE(STATUS " ")
    MESSAGE(STATUS "${BoldBlue}========================================================================================================${Reset}")
    MESSAGE(STATUS "${BoldYellow}MIDDLEWARE${Reset} [otsdaq]: Compilation of the middleware package for OTSDAQ")
    MESSAGE(STATUS "${BoldYellow}MIDDLEWARE${Reset} [otsdaq]: [${BoldCyan}${PH2ACF_BASE_DIR}/CMakeLists.txt${Reset}]")
    MESSAGE(STATUS " ")

    include_directories($ENV{OTSDAQ_CMSOUTERTRACKER_DIR}/uhal/uhal_2_7_5/uhal/uhal/include)
    include_directories($ENV{OTSDAQ_CMSOUTERTRACKER_DIR}/uhal/uhal_2_7_5/uhal/log/include)
    include_directories($ENV{OTSDAQ_CMSOUTERTRACKER_DIR}/uhal/uhal_2_7_5/uhal/grammars/include)

    #library dirs
    #link_directories(${UHAL_UHAL_LIB_PREFIX})

    cet_make_exec(RunController
        SOURCE
        RunController.cc MiddlewareController.cc
        LIBRARIES
    #${Boost_FILESYSTEM_LIBRARY}
    #${Boost_SYSTEM_LIBRARY }
    #${Boost_THREAD_LIBRARY }
    ${Boost_REGEX_LIBRARY}
    ${Boost_IOSTREAMS_LIBRARY}

            ${ROOT_BASIC_LIB_LIST}
              cactus_uhal_uhal
              cactus_uhal_log
              cactus_uhal_grammars
              Ph2_Interface_${Ph2_ACF_Master}
              Ph2_System_${Ph2_ACF_Master}
              Ph2_Utils_${Ph2_ACF_Master}
              NetworkUtils_${Ph2_ACF_Master}
              Ph2_Tools_${Ph2_ACF_Master}

#Ph2_DQMUtils_${Ph2_ACF_Master }
#$ENV{XDAQ_LIB } / libxdaq.so
#$ENV{XDAQ_LIB } / libconfig.so
#$ENV{XDAQ_LIB } / libpeer.so
#$ENV{XDAQ_LIB } / libtoolbox.so
#$ENV{XDAQ_LIB } / liblog4cplus.so
#$ENV{XDAQ_LIB } / libcgicc.so
#$ENV{XDAQ_LIB } / libxcept.so
#$ENV{XDAQ_LIB } / libxoap.so
#$ENV{XDAQ_LIB } / libxdata.so
#$ENV{XDAQ_LIB } / libxgi.so
#$ENV{XDAQ_LIB } / liblogudpappender.so
#$ENV{XDAQ_LIB } / liblogxmlappender.so
#$ENV{XDAQ_LIB } / libmimetic.so
#$ENV{XDAQ_LIB } / libasyncresolv
#$ENV{XDAQ_LIB } / libb2innub.so
#/ usr / lib64 / libuuid.so

#$ENV{XDAQ_LIB } / libb2ineventing.so
#$ENV{XDAQ_LIB } / libb2inutils.so
#$ENV{XDAQ_LIB } / libb2innub.so
#$ENV{XDAQ_LIB } / libesapi.so
#$ENV{XDAQ_LIB } / libesxbeat.so
#$ENV{XDAQ_LIB } / libesxtreme.so
#$ENV{XDAQ_LIB } / libeventingapi.so
#$ENV{XDAQ_LIB } / libeventingcore.so
#$ENV{XDAQ_LIB } / libeventingduple.so
#$ENV{XDAQ_LIB } / libexecutive.so
#$ENV{XDAQ_LIB } / libgevb2g.so
#$ENV{XDAQ_LIB } / libhyperdaq.so
#$ENV{XDAQ_LIB } / libi2o.so
#$ENV{XDAQ_LIB } / libi2outils.a
#$ENV{XDAQ_LIB } / libjansson.so
#$ENV{XDAQ_LIB } / libmstreamio2g.so
#$ENV{XDAQ_LIB } / libptblit.so
#$ENV{XDAQ_LIB } / libptfifo.so
#$ENV{XDAQ_LIB } / libpthttp.so
#$ENV{XDAQ_LIB } / libptibv.so
#$ENV{XDAQ_LIB } / libpttcp.so
#$ENV{XDAQ_LIB } / libptudapl.so
#$ENV{XDAQ_LIB } / libptutcp.so
#$ENV{XDAQ_LIB } / libroundtrip.so
#$ENV{XDAQ_LIB } / libsentinelbridge2g.so
#$ENV{XDAQ_LIB } / libsentinelprobe.so
#$ENV{XDAQ_LIB } / libsentinelsentineld.so
#$ENV{XDAQ_LIB } / libsentinel.so
#$ENV{XDAQ_LIB } / libsentinelspotlight2g.so
#$ENV{XDAQ_LIB } / libsentineltester.so
#$ENV{XDAQ_LIB } / libsentinelutils.so
#$ENV{XDAQ_LIB } / libssh.so
#$ENV{XDAQ_LIB } / libssh_threads.so
#$ENV{XDAQ_LIB } / libtcpla.so
#$ENV{XDAQ_LIB } / libtimeline.so
#$ENV{XDAQ_LIB } / libtracerd.so
#$ENV{XDAQ_LIB } / libtracerprobe.so
#$ENV{XDAQ_LIB } / libwsaddressing.so
#$ENV{XDAQ_LIB } / libxaccess.so
#$ENV{XDAQ_LIB } / libxalan - c.so
#$ENV{XDAQ_LIB } / libxalanMsg.so
#$ENV{XDAQ_LIB } / libxmasadmin.so
#$ENV{XDAQ_LIB } / libxmasbridge2g.so
#$ENV{XDAQ_LIB } / libxmasbroker2g.so
#$ENV{XDAQ_LIB } / libxmascollector2g.so
#$ENV{XDAQ_LIB } / libxmasheartbeatheartbeatd.so
#$ENV{XDAQ_LIB } / libxmasheartbeatprobe.so
#$ENV{XDAQ_LIB } / libxmasheartbeat.so
#$ENV{XDAQ_LIB } / libxmaslas2g.so
#$ENV{XDAQ_LIB } / libxmasprobe.so
#$ENV{XDAQ_LIB } / libxmassensord.so
#$ENV{XDAQ_LIB } / libxmasslash2g.so
#$ENV{XDAQ_LIB } / libxmassmarthub.so
#$ENV{XDAQ_LIB } / libxmastester.so
#$ENV{XDAQ_LIB } / libxmasutils.so
#$ENV{XDAQ_LIB } / libxmemprobe.so
#$ENV{XDAQ_LIB } / libxoapfilter.so
#$ENV{XDAQ_LIB } / libxplore.so
#$ENV{XDAQ_LIB } / libxploreutils.so
#$ENV{XDAQ_LIB } / libxrelay.so
#$ENV{XDAQ_LIB } / libxslp.so
    )

    MESSAGE(STATUS " ")
    MESSAGE(STATUS "${BoldYellow}MIDDLEWARE${Reset} [otsdaq]: [${BoldCyan}${PH2ACF_BASE_DIR}/CMakeLists.txt${Reset}]")
    MESSAGE(STATUS "${BoldYellow}MIDDLEWARE${Reset} [otsdaq]: Compilation of the middleware package done.")
    MESSAGE(STATUS "${BoldBlue}========================================================================================================${Reset}")
    MESSAGE(STATUS " ")

endif()
