if(NOT DEFINED ENV{OTSDAQ_CMSOUTERTRACKER_DIR})

    MESSAGE(STATUS " ")
    MESSAGE(STATUS "    ${BoldYellow}========================================================================================================${Reset}")
    MESSAGE(STATUS "    ${BoldYellow}MIDDLEWARE${Reset} [stand-alone/middleware]: [${BoldCyan}Ph2_ACF/src/CMakeLists.txt${Reset}]. ${BoldRed}Begin...${Reset}")
    MESSAGE(STATUS " ")

    # Includes
    include_directories(${UHAL_UHAL_INCLUDE_PREFIX})
    include_directories(${UHAL_GRAMMARS_INCLUDE_PREFIX})
    include_directories(${UHAL_LOG_INCLUDE_PREFIX})
    include_directories(${PROJECT_SOURCE_DIR}/HWDescription)
    include_directories(${PROJECT_SOURCE_DIR}/HWInterface)
    include_directories(${PROJECT_SOURCE_DIR}/Utils)
    include_directories(${PROJECT_SOURCE_DIR}/System)
    include_directories(${PROJECT_SOURCE_DIR}/tools)
    include_directories(${PROJECT_SOURCE_DIR}/ProductionTools)
    include_directories(${PROJECT_SOURCE_DIR})
    include_directories(${CMAKE_CURRENT_SOURCE_DIR})

    # Library dirs
    link_directories(${UHAL_UHAL_LIB_PREFIX})
    link_directories(${UHAL_LOG_LIB_PREFIX})
    link_directories(${UHAL_GRAMMARS_LIB_PREFIX})
    link_directories(${PROJECT_SOURCE_DIR/lib})

    # Initial set of libraries
    set(LIBS ${LIBS} Ph2_Description Ph2_Interface Ph2_Utils Ph2_System Ph2_Tools Ph2_DQMUtils Ph2_RootUtils Ph2_ProductionTools NetworkUtils)

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
        set(LIBS ${LIBS} ${PH2_ANTENNA_LIBRARIES} usb)
        set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} $ENV{AntennaFlag}")
    endif()

    # Check for TestCard USBDriver
    if(${PH2_TCUSB_FOUND})
      include_directories(${PH2_TCUSB_INCLUDE_DIRS})
      link_directories(${PH2_TCUSB_LIBRARY_DIRS})
      set(LIBS ${LIBS} ${PH2_TCUSB_LIBRARIES} usb)
      set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} $ENV{TCUSBFlag}")
      if($ENV{UseTCUSBforROH})
          set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} $ENV{TCUSBforROHFlag}")
      else($ENV{UseTCUSBforROH})
          set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} $ENV{TCUSBforSEHFlag}")
      endif($ENV{UseTCUSBforROH})
      if($ENV{UseTCUSBTcpServer})
          set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} $ENV{TCUSBTcpServerFlag}")
      endif($ENV{UseTCUSBTcpServer})
    endif(${PH2_TCUSB_FOUND})

    #boost also needs to be linked
    if(Boost_FOUND)
        set(LIBS ${LIBS} ${Boost_LIBRARIES})
        set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -lboost_date_time -lboost_iostreams")
    endif()


    # Find root and link against it
    if(${ROOT_FOUND})
        include_directories(${ROOT_INCLUDE_DIRS})
        set(LIBS ${LIBS} ${ROOT_LIBRARIES})
        if(NoDataShipping)
            set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} $ENV{UseRootFlag}")
        endif()

        #check for THttpServer
        if(${ROOT_HAS_HTTP})
            set(LIBS ${LIBS} ${ROOT_RHTTP_LIBRARY})
            set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} $ENV{HttpFlag}")
        endif()
    endif()

    # Check if the setup is based on the multiplexing backplane
    if(DEFINED ENV{MultiplexingFlag})
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} $ENV{MultiplexingFlag}")
    endif(DEFINED ENV{MultiplexingFlag})

    # Build eudaq producer
    if (USE_EUDAQ)
        set(LIBS ${LIBS} ${EUDAQ_LIB})
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} $ENV{EuDaqFlag}")
    endif(USE_EUDAQ)

    ####################################
    ## EXECUTABLES
    ####################################

    file(GLOB BINARIES RELATIVE ${PROJECT_SOURCE_DIR}/src *.cc)

    # Remove binaries
    if(CompileForShep AND CompileForHerd)
        list(REMOVE_ITEM BINARIES *.cc)
        file(GLOB BINARIES RELATIVE ${PROJECT_SOURCE_DIR}/src supervisor.cc CMSITminiDAQ.cc fpgaconfig.cc)
    else()
        list(REMOVE_ITEM BINARIES supervisor.cc)
    endif()

    message("--     ${BoldCyan}#### Building the following executables: ####${Reset}")

    foreach(sourcefile ${BINARIES})
        string(REPLACE ".cc" "" name ${sourcefile})
        message(STATUS "    ${name}")
        add_executable(${name} ${sourcefile})
        if (("${name}" STREQUAL "miniDAQ") OR ("${name}" STREQUAL "miniSLinkDQM"))
          target_link_libraries(${name} ${LIBS} Ph2_RootUtils RootWeb)
        else()
          target_link_libraries(${name} ${LIBS})
        endif()
        #check for PowerSupply
        if ("${name}" STREQUAL "SEHTest")
            if(${PH2_POWERSUPPLY_FOUND})
                target_link_libraries(${name} ${LIBS} "${PH2_POWERSUPPLY_LIBRARIES}")
            endif()
        endif()
        if ("${name}" STREQUAL "powersupply")
            if(${PH2_POWERSUPPLY_FOUND})
                target_link_libraries(${name} ${LIBS} "${PH2_POWERSUPPLY_LIBRARIES}")
            endif()
        endif()

        if ("${name}" STREQUAL "powersupply")
            if(${PH2_POWERSUPPLY_FOUND})
                target_link_libraries(${name} ${LIBS} "${PH2_POWERSUPPLY_LIBRARIES}")
            endif()
        endif()
    endforeach(sourcefile ${BINARIES})

    message("--     ${BoldCyan}#### End ####${Reset}")

    MESSAGE(STATUS " ")
    MESSAGE(STATUS "    ${BoldYellow}MIDDLEWARE${Reset} [stand-alone/middleware]: [${BoldCyan}Ph2_ACF/src/CMakeLists.txt${Reset}]. ${BoldGreen}DONE!${Reset}")
    MESSAGE(STATUS "    ${BoldBlue}========================================================================================================${Reset}")
    MESSAGE(STATUS " ")

else()

    MESSAGE(STATUS " ")
    MESSAGE(STATUS "    ${BoldBlue}========================================================================================================${Reset}")
    MESSAGE(STATUS "    ${BoldYellow}MIDDLEWARE${Reset} [otsdaq/middleware]: [${BoldCyan}Ph2_ACF/src/CMakeLists.txt${Reset}]. ${BoldRed}Begin...${Reset}")
    MESSAGE(STATUS " ")

    link_directories(${UHAL_UHAL_LIB_PREFIX})
    link_directories(${PROJECT_SOURCE_DIR/lib})

    MESSAGE(STATUS " ")
    MESSAGE(STATUS "    ${BoldYellow}MIDDLEWARE${Reset} [otsdaq/middleware]: [${BoldCyan}Ph2_ACF/src/CMakeLists.txt${Reset}]. ${BoldGreen}DONE!${Reset}")
    MESSAGE(STATUS "    ${BoldBlue}========================================================================================================${Reset}")
    MESSAGE(STATUS " ")

endif()
