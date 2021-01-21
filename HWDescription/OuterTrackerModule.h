/*!

    \file                           OuterTrackerModule.h
    \brief                          OuterTrackerModule Description class
    \author                         Sarah Seif El Nasr-Storey, Fabio Ravera
    \version                        1.0
    \date                           25/06/19
    Support :                       mail to : fabio.ravera@cern.ch

*/

#ifndef OuterTrackerModule_h__
#define OuterTrackerModule_h__

#include "../Utils/Container.h"
#include "../Utils/Visitor.h"
#include "../Utils/easylogging++.h"
#include "Cic.h"
#include "FrontEndDescription.h"
#include "MPA.h"
#include "Module.h"
#include "SSA.h"
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
 * \class OuterTrackerModule
 * \brief handles a vector of Chip which are connected to the OuterTrackerModule
 */
class OuterTrackerModule : public Module
{
  public:
    // C'tors take FrontEndDescription or hierachy of connection
    OuterTrackerModule(const FrontEndDescription& pFeDesc, uint8_t pModuleId);
    OuterTrackerModule(uint8_t pBeId, uint8_t pFMCId, uint8_t pFeId, uint8_t pModuleId);

    // Default C'tor
    OuterTrackerModule();

    // D'tor
    ~OuterTrackerModule()
    {
        delete fCic;
        fCic = nullptr;
    };

    // uint8_t getNMPA() const
    // {
    //     return fMPAVector.size();
    // }

    // uint8_t getNSSA() const
    // {
    //     return fSSAVector.size();
    // }

    // /*!
    //  * \brief Adding a Chip to the vector
    //  * \param pChip
    //  */
    // void addCic ( Cic& pCic )
    // {

    //     //get the FrontEndType of the Cic and set the module one accordingly
    //     //this is the case when no Cic type has been set so get the one from the Cic
    //     if (fType == FrontEndType::CIC)
    //         fType = pCic.getFrontEndType();
    //     //else, the Cic type has already been set - if it is different from another Cic, rais a warning
    //     //no different Cics should be on a module
    //     else if (fType != pCic.getFrontEndType() )
    //     {
    //         LOG (ERROR) << "Error, Cics of a module should not be of different type! - aborting";
    //         exit (1);
    //     }

    //     fCicVector.push_back ( &pCic );
    // }
    void addCic(Cic* pCic)
    {
        if(fCic != nullptr)
        {
            LOG(ERROR) << "Error, Cic for this module was already initialized - aborting";
            exit(1);
        }
        fCic = pCic;
        pCic = nullptr;
    }

    // void addMPA ( MPA& pMPA )
    // {
    //     fMPAVector.push_back ( &pMPA );
    // }
    // void addMPA ( MPA* pMPA )
    // {
    //     fMPAVector.push_back ( pMPA );
    // }

    Cic* fCic;
    // std::vector < MPA* > fMPAVector;
    // std::vector < SSA* > fSSAVector;

  protected:
    // moduleID
    uint8_t fModuleId;
};
} // namespace Ph2_HwDescription

#endif
