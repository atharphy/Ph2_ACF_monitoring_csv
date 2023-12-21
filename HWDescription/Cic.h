/*!

        \file                   Cic.h
        \brief                  Cic Description class, config of the Cics
 */

#ifndef Cic_h__
#define Cic_h__

#include "Chip.h"
#include "FrontEndDescription.h"
#include "Utils/ConsoleColor.h"
#include "Utils/Exception.h"
#include "Utils/Visitor.h"
#include "Utils/easylogging++.h"

#include <iostream>
#include <set>
#include <stdint.h>
#include <string>
#include <utility>

/*!
 * \namespace Ph2_HwDescription
 * \brief Namespace regrouping all the hardware description
 */
namespace Ph2_HwDescription
{
using CicRegPair = std::pair<std::string, ChipRegItem>;

/*!
 * \class Cic
 * \brief Read/Write Cic's registers on a file, contains a register map
 */
class Cic : public Chip
{
  public:
    // C'tors which take BeBoardId, FMCId, HybridId, CicId
    Cic(uint8_t pBeBoardId, uint8_t pFMCId, uint8_t pOpticalGroupId, uint8_t pHybridId, uint8_t pChipId, const std::string& filename);

    // C'tors with object FE Description
    Cic(const FrontEndDescription& pFeDesc, uint8_t pChipId, const std::string& filename);

    Cic(const Cic&) = delete;

    /*!
     * \brief acceptor method for HwDescriptionVisitor
     * \param pVisitor
     */
    virtual void accept(HwDescriptionVisitor& pVisitor) { pVisitor.visitChip(*this); }
    /*!
     * \brief Load RegMap from a file
     * \param filename
     */
    void loadfRegMap(const std::string& filename) override;

    std::stringstream getRegMapStream() override;

    virtual uint8_t getNumberOfBits(const std::string& dacName) { return 8; };

    void    setDriveStrength(uint8_t pDriveStrength) { fDriveStrength = pDriveStrength; }
    uint8_t getDriveStrength() { return fDriveStrength; }

    void    setEdgeSelect(uint8_t pEdgeSel) { fEdgeSel = pEdgeSel; }
    uint8_t getEdgeSelect() { return fEdgeSel; }

  protected:
    uint8_t fDriveStrength{5}; // drive strength 1-5
    uint8_t fEdgeSel{0};       // 0 - positive edge, 1 - negative edge
};
} // namespace Ph2_HwDescription

#endif
