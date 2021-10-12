/*!

        \file                           Hybrid.h
        \brief                          Hybrid Description class
        \author                         Lorenzo BIDEGAIN
        \version                        1.0
        \date                           25/06/14
        Support :                       mail to : lorenzo.bidegain@gmail.com

 */

#ifndef Hybrid_h__
#define Hybrid_h__

#include "FrontEndDescription.h"
// #include "RD53.h"
#include "../Utils/Container.h"
#include "../Utils/Visitor.h"
#include "../Utils/easylogging++.h"
#include "ReadoutChip.h"
#include <stdint.h>
#include <vector>

// FE Hybrid HW Description Class

/*!
 * \namespace Ph2_HwDescription
 * \brief Namespace regrouping all the hardware description
 */
namespace Ph2_HwDescription
{
/*!
 * \class Hybrid
 * \brief handles a vector of Chip which are connected to the Hybrid
 */
class Hybrid
    : public FrontEndDescription
    , public HybridContainer
{
  public:
    // C'tors take FrontEndDescription or hierachy of connection
    Hybrid(const FrontEndDescription& pFeDesc, uint8_t pHybridId);
    Hybrid(uint8_t pBeId, uint8_t pFMCId, uint8_t pFeId, uint8_t pHybridId);
    Hybrid(uint8_t pBeId, uint8_t pFMCId, uint8_t pFeId, uint8_t pHybridId, uint8_t pLinkId);

    // Default C'tor
    Hybrid();

    // D'tor
    ~Hybrid(){};

    /*!
     * \brief acceptor method for HwDescriptionVisitor
     * \param pVisitor
     */
    void accept(HwDescriptionVisitor& pVisitor)
    {
        pVisitor.visitHybrid(*this);

        for(auto cChip: *this) static_cast<ReadoutChip*>(cChip)->accept(pVisitor);
    }
    /*!
     * \brief Get the number of Chip connected to the Hybrid
     * \return The size of the vector
     */
    uint8_t getNChip() const { return this->size(); }

    uint8_t getLinkId() const { return fLinkId; };

    void setLinkId(uint8_t pLinkId) { fLinkId = pLinkId; };

  protected:
    uint8_t fLinkId;
};
} // namespace Ph2_HwDescription

#endif
