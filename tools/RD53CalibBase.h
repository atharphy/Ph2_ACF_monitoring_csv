/*!
  \file                  RD53CalibBase.h
  \brief                 Implementaion of CalibBase
  \author                Mauro DINARDO
  \version               1.0
  \date                  28/06/18
  Support:               email to mauro.dinardo@cern.ch
*/

#ifndef RD53CalibBase_H
#define RD53CalibBase_H

#include "HWDescription/RD53.h"
#include "Tool.h"
#include "Utils/Container.h"
#include "Utils/ContainerFactory.h"
#include "Utils/RD53ChannelGroupHandler.h"

#ifdef __USE_ROOT__
#include "TApplication.h"
#endif

// ##################################
// # Basic class for IT calibration #
// ##################################
class CalibBase : public Tool
{
  public:
    void chipErrorReport() const;
    void copyMaskFromDefault(const std::string& which = "all") const;
    void saveChipRegisters(int currentRun, bool doUpdateChip);
    void downloadNewDACvalues(DetectorDataContainer& DACcontainer, const std::string& regName, bool checkAgainst = false, int value = 0);
    void saveSCurveOrGaindValues(const std::vector<DetectorDataContainer*>& detectorContainerVector,
                                 int                                        theCurrentRun,
                                 const std::vector<uint16_t>&               dacList,
                                 size_t                                     offset,
                                 size_t                                     nEvents,
                                 const std::string&                         name);

    virtual void   localConfigure(const std::string& histoFileName = "", int currentRun = -1) = 0;
    virtual void   run()                                                                      = 0;
    virtual void   draw(bool doSaveData = true)                                               = 0;
    virtual size_t getNumberIterations() { return 0; };

    template <typename T>
    void initializeFiles(const std::string& histoFileName, const std::string& calibName, T*& histos, int currentRun = -1, bool saveBinaryData = false)
    {
        theHistoFileName = histoFileName;

        if(saveBinaryData == true)
        {
            this->fDirectoryName = dataOutputDir != "" ? dataOutputDir : RD53Shared::RESULTDIR;
            this->addFileHandler(std::string(this->fDirectoryName) + "/Run" + RD53Shared::fromInt2Str(currentRun) + "_" + calibName + ".raw", 'w');
            this->initializeWriteFileHandler();
        }

        delete histos;
        histos = new T;
    }

  protected:
    std::string theHistoFileName;
    std::string dataOutputDir;

  private:
    virtual void fillHisto() = 0;
};

#endif
