string(ASCII 27 Esc)
set(Reset       "${Esc}[m"    )
set(Bold        "${Esc}[1m"   )
set(Red         "${Esc}[31m"  )
set(Green       "${Esc}[32m"  )
set(Yellow      "${Esc}[33m"  )
set(Blue        "${Esc}[34m"  )
set(Magenta     "${Esc}[35m"  )
set(Cyan        "${Esc}[36m"  )
set(White       "${Esc}[37m"  )
set(BoldRed     "${Esc}[1;31m")
set(BoldGreen   "${Esc}[1;32m")
set(BoldYellow  "${Esc}[1;33m")
set(BoldBlue    "${Esc}[1;34m")
set(BoldMagenta "${Esc}[1;35m")
set(BoldCyan    "${Esc}[1;36m")
set(BoldWhite   "${Esc}[1;37m")

if(NOT DEFINED ENV{OTSDAQ_CMSOUTERTRACKER_DIR})

    MESSAGE(STATUS " ")
    MESSAGE(STATUS "${BoldBlue}========================================================================================================${Reset}")
    MESSAGE(STATUS "    ${BoldYellow}MIDDLEWARE${Reset} [stand-alone]: Compilation of the middleware package")
    MESSAGE(STATUS "    ${BoldYellow}MIDDLEWARE${Reset} [stand-alone]: [${BoldCyan}${PH2ACF_BASE_DIR}/CMakeLists.txt${Reset}]")
    MESSAGE(STATUS " ")

    cmake_minimum_required(VERSION 2.8)
    project(Ph2_ACF)

   # Set the output directory
    if ( ${CMAKE_SOURCE_DIR} STREQUAL ${CMAKE_BINARY_DIR} )
       MESSAGE( STATUS "${BoldRed}" )
       MESSAGE( FATAL_ERROR "${Reset}In-source builds not allowed. Please make a new directory (called a build directory) and run CMake from there. You may need to remove CMakeCache.txt." )
    endif()

    # ---------- Setup output Directories -------------------------
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY
        ${PROJECT_SOURCE_DIR}/lib
        CACHE PATH
        "Single Directory for all Libraries"
       )

    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY
        ${PROJECT_SOURCE_DIR}/bin
        CACHE PATH
        "Single Directory for all Executables."
       )

    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY
        ${PROJECT_SOURCE_DIR}/bin
        CACHE PATH
        "Single Directory for all static libraries."
       )
    # ---------- Setup output Directories -------------------------

    # Set the cmake module path
    set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/")

    set(CompileForShep "$ENV{CompileForShep}")
    set(CompileForHerd "$ENV{CompileForHerd}")
    if ((NOT CompileForShep) AND (NOT CompileForHerd))
        set(NoDataShipping true)
    else()
        set(NoDataShipping false)
    endif()

    MESSAGE(STATUS "")
    MESSAGE(STATUS "    ${BoldYellow}COMPILATION FLAGS:${Reset}")
    MESSAGE(STATUS "    CompileForShep = ${BoldRed}${CompileForShep}${Reset}")
    MESSAGE(STATUS "    CompileForHerd = ${BoldRed}${CompileForHerd}${Reset}")
    MESSAGE(STATUS "    NoDataShipping = ${BoldRed}${NoDataShipping}${Reset}")

    # If it's a recent gcc compiler, then let's add some
    # compiler options to avoid warnings promoted to errors

    set (GCC_NOERROR_FLAGS " -Wno-error=unused-result    -Wno-error=deprecated-declarations")
    set (GCC_6_NOERROR_FLAGS "-Wno-error=misleading-indentation")
    set (GCC_7_NOERROR_FLAGS "-Wno-expansion-to-defined -Wno-error=format-overflow")
    set (GCC_8_NOERROR_FLAGS "-Wno-error=catch-value -Wno-error=class-memaccess -Wno-error=format-overflow")
    set (GCC_9_NOERROR_FLAGS "-Wno-error=deprecated-copy -Wno-error=int-in-bool-context -Wno-error=unused-variable")

    if ("${CMAKE_CXX_COMPILER_ID}" MATCHES "GNU")
      EXECUTE_PROCESS( COMMAND gcc -dumpversion OUTPUT_VARIABLE GCC_VERSION )
      if(NOT (GCC_VERSION VERSION_LESS 6))
        set (GCC_NOERROR_FLAGS "${GCC_NOERROR_FLAGS} ${GCC_6_NOERROR_FLAGS}")
      endif()
      if(NOT (GCC_VERSION VERSION_LESS 7))
        set (GCC_NOERROR_FLAGS "${GCC_NOERROR_FLAGS} ${GCC_7_NOERROR_FLAGS}")
      endif()
      if(NOT (GCC_VERSION VERSION_LESS 8))
        set (GCC_NOERROR_FLAGS "${GCC_NOERROR_FLAGS} ${GCC_8_NOERROR_FLAGS}")
      endif()
      if(NOT (GCC_VERSION VERSION_LESS 9))
        set (GCC_NOERROR_FLAGS "${GCC_NOERROR_FLAGS} ${GCC_9_NOERROR_FLAGS}")
      endif()
    endif ()
    set (CMAKE_CXX_FLAGS "-g -std=c++1y -gdwarf-4 -O3 -fopenmp -fno-omit-frame-pointer -Werror -pedantic -Wno-unused-parameter -Wno-ignored-qualifiers -Wno-missing-field-initializers -Wall -Werror=return-type -Wextra -Winit-self -Wno-unused-local-typedefs -Woverloaded-virtual -Wnon-virtual-dtor -Wdelete-non-virtual-dtor ${GCC_NOERROR_FLAGS} -pthread -Wcpp -fPIC ${CMAKE_CXX_FLAGS} -DELPP_THREAD_SAFE")
    if(NoDataShipping)
        set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} $ENV{UseRootFlag}")
    endif()


    # Check for external dependences
    message("--")
    message("--     ${BoldCyan}#### Checking for external Dependencies ####${Reset}")
    if(CompileForShep OR NoDataShipping)
        #ROOT
        find_package(ROOT COMPONENTS RHTTP)
        if(ROOT_FOUND)
            #check for THttpServer
            if(EXISTS ${ROOT_RHTTP_LIBRARY})
                message(STATUS "    Found THttp Server support - enabling compiler flags")
                set(ROOT_HAS_HTTP TRUE)
            elseif()
                message(STATUS "    ROOT built without THttp Server - disabling compiler flags")
            endif()
            # set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -")
            message(STATUS "    Using ROOT version ${ROOT_VERSION}")
        endif(ROOT_FOUND)
    endif()

    # AMC13
    find_package(CACTUS)      #========================================================
    if(${CACTUS_AMC13_FOUND})
        message(STATUS "    Enabling AMC13 component")
    endif(${CACTUS_AMC13_FOUND})

    # Antenna
    find_package(PH2_ANTENNA) #========================================================
    if(PH2_ANTENNA_FOUND)
        message(STATUS "    Building the Antenna components")
    endif(PH2_ANTENNA_FOUND)

    # Ph2_TCUSB [USB control of hybrids]
    if ($ENV{CompileWithTCUSB})
      find_package(PH2_TCUSB)
      if(PH2_TCUSB_FOUND)
        if($ENV{UseTCUSBforROH})
            message(STATUS "    Building the ROH TestCards USB components")
            set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} $ENV{TCUSBforROHFlag}")
        else($ENV{UseTCUSBforROH})
            message(STATUS "    Building the SEH TestCards USB components")
            set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} $ENV{TCUSBforSEHFlag}")
        endif($ENV{UseTCUSBforROH})
        if($ENV{UseTCUSBTcpServer})
            message(STATUS "    Building the components to use the TCP Server")
            set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} $ENV{TCUSBTcpServerFlag}")
        endif($ENV{UseTCUSBTcpServer})
      endif(PH2_TCUSB_FOUND)
    endif($ENV{CompileWithTCUSB})

    # EUDAQ
    if ($ENV{CompileWithEUDAQ})
        set(USE_EUDAQ ON)
        include_directories($ENV{EUDAQDIR}/include)
        set(EUDAQ_LIB -L$ENV{EUDAQLIB} EUDAQ)
        message(STATUS "    EUDAQ variable is defined. EUDAQ directory: $ENV{EUDAQDIR}")
    else($ENV{CompileWithEUDAQ})
        message(STATUS "    EUDAQ variable is not defined. Building without EUDAQ producer")
    endif($ENV{CompileWithEUDAQ})

    # ZeroMQ
    find_package(ZMQ)         #========================================================
    if(ZMQ_FOUND)
        find_package(PH2_USBINSTLIB)
        if(PH2_USBINSTLIB_FOUND)
        endif(PH2_USBINSTLIB_FOUND)
    endif(ZMQ_FOUND)

    # Boost
    set(BOOST_INCLUDE_DIRS "$ENV{BOOST_INCLUDE}")
    set(BOOST_LIBRARY_DIRS "$ENV{BOOST_LIB}")
    find_package(Boost 1.53 REQUIRED system filesystem thread program_options serialization)

    message("--     ${BoldCyan}#### Done ####${Reset}")

    # All the subdirs
    if(${CACTUS_AMC13_FOUND})
        add_subdirectory(AMC13)
    endif()

    MESSAGE(STATUS " ")
    if(CompileForHerd OR NoDataShipping)
        add_subdirectory(tools)
        add_subdirectory(ProductionTools)
        add_subdirectory(HWDescription)
        add_subdirectory(HWInterface)
        add_subdirectory(System)
    endif()

    if(CompileForHerd)
        add_subdirectory(miniDAQ)
    endif()

    if(CompileForShep OR NoDataShipping)
        add_subdirectory(DQMUtils)
        add_subdirectory(RootUtils)
    endif()

    if(NoDataShipping OR (CompileForShep AND CompileForHerd))
        add_subdirectory(src)
    endif()

    if(NoDataShipping)
        add_subdirectory(RootWeb)
    endif()

    add_subdirectory(Utils)
    add_subdirectory(NetworkUtils)

    message("--     ${BoldRed}#### Don't forget to set the path and ld_library_path by sourcing setup.sh before running ###${Reset}")
    #set(ENV{PATH} "$ENV{PATH}:${PROJECT_SOURCE_DIR}/bin")
    #set(ENV{LD_LIBRARY_PATH} "${PROJECT_SOURCE_DIR}/lib:$ENV(LD_LIBRARY_PATH)")

    MESSAGE(STATUS " ")
    MESSAGE(STATUS "${BoldYellow}MIDDLEWARE${Reset} [stand-alone]: [${BoldCyan}${PH2ACF_BASE_DIR}/CMakeLists.txt${Reset}]")
    MESSAGE(STATUS "${BoldYellow}MIDDLEWARE${Reset} [stand-alone]: Compilation of the middleware package done.")
    MESSAGE(STATUS "${BoldBlue}========================================================================================================${Reset}")
    MESSAGE(STATUS " ")

