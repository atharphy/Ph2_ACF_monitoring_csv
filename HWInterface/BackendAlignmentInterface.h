/*!

        \file                                            GbtInterface.h
        \brief                                           User Interface to the Cics
        \version                                         1.0

 */

#ifndef __BACKENDALIGNMENTINTERFACE_H__
#define __BACKENDALIGNMENTINTERFACE_H__

#include "BeBoardInterface.h"

namespace Ph2_HwDescription
{
class BeBoard;
}
/*!
 * \namespace Ph2_HwInterface
 * \brief Namespace regrouping all the interfaces to the hardware
 */
namespace Ph2_HwInterface
{
/*!
 * \class GbtInterface
 * \brief Class representing the User Interface to the alignment of data lines in the back-end
 */
class BackendAlignmentInterface : public BeBoardInterface
{
    class AlignerObject
    {
      public:
        uint8_t  fHybrid  = 0;
        uint8_t  fChip    = 0;
        uint8_t  fLine    = 0;
        uint8_t  fType    = 0;
        uint16_t fCommand = 0;
    };
    struct LineConfiguration
    {
        uint8_t fMode          = 0;
        uint8_t fDelay         = 0;
        uint8_t fBitslip       = 0;
        uint8_t fPattern       = 0;
        uint8_t fPatternPeriod = 0;
    };
    struct Status
    {
        uint8_t fDone                   = 0;
        uint8_t fWordAlignmentFSMstate  = 0;
        uint8_t fPhaseAlignmentFSMstate = 0;
        uint8_t fFSMstate               = 0;
    };

  public:
    /*!
     * \brief Constructor of the GbtInterface Class
     * \param pBoardMap
     */
    BackendAlignmentInterface(const BeBoardFWMap& pBoardMap);
    /*!
     * \brief Destructor of the GbtInterface Class
     */
    ~BackendAlignmentInterface();
    //
    void SendCommand(Ph2_HwDescription::BeBoard* pBoard);
    void ConfigurePattern(Ph2_HwDescription::BeBoard* pBoard, uint8_t pPattern, uint16_t pPatternPeriod);
    //
    void PhaseAlign(Ph2_HwDescription::BeBoard* pBoard);
    void WordAlign(Ph2_HwDescription::BeBoard* pBoard);
    bool TuneLine(Ph2_HwDescription::BeBoard* pBoard);
    bool GetLineStatus(Ph2_HwDescription::BeBoard* pBoard);
    void SendControl(Ph2_HwDescription::BeBoard* pBoard, std::string pCommand);
    //
    void SetLine(uint8_t pHybrid, uint8_t pChip, uint8_t pLine);
    void SelectLine(uint8_t pLine);
    void SelectFrontEnd(uint8_t pHybrid, uint8_t pChip);
    //
    bool GetStatus(Ph2_HwDescription::BeBoard* pBoard);

  protected:
    uint32_t encodeCommand() { return ((fObject.fHybrid & 0xF) << 28) + ((fObject.fChip & 0xF) << 24) + ((fObject.fLine & 0xF) << 20) + ((fObject.fType & 0xF) << 16) + fObject.fCommand; }
    //
    void SetPattern();
    void SetPatternLength();
    //
    void SetControlWord(std::string pCommand);
    void SetLineMode(uint8_t pMode = 0, uint8_t pDelay = 0, uint8_t pBitSlip = 0, uint8_t pEnableL1 = 0, uint8_t pMasterLine = 0);
    void ParseStatus(uint32_t pReply);

  private:
    //
    AlignerObject     fObject;
    LineConfiguration fLineConfig;
    Status            fStatus;

    // maps to decode status of word and phase alignment FSM
    std::map<int, std::string> fPhaseFSMStateMap = {{0, "IdlePHASE"},
                                                    {1, "ResetIDELAYE"},
                                                    {2, "WaitResetIDELAYE"},
                                                    {3, "ApplyInitialDelay"},
                                                    {4, "CheckInitialDelay"},
                                                    {5, "InitialSampling"},
                                                    {6, "ProcessInitialSampling"},
                                                    {7, "ApplyDelay"},
                                                    {8, "CheckDelay"},
                                                    {9, "Sampling"},
                                                    {10, "ProcessSampling"},
                                                    {11, "WaitGoodDelay"},
                                                    {12, "FailedInitial"},
                                                    {13, "FailedToApplyDelay"},
                                                    {14, "TunedPHASE"},
                                                    {15, "Unknown"}};
    std::map<int, std::string> fWordFSMStateMap  = {{0, "IdleWORD or WaitIserdese"},
                                                   {1, "WaitFrame"},
                                                   {2, "ApplyBitslip"},
                                                   {3, "WaitBitslip"},
                                                   {4, "PatternVerification"},
                                                   {5, "Not Defined"},
                                                   {6, "Not Defined"},
                                                   {7, "Not Defined"},
                                                   {8, "Not Defined"},
                                                   {9, "Not Defined"},
                                                   {10, "Not Defined"},
                                                   {11, "Not Defined"},
                                                   {12, "FailedFrame"},
                                                   {13, "FailedVerification"},
                                                   {14, "TunedWORD"},
                                                   {15, "Unknown"}};
};
} // namespace Ph2_HwInterface

#endif
