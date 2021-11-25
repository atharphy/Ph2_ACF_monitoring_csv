#ifndef WorkerTester_h__
#define WorkerTester_h__

#include "Tool.h"

using namespace Ph2_HwDescription;

class WorkerTester : public Tool {
    public :
        WorkerTester();
	~WorkerTester();

	void ResetCPB();

        void PrepareForTests();

    	void WriteCommandCPB(const std::vector<uint32_t>& pCommandVector, bool pVerbose=false);
    	std::vector<uint32_t> ReadReplyCPB(uint8_t pNWords, bool pVerbose=false);

        void PrintFSMState(uint8_t pLinkId);

        // function for IC transactions 
        bool WriteLpGBTRegister(uint8_t pLinkId, uint16_t pRegisterAddress, uint8_t pRegisterValue, bool pVerbose=false);
        uint8_t ReadLpGBTRegister(uint8_t pLinkId, uint16_t pRegisterValue, bool pVerbose=false);
        // function for I2C transactions using lpGBT I2C Masters
        bool I2CWrite(uint8_t pLinkId, uint8_t pMasterId, uint8_t pSlaveAddress, uint32_t pSlaveData, uint8_t pNBytes, bool pVerbose=false);
        uint8_t I2CRead(uint8_t pLinkId, uint8_t pMasterId, uint8_t pSlaveAddress, uint8_t pNBytes, bool pVerbose=false);
        // function for front-end slow control
        bool WriteFERegister(Chip* pChip, uint16_t pRegisterAddress, uint8_t pRegisterValue, bool pRetry = false, bool pVerbose=false);
        uint8_t ReadFERegister(Chip* pChip, uint16_t pRegisterAddress, bool pVerbose=false);

        bool TestICRead();
        bool TestICWrite();
        bool TestI2CRead();
        bool TestI2CWrite();


    private :

        void ResetI2CMasters(OpticalGroup* cOpticalGroup);
        void SetHybridClocks(OpticalGroup* cOpticalGroup);
        void EnableHybridChips(OpticalGroup* cOptialGroup);

        bool TestICRead(OpticalGroup* cOpticalGroup);
        bool TestICWrite(OpticalGroup* cOpticalGroup);
        bool TestI2CRead(OpticalGroup* cOpticalGroup);
        bool TestI2CWrite(OpticalGroup* cOpticalGroup);


};
#endif
