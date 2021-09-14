if(NOT DEFINED ENV{OTSDAQ_CMSOUTERTRACKER_DIR})

    MESSAGE(STATUS " ")
    MESSAGE(STATUS "    ${BoldBlue}========================================================================================================${Reset}")
    MESSAGE(STATUS "    ${BoldYellow}MIDDLEWARE${Reset} [stand-alone/middleware]: [${BoldCyan}Ph2_ACF/System/CMakeLists.txt${Reset}]. ${BoldRed}Begin...${Reset}")
    MESSAGE(STATUS " ")

    # Includes
    include_directories(${PROJECT_SOURCE_DIR}/HWDescription)
    include_directories(${PROJECT_SOURCE_DIR}/HWInterface)
    include_directories(${PROJECT_SOURCE_DIR}/Utils)
    include_directories(${PROJECT_SOURCE_DIR}/NetworkUtils)
    include_directories(${PROJECT_SOURCE_DIR})
    include_directories(${UHAL_UHAL_INCLUDE_PREFIX})
    include_directories(${UHAL_GRAMMARS_INCLUDE_PREFIX})
    include_directories(${UHAL_LOG_INCLUDE_PREFIX})
    include_directories(${CMAKE_CURRENT_SOURCE_DIR})

    # Library directories
    link_directories(${UHAL_UHAL_LIB_PREFIX})
    link_directories(${UHAL_LOG_LIB_PREFIX})
    link_directories(${UHAL_GRAMMARS_LIB_PREFIX})

    # Find root and link against it
    if(${ROOT_FOUND})
        include_directories(${ROOT_INCLUDE_DIRS})
        set(LIBS ${LIBS} ${ROOT_LIBRARIES})
    endif()

    # Find source files
    file(GLOB HEADERS *.h)
    file(GLOB SOURCES *.cc)

    # Add the library
    add_library(Ph2_System STATIC ${SOURCES} ${HEADERS})
    set(LIBS ${LIBS} Ph2_Description Ph2_Interface Ph2_Utils NetworkUtils)
    TARGET_LINK_LIBRARIES(Ph2_System ${LIBS})

      #check for TestCard USBDriver
      if(${PH2_TCUSB_FOUND})
        include_directories(${PH2_TCUSB_INCLUDE_DIRS})
        link_directories(${PH2_TCUSB_LIBRARY_DIRS})
        set(LIBS ${LIBS} ${PH2_TCUSB_LIBRARIES} usb)
        set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} $ENV{TCUSBFlag}")

    endif()
    #check for TestCard USBDriver
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

    ####################################
    ## EXECUTABLES
    ####################################

    file(GLOB BINARIES RELATIVE ${PROJECT_SOURCE_DIR}/System *.cc)

    message("--     ${BoldCyan}#### Building the following executables: ####${Reset}")
    foreach( sourcefile ${BINARIES} )
        string(REPLACE ".cc" "" name ${sourcefile})
        message(STATUS "    ${name}")
    endforeach(sourcefile ${BINARIES})
    message("--     ${BoldCyan}#### End ####${Reset}")

    MESSAGE(STATUS " ")
    MESSAGE(STATUS "    ${BoldYellow}MIDDLEWARE${Reset} [stand-alone/middleware]: [${BoldCyan}Ph2_ACF/System/CMakeLists.txt${Reset}]. ${BoldGreen}DONE!${Reset}")
    MESSAGE(STATUS "    ${BoldBlue}========================================================================================================${Reset}")
    MESSAGE(STATUS " ")

else()

    MESSAGE(STATUS " ")
    MESSAGE(STATUS "    ${BoldBlue}========================================================================================================${Reset}")
    MESSAGE(STATUS "    ${BoldYellow}MIDDLEWARE${Reset} [otsdaq/middleware]: [${BoldCyan}Ph2_ACF/System/CMakeLists.txt${Reset}]. ${BoldRed}Begin...${Reset}")
    MESSAGE(STATUS " ")

    include_directories($ENV{OTSDAQ_CMSOUTERTRACKER_DIR}/uhal/uhal_2_7_5/uhal/uhal/include)
    include_directories($ENV{OTSDAQ_CMSOUTERTRACKER_DIR}/uhal/uhal_2_7_5/uhal/log/include)
    include_directories($ENV{OTSDAQ_CMSOUTERTRACKER_DIR}/uhal/uhal_2_7_5/uhal/grammars/include)

    cet_set_compiler_flags(
                           EXTRA_FLAGS -Wno-reorder -Wl,--undefined
                          )

    cet_make(
             LIBRARY_NAME Ph2_System_${Ph2_ACF_Master}
             LIBRARIES
             Ph2_Description_${Ph2_ACF_Master}
             Ph2_Interface_${Ph2_ACF_Master}
             Ph2_Utils_${Ph2_ACF_Master}
            )

    install_headers()
    install_source()

    MESSAGE(STATUS " ")
    MESSAGE(STATUS "    ${BoldYellow}MIDDLEWARE${Reset} [otsdaq/middleware]: [${BoldCyan}Ph2_ACF/System/CMakeLists.txt${Reset}]. ${BoldGreen}DONE!${Reset}")
    MESSAGE(STATUS "    ${BoldBlue}========================================================================================================${Reset}")
    MESSAGE(STATUS " ")

endif()
