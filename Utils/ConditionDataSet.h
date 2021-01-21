#ifndef _CONDITIONDATASET_H__
#define _CONDITIONDATASET_H__

#include <iostream>
#include <vector>

#include "../HWDescription/Definition.h"

struct CondDataItem
{
  public:
    std::string fRegName;
    uint8_t     fUID;
    uint8_t     fFeId;
    uint8_t     fCbcId;
    uint8_t     fPage;
    uint8_t     fRegister;
    uint32_t    fValue;

    CondDataItem(std::string pRegName, uint8_t pUID, uint8_t pFeId, uint8_t pCbcId, uint8_t pPage = 0, uint8_t pRegister = 0, uint32_t pValue = 0)
        : fRegName(pRegName), fUID(pUID), fFeId(pFeId), fCbcId(pCbcId), fPage(pPage), fRegister(pRegister), fValue(pValue)
    {
    }

    CondDataItem() : fRegName(""), fUID(0), fFeId(0), fCbcId(0), fPage(0), fRegister(0), fValue(0) {}
};

class ConditionDataSet
{
  public:
    SLinkDebugMode            fDebugMode;
    std::vector<CondDataItem> fCondDataVector;
    bool                      fHasI2C;
    bool                      fHasTDC;

    ConditionDataSet() : fDebugMode(SLinkDebugMode::SUMMARY) { fCondDataVector.clear(); }

    ConditionDataSet(SLinkDebugMode pMode, bool pCondData) : fDebugMode(pMode) { fCondDataVector.clear(); }

    void setDebugMode(SLinkDebugMode pMode) { fDebugMode = pMode; }

    SLinkDebugMode getDebugMode() { return fDebugMode; }

    void addCondData(std::string pRegName, uint8_t pUID, uint8_t pFeId, uint8_t pCbcId, uint8_t pPage = 0, uint8_t pRegister = 0, uint32_t pValue = 0)
    {
        if(pUID == 1)
            fHasI2C = true;
        else if(pUID == 3)
            fHasTDC = true;

        fCondDataVector.emplace_back(pRegName, pUID, pFeId, pCbcId, pPage, pRegister, pValue);
    }

    bool getCondDataEnabled()
    {
        if(fCondDataVector.size() != 0)
            return true;
        else
            return false;
    }

    bool testEffort() { return fHasI2C || fHasTDC; }
};

#endif
