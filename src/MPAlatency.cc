#include "../HWDescription/BeBoard.h"
#include "../HWDescription/Chip.h"
#include "../HWDescription/Definition.h"
#include "../HWDescription/Hybrid.h"
#include "../HWInterface/BeBoardInterface.h"
#include "../HWInterface/ChipInterface.h"
#include "../HWInterface/ReadoutChipInterface.h"
#include "../Utils/Timer.h"
#include "../Utils/argvparser.h"
#include "../tools/LatencyScan.h"
#include "TApplication.h"
#include "TROOT.h"
#include "tools/BackEndAlignment.h"
#include <cstring>

using namespace Ph2_HwDescription;
using namespace Ph2_HwInterface;
using namespace Ph2_System;
using namespace CommandLineProcessing;

INITIALIZE_EASYLOGGINGPP

int main(int argc, char* argv[])
{
    // configure the logger
    el::Configurations conf(std::string(std::getenv("PH2ACF_BASE_DIR")) + "/settings/logger.conf");
    el::Loggers::reconfigureAllLoggers(conf);

    ArgvParser cmd;

    // init
    cmd.setIntroductoryDescription("CMS Ph2_ACF  calibration routine using K. Uchida's algorithm or a fast algorithm");
    // error codes
    cmd.addErrorCode(0, "Success");
    cmd.addErrorCode(1, "Error");
    // options
    cmd.setHelpOption("h", "help", "Print this help page");

    cmd.defineOption("file", "Hw Description File", ArgvParser::OptionRequiresValue);
    cmd.defineOptionAlternative("file", "f");
    cmd.defineOption("batch", "Run the application in batch mode", ArgvParser::NoOptionAttribute);
    cmd.defineOptionAlternative("batch", "b");

    bool batchMode = (cmd.foundOption("batch")) ? true : false;

    int result = cmd.parse(argc, argv);

    if(result != ArgvParser::NoParserError)
    {
        LOG(INFO) << cmd.parseErrorDescription(result);
        exit(1);
    }

    // now query the parsing results
    std::string cHWFile = (cmd.foundOption("file")) ? cmd.optionValue("file") : "settings/D19C_MPA_PreCalibSYNC.xml";

    TApplication cApp("Root Application", &argc, argv);

    Timer t;

    // create a genereic Tool Object, I can then construct all other tools from that using the Inherit() method
    // this tool stays on the stack and lives until main finishes - all other tools will update the HWStructure from
    // cTool
    Tool              cTool;
    std::stringstream outp;
    cTool.InitializeHw(cHWFile, outp);
    cTool.InitializeSettings(cHWFile, outp);
    LOG(INFO) << outp.str();
    outp.str("");
    cTool.ConfigureHw();
    cTool.CreateResultDirectory("Latency");
    cTool.InitResultFile("Latency");

    if(batchMode) gROOT->SetBatch(true);
    BeBoard* pBoard = static_cast<BeBoard*>(cTool.fDetectorContainer->at(0));
    pBoard->setFrontEndType(FrontEndType::MPA);

    // cTool.StartHttpServer();

    // align back-end .. if this moves to firmware then we can get rid of this step
    BackEndAlignment cBackEndAligner;
    cBackEndAligner.Inherit(&cTool);
    cBackEndAligner.Initialise();
    bool cAligned = cBackEndAligner.Align();
    cBackEndAligner.resetPointers();
    if(!cAligned)
    {
        LOG(ERROR) << BOLDRED << "Failed to align back-end" << RESET;
        exit(0);
    }

    LOG(INFO) << BOLDRED << "LatencyScan" << RESET;
    LatencyScan cLatencyScan;
    LOG(INFO) << BOLDRED << "Inherit" << RESET;
    cLatencyScan.Inherit(&cTool);
    LOG(INFO) << BOLDRED << "INIT" << RESET;
    cLatencyScan.Initialize(0, 100);
    LOG(INFO) << BOLDRED << "Scan" << RESET;
    pBoard->setFrontEndType(FrontEndType::MPA);

    std::vector<int> rows{5, 74, 51, 98, 20};
    std::vector<int> cols{1};

    for(auto cOpticalGroup: *pBoard)
    {
        for(auto cFe: *cOpticalGroup)
        {
            for(auto cChip: *cFe)
            {
                if(cChip->getFrontEndType() == FrontEndType::MPA)
                {
                    static_cast<MPAInterface*>(cTool.fReadoutChipInterface)->Set_calibration(cChip, 50);
                    static_cast<MPAInterface*>(cTool.fReadoutChipInterface)->Set_threshold(cChip, 100);
                    static_cast<MPAInterface*>(cTool.fReadoutChipInterface)->Activate_sync(cChip);
                    // static_cast<MPAInterface*>(cTool.fReadoutChipInterface)->WriteChipReg(cChip, "ENFLAGS_ALL", 0x57);
                    static_cast<MPAInterface*>(cTool.fReadoutChipInterface)->WriteChipReg(cChip, "ENFLAGS_ALL", 0x0);
                    static_cast<MPAInterface*>(cTool.fReadoutChipInterface)->WriteChipReg(cChip, "ClusterCut_ALL", 2);
                    for(auto rr: rows)
                    {
                        for(auto cc: cols)
                        {
                            auto pngl = static_cast<MPA*>(cChip)->PNglobal(std::pair<uint32_t, uint32_t>{cc, rr});
                            static_cast<MPAInterface*>(cTool.fReadoutChipInterface)->WriteChipReg(cChip, "ENFLAGS_P" + std::to_string(pngl), 0x57);
                        }
                    }
                }
                if(cChip->getFrontEndType() == FrontEndType::SSA) { static_cast<SSAInterface*>(cTool.fReadoutChipInterface)->WriteChipReg(cChip, "ENFLAGS", 0); }
            }
        }
    }

    auto hitlatmpa = cLatencyScan.ScanLatency(30, 50);

    for(auto cOpticalGroup: *pBoard)
    {
        for(auto cFe: *cOpticalGroup)
        {
            for(auto cChip: *cFe)
            {
                if(cChip->getFrontEndType() == FrontEndType::MPA) { static_cast<MPAInterface*>(cTool.fReadoutChipInterface)->WriteChipReg(cChip, "ENFLAGS_ALL", 0); }
                if(cChip->getFrontEndType() == FrontEndType::SSA)
                {
                    static_cast<SSAInterface*>(cTool.fReadoutChipInterface)->WriteChipReg(cChip, "FE_Calibration", 1);
                    static_cast<SSAInterface*>(cTool.fReadoutChipInterface)->WriteChipReg(cChip, "ENFLAGS", 0);
                    static_cast<SSAInterface*>(cTool.fReadoutChipInterface)->WriteChipReg(cChip, "Bias_CALDAC", 120);
                    static_cast<SSAInterface*>(cTool.fReadoutChipInterface)->WriteChipReg(cChip, "Bias_THDAC", 60);

                    for(auto rr: rows) { static_cast<SSAInterface*>(cTool.fReadoutChipInterface)->WriteChipReg(cChip, "ENFLAGS_S" + std::to_string(rr + 1), 19); }
                }
            }
        }
    }

    cLatencyScan.Initialize(0, 100);
    auto hitlatssa = cLatencyScan.ScanLatency(30, 50);

    for(auto cOpticalGroup: *pBoard)
    {
        for(auto cFe: *cOpticalGroup)
        {
            for(auto cChip: *cFe)
            {
                if(cChip->getFrontEndType() == FrontEndType::MPA)
                {
                    uint8_t hitlmpa = hitlatmpa[cFe];
                    LOG(INFO) << BOLDRED << "Hit max " << +hitlmpa << RESET;
                    static_cast<MPAInterface*>(cTool.fReadoutChipInterface)->WriteChipReg(cChip, "L1Offset_1_ALL", (0x00FF & hitlmpa) >> 0);
                    static_cast<MPAInterface*>(cTool.fReadoutChipInterface)->WriteChipReg(cChip, "L1Offset_2_ALL", (0x0100 & hitlmpa) >> 8);
                }

                if(cChip->getFrontEndType() == FrontEndType::SSA)
                {
                    uint8_t hitlssa = hitlatssa[cFe];
                    LOG(INFO) << BOLDRED << "Hit max " << +hitlssa << RESET;
                    static_cast<SSAInterface*>(cTool.fReadoutChipInterface)->WriteChipReg(cChip, "L1-Latency_LSB", (0x00FF & hitlssa) >> 0);
                    static_cast<SSAInterface*>(cTool.fReadoutChipInterface)->WriteChipReg(cChip, "L1-Latency_MSB", (0x0100 & hitlssa) >> 8);
                }
            }
        }
    }

    for(size_t irt = 0; irt < 10; irt++)
    {
        LOG(INFO) << BOLDRED << "RetimePix " << irt << RESET;
        bool found = false;
        for(auto cOpticalGroup: *pBoard)
        {
            for(auto cFe: *cOpticalGroup)
            {
                for(auto cChip: *cFe)
                {
                    if(cChip->getFrontEndType() == FrontEndType::MPA)
                    {
                        static_cast<MPAInterface*>(cTool.fReadoutChipInterface)->Activate_ps(cChip);
                        static_cast<MPAInterface*>(cTool.fReadoutChipInterface)->WriteChipReg(cChip, "ENFLAGS_ALL", 0x0);
                        static_cast<MPAInterface*>(cTool.fReadoutChipInterface)->WriteChipReg(cChip, "ClusterCut_ALL", 4);
                        static_cast<MPAInterface*>(cTool.fReadoutChipInterface)->WriteChipReg(cChip, "RetimePix", irt);
                        for(auto rr: rows)
                        {
                            for(auto cc: cols)
                            {
                                auto pngl = static_cast<MPA*>(cChip)->PNglobal(std::pair<uint32_t, uint32_t>{cc, rr});
                                static_cast<MPAInterface*>(cTool.fReadoutChipInterface)->WriteChipReg(cChip, "ENFLAGS_P" + std::to_string(pngl), 0x57);
                            }
                        }
                    }
                }
            }
        }

        auto stublat = cLatencyScan.ScanStubLatency(0, 50);

        for(auto cOpticalGroup: *pBoard)
        {
            for(auto cFe: *cOpticalGroup)
            {
                if(stublat[cFe] != 0)
                {
                    found = true;
                    LOG(INFO) << BOLDRED << "Stub max " << unsigned(stublat[cFe]) << " RetimePix " << irt << RESET;
                    cTool.fBeBoardInterface->WriteBoardReg(pBoard, "fc7_daq_cnfg.readout_block.global.common_stubdata_delay", stublat[cFe]);
                }
            }
        }

        if(found) break;
    }

    LOG(INFO) << BOLDRED << "Done" << RESET;

    cTool.Destroy();

    return 0;
}
