/* Copyright 2014 Institut Pluridisciplinaire Hubert Curien
   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.

   FileName :       D19cFpgaConfig.cc
   Content :        FPGA configuration
   Programmer :     Christian Bonnin
   Version :
   Date of creation : 2014-07-10
   Support :        mail to : christian.bonnin@iphc.cnrs.fr
*/

#include "D19cFpgaConfig.h"
#include "BeBoardFWInterface.h"
#include <boost/format.hpp>
#include <boost/thread.hpp>
#include <fstream>
#include <sys/stat.h>
#include <time.h>

using namespace std;
using namespace Ph2_HwDescription;

#define SECURE_MODE_PASSWORD "RuleBritannia"

namespace Ph2_HwInterface
{
D19cFpgaConfig::D19cFpgaConfig(BeBoardFWInterface* pbbi) : FpgaConfig(pbbi), lNode(nullptr)
{
    // Quick fix for IT - OT incompatibilities
    try
    {
        lNode = new fc7::MmcPipeInterface(dynamic_cast<const fc7::MmcPipeInterface&>(fwManager->getUhalNode("system.buf_cta")));
    }
    catch(const std::exception& lExc)
    {
        lNode = new fc7::MmcPipeInterface(dynamic_cast<const fc7::MmcPipeInterface&>(fwManager->getUhalNode("buf_cta")));
    }
}

D19cFpgaConfig::~D19cFpgaConfig() { delete lNode; }

void D19cFpgaConfig::runUpload(const std::string& strImage, const char* szFile)
{
    numUploadingFpga = 1;
    progressValue    = 0;
    progressString   = "Starting upload";
    boost::thread(&D19cFpgaConfig::dumpFromFileIntoSD, this, strImage, szFile);
}

void D19cFpgaConfig::dumpFromFileIntoSD(const std::string& strImage, const char* pstrFile)
{
    if(string(pstrFile).compare(string(pstrFile).length() - 4, 4, ".bit") == 0)
    {
        fc7::XilinxBitFile bitFile(pstrFile);
        lNode->FileToSD(strImage, bitFile, &progressValue, &progressString);
    }
    else
    {
        fc7::XilinxBinFile binFile(pstrFile);
        lNode->FileToSD(strImage, binFile, &progressValue, &progressString);
    }

    progressValue = 100;
    lNode->RebootFPGA(strImage, SECURE_MODE_PASSWORD);
}

void D19cFpgaConfig::jumpToImage(const std::string& strImage) { lNode->RebootFPGA(strImage, SECURE_MODE_PASSWORD); }

void D19cFpgaConfig::runDownload(const std::string& strImage, const char* szFile)
{
    vector<string> lstNames = lNode->ListFilesOnSD();

    for(size_t iName = 0; iName < lstNames.size(); iName++)
    {
        if(!strImage.compare(lstNames[iName])) numUploadingFpga = iName + 1;

        break;
    }

    progressValue  = 0;
    progressString = "Downloading configuration";
    boost::thread(&D19cFpgaConfig::downloadImage, this, strImage, szFile);
}

void D19cFpgaConfig::downloadImage(const std::string& strImage, const std::string& strDestFile)
{
    fc7::Firmware bitStream1 = lNode->FileFromSD(strImage, &progressValue, 0);
    progressString           = "Checking download";
    fc7::Firmware bitStream2 = lNode->FileFromSD(strImage, &progressValue, 33);
    fc7::Firmware bitStream3("empty");
    ofstream      oFile;
    oFile.open(strDestFile, ios::out | ios::binary);

    // for (const auto& uVal:bitStream.Bitstream())
    for(size_t idx = 0; idx < bitStream1.Bitstream().size(); idx++)
    {
        if(bitStream1.Bitstream()[idx] != bitStream2.Bitstream()[idx])
        {
            if(bitStream3.Bitstream().empty())
            {
                progressString = "Errors found, checking again";
                bitStream3     = lNode->FileFromSD(strImage, &progressValue, 66);
            }

            if(bitStream1.Bitstream()[idx] == bitStream3.Bitstream()[idx])
                oFile << (char)bitStream1.Bitstream()[idx];
            else if(bitStream2.Bitstream()[idx] == bitStream3.Bitstream()[idx])
                oFile << (char)bitStream2.Bitstream()[idx];
            else
                throw fc7::CorruptedFile();
        }
        else
            oFile << (char)bitStream1.Bitstream()[idx];
    }

    oFile.close();
    progressValue = 100;
}

std::vector<std::string> D19cFpgaConfig::getFirmwareImageNames() { return lNode->ListFilesOnSD(); }

void D19cFpgaConfig::deleteFirmwareImage(const std::string& strId) { lNode->DeleteFromSD(strId, SECURE_MODE_PASSWORD); }

void D19cFpgaConfig::resetBoard() { lNode->BoardHardReset(SECURE_MODE_PASSWORD); }
} // namespace Ph2_HwInterface
