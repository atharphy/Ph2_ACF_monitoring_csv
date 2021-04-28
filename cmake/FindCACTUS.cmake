# - Try to find cactus

#macro(find_cactus_in_extern arg)
# find_path(CACTUS_ROOT uhal.hpp
#    HINTS ${PROJECT_SOURCE_DIR}/extern/cactus ${arg})
#endmacro()

#find_cactus_in_extern("")

#set(CACTUS_ROOT ${PROJECT_SOURCE_DIR}/extern/cactus )

file(GLOB_RECURSE uhal_include $ENV{CACTUSROOT}*uhal.hpp)
if(uhal_include)
    set(CACTUS_ROOT "$ENV{CACTUSROOT}")
    set(CACTUS_FOUND TRUE)
    #MESSAGE(STATUS "Found uHAL in ${CACTUS_ROOT}")
    file(GLOB_RECURSE amc13_include ${CACTUS_ROOT}/amc13/*AMC13.hh)
    if(amc13_include)
        set(CACTUS_AMC13_FOUND TRUE)
        #message(STATUS "Found AMC13 compoenent in ${CACTUS_ROOT}")
    else(amc13_include)
        set(CACTUS_AMC13_FOUND FALSE)
        #message(STATUS "Could NOT find AMC13 compoenent")
    endif(amc13_include)
else(uhal_include)
file(GLOB_RECURSE extern_file ${PROJECT_SOURCE_DIR}/extern/*uhal.hpp)
if (extern_file)
    # strip the file and 'include' path away:
    get_filename_component(extern_lib_path "${extern_file}" PATH)
    get_filename_component(extern_lib_path "${extern_lib_path}" PATH)
    get_filename_component(extern_lib_path "${extern_lib_path}" PATH)
    #MESSAGE(STATUS "Found CACTUS package in 'extern' subfolder: ${extern_lib_path}")
    set(CACTUS_ROOT ${extern_lib_path})
    set(CACTUS_FOUND TRUE)
endif(extern_file)
endif(uhal_include)



# could not find the package at the usual locations -- try to copy from AFS if accessible
if (NOT CACTUS_ROOT)
  MESSAGE(WARNING "Could not find CACTUS package required by Ph2_ACF. Please refer to the documentation on how to obtain the software.")
  set(CACTUS_FOUND FALSE)
endif()

#set(EXTERN_ERLANG_PREFIX ${CACTUS_ROOT}/lib/erlang )
#set(EXTERN_ERLANG_BIN_PREFIX ${EXTERN_ERLANG_PREFIX}/bin )

#set(EXTERN_BOOST_PREFIX ${CACTUS_ROOT} )
#set(EXTERN_BOOST_INCLUDE_PREFIX ${EXTERN_BOOST_PREFIX}/include )
#set(EXTERN_BOOST_LIB_PREFIX ${EXTERN_BOOST_PREFIX}/lib )

#set(EXTERN_PUGIXML_PREFIX ${CACTUS_ROOT}  )
#set(EXTERN_PUGIXML_INCLUDE_PREFIX ${EXTERN_PUGIXML_PREFIX}/include/pugixml )
#set(EXTERN_PUGIXML_LIB_PREFIX ${EXTERN_PUGIXML_PREFIX}/lib )

set(UHAL_GRAMMARS_PREFIX ${CACTUS_ROOT} )
set(UHAL_GRAMMARS_INCLUDE_PREFIX $ENV{CACTUSGRAMMARINCLUDE} )
set(UHAL_GRAMMARS_LIB_PREFIX $ENV{CACTUSGRAMMARLIB} )

set(UHAL_LOG_PREFIX ${CACTUS_ROOT} )
set(UHAL_LOG_INCLUDE_PREFIX $ENV{CACTUSLOGINCLUDE} )
set(UHAL_LOG_LIB_PREFIX $ENV{CACTUSLOGLIB} )

set(UHAL_UHAL_PREFIX ${CACTUS_ROOT} )
set(UHAL_UHAL_INCLUDE_PREFIX $ENV{CACTUSINCLUDE} )
set(UHAL_UHAL_LIB_PREFIX $ENV{CACTUSLIB} )

if(${CACTUS_AMC13_FOUND})
    set(UHAL_AMC13_PREFIX ${CACTUS_ROOT})
    set(UHAL_AMC13_INCLUDE_PREFIX $ENV{CACTUSINCLUDE}/amc13 )
    set(UHAL_AMC13_LIB_PREFIX $ENV{CACTUSLIB} )
endif(${CACTUS_AMC13_FOUND})

include(FindPackageHandleStandardArgs)
# handle the QUIETLY and REQUIRED arguments and set ZESTSC1_FOUND to TRUE
# if all listed variables are TRUE
find_package_handle_standard_args(CACTUS  DEFAULT_MSG CACTUS_ROOT)
find_package_handle_standard_args(AMC13  DEFAULT_MSG CACTUS_AMC13_FOUND)

mark_as_advanced(CACTUS_ROOT UHAL_AMC13_PREFIX)
