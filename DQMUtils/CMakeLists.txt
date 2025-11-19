if(NOT DEFINED ENV{OTSDAQ_CMSTRACKER_DIR})

    MESSAGE(STATUS " ")
    MESSAGE(STATUS "    ${BoldYellow}========================================================================================================${Reset}")
    MESSAGE(STATUS "    ${BoldYellow}DQM UTILS${Reset} [stand-alone]: [${BoldCyan}Ph2_ACF/DQMUtils/CMakeLists.txt${Reset}]. ${BoldRed}Begin...${Reset}")
    MESSAGE(STATUS " ")

    # Includes
    include_directories(${PROJECT_SOURCE_DIR})

    # Boost also needs to be linked
    #    include_directories(${Boost_INCLUDE_DIRS})
    #link_directories(${Boost_LIBRARY_DIRS})
    set(LIBS ${LIBS} ${Boost_ANY_LIBRARY} ${Boost_ITERATOR_LIBRARY} boost_serialization Ph2_Parser Ph2_Utils)

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

    # Find source files
    file(GLOB SOURCES *.cc)
    add_library(Ph2_DQMUtils STATIC ${SOURCES})
    TARGET_LINK_LIBRARIES(Ph2_DQMUtils ${LIBS})

    include_directories(${PROJECT_SOURCE_DIR}/Utils)

    set(DICT_HEADERS
        HybridL1EventInfo.h
    )

    set(LINKDEF_FILE
        LinkDef.h
    )

    ROOT_GENERATE_DICTIONARY(
        G__Ph2_EventDict
        ${DICT_HEADERS}
        LINKDEF ${LINKDEF_FILE}
        OPTIONS "-I${PROJECT_SOURCE_DIR}/Utils"   # <-- IMPORTANT: QUOTED!
    )

    add_library(Ph2_EventDict SHARED
        ${CMAKE_CURRENT_BINARY_DIR}/G__Ph2_EventDict.cxx
    )

    target_include_directories(Ph2_EventDict PUBLIC
        ${PROJECT_SOURCE_DIR}/Utils
    )

    target_link_libraries(Ph2_EventDict
        PUBLIC Ph2_Utils ${ROOT_LIBRARIES}
    )

    add_custom_command(TARGET Ph2_EventDict POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                ${CMAKE_CURRENT_BINARY_DIR}/G__Ph2_EventDict_rdict.pcm
                ${PROJECT_SOURCE_DIR}/lib/
    )

    file(GLOB BINARIES RELATIVE ${PROJECT_SOURCE_DIR}/RootUtils *.cc)

    message("--     ${BoldCyan}#### Building the following executables: ####${Reset}")
    foreach( sourcefile ${BINARIES} )
        string(REPLACE ".cc" "" name ${sourcefile})
        message(STATUS "    ${name}")
    endforeach(sourcefile ${BINARIES})
    message("--     ${BoldCyan}#### End ####${Reset}")

    MESSAGE(STATUS " ")
    MESSAGE(STATUS "    ${BoldYellow}DQM UTILS${Reset} [stand-alone]: [${BoldCyan}Ph2_ACF/DQMUtils/CMakeLists.txt${Reset}]. ${BoldGreen}DONE!${Reset}")
    MESSAGE(STATUS "    ${BoldBlue}========================================================================================================${Reset}")
    MESSAGE(STATUS " ")

else()

    MESSAGE(STATUS " ")
    MESSAGE(STATUS "    ${BoldBlue}========================================================================================================${Reset}")
    MESSAGE(STATUS "    ${BoldYellow}DQM UTILS${Reset} [otsdaq]: [${BoldCyan}Ph2_ACF/DQMUtils/CMakeLists.txt${Reset}]. ${BoldRed}Begin...${Reset}")
    MESSAGE(STATUS " ")
    
    cet_make(LIBRARY_NAME Ph2_DQMUtils
            LIBRARIES
            ROOT::Core
    )

    install_headers()
    install_source()

    MESSAGE(STATUS " ")
    MESSAGE(STATUS "    ${BoldYellow}DQM UTILS${Reset} [otsdaq]: [${BoldCyan}Ph2_ACF/DQMUtils/CMakeLists.txt${Reset}]. ${BoldGreen}DONE!${Reset}")
    MESSAGE(STATUS "    ${BoldBlue}========================================================================================================${Reset}")
    MESSAGE(STATUS " ")

endif()
