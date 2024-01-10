#ifndef __REGISTER_HELPER_H__
#define __REGISTER_HELPER_H__

#include "string"
#include "map"

namespace Ph2_HwInterface
{
class ReadoutChipInterface;
class BeBoardInterface;
class lpGBTInterface;
class CicInterface;
class BeBoardFWInterface;
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
                   Ph2_HwInterface::CicInterface*         theCicInterface,
                   std::map<uint16_t, Ph2_HwInterface::BeBoardFWInterface*>* theBeBoardFWMap);

    RegisterHelper(const RegisterHelper&) = delete;

    ~RegisterHelper(){};

    void takeSnapshot();
    void restoreSnapshot();
    void freeFrontEndRegister(const FrontEndType theFrontEndType, std::string registerName);
    void freeBoardRegister(std::string registerName);

    void dumpBeBoardRegisterIntoXml(std::string outputFileName);

  private:
    void clearSnapshot();
    void resetFreeRegisters();

    DetectorContainer*                     fDetectorContainer{nullptr};
    Ph2_HwInterface::BeBoardInterface*     fBeBoardInterface{nullptr};
    Ph2_HwInterface::ReadoutChipInterface* fReadoutChipInterface{nullptr};
    Ph2_HwInterface::lpGBTInterface*       flpGBTInterface{nullptr};
    Ph2_HwInterface::CicInterface*         fCicInterface{nullptr}; // Interface to a CIC [only valid for OT]
    std::map<uint16_t, Ph2_HwInterface::BeBoardFWInterface*>* fBeBoardFWMap{nullptr};
};
} // namespace Ph2_System

#endif