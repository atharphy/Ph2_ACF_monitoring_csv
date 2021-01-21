/*!
  \file                  lpGBT.h
  \brief                 lpGBT description class, config of the lpGBT
  \author                Mauro DINARDO
  \version               1.0
  \date                  28/06/18
  Support:               email to mauro.dinardo@cern.ch
*/

#ifndef lpGBT_H
#define lpGBT_H

#include "../Utils/ConsoleColor.h"
#include "../Utils/easylogging++.h"
#include "Chip.h"

#include <iomanip>

namespace Ph2_HwDescription
{
class lpGBT : public Chip
{
  public:
    lpGBT(uint8_t pBeId, uint8_t FMCId, uint8_t pOptGroupId, const std::string& fileName);

    void    loadfRegMap(const std::string& fileName) override;
    void    saveRegMap(const std::string& fileName) override;
    uint8_t getNumberOfBits(const std::string& dacName) override { return 0; }

  private:
    std::string configFileName;
};
} // namespace Ph2_HwDescription

#endif
