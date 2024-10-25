/*
 ---- Instructions ----

 compile:
 g++ $(root-config --libs --cflags)  -o DecodeCounters DecodeCounters.cc
 
 Run
./DecodeCounters Results/Run_<runNumber>

*/


#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <iomanip>
#include <stdint.h>
#include <string>
#include <array>
#include <bitset>
#include <TH2I.h>
#include <TCanvas.h>
#include <TFile.h>

// Function to convert an 8-character hex string to uint32_t
uint32_t hexStringToUint32(const std::string& hexString) {
    if (hexString.length() != 8) {
        throw std::invalid_argument("Invalid hex string length");
    }

    return static_cast<uint32_t>(std::stoul(hexString, nullptr, 16));
}

void printWord(const std::array<uint32_t, 32>& word)
{
    for(size_t hexWord=0; hexWord<32; ++hexWord)
    {
        std::cout << std::hex << std::setw(8) << std::setfill('0') << word[hexWord] << " ";
    }
    std::cout << std::dec << std::endl;
}

std::string parseCounterRaw(const std::string& fileName, size_t numberOfWords = 0)
{
    std::ifstream inputFile(fileName);
    std::vector<uint32_t> hexValues;
    std::string fileContents, word;

    if (!inputFile.is_open()) {
        std::cerr << "Error opening file: " << fileName << std::endl;
        return "";
    }

    // Read the entire file into a string
    std::stringstream buffer;
    buffer << inputFile.rdbuf();
    fileContents = buffer.str();

    std::stringstream ss(fileContents);

    // Process each 8-character hex word separated by spaces
    while (ss >> word) {
        try {
            // Convert hex word to uint32_t
            uint32_t value = hexStringToUint32(word);
            
            // Add the value to the vector
            hexValues.push_back(value);
        } catch (const std::invalid_argument& e) {
            std::cerr << "Invalid hex word: " << word << std::endl;
            continue;
        }
    }

    inputFile.close();

    if(numberOfWords == 0) numberOfWords = hexValues.size()/32;

    std::cout<< __PRETTY_FUNCTION__ << " [" << __LINE__ << "] total banks = " << numberOfWords << std::endl;

    std::vector<std::array<uint32_t, 32>> theWordArray(numberOfWords);

    for(size_t wordNumber=0; wordNumber<numberOfWords; ++wordNumber)
    {
        for(size_t hexWord=0; hexWord<32; ++hexWord)
        {
            theWordArray[wordNumber][hexWord] = hexValues[wordNumber*32 + hexWord];
        }
    }

    auto invert = [](std::array<uint32_t, 32>& theWord)
    {
        std::array<uint32_t, 32> theNewWord;
        for(size_t word256=0; word256<4; ++word256)
        {
            for(size_t word32=0; word32<8; ++word32)
            {
                theNewWord[word256*8 + word32] = theWord[word256*8 + (word32 + 4)%8];
            }
        }

        theWord = theNewWord;
    };



    for(auto& word: theWordArray)
    {
        invert(word);
    }

    std::array<uint32_t, 32> theStartWord;
    for(auto& thehexWord: theStartWord) thehexWord = 0xaaaaaaaa;
    theStartWord[7] = 0xaaaaaaaf;
    
    auto searchForBadBad = [](const std::array<uint32_t, 32>& theWord)
    {

        if(theWord[0] != 0xbadbadba) return false;
        if(theWord[1] != 0xdbadbadb) return false; 
        if(theWord[2] != 0xadbadbad) return false; 
        if(theWord[3] != 0xbadbadba) return false; 
        if(theWord[4] != 0xdbadbadb) return false; 
        if((theWord[5] & 0xffffff00) != 0xadbadd00) return false;
        return true;
    };

    auto searchStart = [&theStartWord](const std::array<uint32_t, 32>& theWord)
    {
        for(size_t i =0; i<32; i++)
        {
            if(theWord[i] != theStartWord[i]) return false;
        }
        return true;
    };

    auto printHybrid = [](const std::array<uint32_t, 32>& theWord, size_t offset, std::vector<std::bitset<48>>& theDecodedHydridData)
    {
        std::stringstream theBitStream;

        for(size_t stubDataWord = offset; stubDataWord< offset + 12; ++stubDataWord)
        {
            std::bitset<32> theBits(theWord[stubDataWord]);
            theBitStream<<theBits.to_string();
        }

        for (int i = 0; i < 382; i += 48) {
            std::string subValue = theBitStream.str().substr(i, 48);
            theDecodedHydridData.push_back(std::bitset<48>(subValue));
            // std::cout << theDecodedHydridData.back() << std::endl;
        }
    };

    std::vector<std::bitset<48>> theDecodedHydrid0Data;
    std::vector<std::bitset<48>> theDecodedHydrid1Data;

    bool startWordFound = false;
    for(size_t wordNumber=0; wordNumber<numberOfWords; ++wordNumber)
    {
        // if(wordNumber>10) abort();
        // printWord(theWordArray[wordNumber]);
        // printWord(theStartWord);
        // std::cout << "1kb word number " << wordNumber << std::endl;
        // printWord(theWordArray[wordNumber]);
        if(searchStart(theWordArray[wordNumber]))
        {
            
            startWordFound = true;
            // std::cout << "Package start pattern found at 1kb word number " << wordNumber << std::endl;
            continue;
        }
        if(!startWordFound) continue;
        if(!searchForBadBad(theWordArray[wordNumber]))
        {
            // std::cout << "Missing badbdabad pattern at 1kb word number " << wordNumber << std::endl;
            continue;
        }

        // std::cout << "Hybrid 0" << std::endl;
        // printHybrid(theWordArray[wordNumber], 20, theDecodedHydrid0Data);

        // std::cout << "Hybrid 1 - word number = " << wordNumber << std::endl;

        printHybrid(theWordArray[wordNumber], 8, theDecodedHydrid1Data);
    }

    size_t numberOfSkip = 0;
    std::bitset<48> startPattern    ("100000000000000000000000100000000000000000000000");
    std::bitset<48> startPatternMask("111111111100000000000011111100000000000000000000");
    // some times the start pattern is saved, sometimes is not
    std::bitset<48> counterStartPattern    ("100000000000000000000000100000000011111111111111");
    std::bitset<48> counterStartPatternMask("111111111100000000000011111100000011111111111111");
    for(auto theBXword: theDecodedHydrid1Data)
    {
        if((theBXword & counterStartPatternMask) == counterStartPattern)
        {
            numberOfSkip += 8;
            break;
        }
        if((theBXword & startPatternMask) == startPattern) break;
        ++numberOfSkip;
    }
    theDecodedHydrid1Data.erase(theDecodedHydrid1Data.begin(), theDecodedHydrid1Data.begin() + numberOfSkip);

    std::string outputFileName = fileName.substr(0, fileName.length() - 4) + ".root";
    TFile theFile(outputFileName.c_str(), "RECREATE");

    std::vector<TH2I*> theHistogramList;
    for(size_t chip=0; chip<8; ++chip)
    {
        theHistogramList.push_back(new TH2I(Form("MPA%i", chip), Form("MPA %i", chip), 120, -0.5, 119.5, 17, -0.5, 16.5));
        theHistogramList.back()->SetStats(false);
    }

    size_t numberOfBanks = theDecodedHydrid1Data.size()/8;
    std::cout<< __PRETTY_FUNCTION__ << " [" << __LINE__ << "] numberOfBanks = " << numberOfBanks << std::endl;
    
    for(size_t bankNumber=0; bankNumber<numberOfBanks; ++bankNumber)
    {
        if(bankNumber>= 17*120) break;
        uint16_t pixelRow = bankNumber%16;
        uint16_t pixelCol = bankNumber/16;
        if(bankNumber>= 16*120)
        {
            pixelRow = 16;
            pixelCol = bankNumber - 16*120;
        }
        std::string fullWord = "";
        for(size_t wordNumber=0; wordNumber<8; ++wordNumber)
        {
            // if(bankNumber>= 16*120)
            // {
            //     std::cout<< theDecodedHydrid1Data[bankNumber*8 + wordNumber].to_string() << std::endl;
            // }
            fullWord += theDecodedHydrid1Data[bankNumber*8 + wordNumber].to_string();
        }


        for(size_t chipPosition=0; chipPosition<8; ++chipPosition)
        {
            std::string chipData = fullWord.substr(28 + chipPosition*21, 21);
            uint8_t chipId = std::bitset<3>(chipData.substr(3, 3)).to_ulong();
            uint16_t counterLow = std::bitset<7>(chipData.substr(6, 7)).to_ulong();
            uint16_t counterHigh = std::bitset<7>(chipData.substr(14, 7)).to_ulong() << 7;
            uint16_t totalCounter = counterLow + counterHigh;
            if(totalCounter-- == 0) continue; // not a real counter since they are always at least 1;
            theHistogramList[chipId]->SetBinContent(pixelCol + 1, pixelRow + 1, totalCounter);
            if(pixelRow == 16 && totalCounter > 0) std::cout<< __PRETTY_FUNCTION__ << " [" << __LINE__ << "] FOUND!!! " << totalCounter << std::endl;
            if(totalCounter > 254) std::cout << fullWord << std::endl;
        }

        std::bitset<382> theBank(fullWord);
        // std::cout<<theBank<<std::endl;
    }

    TCanvas* theCanvas = new TCanvas();
    theCanvas->Divide(2,4);

    for(size_t chip=0; chip<8; ++chip)
    {
        theCanvas->cd(chip+1);
        theHistogramList[chip]->Draw("colz");
        theHistogramList[chip]->Write();
    }

    theCanvas->Write();

    theFile.Close();

    return outputFileName;
}

