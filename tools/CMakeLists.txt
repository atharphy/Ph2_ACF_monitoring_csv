if(NOT DEFINED ENV{OTSDAQ_CMSTRACKER_DIR})

    MESSAGE(STATUS " ")
    MESSAGE(STATUS "    ${BoldYellow}========================================================================================================${Reset}")
    MESSAGE(STATUS "    ${BoldYellow}TOOLS${Reset} [stand-alone]: [${BoldCyan}Ph2_ACF/tools/CMakeLists.txt${Reset}]. ${BoldRed}Begin...${Reset}")
    MESSAGE(STATUS " ")

    # Includes
    include_directories(${UHAL_UHAL_INCLUDE_PREFIX})
    include_directories(${grammars})
    include_directories(${UHAL_LOG_INCLUDE_PREFIX})
    include_directories(${PROJECT_SOURCE_DIR})
    include_directories(${PROJECT_SOURCE_DIR}/Utils)

    # Library dirs
    link_directories(${UHAL_UHAL_LIB_PREFIX})
    link_directories(${UHAL_LOG_LIB_PREFIX})
    link_directories(${UHAL_GRAMMARS_LIB_PREFIX})

    # Find source files
    file(GLOB HEADERS *.h)
    file(GLOB SOURCES *.cc)

    # Boost also needs to be linked
    include_directories(${Boost_INCLUDE_DIRS})
    link_directories(${Boost_LIBRARY_DIRS})
    set(LIBS ${LIBS} boost_serialization ${Boost_ARCHIVE_LIBRARY} ${Boost_SERIALIZATION_LIBRARY} ${Boost_MULTIPRECISION_LIBRARY})

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

    # Check for TestCard USBDriver
    if(${PH2_TCUSB_FOUND})
        include_directories(${PH2_TCUSB_INCLUDE_DIRS})
        link_directories(${PH2_TCUSB_LIBRARY_DIRS})
        set(LIBS ${LIBS} usb ${PH2_TCUSB_LIBRARIES})
        set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} $ENV{TCUSBFlag}")
        if($ENV{UseTCUSBforROH})
            message(STATUS "    Building the ROH TestCards USB components")
            set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} $ENV{TCUSBforROHFlag}")
        endif($ENV{UseTCUSBforROH})
        if($ENV{UseTCUSBforSEH})
            message(STATUS "    Building the SEH TestCards USB components")
            set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} $ENV{TCUSBforSEHFlag}")
        endif($ENV{UseTCUSBforSEH})
    endif(${PH2_TCUSB_FOUND})

    #last but not least, find root and link against it
    if(NoDataShipping)
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
    endif()

    set(LIBS ${LIBS} Ph2_Description Ph2_Interface Ph2_Utils Ph2_System)
    if(NoDataShipping)
        set(LIBS ${LIBS} Ph2_DQMUtils)
    endif()

    file(GLOB BINARIES RELATIVE ${PROJECT_SOURCE_DIR}/tools *.cc)

    # Build eudaq producer
    if (USE_EUDAQ)
        set(LIBS ${LIBS} ${EUDAQ_LIB})
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} $ENV{EuDaqFlag}")
    else(USE_EUDAQ)
        list(REMOVE_ITEM BINARIES RD53eudaqProducer.cc)
        list(REMOVE_ITEM SOURCES ${PROJECT_SOURCE_DIR}/tools/RD53eudaqProducer.cc)
    endif(USE_EUDAQ)

    ###############
    # EXECUTABLES #
    ###############

    message("--     ${BoldCyan}#### Building the following executables: ####${Reset}")

    # Add the library
    add_library(Ph2_Tools STATIC ${SOURCES} ${HEADERS})

    foreach(sourcefile ${BINARIES})
        string(REPLACE ".cc" "" name ${sourcefile})
        message(STATUS "    ${name}")
    endforeach(sourcefile ${BINARIES})

    TARGET_LINK_LIBRARIES(Ph2_Tools ${LIBS})

    message("--     ${BoldCyan}#### End ####${Reset}")

    MESSAGE(STATUS " ")
    MESSAGE(STATUS "    ${BoldYellow}TOOLS${Reset} [stand-alone]: [${BoldCyan}Ph2_ACF/tools/CMakeLists.txt${Reset}]. ${BoldGreen}DONE!${Reset}")
    MESSAGE(STATUS "    ${BoldBlue}========================================================================================================${Reset}")
    MESSAGE(STATUS " ")

else()

    MESSAGE(STATUS " ")
    MESSAGE(STATUS "    ${BoldBlue}========================================================================================================${Reset}")
    MESSAGE(STATUS "    ${BoldYellow}TOOLS${Reset} [otsdaq]: [${BoldCyan}Ph2_ACF/tools/CMakeLists.txt${Reset}]. ${BoldRed}Begin...${Reset}")
    MESSAGE(STATUS " ")

    include_directories(${UHAL_DIR}/uhal/include)
    include_directories(${UHAL_DIR}/log/include)
    include_directories(${UHAL_DIR}/grammars/include)
    
    cet_set_compiler_flags(
     EXTRA_FLAGS -Wno-reorder -Wl,--undefined
     )

    cet_make(LIBRARY_NAME Ph2_Tools
            LIBRARIES
            pthread
            ${Boost_SYSTEM_LIBRARY}
            $ENV{ROOTSYS}/lib/libCore.so
            EXCLUDE RD53eudaqProducer.h RD53eudaqProducer.cc
            EXCLUDE Eudaq2Producer.h Eudaq2Producer.cc
            EXCLUDE SSAPhysics.h SSAPhysics.cc
            )

    install_headers()
    install_source()

    MESSAGE(STATUS " ")
    MESSAGE(STATUS "    ${BoldYellow}TOOLS${Reset} [otsdaq]: [${BoldCyan}Ph2_ACF/tools/CMakeLists.txt${Reset}]. ${BoldGreen}DONE!${Reset}")
    MESSAGE(STATUS "    ${BoldBlue}========================================================================================================${Reset}")
    MESSAGE(STATUS " ")

endif()
