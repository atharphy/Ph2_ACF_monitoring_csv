#include "tools/ConfigureOnly.h"
#include "System/RegisterHelper.h"

ConfigureOnly::ConfigureOnly() : Tool() {}

ConfigureOnly::~ConfigureOnly() {}

void ConfigureOnly::Running()
{
    // fRegisterHelper->dumpBeBoardRegisterIntoXml("Results/outputTest.xml");
}

void ConfigureOnly::Stop() {}

void ConfigureOnly::Pause() {}

void ConfigureOnly::Resume() {}
