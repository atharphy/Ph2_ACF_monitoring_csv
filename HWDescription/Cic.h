/*!

        \file                   Cic.h
        \brief                  Cic Description class, config of the Cics
 */

#ifndef Cic_h__
#define Cic_h__

#include "../Utils/ConsoleColor.h"
#include "../Utils/Exception.h"
#include "../Utils/Visitor.h"
#include "../Utils/easylogging++.h"
#include "Chip.h"
#include "FrontEndDescription.h"

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
    // C'tors which take BeId, FMCId, FeID, CicId
    Cic(uint8_t pBeId, uint8_t pFMCId, uint8_t pFeId, uint8_t pCicId, const std::string& filename);

    // C'tors with object FE Description
    Cic(const FrontEndDescription& pFeDesc, uint8_t pCicId, const std::string& filename);

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

    /*!
     * \brief Write the registers of the Map in a file
     * \param filename
     */
    void saveRegMap(const std::string& filename) override;

    virtual uint8_t getNumberOfBits(const std::string& dacName) { return 8; };

  protected:
};
} // namespace Ph2_HwDescription

#endif
