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

#ifdef __USE_ROOT__

#include "../Utils/CommonVisitors.h"
#include "../Utils/Visitor.h"
#include "Channel.h"
#include "Tool.h"

#include <map>

#include "TCanvas.h"
#include "TGraphErrors.h"
#include "TProfile.h"
#include "TString.h"
#include "TText.h"

using namespace Ph2_System;

// Typedefs for Containers
typedef std::map<uint32_t, std::set<std::string>> BadRegisters;

class RegisterTester : public Tool
{
  public:
    RegisterTester();

    // D'tor
    ~RegisterTester();

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

  private:
    // Containers
    BadRegisters fBadRegisters;

    // Counters
    uint32_t fNBadRegisters;

    // functions/procedures
    void PrintTestResults(std::ostream& os = std::cout);
};
#endif
#endif
