/*
  FileName :                    RegManager.cc
  Content :                     RegManager class, permit connection & r/w registers
  Programmer :                  Nicolas PIERRE
  Version :                     1.0
  Date of creation :            06/06/14
  Support :                     mail to : nico.pierre@icloud.com
*/

#include "RegManager.h"
#include "../HWDescription/Definition.h"
#include "../Utils/ConsoleColor.h"
#include "../Utils/Utilities.h"
#include <uhal/uhal.hpp>

#include <boost/iostreams/device/file.hpp>
#include <boost/iostreams/filter/gzip.hpp>
#include <boost/iostreams/filtering_stream.hpp>

#define DEV_FLAG 0

namespace Ph2_HwInterface
{
RegManager::RegManager(const char* puHalConfigFileName, uint32_t pBoardId)
{
    if(mode != Mode::Replay)
    {
        uhal::disableLogging();
        fUHalConfigFileName = puHalConfigFileName;
        uhal::ConnectionManager cm(fUHalConfigFileName);
        char                    cBuff[7];
        sprintf(cBuff, "board%d", pBoardId);
        fBoard = new uhal::HwInterface(cm.getDevice((cBuff)));
        fBoard->setTimeoutPeriod(10000);
    }
}

RegManager::RegManager(const char* pId, const char* pUri, const char* pAddressTable) : fBoard(nullptr), fUri(pUri), fAddressTable(pAddressTable), fId(pId)
{
    if(mode != Mode::Replay)
    {
        uhal::disableLogging();
        fBoard = new uhal::HwInterface(uhal::ConnectionManager::getDevice(fId, fUri, fAddressTable));
        fBoard->setTimeoutPeriod(10000);
    }
}

RegManager::~RegManager() { delete fBoard; }

bool RegManager::WriteReg(const std::string& pRegNode, const uint32_t& pVal)
{
    if(mode == Mode::Replay) return true;

    fBoard->getNode(pRegNode).write(pVal);
    fBoard->dispatch();

    // Verify if the writing is done correctly
    if(DEV_FLAG)
    {
        uhal::ValWord<uint32_t> reply = fBoard->getNode(pRegNode).read();
        fBoard->dispatch();

        uint32_t comp = reply.value();

        if(comp == pVal)
        {
            LOG(DEBUG) << "Values written correctly: " << pRegNode << "=" << pVal;
            return true;
        }

        LOG(DEBUG) << "\nERROR !!\nValues are not consistent:\nExpected : " << pVal << "\nActual: " << comp;
    }

    return false;
}

bool RegManager::WriteStackReg(const std::vector<std::pair<std::string, uint32_t>>& pVecReg)
{
    if(mode == Mode::Replay) return true;

    for(auto const& v: pVecReg) fBoard->getNode(v.first).write(v.second);

    try
    {
        fBoard->dispatch();
    }
    catch(...)
    {
        std::cerr << "Error while writing the following parameters: ";

        for(auto const& v: pVecReg) std::cerr << v.first << ", ";

        std::cerr << std::endl;
        throw;
    }

    if(DEV_FLAG)
    {
        int      cNbErrors = 0;
        uint32_t comp;

        for(auto const& v: pVecReg)
        {
            uhal::ValWord<uint32_t> reply = fBoard->getNode(v.first).read();
            fBoard->dispatch();

            comp = reply.value();

            if(comp == v.second) LOG(DEBUG) << "Values written correctly: " << v.first << "=" << v.second;
        }

        if(cNbErrors == 0)
        {
            LOG(DEBUG) << "All values written correctly";
            return true;
        }

        LOG(DEBUG) << "\nERROR !!\n" << cNbErrors << " have not been written correctly";
    }

    return false;
}

bool RegManager::WriteBlockReg(const std::string& pRegNode, const std::vector<uint32_t>& pValues)
{
    if(mode == Mode::Replay) return true;

    fBoard->getNode(pRegNode).writeBlock(pValues);
    fBoard->dispatch();

    bool cWriteCorr = true;

    // Verifying block
    if(DEV_FLAG)
    {
        int cErrCount = 0;

        uhal::ValVector<uint32_t> cBlockRead = fBoard->getNode(pRegNode).readBlock(pValues.size());
        fBoard->dispatch();

        // Use size_t and not an iterator as op[] only works with size_t type
        for(std::size_t i = 0; i != cBlockRead.size(); i++)
        {
            if(cBlockRead[i] != pValues.at(i))
            {
                cWriteCorr = false;
                cErrCount++;
            }
        }

        LOG(DEBUG) << "Block Write finished !!\n" << cErrCount << " values failed to write";
    }

    return cWriteCorr;
}

bool RegManager::WriteBlockAtAddress(uint32_t uAddr, const std::vector<uint32_t>& pValues, bool bNonInc)
{
    if(mode == Mode::Replay) return true;

    fBoard->getClient().writeBlock(uAddr, pValues, bNonInc ? uhal::defs::NON_INCREMENTAL : uhal::defs::INCREMENTAL);
    fBoard->dispatch();

    bool cWriteCorr = true;

    // Verifying block
    if(DEV_FLAG)
    {
        int cErrCount = 0;

        uhal::ValVector<uint32_t> cBlockRead = fBoard->getClient().readBlock(uAddr, pValues.size(), bNonInc ? uhal::defs::NON_INCREMENTAL : uhal::defs::INCREMENTAL);
        fBoard->dispatch();

        // Use size_t and not an iterator as op[] only works with size_t type
        for(std::size_t i = 0; i != cBlockRead.size(); i++)
        {
            if(cBlockRead[i] != pValues.at(i))
            {
                cWriteCorr = false;
                cErrCount++;
            }
        }

        LOG(DEBUG) << "BlockWriteAtAddress finished !!\n" << cErrCount << " values failed to write";
    }

    return cWriteCorr;
}

uint32_t RegManager::ReadReg(const std::string& pRegNode)
{
    if(mode == Mode::Replay) return replayRead();

    uhal::ValWord<uint32_t> cValRead = fBoard->getNode(pRegNode).read();
    fBoard->dispatch();

    if(DEV_FLAG)
    {
        uint32_t read = cValRead.value();
        LOG(DEBUG) << "Value in register ID " << pRegNode << " : " << read;
    }

    if(mode == Mode::Capture) captureRead(cValRead.value());

    return cValRead.value();
}

uint32_t RegManager::ReadAtAddress(uint32_t uAddr, uint32_t uMask)
{
    if(mode == Mode::Replay) return replayRead();

    uhal::ValWord<uint32_t> cValRead = fBoard->getClient().read(uAddr, uMask);
    fBoard->dispatch();

    if(DEV_FLAG)
    {
        uint32_t read = cValRead.value();
        LOG(DEBUG) << "Value at address " << std::hex << uAddr << std::dec << " : " << read;
    }

    if(mode == Mode::Capture) captureRead(cValRead.value());

    return cValRead.value();
}

std::vector<uint32_t> RegManager::ReadBlockReg(const std::string& pRegNode, const uint32_t& pBlockSize)
{
    if(mode == Mode::Replay) return replayBlockRead(pBlockSize);

    uhal::ValVector<uint32_t> cBlockRead = fBoard->getNode(pRegNode).readBlock(pBlockSize);
    fBoard->dispatch();

    if(DEV_FLAG)
    {
        LOG(DEBUG) << "Values in register block " << pRegNode << " : ";

        for(std::size_t i = 0; i != cBlockRead.size(); i++)
        {
            uint32_t read = static_cast<uint32_t>(cBlockRead[i]);
            LOG(DEBUG) << " " << read << " ";
        }
    }

    if(mode == Mode::Capture) captureBlockRead(cBlockRead.value());

    return std::move(cBlockRead.value());
}

std::vector<uint32_t> RegManager::ReadBlockRegOffset(const std::string& pRegNode, const uint32_t& pBlocksize, const uint32_t& pBlockOffset)
{
    if(mode == Mode::Replay) return replayBlockRead(pBlocksize);

    uhal::ValVector<uint32_t> cBlockRead = fBoard->getNode(pRegNode).readBlockOffset(pBlocksize, pBlockOffset);
    fBoard->dispatch();

    if(DEV_FLAG)
    {
        LOG(DEBUG) << "Values in register block " << pRegNode << " : ";

        for(std::size_t i = 0; i != cBlockRead.size(); i++)
        {
            uint32_t read = static_cast<uint32_t>(cBlockRead[i]);
            LOG(DEBUG) << " " << read << " ";
        }
    }

    if(mode == Mode::Capture) captureBlockRead(cBlockRead.value());

    return std::move(cBlockRead.value());
}

void RegManager::StackReg(const std::string& pRegNode, const uint32_t& pVal, bool pSend)
{
    for(std::vector<std::pair<std::string, uint32_t>>::iterator cIt = fStackReg.begin(); cIt != fStackReg.end(); cIt++)
        if(cIt->first == pRegNode) fStackReg.erase(cIt);

    std::pair<std::string, uint32_t> cPair(pRegNode, pVal);
    fStackReg.push_back(cPair);

    if(pSend || fStackReg.size() == 100)
    {
        WriteStackReg(fStackReg);
        fStackReg.clear();
    }
}

const uhal::Node& RegManager::getUhalNode(const std::string& pStrPath) { return fBoard->getNode(pStrPath); }

// ##############################################
// # Capure and replay data stream to/from FPGA #
// ##############################################
RegManager::Mode                    RegManager::mode = RegManager::Mode::Default;
boost::iostreams::filtering_ostream capture_file{};
boost::iostreams::filtering_istream replay_file{};

void RegManager::enableCapture(const std::string filename)
{
    capture_file.push(boost::iostreams::gzip_compressor());
    capture_file.push(boost::iostreams::file_sink(filename));
    mode = Mode::Capture;
}

void RegManager::enableReplay(const std::string filename)
{
    replay_file.push(boost::iostreams::gzip_decompressor());
    replay_file.push(boost::iostreams::file_source(filename));
    mode = Mode::Replay;
}

template <class S, class T>
void read_binary(S& stream, T& data)
{
    stream.read(reinterpret_cast<char*>(&data), sizeof(data));
}

template <class S, class T>
void write_binary(S& stream, const T& data)
{
    stream.write(reinterpret_cast<const char*>(&data), sizeof(data));
}

uint32_t RegManager::replayRead() { return replayBlockRead(1)[0]; }

std::vector<uint32_t> RegManager::replayBlockRead(size_t size)
{
    uint32_t read_size;
    read_binary(replay_file, read_size);

    if(read_size != size)
    {
        LOG(ERROR) << BOLDRED << "Binary data replay error" << RESET;
        throw;
    }

    std::vector<uint32_t> data(read_size);
    for(auto& d: data) read_binary(replay_file, d);

    return data;
}

void RegManager::captureRead(uint32_t value) { captureBlockRead({value}); }

void RegManager::captureBlockRead(std::vector<uint32_t> data)
{
    write_binary(capture_file, uint32_t(data.size()));
    for(const auto& d: data) write_binary(capture_file, d);
}
} // namespace Ph2_HwInterface
