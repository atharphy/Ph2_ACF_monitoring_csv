/*!
        \file                                            ReadoutChipInterface.h
        \brief                                           User Interface to the Chip, base class for, CBC, MPA, SSA, RD53
        \author                                          Fabio RAVERA
        \version                                         1.0
        \date                        25/02/19
        Support :                    mail to : fabio.ravera@cern.ch
 */

#ifndef __READOUTCHIPINTERFACE_H__
#define __READOUTCHIPINTERFACE_H__

#include "BeBoardFWInterface.h"
#include "ChipInterface.h"
#include <vector>

template <typename T>
class ChannelContainer;
/*!
 * \namespace Ph2_HwInterface
 * \brief Namespace regrouping all the interfaces to the hardware
 */
namespace Ph2_HwInterface
{
using BeBoardFWMap = std::map<uint16_t, BeBoardFWInterface*>; /*!< Map of Board connected */

/*!
 * \class ReadoutChipInterface
 * \brief Class representing the User Interface to the Chip on different boards
 */
class ReadoutChipInterface : public ChipInterface
{
  protected:
    std::map<uint32_t, Ph2_HwDescription::ChipRegMap> fModifiedRegisters;
    std::map<uint16_t, std::string> fMap;
  public:
    /*!
     * \brief Constructor of the ReadoutChipInterface Class
     * \param pBoardMap
     */
    ReadoutChipInterface(const BeBoardFWMap& pBoardMap);

    /*!
     * \brief Destructor of the ReadoutChipInterface Class
     */
    ~ReadoutChipInterface();

    /*!
     * \brief Clear Register Map
     */
    void                          ClearModifiedRegisterMap() { fModifiedRegisters.clear(); }
    Ph2_HwDescription::ChipRegMap GetModifiedRegisterMap(Ph2_HwDescription::Chip* pChip)
    {
        Ph2_HwDescription::ChipRegMap cMap;
        uint32_t                      cChipId = (uint8_t)(pChip->getFrontEndType() == FrontEndType::MPA || pChip->getFrontEndType() == FrontEndType::RD53) << 12;
        cChipId                               = cChipId | pChip->getOpticalId() << 8 | pChip->getHybridId() << 4 | pChip->getId();
        auto cIter                            = fModifiedRegisters.find(cChipId);
        if(cIter != fModifiedRegisters.end())
        {
            // std::cout << "GetModifiedRegisterMap interface --- " << +cChipId << " contains " << cIter->second.size() << " items.\n";
            return cIter->second;
        }
        else
        {
            // std::cout << "GetModifiedRegisterMap interface --- " << +cChipId << " contains " << 0 << " items.\n";
            return cMap;
        }
    }
    void OverwriteModifiedRegisterMap(Ph2_HwDescription::Chip* pChip, Ph2_HwDescription::ChipRegMap pRegMap)
    {
        uint32_t cChipId = (uint8_t)(pChip->getFrontEndType() == FrontEndType::MPA || pChip->getFrontEndType() == FrontEndType::RD53) << 12;
        cChipId          = cChipId | pChip->getOpticalId() << 8 | pChip->getHybridId() << 4 | pChip->getId();
        auto cIter       = fModifiedRegisters.find(cChipId);
        if(cIter != fModifiedRegisters.end()) fModifiedRegisters.erase(cChipId);
        // std::cout << "Overwriting map with an item that has " << pRegMap.size() << " entries.\n";
        fModifiedRegisters[cChipId] = pRegMap;
        // std::cout << "OverwriteModifiedRegisterMap interface --- " << +cChipId << " contains " << fModifiedRegisters[cChipId].size() << " items.\n";
    }
    void UpdateModifiedRegMap(Ph2_HwDescription::Chip* pChip, uint16_t pRegisterAddress)
    {
        if( fMap.size() == 0 )
        {
            for(auto& cRegItem: pChip->getRegMap() )
            {
                fMap[cRegItem.second.fAddress] = cRegItem.first;
            }
        }
        // modified registers map
        uint32_t cChipId = (uint8_t)(pChip->getFrontEndType() == FrontEndType::MPA || pChip->getFrontEndType() == FrontEndType::RD53) << 12;
        cChipId          = cChipId | pChip->getOpticalId() << 8 | pChip->getHybridId() << 4 | pChip->getId();
        auto cMapIter = fModifiedRegisters.find(cChipId);
        if(cMapIter == fModifiedRegisters.end())
        {
            Ph2_HwDescription::ChipRegMap cRegMap;
            fModifiedRegisters[cChipId] = cRegMap;
        }
        cMapIter      = fModifiedRegisters.find(cChipId);
        auto& cModMap = cMapIter->second;
        // if register is in map 
        bool cFound = pChip->getRegMap().find(fMap[pRegisterAddress]) != pChip->getRegMap().end();
        if(cFound)
        {
            auto cRegItem = pChip->getRegItem(fMap[pRegisterAddress]);
            if(cModMap.find(fMap[pRegisterAddress]) == cModMap.end()) { 
                auto cSize = cModMap.size();
                cModMap[fMap[pRegisterAddress]] = cRegItem; 
                LOG (DEBUG) << BOLDMAGENTA << "ReadoutChipInterface - ModMap contained " << cSize << " items....now has " 
                    << cModMap.size() << " items that " << fMap[pRegisterAddress] << " register has been modified"
                    << RESET;
        
            }
        }
    }

