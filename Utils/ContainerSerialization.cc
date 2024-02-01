#include "Utils/ContainerSerialization.h"
#include "Utils/EmptyContainer.h"
#include "Utils/GainFit.h"
#include "Utils/GenericDataArray.h"
#include "Utils/GenericDataVector.h"
#include "Utils/Occupancy.h"
#include "Utils/OccupancyAndPh.h"
#include "Utils/ThresholdAndNoise.h"
#include "Utils/ValueAndTime.h"

BOOST_CLASS_EXPORT_IMPLEMENT(BOOST_IDENTITY_TYPE((ChannelDataContainer<uint8_t>)))
BOOST_CLASS_EXPORT_IMPLEMENT(BOOST_IDENTITY_TYPE((ChannelDataContainer<uint32_t>)))
BOOST_CLASS_EXPORT_IMPLEMENT(BOOST_IDENTITY_TYPE((ChannelDataContainer<float>)))
BOOST_CLASS_EXPORT_IMPLEMENT(BOOST_IDENTITY_TYPE((ChannelDataContainer<Occupancy>)))
BOOST_CLASS_EXPORT_IMPLEMENT(BOOST_IDENTITY_TYPE((ChannelDataContainer<OccupancyAndPh>)))
BOOST_CLASS_EXPORT_IMPLEMENT(BOOST_IDENTITY_TYPE((ChannelDataContainer<ThresholdAndNoise>)))
BOOST_CLASS_EXPORT_IMPLEMENT(BOOST_IDENTITY_TYPE((ChannelDataContainer<EmptyContainer>)))
BOOST_CLASS_EXPORT_IMPLEMENT(BOOST_IDENTITY_TYPE((ChannelDataContainer<GainFit>)))

