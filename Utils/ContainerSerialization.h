#ifndef __CONTAINER_SERIALIZATION__
#define __CONTAINER_SERIALIZATION__


#include <boost/utility/identity_type.hpp>
#include <boost/serialization/export.hpp>
#include "Utils/DataContainer.h"

class Occupancy;
BOOST_CLASS_EXPORT_KEY(BOOST_IDENTITY_TYPE((Summary<Occupancy,Occupancy>)))
BOOST_CLASS_EXPORT_KEY(BOOST_IDENTITY_TYPE((ChannelDataContainer<Occupancy>)))

#include "Utils/Occupancy.h"
#include <iostream>


template<uint N>
struct Serialize
{
    template<class Archive, typename... Args>
    static void serialize(Archive & theArchive, std::tuple<Args...> & theTuple)
    {
        theArchive & std::get<N-1>(theTuple);
        Serialize<N-1>::serialize(theArchive, theTuple);
    }
};

template<>
struct Serialize<0>
{
    template<class Archive, typename... Args>
    static void serialize(Archive & theArchive, std::tuple<Args...> & theTuple)
    {
        (void) theArchive;
        (void) theTuple;
    }
};

template<class Archive, typename... Args>
void serialize(Archive & theArchive, std::tuple<Args...> & theTuple)
{
    Serialize<sizeof...(Args)>::serialize(theArchive, theTuple);
}


class ContainerSerialization
{
  public:
    ContainerSerialization();
    ~ContainerSerialization();

    template<typename T, typename... Args>
    std::string serializeContainer(const std::string& calibrationName, T& theInputContainer, Args&... extraArguments) const
    {
        std::ostringstream ouputStream;
        boost::archive::text_oarchive theArchive(ouputStream);

        theArchive << calibrationName;
        theArchive << theInputContainer;

        auto extraArgumentTuple = std::tuple<Args&...>(extraArguments...);
        serialize(theArchive, extraArgumentTuple);
        return ouputStream.str();
    }

    template<typename T,  typename... Args>
    bool deserializeIntoContainer(const std::string& inputBuffer, const std::string& calibrationName, T& theOutputContainer, Args&... extraArguments) const
    {
        std::istringstream inputStream(inputBuffer);
        boost::archive::text_iarchive theArchive(inputStream);
        std::string inputCalibrationName;
        theArchive >> inputCalibrationName;
        if(inputCalibrationName != calibrationName) return false;
        theArchive >> theOutputContainer;
        auto extraArgumentTuple = std::tuple<Args&...>(extraArguments...);
        serialize(theArchive, extraArgumentTuple);
        return true;
    }

};


#endif