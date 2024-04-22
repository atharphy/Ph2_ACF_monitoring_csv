/*!
        \file                           OpticalGroup.h
        \brief                          OpticalGroup Description class
        \author                         Fabio Ravera
        \version                        1.0
        \date                           02/04/20
        Support :                       mail to : fabio.ravera@cern.ch
 */

#ifndef OpticalGroup_H
#define OpticalGroup_H

#include "FrontEndDescription.h"
#include "Hybrid.h"
#include "Utils/Container.h"
#include "Utils/Visitor.h"
#include "lpGBT.h"

#include <vector>

/*!
 * \namespace Ph2_HwDescription
 * \brief Namespace regrouping all the hardware description
 */
namespace Ph2_HwDescription
{
/*!
 * \class OpticalGroup
 * \brief handles a vector of Chip which are connected to the OpticalGroup
 */
class OpticalGroup
    : public FrontEndDescription
    , public OpticalGroupContainer
{
  public:
    // C'tors take FrontEndDescription or hierachy of connection
    OpticalGroup(const FrontEndDescription& pFeDesc, uint8_t pOpticalGroupId);
    OpticalGroup(uint8_t pBeBoardId, uint8_t pFMCId, uint8_t pOpticalGroupId);

    // Default C'tor
    OpticalGroup();

    OpticalGroup(const OpticalGroup&) = delete;

    // D'tor
    ~OpticalGroup() { delete flpGBT; };

    /*!
     * \brief acceptor method for HwDescriptionVisitor
     * \param pVisitor
     */
    void accept(HwDescriptionVisitor& pVisitor)
    {
        pVisitor.visitOpticalGroup(*this);

        for(auto* cHybrid: *this) static_cast<Hybrid*>(cHybrid)->accept(pVisitor);
    }
    void addlpGBT(lpGBT* plpGBT) { flpGBT = plpGBT; }

    lpGBT* flpGBT    = nullptr;
    bool   fIsLocked = false;

    std::pair<uint8_t, uint16_t> getStubCnfg() { return std::make_pair(fStubPackageDelay, fStubLatency); }
    void                         setStubCnfg(std::pair<uint8_t, uint16_t> pCnfg)
    {
        fStubPackageDelay = pCnfg.first;
        fStubLatency      = pCnfg.second;
    }

    void addNTC(std::string cNTCType, std::string cNTCADC, std::string cNTCLookUpTable) { fNTCMap.insert(std::make_pair(cNTCType, std::make_pair(cNTCADC, cNTCLookUpTable))); }

    std::map<std::string, std::pair<std::string, std::string>> getNTCMap() const { return fNTCMap; }

    std::map<std::string, std::pair<std::string, std::string>> fNTCMap;

    std::map<uint8_t, std::vector<uint8_t>>            getLpGBTrxGroupsAndChannels() const;
    std::map<std::pair<uint8_t, uint8_t>, std::string> getLpGBTrxGroupsAndChannelsPerHybrid() const;

  protected:
    uint8_t  fStubPackageDelay{0};
    uint16_t fStubLatency{0};

  private:
};
} // namespace Ph2_HwDescription

#endif