BOOST_CLASS_EXPORT_IMPLEMENT(BOOST_IDENTITY_TYPE((Summary<Occupancy, Occupancy>)))
BOOST_CLASS_EXPORT_IMPLEMENT(BOOST_IDENTITY_TYPE((Summary<OccupancyAndPh, OccupancyAndPh>)))
BOOST_CLASS_EXPORT_IMPLEMENT(BOOST_IDENTITY_TYPE((Summary<GenericDataVector, OccupancyAndPh>)))
BOOST_CLASS_EXPORT_IMPLEMENT(BOOST_IDENTITY_TYPE((Summary<EmptyContainer, Occupancy>)))
BOOST_CLASS_EXPORT_IMPLEMENT(BOOST_IDENTITY_TYPE((Summary<GainFit, GainFit>)))
BOOST_CLASS_EXPORT_IMPLEMENT(BOOST_IDENTITY_TYPE((Summary<EmptyContainer, float>)))
BOOST_CLASS_EXPORT_IMPLEMENT(BOOST_IDENTITY_TYPE((Summary<EmptyContainer, ValueAndTime<uint16_t>>)))
BOOST_CLASS_EXPORT_IMPLEMENT(BOOST_IDENTITY_TYPE((Summary<ValueAndTime<uint16_t>, EmptyContainer>)))
BOOST_CLASS_EXPORT_IMPLEMENT(BOOST_IDENTITY_TYPE((Summary<EmptyContainer, ValueAndTime<float>>)))
BOOST_CLASS_EXPORT_IMPLEMENT(BOOST_IDENTITY_TYPE((Summary<ValueAndTime<float>, EmptyContainer>)))
BOOST_CLASS_EXPORT_IMPLEMENT(BOOST_IDENTITY_TYPE((Summary<EmptyContainer, std::pair<uint16_t, uint16_t>>)))
BOOST_CLASS_EXPORT_IMPLEMENT(BOOST_IDENTITY_TYPE((Summary<EmptyContainer, double>)))
BOOST_CLASS_EXPORT_IMPLEMENT(BOOST_IDENTITY_TYPE((Summary<ThresholdAndNoise, ThresholdAndNoise>)))
BOOST_CLASS_EXPORT_IMPLEMENT(BOOST_IDENTITY_TYPE((Summary<uint16_t, EmptyContainer>)))
BOOST_CLASS_EXPORT_IMPLEMENT(BOOST_IDENTITY_TYPE((Summary<EmptyContainer, EmptyContainer>)))
BOOST_CLASS_EXPORT_IMPLEMENT(BOOST_IDENTITY_TYPE((Summary<EmptyContainer, uint8_t>)))
BOOST_CLASS_EXPORT_IMPLEMENT(BOOST_IDENTITY_TYPE((Summary<EmptyContainer, uint16_t>)))
BOOST_CLASS_EXPORT_IMPLEMENT(BOOST_IDENTITY_TYPE((Summary<EmptyContainer, std::string>)))
BOOST_CLASS_EXPORT_IMPLEMENT(BOOST_IDENTITY_TYPE((Summary<std::string, EmptyContainer>)))
BOOST_CLASS_EXPORT_IMPLEMENT(BOOST_IDENTITY_TYPE((Summary<std::string, std::string>)))
BOOST_CLASS_EXPORT_IMPLEMENT(BOOST_IDENTITY_TYPE((Summary<uint32_t, uint32_t>)))
BOOST_CLASS_EXPORT_IMPLEMENT(BOOST_IDENTITY_TYPE((Summary<GenericDataArray<VECSIZE, uint16_t>, EmptyContainer>)))
BOOST_CLASS_EXPORT_IMPLEMENT(BOOST_IDENTITY_TYPE((Summary<GenericDataArray<TDCBINS, uint16_t>, EmptyContainer>)))
BOOST_CLASS_EXPORT_IMPLEMENT(BOOST_IDENTITY_TYPE((Summary<GenericDataArray<VECSIZE, GenericDataArray<VECSIZE, uint16_t>>, EmptyContainer>)))
BOOST_CLASS_EXPORT_IMPLEMENT(BOOST_IDENTITY_TYPE((Summary<GenericDataArray<NCHANNELS + 1, uint32_t>, EmptyContainer>)))
BOOST_CLASS_EXPORT_IMPLEMENT(BOOST_IDENTITY_TYPE((Summary<GenericDataArray<HYBRID_CHANNELS_OT + 1, uint32_t>, GenericDataArray<NCHANNELS + 1, uint32_t>>)))
BOOST_CLASS_EXPORT_IMPLEMENT(BOOST_IDENTITY_TYPE((Summary<GenericDataArray<TOTAL_CHANNELS_OT + 1, uint32_t>, GenericDataArray<HYBRID_CHANNELS_OT + 1, uint32_t>>)))
BOOST_CLASS_EXPORT_IMPLEMENT(BOOST_IDENTITY_TYPE((Summary<GenericDataArray_2D<TOTAL_CHANNELS_OT, TOTAL_CHANNELS_OT, uint32_t>, EmptyContainer>)))
BOOST_CLASS_EXPORT_IMPLEMENT(BOOST_IDENTITY_TYPE((Summary<std::vector<double>, EmptyContainer>)))
BOOST_CLASS_EXPORT_IMPLEMENT(BOOST_IDENTITY_TYPE((Summary<std::vector<float>, EmptyContainer>)))
BOOST_CLASS_EXPORT_IMPLEMENT(BOOST_IDENTITY_TYPE((Summary<std::vector<uint16_t>, EmptyContainer>)))
BOOST_CLASS_EXPORT_IMPLEMENT(BOOST_IDENTITY_TYPE((Summary<std::vector<uint8_t>, EmptyContainer>)))

ContainerSerialization::ContainerSerialization(const std::string& calibrationName) : fCalibrationName(calibrationName) {}

ContainerSerialization::~ContainerSerialization() {}

bool ContainerSerialization::attachDeserializer(std::string& inputBuffer)
{
    std::istringstream            inputStream(inputBuffer);
    boost::archive::text_iarchive theArchive(inputStream);
    std::string                   inputCalibrationName;
    theArchive >> inputCalibrationName;
    if(inputCalibrationName != fCalibrationName) return false;
    fStream = std::move(inputBuffer);
    return true;
}
