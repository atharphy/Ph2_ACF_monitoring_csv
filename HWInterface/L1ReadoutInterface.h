#ifndef _L1ReadoutInterface_H__
#define _L1ReadoutInterface_H__

#include "RegManager.h"
#include "../Utils/Utilities.h"
#include "../Utils/easylogging++.h"
#include <string>


#include "FastCommandInterface.h"
#include "TriggerInterface.h"

namespace Ph2_HwInterface
{
class L1ReadoutInterface : public RegManager
{
    public: // constructors 
  
        L1ReadoutInterface(const std::string& puHalConfigFileName, uint32_t pBoardId);
        L1ReadoutInterface(const std::string& pId, const std::string& pUri, const std::string& pAddressTable);
        ~L1ReadoutInterface();
    protected : 
        FastCommandInterface* fFastCommandInterface; 
        TriggerInterface*     fTriggerInterface;


    public: // virtual functions 
    
        virtual void FillData()
        {
            LOG(ERROR) << BOLDRED << __PRETTY_FUNCTION__ << "\tError: implementation of virtual member function L1ReadoutInterface::ReadData is absent" << RESET;
        }
        virtual bool WaitForData()
        {
            LOG(ERROR) << BOLDRED << __PRETTY_FUNCTION__ << "\tError: implementation of virtual member function L1ReadoutInterface::WaitForData is absent" << RESET;
            return false;
        }
        virtual bool ReadEvents()
        {
            LOG(ERROR) << BOLDRED << __PRETTY_FUNCTION__ << "\tError: implementation of virtual member function L1ReadoutInterface::ReadEvents is absent" << RESET;
            return false;
        }

        std::vector<uint32_t> getData(){return fData;}
        // function to link FEConfigurationInterface 
        void LinkFastCommandInterface(FastCommandInterface* pInterface) {fFastCommandInterface=pInterface;}
        void LinkTriggerInterface(TriggerInterface* pInterface) {fTriggerInterface=pInterface;}

        void setNEvents(uint32_t pNEvents){ fNEvents = pNEvents; }
        uint32_t getNEvents(){ return fNEvents; }
    private : 
        std::vector<uint32_t> fData; 
        uint32_t              fNEvents; 



    
    
};
} // namespace Ph2_HwInterface
#endif