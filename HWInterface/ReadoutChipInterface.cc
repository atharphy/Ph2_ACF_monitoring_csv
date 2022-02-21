/*!
  \file                                            ReadoutChipInterface.h
  \brief                                           User Interface to the Chip, base class for, CBC, MPA, SSA, RD53
  \author                                          Fabio RAVERA
  \version                                         1.0
  \date                        25/02/19
  Support :                    mail to : fabio.ravera@cern.ch
*/

#include "ReadoutChipInterface.h"
#include "../Utils/ConsoleColor.h"

#define DEV_FLAG 0

using namespace Ph2_HwDescription;

namespace Ph2_HwInterface
{
ReadoutChipInterface::ReadoutChipInterface(const BeBoardFWMap& pBoardMap) : ChipInterface(pBoardMap)
{
#ifdef COUNT_FLAG
    LOG(DEBUG) << "Counting number of Transactions!";
#endif
}

ReadoutChipInterface::~ReadoutChipInterface() {}
} // namespace Ph2_HwInterface
