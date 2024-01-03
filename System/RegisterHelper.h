#ifndef __REGISTER_HELPER_H__
#define __REGISTER_HELPER_H__

#include "string"

namespace Ph2_HwInterface
{
class ReadoutChipInterface;
class BeBoardInterface;
class lpGBTInterface;
class CicInterface;
} // namespace Ph2_HwInterface
class DetectorContainer;
enum class FrontEndType;

namespace Ph2_System
{
class RegisterHelper
{
  public:
    RegisterHelper(DetectorContainer*                     theDetectorContainer,
                   Ph2_HwInterface::BeBoardInterface*     theBeBoardInterface,
                   Ph2_HwInterface::ReadoutChipInterface* theReadoutChipInterface,
                   Ph2_HwInterface::lpGBTInterface*       thelpGBTInterface,
                   Ph2_HwInterface::CicInterface*         theCicInterface);

    RegisterHelper(const RegisterHelper&) = delete;

    ~RegisterHelper(){};

    void takeSnapshot();
    void restoreSnapshot();
    void freeFrontEndRegister(const FrontEndType theFrontEndType, std::string registerName);
    void freeBoardRegister(std::string registerName);

  private:
    void clearSnapshot();
    void resetFreeRegisters();

    DetectorContainer*                     fDetectorContainer{nullptr};
    Ph2_HwInterface::BeBoardInterface*     fBeBoardInterface{nullptr};
    Ph2_HwInterface::ReadoutChipInterface* fReadoutChipInterface{nullptr};
    Ph2_HwInterface::lpGBTInterface*       flpGBTInterface{nullptr};
    Ph2_HwInterface::CicInterface*         fCicInterface{nullptr}; // Interface to a CIC [only valid for OT]
};
} // namespace Ph2_System

#endif