else() # ------------------------------- Compilation in the otsdaq environment ---------------------------------------------

    MESSAGE(STATUS " ")
    MESSAGE(STATUS "${BoldBlue}========================================================================================================${Reset}")
    MESSAGE(STATUS "${BoldYellow}MIDDLEWARE${Reset} [otsdaq]: Compilation of the middleware package for OTSDAQ")
    MESSAGE(STATUS "${BoldYellow}MIDDLEWARE${Reset} [otsdaq]: [${BoldCyan}${PH2ACF_BASE_DIR}/CMakeLists.txt${Reset}]")
    MESSAGE(STATUS " ")

    MESSAGE (STATUS "${CMAKE_CXX_FLAGS}")

    set(CompileForShep "$ENV{CompileForShep}")
    set(CompileForHerd "$ENV{CompileForHerd}")
    if ((NOT CompileForShep) AND (NOT CompileForHerd))
        set(NoDataShipping true)
    else()
        set(NoDataShipping false)
    endif()

    # AMC13
    if(${CACTUS_AMC13_FOUND})
        message(STATUS "    Enabling AMC13 component")
    endif(${CACTUS_AMC13_FOUND})

    # Antenna
    find_package(PH2_ANTENNA) #========================================================
    if(PH2_ANTENNA_FOUND)
        message(STATUS "    Building the Antenna components")
    endif(PH2_ANTENNA_FOUND)

    # ZeroMQ
    find_package(ZMQ)         #========================================================
    if(ZMQ_FOUND)
        find_package(PH2_USBINSTLIB)
        if(PH2_USBINSTLIB_FOUND)
        endif(PH2_USBINSTLIB_FOUND)
    endif(ZMQ_FOUND)


   # EUDAQ
    if ($ENV{CompileWithEUDAQ})
        set(USE_EUDAQ ON)
        include_directories($ENV{EUDAQDIR}/include $ENV{EUDAQDIR}/user/tlu/module/src)
        set(EUDAQ_LIB -L$ENV{EUDAQLIB} eudaq_core)
        message(STATUS "    EUDAQ variable is defined. EUDAQ directory: $ENV{EUDAQDIR}")
    else($ENV{CompileWithEUDAQ})
        message(STATUS "    EUDAQ variable is not defined. Building without EUDAQ producer")
    endif($ENV{CompileWithEUDAQ})

    message("--     ${BoldCyan}#### Done ####${Reset}")

    # All the subdirs
    if(${CACTUS_AMC13_FOUND})
        add_subdirectory(AMC13)
    endif()

    add_subdirectory(tools)
    add_subdirectory(ProductionTools)
    add_subdirectory(DQMUtils)
    add_subdirectory(Utils)
    add_subdirectory(RootUtils)
    add_subdirectory(NetworkUtils)
    add_subdirectory(HWDescription)
    add_subdirectory(HWInterface)
    add_subdirectory(System)
    add_subdirectory(miniDAQ)

    MESSAGE(STATUS " ")
    MESSAGE(STATUS "${BoldYellow}MIDDLEWARE${Reset} [otsdaq]: [${BoldCyan}${PH2ACF_BASE_DIR}/CMakeLists.txt${Reset}]")
    MESSAGE(STATUS "${BoldYellow}MIDDLEWARE${Reset} [otsdaq]: Compilation of the middleware package done.")
    MESSAGE(STATUS "${BoldBlue}========================================================================================================${Reset}")
    MESSAGE(STATUS " ")

endif()
