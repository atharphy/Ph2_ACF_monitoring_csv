#ifndef __EXCEPTION_HANDLER__
#define __EXCEPTION_HANDLER__

#include "iostream"

class DetectorContainer;
class DetectorDataContainer;


namespace Ph2_HwInterface
{
    class BeBoardFWInterface;

    class ExceptionHandler {
    private:
        static ExceptionHandler* fInstance;
        ExceptionHandler() {}; // Private constructor to prevent instantiation outside of the class
        ~ExceptionHandler();
        void removeContainerExceptionQuery(std::vector<std::string>& theFunctionNameList);
        void initializeQueryFunctionNameContainer();
        void updateFWInformation(uint16_t boardId);
        DetectorDataContainer *fQueryFunctionNames {nullptr};
        DetectorContainer     *fDetectorContainer  {nullptr};
        BeBoardFWInterface    *fFirmwareInterface  {nullptr};

    public:
        static ExceptionHandler* getInstance() {
            if (fInstance == nullptr) {
                fInstance = new ExceptionHandler();
            }
            return fInstance;
        }

        void setDetectorContainer(DetectorContainer *theDetectorContainer);
        void setFirmwareInterface(Ph2_HwInterface::BeBoardFWInterface *theBeBoardFWInterface) {fFirmwareInterface = theBeBoardFWInterface;}

        void disableChip(uint16_t boardId, uint16_t opticalGroupId, uint16_t hybridId, uint16_t chipId);
        void disableHybrid(uint16_t boardId, uint16_t opticalGroupId, uint16_t hybridId);
        void disableOpticalGroup(uint16_t boardId, uint16_t opticalGroupId);
        void disableBoard(uint16_t boardId);

        void resetExceptionQueries();
    };

} // namespace Ph2_HwInterface

#endif