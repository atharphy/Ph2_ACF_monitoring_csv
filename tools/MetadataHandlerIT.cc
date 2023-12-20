#include "tools/MetadataHandlerIT.h"
#ifdef __USE_ROOT__
#include "DQMUtils/DQMMetadataIT.h"
#endif

MetadataHandlerIT::MetadataHandlerIT() 
: MetadataHandler()
{}

MetadataHandlerIT::~MetadataHandlerIT()
{}


void MetadataHandlerIT::initMetadataHardwareSpecific()
{
#ifdef __USE_ROOT__
    fDQMMetadata = new DQMMetadataIT();
#endif
}

void MetadataHandlerIT::fillInitalConditionsHardwareSpecific()
{

}

void MetadataHandlerIT::fillFinalConditionsHardwareSpecific()
{

}
