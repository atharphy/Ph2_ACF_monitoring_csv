/*!

        \file                           Module.h
        \brief                          Module Description class
        \author                         Lorenzo BIDEGAIN
        \version                        1.0
        \date                           25/06/14
        Support :                       mail to : lorenzo.bidegain@gmail.com

 */

#ifndef Module_h__
#define Module_h__

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
 * \class Module
 * \brief handles a vector of Chip which are connected to the Module
 */
class Module
    : public FrontEndDescription
    , public ModuleContainer
{
  public:
    // C'tors take FrontEndDescription or hierachy of connection
    Module(const FrontEndDescription& pFeDesc, uint8_t pModuleId);
    Module(uint8_t pBeId, uint8_t pFMCId, uint8_t pFeId, uint8_t pModuleId);
    Module(uint8_t pBeId, uint8_t pFMCId, uint8_t pFeId, uint8_t pModuleId, uint8_t pLinkId);

    // Default C'tor
    Module();

    // D'tor
    ~Module(){};

    /*!
     * \brief acceptor method for HwDescriptionVisitor
     * \param pVisitor
     */
    void accept(HwDescriptionVisitor& pVisitor)
    {
        pVisitor.visitModule(*this);

        for(auto cChip: *this) static_cast<ReadoutChip*>(cChip)->accept(pVisitor);
    }
    /*!
     * \brief Get the number of Chip connected to the Module
     * \return The size of the vector
     */
    uint8_t getNChip() const { return this->size(); }

    // void addReadoutChip ( ReadoutChip& pChip )
    // {
    //     //get the FrontEndType of the Chip and set the module one accordingly
    //     //this is the case when no chip type has been set so get the one from the Chip
    //     if (fType == FrontEndType::UNDEFINED)
    //         fType = pChip.getFrontEndType();
    //     //else, the chip type has already been set - if it is different from another Chip, rais a warning
    //     //no different chips should be on a module
    //     else if (fType != pChip.getFrontEndType() )
    //     {
    //         LOG (ERROR) << "Error, Chips of a module should not be of different type! - aborting";
    //         exit (1);
    //     }

    //     fReadoutChipVector.push_back ( &pChip );
    // }
    // void addReadoutChip ( ReadoutChip* pChip )
    // {
    //     //get the FrontEndType of the Chip and set the module one accordingly
    //     //this is the case when no chip type has been set so get the one from the Chip
    //     if (fType == FrontEndType::UNDEFINED)
    //         fType = pChip->getFrontEndType();
    //     //else, the chip type has already been set - if it is different from another Chip, rais a warning
    //     //no different chips should be on a module
    //     else if (fType != pChip->getFrontEndType() )
    //     {
    //         LOG (ERROR) << "Error, Chips of a module should not be of different type! - aborting";
    //         exit (1);
    //     }

    //     fReadoutChipVector.push_back ( pChip );
    // }

    // uint8_t getModuleId() const
    // {
    //     return fModuleId;
    // };

    uint8_t getLinkId() const { return fLinkId; };
    // /*!
    //  * \brief Set the Module Id
    //  * \param pModuleId
    //  */
    // void setModuleId ( uint8_t pModuleId )
    // {
    //     fModuleId = pModuleId;
    // };
    void setLinkId(uint8_t pLinkId) { fLinkId = pLinkId; };

    // std::vector < RD53* > fRD53Vector;
    // std::vector < ReadoutChip* > fReadoutChipVector;

  protected:
    // uint8_t fModuleId;
    // link ID
    uint8_t fLinkId;
};
} // namespace Ph2_HwDescription

#endif
