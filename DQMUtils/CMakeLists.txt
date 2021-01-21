if(NOT DEFINED ENV{OTSDAQ_CMSOUTERTRACKER_DIR})

MESSAGE(STATUS " ") 
MESSAGE(STATUS "    ${BoldYellow}========================================================================================================${Reset}") 
MESSAGE(STATUS "    ${BoldYellow}MIDDLEWARE${Reset} [stand-alone/middleware]: [${BoldCyan}Ph2_ACF/DQMUtils/CMakeLists.txt${Reset}]. ${BoldRed}Begin...${Reset}") 
MESSAGE(STATUS " ") 

#includes
include_directories(${UHAL_UHAL_INCLUDE_PREFIX})
include_directories(${CMAKE_CURRENT_SOURCE_DIR})
include_directories(${PROJECT_SOURCE_DIR}/RootUtils)
include_directories(${PROJECT_SOURCE_DIR}/HWDescription)
include_directories(${PROJECT_SOURCE_DIR})

#last but not least, find root and link against it
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


#find source files
file(GLOB SOURCES *.cc)
add_library(Ph2_DQMUtils STATIC ${SOURCES})

file(GLOB BINARIES RELATIVE ${PROJECT_SOURCE_DIR}/RootUtils *.cc)

message("--     ${BoldCyan}#### Building the following executables: ####${Reset}")
foreach( sourcefile ${BINARIES} )
    string(REPLACE ".cc" "" name ${sourcefile})
    message(STATUS "    ${name}")
endforeach(sourcefile ${BINARIES})
message("--     ${BoldCyan}#### End ####${Reset}")

MESSAGE(STATUS " ") 
MESSAGE(STATUS "    ${BoldYellow}MIDDLEWARE${Reset} [stand-alone/middleware]: [${BoldCyan}Ph2_ACF/DQMUtils/CMakeLists.txt${Reset}]. ${BoldGreen}DONE!${Reset}") 
MESSAGE(STATUS "    ${BoldBlue}========================================================================================================${Reset}") 
MESSAGE(STATUS " ") 

else()

include_directories($ENV{OTSDAQ_CMSOUTERTRACKER_DIR}/uhal/uhal_2_7_5/uhal/uhal/include)
include_directories($ENV{OTSDAQ_CMSOUTERTRACKER_DIR}/uhal/uhal_2_7_5/uhal/log/include)
include_directories($ENV{OTSDAQ_CMSOUTERTRACKER_DIR}/uhal/uhal_2_7_5/uhal/grammars/include)

cet_set_compiler_flags(
 EXTRA_FLAGS -Wno-reorder -Wl,--undefined -D__OTSDAQ__
)
 
cet_make(LIBRARY_NAME Ph2_DQMUtils_${Ph2_ACF_Master}
        LIBRARIES
#    Ph2_Utils_${Ph2_ACF_Master}
#    pthread
        )

install_headers()
install_source()
endif()