    /*!
     * \brief setChannels fo be injected
     * \param pChip: pointer to Chip object
     * \param group: group of channels under test
     * \param pVerifLoop: perform a readback check
     */
    virtual bool setInjectionSchema(Ph2_HwDescription::ReadoutChip* pChip, const ChannelGroupBase* group, bool pVerifLoop = true)
    {
        LOG(ERROR) << BOLDRED << __PRETTY_FUNCTION__ << "\tError: implementation of virtual member function is absent" << RESET;
        return false;
    }

    virtual bool enableInjection(Ph2_HwDescription::ReadoutChip* pChip, bool inject = true, bool pVerifLoop = true)
    {
        LOG(ERROR) << BOLDRED << __PRETTY_FUNCTION__ << "\tError: implementation of virtual member function is absent" << RESET;
        return false;
    }

    virtual bool setInjectionAmplitude(Ph2_HwDescription::ReadoutChip* pChip, uint8_t injectionAmplitude, bool pVerifLoop = true)
    {
        LOG(ERROR) << BOLDRED << __PRETTY_FUNCTION__ << "\tError: implementation of virtual member function is absent" << RESET;
        return false;
    }

    /*!
     * \brief Mask the channels not belonging to the group under test
     * \param pChip: pointer to Chip object
     * \param group: group of channels under test
     * \param pVerifLoop: perform a readback check
     */
    virtual bool maskChannelsGroup(Ph2_HwDescription::ReadoutChip* pChip, const ChannelGroupBase* group, bool pVerifLoop = true)
    {
        LOG(ERROR) << BOLDRED << __PRETTY_FUNCTION__ << "\tError: implementation of virtual member function is absent" << RESET;
        return false;
    }

    /*!
     * \brief mask and inject with one function to increase speed
     * \param pChip: pointer to Chip object
     * \param group: group of channels under test
     * \param mask: mask channel not belonging to the group under test
     * \param inject: inject channels belonging to the group under test
     * \param pVerifLoop: perform a readback check
     */
    virtual bool maskChannelsAndSetInjectionSchema(Ph2_HwDescription::ReadoutChip* pChip, const ChannelGroupBase* group, bool mask, bool inject, bool pVerifLoop = true) = 0;

    /*!
     * \brief Reapply the stored mask for the Chip, use it after group masking is applied
     * \param pChip: pointer to Chip object
     * \param pVerifLoop: perform a readback check
     * \param pBlockSize: the number of registers to be written at once, default is 310
     */
    virtual bool ConfigureChipOriginalMask(Ph2_HwDescription::ReadoutChip* pChip, bool pVerifLoop = true, uint32_t pBlockSize = 310) = 0;

    /*!
     * \brief Write all Local registers on Chip and Chip Config File (able to recognize local parameter names)
     * \param pCbc
     * \param pRegNode : Node of the register to write
     * \param pValue : Value to write
     */
    virtual bool WriteChipAllLocalReg(Ph2_HwDescription::ReadoutChip* pChip, const std::string& dacName, ChipContainer& pValue, bool pVerifLoop = true) = 0;

    /*!
     * \brief Read all Local registers on Chip and Chip Config File (able to recognize local parameter names)
     * \param pCbc
     * \param pRegNode : Node of the register to write
     * \param pValue : Readout value
     */
    virtual void ReadChipAllLocalReg(Ph2_HwDescription::ReadoutChip* pChip, const std::string& dacName, ChipContainer& pValue) {}

    /*!
     * \brief Mask all channels of the chip
     * \param pChip: pointer to Chip object
     * \param mask: if true mask, if false unmask
     * \param pVerifLoop: perform a readback check
     * \param pBlockSize: the number of registers to be written at once, default is 310
     */
    virtual bool MaskAllChannels(Ph2_HwDescription::ReadoutChip* pChip, bool mask, bool pVerifLoop = true) = 0;

    /*!
     * \brief Monitorign memeber functions
     */
    virtual float ReadHybridTemperature(Ph2_HwDescription::ReadoutChip* pChip)
    {
        LOG(ERROR) << BOLDRED << __PRETTY_FUNCTION__ << "\tError: implementation of virtual member function is absent" << RESET;
        return 0;
    }

    virtual float ReadHybridVoltage(Ph2_HwDescription::ReadoutChip* pChip)
    {
        LOG(ERROR) << BOLDRED << __PRETTY_FUNCTION__ << "\tError: implementation of virtual member function is absent" << RESET;
        return 0;
    }

    virtual float ReadChipMonitor(Ph2_HwDescription::ReadoutChip* pChip, const std::string& observableName)
    {
        LOG(ERROR) << BOLDRED << __PRETTY_FUNCTION__ << "\tError: implementation of virtual member function is absent" << RESET;
        return 0;
    }

    virtual int CheckChipID(Ph2_HwDescription::Chip* pChip, const int chipIDfromDB)
    {
        LOG(ERROR) << BOLDRED << __PRETTY_FUNCTION__ << "\tError: implementation of virtual member function is absent" << RESET;
        return 0;
    }
};
} // namespace Ph2_HwInterface

#endif