int main(int argc, char* argv[]) 
{
    // Check if the file name is provided as an argument
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <folderName>" << std::endl;
        return 1;
    }

    uint16_t stripThresholdStart = 65;
    uint16_t pixelThresholdStart = 200;
    uint16_t totalThresholdOffset = 30;

    // uint16_t delayStart = 15346;
    // uint16_t delayOffset = 50;

    uint16_t delayStart = 0x3bf2;
    uint16_t delayOffset = 0;

    std::string folderName = argv[1];

    std::string ouputFileName = folderName + "/SCurve.root";
    TFile theSCurveFile(ouputFileName.c_str(), "RECREATE");

    std::vector<TH2I*> theHistogramList;
    for(size_t chip=0; chip<8; ++chip)
    {
        theHistogramList.push_back(new TH2I(Form("SCurve_MPA%i", chip), Form("SCurve MPA %i", chip), 120*16, -0.5, 120*16-1, totalThresholdOffset + 1, pixelThresholdStart -0.5, pixelThresholdStart + totalThresholdOffset + 0.5));
        theHistogramList.back()->SetStats(false);
    }


    for(uint16_t delay = delayStart; delay <= delayStart + delayOffset; ++delay)
    {
        std::cout<< __PRETTY_FUNCTION__ << " [" << __LINE__ << "] delay = " << delay << std::endl;
        

        for(uint16_t thrOffset = 0; thrOffset <= totalThresholdOffset; ++thrOffset)
        {
            uint16_t stripThreshold = stripThresholdStart + thrOffset;
            uint16_t pixelThreshold = pixelThresholdStart + thrOffset;
            std::string fileName = folderName + + "/fastCounter_StripTh_" +  std::to_string(stripThreshold) + "_PixelTh_" + std::to_string(pixelThreshold) + "_delay_" + std::to_string(delay) + ".txt";
            std::string parsedFileName = parseCounterRaw(fileName);
            if(parsedFileName == "") continue;

            TFile parsedFile(parsedFileName.c_str());
            for(size_t chip=0; chip<8; ++chip)
            {
                auto theOccupancyPlot = (TH2I*)(parsedFile.Get(Form("MPA%i", chip)));
                for(uint16_t col = 0; col<120; ++col)
                {
                    for(uint16_t row = 0; row<16; ++row)
                    {
                        theHistogramList[chip]->SetBinContent(col + row*120 + 1, thrOffset+1, theOccupancyPlot->GetBinContent(col+1, row+1));
                    }
                }
            }
            parsedFile.Close();
        }
    }

    TCanvas* theCanvas = new TCanvas();
    theCanvas->Divide(2,4);

    theSCurveFile.cd();

    for(size_t chip=0; chip<8; ++chip)
    {
        theCanvas->cd(chip+1);
        theHistogramList[chip]->Draw("colz");
        theHistogramList[chip]->Write();
    }

    theCanvas->Write();

    theSCurveFile.Close();


    return 0;

}
