if(NOT DEFINED ENV{OTSDAQ_CMSOUTERTRACKER_DIR})

    MESSAGE(STATUS " ")
    MESSAGE(STATUS "    ${BoldBlue}========================================================================================================${Reset}")
    MESSAGE(STATUS "    ${BoldYellow}MIDDLEWARE${Reset} [stand-alone/middleware]: [${BoldCyan}Ph2_ACF/HWDescription/CMakeLists.txt${Reset}]. ${BoldRed}Begin...${Reset}")
    MESSAGE(STATUS " ")

    include_directories(${CMAKE_CURRENT_SOURCE_DIR})

    # Boost also needs to be linked
    include_directories(${Boost_INCLUDE_DIRS})
    link_directories(${Boost_LIBRARY_DIRS})
    set(LIBS ${LIBS} ${Boost_ITERATOR_LIBRARY})

    # c++20 sources
    add_library(Ph2_HWDescription_cxx20 OBJECT RD53BCommands.cc)
    target_compile_options(Ph2_HWDescription_cxx20 PRIVATE -std=c++20)

    # Find source files
    file(GLOB SOURCES *.cc)
    list(FILTER SOURCES EXCLUDE REGEX ".*RD53BCommands\\.cc$") # @TMP@
    add_library(Ph2_Description STATIC ${SOURCES})

    ###############
    # EXECUTABLES #
    ###############

    file(GLOB BINARIES RELATIVE ${PROJECT_SOURCE_DIR}/HWDescription *.cc)

    message("--     ${BoldCyan}#### Building the following executables: ####${Reset}")
    foreach( sourcefile ${BINARIES} )
        string(REPLACE ".cc" "" name ${sourcefile})
        message(STATUS "    ${name}")
    endforeach(sourcefile ${BINARIES})
    message("--     ${BoldCyan}#### End ####${Reset}")

    MESSAGE(STATUS " ")
    MESSAGE(STATUS "    ${BoldYellow}MIDDLEWARE${Reset} [stand-alone/middleware]: [${BoldCyan}Ph2_ACF/HWDescription/CMakeLists.txt${Reset}]. ${BoldRed}DONE!${Reset}")
    MESSAGE(STATUS "    ${BoldBlue}========================================================================================================${Reset}")
    MESSAGE(STATUS " ")

else()

    MESSAGE(STATUS " ")
    MESSAGE(STATUS "    ${BoldBlue}========================================================================================================${Reset}")
    MESSAGE(STATUS "    ${BoldYellow}MIDDLEWARE${Reset} [otsdaq/middleware]: [${BoldCyan}Ph2_ACF/HWDescription/CMakeLists.txt${Reset}]. ${BoldRed}Begin...${Reset}")
    MESSAGE(STATUS " ")

    cet_set_compiler_flags(
     EXTRA_FLAGS -Wno-reorder -Wl,--undefined
    )

    cet_make(LIBRARY_NAME Ph2_Description_${Ph2_ACF_Master}
             LIBRARIES
             Ph2_Utils_${Ph2_ACF_Master}
             pthread
             EXCLUDE MPA.h MPA.cc
             )

    install_headers()
    install_source()

    MESSAGE(STATUS " ")
    MESSAGE(STATUS "    ${BoldYellow}MIDDLEWARE${Reset} [otsdaq/middleware]: [${BoldCyan}Ph2_ACF/HWDescription/CMakeLists.txt${Reset}]. ${BoldGreen}DONE!${Reset}")
    MESSAGE(STATUS "    ${BoldBlue}========================================================================================================${Reset}")
    MESSAGE(STATUS " ")

endif()
