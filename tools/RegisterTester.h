/*!
 *
 * \file RegeristerTest.h
 * \brief Register Tester  class
 * \author Sarah SEIF EL NASR_STOREY
 * \date 19 / 10 / 16
 *
 * \Support : sarah.storey@cern.ch
 *
 */

#ifndef RegisterTester_h__
#define RegisterTester_h__


#include "../Utils/Container.h"
#include "../Utils/ContainerFactory.h"
#include "../Utils/ContainerRecycleBin.h"
#include "Tool.h"

#ifdef __USE_ROOT__
#include "TCanvas.h"
#include "TGraphErrors.h"
#include "TProfile.h"
#include "TString.h"
#include "TText.h"
#endif

using namespace Ph2_System;

// Typedefs for Containers
typedef std::map<uint32_t, std::set<std::string>> BadRegisters;
typedef std::pair<std::string, Ph2_HwDescription::ChipRegItem> Register; 
typedef std::vector<Register> Registers; 

class RegisterTester : public Tool
{
  public:
    RegisterTester();

    // D'tor
    ~RegisterTester();

    void Initialise();
    // Test registers for hybrid test test 
    void RegisterTest(); 


    // Reload CBC registers from file found in directory.
    // If no directory is given use the default files for the different operational modes found in Ph2_ACF/settings
    void ReconfigureRegisters(std::string pDirectoryName = "");
    //
    void TestRegisters();
    // Print test results to a text file in the results directory : registers_test.txt
    void PrintTestReport();

    // Get number of registers which failed the test
    int GetNumFails() { return fNBadRegisters; };

    // Return true if all the CBCs passed the register check.
    bool PassedTest();


    void print(std::vector<uint8_t> pChipIds);
    void Running() override;
    void Stop() override;
    void Pause() override;
    void Resume() override;
    void writeObjects();
    void Reset();
    void initializeRecycleBin() { fRecycleBin.setDetectorContainer(fDetectorContainer); }

  private:
    // timing
    std::chrono::seconds::rep fStartTime;
    std::chrono::seconds::rep fStopTime;

    // Containers
    BadRegisters fBadRegisters;
    ContainerRecycleBin<uint8_t> fRecycleBin;

    // Counters
    uint32_t fNBadRegisters;

    // functions/procedures
    void PrintTestResults(std::ostream& os = std::cout);

    struct
    {
        bool operator()(Register a, Register b) const { return a.second.fPage < b.second.fPage; }
    } customLessThanPage;
    struct
    {
        bool operator()(Register a, Register b) const { return a.second.fPage > b.second.fPage; }
    } customGreaterThanPage;
    
};
#endif
