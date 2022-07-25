#ifndef _D19clpGBTSlowControlWorkerInterface_H__
#define _D19clpGBTSlowControlWorkerInterface_H__

#include "D19cCommandProcessorInterface.h"

namespace LpGBTSlowControlWorker
{
const uint8_t BASE_ID              = 16;
const uint8_t READ_IC              = 2;
const uint8_t WRITE_IC             = 3;
const uint8_t SINGLE_BYTE_READ_I2C = 4;
const uint8_t MULTI_BYTE_WRITE_I2C = 5;
const uint8_t READ_FE              = 6;
const uint8_t WRITE_FE             = 7;
const int     BLOCK_SIZE           = 500;

const std::map<int, std::string> WORKER_FSM_STATE_MAP{{0, "UNDEFINED"},
                                                      {1, "IDLE"},
                                                      {2, "GET_COMMAND_HEADER"},
                                                      {3, "REQ_N_WORDS"},
                                                      {4, "GET_FUNCTION_HEADER"},
                                                      {5, "FORWARD_REPLY_HEADER"},
                                                      {6, "REQ_FUNCTION_ARG"},
                                                      {7, "GET_FUNCTION_ARG"},
                                                      {8, "PERFORM_TRANSACTION"},
                                                      {9, "FORWARD_REPLY_PAYLOADN"}};

const std::map<int, std::string> IC_FSM_STATE_MAP{{0, "UNDEFINED"},
                                                  {1, "IDLE"},
                                                  {2, "SEND_RD_RQ"},
                                                  {3, "FINALIZE_RD"},
                                                  {4, "LOAD_WR_DATA"},
                                                  {5, "SEND_WR_REQ"},
                                                  {6, "FINALIZE_WR"},
                                                  {7, "WAIT_REPLY"},
                                                  {8, "DELAY_GET_REPLY_FRAME"},
                                                  {9, "GET_REPLY_FRAME"},
                                                  {10, "VERIFY_REPLY_FRAME"},
                                                  {11, "GET_REPLY_DATA"}};

const std::map<int, std::string> I2C_FSM_STATE_MAP{{0, "UNDEFINED"},
                                                   {1, "IDLE"},
                                                   {2, "WRITE_I2C_CONF_DATA"},
                                                   {3, "WRITE_I2C_CONF_CMD"},
                                                   {4, "WRITE_I2C_SLAVE_ADDR"},
                                                   {5, "WRITE_I2C_RD_CMD"},
                                                   {6, "GET_I2C_RD_VALUE"},
                                                   {7, "FORWARD_I2C_RD_RESULT"},
                                                   {8, "WRITE_I2C_SLAVE_DATA0"},
                                                   {9, "WRITE_I2C_SLAVE_DATA1"},
                                                   {10, "WRITE_I2C_SLAVE_DATA2"},
                                                   {11, "WRITE_I2C_SLAVE_DATA3"},
                                                   {12, "WRITE_I2C_WRITE_DATA_CMD"},
                                                   {13, "WRITE_I2C_WR_CMD"},
                                                   {14, "FORWARD_I2C_WR_RESULT"},
                                                   {15, "DELAY_GET_I2C_STAT"},
                                                   {16, "GET_I2C_STAT"},
                                                   {17, "CHECK_I2C_STAT"},
                                                   {18, "WAIT_IC_WR_DONE"},
                                                   {19, "WAIT_IC_RD_DONE"}};

const std::map<int, std::string> FE_FSM_STATE_MAP{{0, "UNDEFINED"},
                                                  {1, "IDLE"},
                                                  {2, "SEND_FE_RD_REQ"},
                                                  {3, "GET_FE_RD_VALUE"},
                                                  {4, "FORWARD_FE_RD_RESULT"},
                                                  {5, "SEND_FE_WR_REQ"},
                                                  {6, "FORWARD_FE_WR_RESULT"},
                                                  {7, "WAIT_I2C_WR_DONE"},
                                                  {8, "WAIT_I2C_RD_DONE"}};
} // namespace LpGBTSlowControlWorker
namespace Ph2_HwInterface
{
class D19clpGBTSlowControlWorkerInterface : public D19cCommandProcessorInterface
{
  public:
    D19clpGBTSlowControlWorkerInterface(const std::string& puHalConfigFileName, uint32_t pBoardId);
    D19clpGBTSlowControlWorkerInterface(const std::string& pId, const std::string& pUri, const std::string& pAddressTable);
    ~D19clpGBTSlowControlWorkerInterface();

  public:
    void                  Reset();
    std::vector<uint32_t> EncodeCommand(uint8_t pFunctionId, Ph2_HwDescription::Chip* pChip, const std::vector<Ph2_HwDescription::ChipRegItem>& pRegisterItems, bool pVerify = false);
    bool                  IsDone(uint8_t pFunctionId);
    uint8_t               GetTryCntr(uint8_t pFunctionId);
    void                  PrintStateFSM();
    void                  SelectLink(uint8_t pLinkId);
};
} // namespace Ph2_HwInterface
#endif
