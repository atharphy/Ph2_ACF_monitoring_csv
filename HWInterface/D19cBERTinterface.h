#ifndef __D19C_BERT_INTERFACE_H__
#define __D19C_BERT_INTERFACE_H__

#include <cstdint>

class BoardContainer;
class BoardDataContainer;

namespace Ph2_HwInterface
{

class RegManager;

class BitErrorTestControl
{
    friend class BitErrorTestReply;

  public:
    BitErrorTestControl() {}

    enum class Command
    {
        ReturnConfig           = 0,
        ReturnCounterThreshold = 1,
        Configure              = 2,
        SetCounterThreshold    = 3,
        ReadCounterData        = 4,
        ErrorInject            = 5,
        ReadBERTfirstData      = 6,
        ReadBERTsampledData    = 7
    };

    enum class CounterSelect
    {
        CounterLSB        = 0,
        CounterMSB        = 1,
        FrameErrorCounter = 2,
        BitErrorCounter   = 3
    };

    enum class Mode
    {
        None0 = 0,
        PRBS  = 1,
        LSFR  = 2,
        None3 = 3
    };

    void     resetCommandBits();
    uint32_t encodeCommand() const;
    void getLine(const BitErrorTestControl& theBitErrorTestReply);


    void setHybridId(uint8_t theHybridId) { fHybridId = theHybridId; }
    void setChipId(uint8_t theChipId) { fChipId = theChipId; }
    void setLineId(uint8_t theLineId) { fLineId = theLineId; }
    void setCommand(Command theCommand) { fCommand = theCommand; }
    void setDebugMode(bool theDebugMode) { fDebugMode = theDebugMode; }
    void setCheckMode(bool theCheckMode) { fCheckMode = theCheckMode; }
    void setCounterReset(bool theCounterReset) { fCounterReset = theCounterReset; }
    void setCounterSelect(CounterSelect theCounterSelect) { fCounterSelect = theCounterSelect; }
    void setMode(Mode theMode) { fMode = theMode; }
    void setCheckEnable(bool theCheckEnable) { fCheckEnable = theCheckEnable; }
    void setReceiveEnable(bool theReceiveEnable) { fReceiveEnable = theReceiveEnable; }
    void setCounterThreshold(uint16_t theCounterThreshold) { fCounterThreshold = theCounterThreshold; }
    void setErrorInjection(bool theErrorInjection) { fErrorInjection = theErrorInjection; }
    void setDataLoad(bool theDataLoad) { fDataLoad = theDataLoad; }
    void setPackagePatternLSB(uint16_t thePackagePatternLSB) { fPackagePatternLSB = thePackagePatternLSB; }
    void setPackagePatternMSB(uint16_t thePackagePatternMSB) { fPackagePatternMSB = thePackagePatternMSB; }

  private:
    uint8_t  fHybridId{0};
    uint8_t  fChipId{0};
    uint8_t  fLineId{0};
    Command  fCommand{Command::ReturnConfig};
    bool     fDebugMode{false};
    bool     fCheckMode{false};
    bool     fCounterReset{false};
    CounterSelect fCounterSelect{CounterSelect::CounterLSB};
    Mode fMode{Mode::None0};
    bool fCheckEnable{false};
    bool fReceiveEnable{false};
    uint16_t fCounterThreshold{0};
    bool fErrorInjection{false};
    bool fDataLoad{false};
    uint16_t fPackagePatternLSB{0};
    uint16_t fPackagePatternMSB{0};

    static bool fIsDebugModeActivated;
    static bool fCurrentCheckMode;
    static Mode fCurrentMode;
    static CounterSelect fCurrentCounterSelect;
};


class BitErrorTestReply
{
  public:
    BitErrorTestReply() {};

    void decodeReply(uint32_t reply, const BitErrorTestControl& theBitErrorTestControl);

    uint8_t                   getHybridId (){return fHybridId;}
    uint8_t                   getChipId (){return fChipId;}
    uint8_t                   getLineId (){return fLineId;}
    bool                      getPRBScounterOverflow (){return fPRBScounterOverflow;}
    bool                      getLFSRcounterOverflow (){return fLFSRcounterOverflow;}
    uint8_t                   getPRBScheckStateMachineStatus (){return fPRBScheckStateMachineStatus;}
    bool                      getCheckMode (){return fCheckMode;}
    bool                      getCounterReset (){return fCounterReset;}
    uint8_t                   getCounterSelect (){return fCounterSelect;}
    BitErrorTestControl::Mode getMode (){return fMode;}
    bool                      getCheckEnable (){return fCheckEnable;}
    bool                      getReceiveEnable (){return fReceiveEnable;}
    uint16_t                  getCounterThreshold (){return fCounterThreshold;}
    uint32_t                  getPRBSframeCounterValueEmulator (){return fPRBSframeCounterValueEmulator;}
    uint32_t                  getPRBSbitCounterValueEmulator (){return fPRBSbitCounterValueEmulator;}
    uint32_t                  getPRBSframeCounterValuePredictNext (){return fPRBSframeCounterValuePredictNext;}
    uint32_t                  getPRBSbitCounterValuePredictNext (){return fPRBSbitCounterValuePredictNext;}

    uint32_t                  getPRBSfirstData (){return fPRBSfirstData;}
    uint32_t                  getLFSRfirstData (){return fLFSRfirstData;}
    uint32_t                  getPRBSdata (){return fPRBSdata;}
    uint32_t                  getLFSRdata (){return fLFSRdata;}

  private:
    uint8_t  fHybridId{99};
    uint8_t  fChipId{99};
    uint8_t  fLineId{99};
    bool     fPRBScounterOverflow{false};
    bool     fLFSRcounterOverflow{false};
    uint8_t  fPRBScheckStateMachineStatus{99};
    bool     fCheckMode{false};
    bool     fCounterReset{false};
    uint8_t  fCounterSelect{99};
    BitErrorTestControl::Mode fMode{BitErrorTestControl::Mode::None0};
    bool     fCheckEnable{false};
    bool     fReceiveEnable{false};
    uint16_t fCounterThreshold{999};
    uint32_t fPRBSframeCounterValueEmulator{999};
    uint32_t fPRBSbitCounterValueEmulator{999};
    uint32_t fPRBSframeCounterValuePredictNext{999};
    uint32_t fPRBSbitCounterValuePredictNext{999};
    uint32_t fPRBSfirstData{999};
    uint32_t fLFSRfirstData{999};
    uint32_t fPRBSdata{999};
    uint32_t fLFSRdata{999};
    uint32_t fFrameCounterLSB;
    uint32_t fFrameCounterMSB;
};

class D19cBERTinterface
{

  public:
    D19cBERTinterface(RegManager* theRegManager);
    ~D19cBERTinterface();

    void startBitErrorRateTest(uint8_t hybridId, uint8_t lineId);
    void stopBitErrorRateTest(uint8_t hybridId, uint8_t lineId);
    void haltBitErrorRateTest(uint8_t hybridId, uint8_t lineId);
    uint32_t getBitErrorCounters(uint8_t hybridId, uint8_t lineId);
    uint32_t getFirstData(uint8_t hybridId, uint8_t lineId);
    void injectError(uint8_t hybridId, uint8_t lineId);
    
    BoardDataContainer runBERTonAllHybdrids(BoardContainer* theBoardContainer, uint8_t numberOfLines, uint32_t numberOfSeconds);

  private:
    RegManager* fTheRegManager{nullptr};

    void writeCommand(BitErrorTestControl theBitErrorTestControl);
    BitErrorTestReply readReplay(const BitErrorTestControl& theBitErrorTestControl);


};

}

#endif