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

#include "../Utils/Container.h"
#include "../Utils/Visitor.h"
#include "FrontEndDescription.h"
#include "Module.h"
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
    OpticalGroup(uint8_t pBeId, uint8_t pFMCId, uint8_t pOpticalGroupId);

    // Default C'tor
    OpticalGroup();

    // D'tor
    ~OpticalGroup() { delete flpGBT; };

    /*!
     * \brief acceptor method for HwDescriptionVisitor
     * \param pVisitor
     */
    void accept(HwDescriptionVisitor& pVisitor)
    {
        pVisitor.visitOpticalGroup(*this);

        for(auto* cHybrid: *this) static_cast<Module*>(cHybrid)->accept(pVisitor);
    }

    uint8_t getOpticalGroupId() const { return fOpticalGroupId; };

    void setOpticalGroupId(uint8_t pOpticalGroupId) { fOpticalGroupId = pOpticalGroupId; };

    void addlpGBT(lpGBT* plpGBT) { flpGBT = plpGBT; }

    lpGBT* flpGBT = nullptr;

  protected:
    uint8_t fOpticalGroupId;
};
} // namespace Ph2_HwDescription

#endif
