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
    CalibBase() : showErrorReport(true) {}
    void    chipErrorReport() const;
    void    copyMaskFromDefault(const std::string& which = "all") const;
    void    saveChipRegisters(int currentRun, bool doUpdateChip);
    void    downloadNewDACvalues(DetectorDataContainer& DACcontainer, const std::vector<const char*>& regNames, bool checkAgainst = false, int value = 0);
    void    saveSCurveOrGaindValues(const std::vector<DetectorDataContainer*>& detectorContainerVector,
                                    int                                        theCurrentRun,
                                    const std::vector<uint16_t>&               dacList,
                                    size_t                                     offset,
                                    size_t                                     nEvents,
                                    const std::string&                         name);
    uint8_t assignGroupType(RD53Shared::INJtype injType) const;
    void    prepareChipQueryForEnDis(const std::string& queryName);

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

    template <typename T>
    void fillVectorContainer(DetectorDataContainer& theDataContainer, const size_t nElements, const T value, const int fromBoardIndx = -1)
    {
        for(const auto cBoard: theDataContainer)
        {
            const auto& theBoard = (fromBoardIndx < 0 ? cBoard : theDataContainer.at(fromBoardIndx));

            for(const auto cOpticalGroup: *theBoard)
                for(const auto cHybrid: *cOpticalGroup)
                    for(const auto cChip: *cHybrid)
                    {
                        cChip->getSummary<std::vector<T>>().clear();
                        for(auto i = 0u; i < nElements; i++) cChip->getSummary<std::vector<T>>().push_back(value);
                    }

            if(fromBoardIndx >= 0) break;
        }
    }

  protected:
    std::string theHistoFileName;
    std::string dataOutputDir;
    bool        showErrorReport;

  private:
    virtual void fillHisto() = 0;
};

#endif
