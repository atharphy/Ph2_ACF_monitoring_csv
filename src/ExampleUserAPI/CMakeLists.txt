cmake_minimum_required (VERSION 3.0.00)
project (UserAPI)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake/")

find_package(Boost REQUIRED)
find_package(ROOT REQUIRED)
find_package(CACTUS REQUIRED)
find_package(Protobuf REQUIRED)

set(CMAKE_CXX_COMPILER_ID "c++")
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_FLAGS "-g -pthread -DELPP_THREAD_SAFE $ENV{UseRootFlag}")

include_directories(${ROOT_INCLUDE_DIRS})
include_directories(${CACTUS_INCLUDEDIR})
include_directories(${Protobuf_INCLUDE_DIRS})
include_directories($ENV{PH2ACF_BASE_DIR})
include_directories(
    $ENV{PH2ACF_BASE_DIR}/HWDescription
    $ENV{PH2ACF_BASE_DIR}/HWInterface
    $ENV{PH2ACF_BASE_DIR}/MonitorUtils
    $ENV{PH2ACF_BASE_DIR}/MessageUtils
    $ENV{PH2ACF_BASE_DIR}/Parser
    $ENV{PH2ACF_BASE_DIR}/NetworkUtils
    $ENV{PH2ACF_BASE_DIR}/System
    $ENV{PH2ACF_BASE_DIR}/Utils
    #$ENV{PH2ACF_BASE_DIR}/miniDAQ
    #$ENV{PH2ACF_BASE_DIR}/RootUtils
    #$ENV{PH2ACF_BASE_DIR}/RootWeb
    #$ENV{PH2ACF_BASE_DIR}/src
    #$ENV{PH2ACF_BASE_DIR}/tools
  )

link_directories($ENV{PH2ACF_BASE_DIR}/bin)
link_directories(${CACTUS_LIBDIR})
link_directories(${Protobuf_LIBRARIES})

add_executable (UserAPI UserAPI.cpp)

set(STATIC_LIBS
  Ph2_Utils
  Ph2_System
  Ph2_Description
  Ph2_Interface
  Ph2_MonitorUtils
  Ph2_Parser
  Ph2_Tools
  Ph2_MonitorDQM
  Ph2_DQMUtils
  Ph2_RootUtils
  RootWeb
  MessageUtils
  NetworkUtils
  )

target_link_libraries(UserAPI
  ${ROOT_LIBRARIES}

  "$<LINK_GROUP:RESCAN,${STATIC_LIBS}>"

  pugixml
  boost_regex
  boost_system
  boost_thread
  boost_iostreams
  boost_serialization
  cactus_uhal_uhal
  cactus_uhal_log
  protobuf
)
