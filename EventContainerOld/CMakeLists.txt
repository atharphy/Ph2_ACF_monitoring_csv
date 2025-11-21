if(NOT DEFINED ENV{OTSDAQ_CMSTRACKER_DIR})

    if(NOT DEFINED ENV{OTSDAQ_CMSTRACKER_DIR})
        include_directories(${PROJECT_SOURCE_DIR})

        cmake_minimum_required(VERSION 3.17)
        project(EventContainer)

        set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/")
        find_package(ROOT)
    endif()

    # # Find root and link against it
    if(${ROOT_FOUND})
        include_directories(${ROOT_INCLUDE_DIRS})
    endif()

    add_custom_command(
        OUTPUT G__Ph2_EventDict.cxx
        COMMAND rootcling
            -f G__Ph2_EventDict.cxx
            -rml libPh2_EventDict.so
            -rmf libPh2_EventDict.rootmap
            ${CMAKE_CURRENT_SOURCE_DIR}/OTEventContainers.h
            ${CMAKE_CURRENT_SOURCE_DIR}/LinkDef.h
        DEPENDS OTEventContainers.h LinkDef.h
    )
    
    add_library(Ph2_EventDict SHARED G__Ph2_EventDict.cxx)
    
    target_link_libraries(Ph2_EventDict PRIVATE ${ROOT_LIBRARIES})
    set(DICT_LIB_OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/lib/libPh2_EventDict.so)
    set(DICT_PCM_OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/lib/G__Ph2_EventDict_rdict.pcm)

    # Declare dictionary copy outputs as real build outputs
    add_custom_command(
        OUTPUT ${DICT_LIB_OUTPUT} ${DICT_PCM_OUTPUT}
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_FILE:Ph2_EventDict>
                ${DICT_LIB_OUTPUT}
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                ${CMAKE_CURRENT_BINARY_DIR}/G__Ph2_EventDict_rdict.pcm
                ${DICT_PCM_OUTPUT}
        DEPENDS Ph2_EventDict
        COMMENT "Generating dictionary outputs in EventContainer/lib"
    )

    add_custom_target(copy-dict ALL
        DEPENDS ${DICT_LIB_OUTPUT} ${DICT_PCM_OUTPUT}
    )

else()

endif()