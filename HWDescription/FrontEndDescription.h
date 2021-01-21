/*!
  \file                           FrontEndDescription.h
  \brief                          FrontEndDescription base class to describe all parameters common to all FE Components
  in the DAQ chain \author                         Lorenzo BIDEGAIN \version                        1.0 \date 25/06/14
  Support :                       mail to : lorenzo.bidegain@gmail.com
*/

#ifndef FrontEndDescription_H
#define FrontEndDescription_H

#include "Definition.h"
#include <stdint.h>

/*!
 * \namespace Ph2_HwDescription
 * \brief Namespace regrouping all the hardware description
 */
namespace Ph2_HwDescription
{
/*!
 * \class FrontEndDescription
 * \brief Describe all parameters common to all FE Components in the DAQ chain
 */
class FrontEndDescription
{
  public:
    // 3 C'tors with different parameter sets
    FrontEndDescription(uint8_t pBeId, uint8_t pFMCId, uint8_t pFeId, bool pStatus = true, FrontEndType pType = FrontEndType::UNDEFINED);
    FrontEndDescription();

    // Copy C'tors
    FrontEndDescription(const FrontEndDescription& pFeDesc);

    // Default D'tor
    virtual ~FrontEndDescription();

    /*!
     * \brief Get the Be ID
     * \return The Be ID
     */
    uint8_t getBeId() const { return fBeId; }

    /*!
     * \brief Get the FMC ID
     * \return The FMC ID
     */
    uint8_t getFMCId() const { return fFMCId; }

    /*!
     * \brief Get the FE ID
     * \return The FE ID
     */
    uint8_t getFeId() const { return fFeId; }

    /*!
     * \brief Get the BeBoardIdentifier
     * \return The BeBoardIdentifier
     */
    uint16_t getBeBoardId() const { return fBeId << 8; }

    /*!
     * \brief Get the Status
     * \return The Status
     */
    bool getStatus() const { return fStatus; }

    // Setter methods

    /*!
     * \brief Set the Be ID
     * \param pBeId
     */
    void setBeId(uint8_t pBeId) { fBeId = pBeId; }
    /*!
     * \brief Set the FMC ID
     * \param pFMCId
     */
    void setFMCId(uint8_t pFMCId) { fFMCId = pFMCId; }
    /*!
     * \brief Set the FE ID
     * \param pFeId
     */
    void setFeId(uint8_t pFeId) { fFeId = pFeId; }
    /*!
     * \brief Set the status
     * \param pStatus
     */
    void setStatus(bool pStatus) { fStatus = pStatus; }

    void setFrontEndType(FrontEndType pType) { fType = pType; }

    FrontEndType getFrontEndType() { return fType; }

  protected:
    // BIO Board Id that the FE is connected to
    uint8_t fBeId;
    // Id of the FMC Slot on the BIO Board, all FEs need to know so the right FW registers can be written
    uint8_t fFMCId;
    // Id of the FE (module/hybrid, etc...)
    uint8_t fFeId;
    // status (true=active, false=disabled)
    bool fStatus;
    // Front End type enum (MODULE, CBC2, CBC3, ...)
    FrontEndType fType;
};
} // namespace Ph2_HwDescription

#endif
