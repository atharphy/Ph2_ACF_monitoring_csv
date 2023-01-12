if(NOT DEFINED ENV{OTSDAQ_CMSOUTERTRACKER_DIR})

MESSAGE(STATUS " ")
MESSAGE(STATUS "    ${BoldYellow}========================================================================================================${Reset}")
MESSAGE(STATUS "    ${BoldYellow}MIDDLEWARE${Reset} [stand-alone/middleware]: [${BoldCyan}Ph2_ACF/DQMUtils/CMakeLists.txt${Reset}]. ${BoldRed}Begin...${Reset}")
MESSAGE(STATUS " ")

# Includes
include_directories(${PROJECT_SOURCE_DIR})

# Boost also needs to be linked
include_directories(${Boost_INCLUDE_DIRS})
link_directories(${Boost_LIBRARY_DIRS})
set(LIBS ${LIBS} ${Boost_ANY_LIBRARY} ${Boost_ITERATOR_LIBRARY} Ph2_Parser)

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

cet_set_compiler_flags(
 EXTRA_FLAGS -Wno-reorder -Wl,--undefined -D__OTSDAQ__
)

cet_make(LIBRARY_NAME Ph2_DQMUtils_${Ph2_ACF_Master}
        LIBRARIES
        )

install_headers()
install_source()

endif()
