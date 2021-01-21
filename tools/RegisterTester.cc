#include "RegisterTester.h"
#ifdef __USE_ROOT__

RegisterTester::RegisterTester() : Tool() { fNBadRegisters = 0; }

// D'tor
RegisterTester::~RegisterTester() {}

void RegisterTester::TestRegisters()
{
    // two bit patterns to rest registers with
    uint8_t cFirstBitPattern  = 0xAA;
    uint8_t cSecondBitPattern = 0x55;

    std::ofstream report;
    report.open(fDirectoryName + "/TestReport.txt", std::ofstream::out | std::ofstream::app);
    char line[240];

    for(auto cBoard: *fDetectorContainer)
    {
        for(auto cOpticalGroup: *cBoard)
        {
            for(auto cFe: *cOpticalGroup)
            {
                for(auto cCbc: *cFe)
                {
                    ReadoutChip* theCbc = static_cast<ReadoutChip*>(cCbc);
                    ChipRegMap   cMap   = theCbc->getRegMap();

                    for(const auto& cReg: cMap)
                    {
                        if(!fReadoutChipInterface->WriteChipReg(theCbc, cReg.first, cFirstBitPattern, true))
                        {
                            sprintf(line, "# Writing 0x%.2x to CBC Register %s FAILED.\n", cFirstBitPattern, (cReg.first).c_str());
                            LOG(INFO) << BOLDRED << line << RESET;
                            report << line;
                            fBadRegisters[cCbc->getId()].insert(cReg.first);
                            fNBadRegisters++;
                        }

                        // sleep for 100 ns between register writes
                        std::this_thread::sleep_for(std::chrono::nanoseconds(100));

                        if(!fReadoutChipInterface->WriteChipReg(theCbc, cReg.first, cSecondBitPattern, true))
                        {
                            sprintf(line, "# Writing 0x%.2x to CBC Register %s FAILED.\n", cSecondBitPattern, (cReg.first).c_str());
                            LOG(INFO) << BOLDRED << line << RESET;
                            report << line;
                            fBadRegisters[cCbc->getId()].insert(cReg.first);
                            fNBadRegisters++;
                        }

                        // sleep for 100 ns between register writes
                        std::this_thread::sleep_for(std::chrono::nanoseconds(100));
                    }

                    fBeBoardInterface->ChipReSync(static_cast<BeBoard*>(cBoard));
                }
            }
        }
    }

    report.close();
}

// Reload CBC registers from file found in directory.
// If no directory is given use the default files for the different operational modes found in Ph2_ACF/settings
void RegisterTester::ReconfigureRegisters(std::string pDirectoryName)
{
    for(auto cBoard: *fDetectorContainer)
    {
        fBeBoardInterface->ChipReset(static_cast<BeBoard*>(cBoard));

        for(auto cOpticalGroup: *cBoard)
        {
            for(auto cFe: *cOpticalGroup)
            {
                for(auto cCbc: *cFe)
                {
                    ReadoutChip* theCbc = static_cast<ReadoutChip*>(cCbc);
                    std::string  pRegFile;

                    if(pDirectoryName.empty())
                        pRegFile = "settings/CbcFiles/Cbc_default_electron.txt";
                    else
                    {
                        char buffer[120];
                        sprintf(buffer, "%s/FE%dCBC%d.txt", pDirectoryName.c_str(), cFe->getId(), cCbc->getId());
                        pRegFile = buffer;
                    }

                    theCbc->loadfRegMap(pRegFile);
                    fReadoutChipInterface->ConfigureChip(theCbc);
                    LOG(INFO) << GREEN << "\t\t Successfully (re)configured CBC" << int(cCbc->getId()) << "'s regsiters from " << pRegFile << " ." << RESET;
                }
            }
        }

        fBeBoardInterface->ChipReSync(static_cast<BeBoard*>(cBoard));
    }
}
void RegisterTester::PrintTestReport()
{
    std::ofstream report(fDirectoryName + "/registers_test.txt"); // Creates a file in the current directory
    PrintTestResults(report);
    report.close();
}
void RegisterTester::PrintTestResults(std::ostream& os)
{
    os << "Testing Chip Registers one-by-one with complimentary bit-patterns (0xAA, 0x55)" << std::endl;

    for(const auto& cCbc: fBadRegisters)
    {
        os << "Malfunctioning Registers on Chip " << cCbc.first << " : " << std::endl;

        for(const auto& cReg: cCbc.second) os << cReg << std::endl;
    }

    LOG(INFO) << BOLDBLUE << "Channels diagnosis report written to: " + fDirectoryName + "/registers_test.txt" << RESET;
}

bool RegisterTester::PassedTest()
{
    bool passFlag = ((int)(fNBadRegisters) == 0) ? true : false;
    return passFlag;
}

#endif