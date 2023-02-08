#include "Utils/ContainerSerialization.h"
#include "Utils/Occupancy.h" 
#include "Utils/ThresholdAndNoise.h" 
#include "Utils/EmptyContainer.h" 

BOOST_CLASS_EXPORT_IMPLEMENT(BOOST_IDENTITY_TYPE((ChannelDataContainer<Occupancy>)))
BOOST_CLASS_EXPORT_IMPLEMENT(BOOST_IDENTITY_TYPE((ChannelDataContainer<ThresholdAndNoise>)))
BOOST_CLASS_EXPORT_IMPLEMENT(BOOST_IDENTITY_TYPE((ChannelDataContainer<uint8_t>)))

BOOST_CLASS_EXPORT_IMPLEMENT(BOOST_IDENTITY_TYPE((Summary<Occupancy,Occupancy>)))
BOOST_CLASS_EXPORT_IMPLEMENT(BOOST_IDENTITY_TYPE((Summary<ThresholdAndNoise,ThresholdAndNoise>)))
BOOST_CLASS_EXPORT_IMPLEMENT(BOOST_IDENTITY_TYPE((Summary<uint16_t,EmptyContainer>)))
BOOST_CLASS_EXPORT_IMPLEMENT(BOOST_IDENTITY_TYPE((Summary<EmptyContainer,uint8_t>)))
BOOST_CLASS_EXPORT_IMPLEMENT(BOOST_IDENTITY_TYPE((Summary<EmptyContainer,EmptyContainer>)))

ContainerSerialization::ContainerSerialization(const std::string& calibrationName)
: fCalibrationName(calibrationName) {}

ContainerSerialization::~ContainerSerialization(){}

bool ContainerSerialization::attachDeserializer(std::string& inputBuffer)
{
    std::istringstream inputStream(inputBuffer);
    boost::archive::text_iarchive theArchive(inputStream);
    std::string inputCalibrationName;
    theArchive >> inputCalibrationName;
    if(inputCalibrationName != fCalibrationName) return false;
    fStream = std::move(inputBuffer);
    return true;
}

