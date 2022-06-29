/*!
        \file                                            ChipInterface.h
        \brief                                           User Interface to the Chip, base class for, CBC, MPA, SSA, RD53
        \author                                          Fabio RAVERA
        \version                                         1.0
        \date                        25/02/19
        Support :                    mail to : fabio.ravera@cern.ch
 */

#include "ChipInterface.h"
#include "../Utils/ConsoleColor.h"

#define DEV_FLAG 0

using namespace Ph2_HwDescription;

namespace Ph2_HwInterface
{
ChipInterface::ChipInterface(const BeBoardFWMap& pBoardMap) : fBoardMap(pBoardMap), fBoardFW(nullptr), fPrevBoardIdentifier(65535)
{
}

void ChipInterface::setBoard(uint16_t pBoardIdentifier)
{
    if(fPrevBoardIdentifier != pBoardIdentifier)
    {
        BeBoardFWMap::iterator i = fBoardMap.find(pBoardIdentifier);

        if(i == fBoardMap.end())
            LOG(INFO) << "The Board: " << +pBoardIdentifier << "  doesn't exist";
        else
        {
            fBoardFW             = i->second;
            fPrevBoardIdentifier = pBoardIdentifier;
        }
    }
}

} // namespace Ph2_HwInterface
