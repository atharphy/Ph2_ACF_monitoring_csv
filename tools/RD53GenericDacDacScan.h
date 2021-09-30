/*!
  \file                  RD53GenericDacDacScan.h
  \brief                 Implementaion of a generic DAC-DAC scan
  \author                Mauro DINARDO
  \version               1.0
  \date                  28/05/21
  Support:               email to mauro.dinardo@cern.ch
*/

#ifndef RD53GenericDacDacScan_H
#define RD53GenericDacDacScan_H

#include "RD53PixelAlive.h"

#ifdef __USE_ROOT__
#include "../DQMUtils/RD53GenericDacDacScanHistograms.h"
#endif

// ##############################
// # Generic DAC-DAC test suite #
// ##############################
class GenericDacDacScan : public PixelAlive
{
  public:
    ~GenericDacDacScan()
    {
#ifdef __USE_ROOT__
        this->WriteRootFile();
        this->CloseResultFile();
#endif
    }

    void Running() override;
    void Stop() override;
    void ConfigureCalibration() override;
    void sendData() override;

    void   localConfigure(const std::string fileRes_, int currentRun);
    void   initializeFiles(const std::string fileRes_, int currentRun);
    void   run();
    void   draw();
    void   analyze();
    size_t getNumberIterations() { return PixelAlive::getNumberIterations() * ((stopValueDAC1 - startValueDAC1) / stepDAC1 + 1) * ((stopValueDAC2 - startValueDAC2) / stepDAC2 + 1); }
    void   saveChipRegisters(int currentRun);

#ifdef __USE_ROOT__
    GenericDacDacScanHistograms* histos;
#endif

  private:
    std::string regNameDAC1;
    size_t      startValueDAC1;
    size_t      stopValueDAC1;
    size_t      stepDAC1;
    std::string regNameDAC2;
    size_t      startValueDAC2;
    size_t      stopValueDAC2;
    size_t      stepDAC2;

    std::vector<uint16_t> dac1List;
    std::vector<uint16_t> dac2List;

    DetectorDataContainer theOccContainer;
    DetectorDataContainer theGenericDacDacScanContainer;

    void fillHisto();
    void scanDacDac(const std::string& regNameDAC1, const std::string& regNameDAC2, const std::vector<uint16_t>& dac1List, const std::vector<uint16_t>& dac2List, DetectorDataContainer* theContainer);
    void chipErrorReport() const;

  protected:
    std::string fileRes;
    int         theCurrentRun;
    bool        doUpdateChip;
    bool        doDisplay;
    bool        saveBinaryData;
    bool        isDAC1ChipReg;
    bool        isDAC2ChipReg;
};

#endif
