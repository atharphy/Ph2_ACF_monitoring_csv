/*
 ---- Instructions ----

 compile:
 g++ $(root-config --libs --cflags)  -o DecodeCounters DecodeCounters.cc

 Run
./DecodeCounters Results/Run_<runNumber>

*/

#include <TCanvas.h>
#include <TFile.h>
#include <TH2I.h>
#include <array>
#include <bitset>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdint.h>
#include <string>
#include <vector>

// Function to convert an 8-character hex string to uint32_t
uint32_t hexStringToUint32(const std::string& hexString)
{
    if(hexString.length() != 8) { throw std::invalid_argument("Invalid hex string length"); }

    return static_cast<uint32_t>(std::stoul(hexString, nullptr, 16));
}

void printWord(const std::array<uint32_t, 32>& word)
{
    for(size_t hexWord = 0; hexWord < 32; ++hexWord) { std::cout << std::hex << std::setw(8) << std::setfill('0') << word[hexWord] << " "; }
    std::cout << std::dec << std::endl;
}

std::string parseCounterRaw(const std::string& fileName, size_t numberOfWords = 0)
{
    std::cout << __PRETTY_FUNCTION__ << " [" << __LINE__ << "] parsing file " << fileName << std::endl;

    std::ifstream         inputFile(fileName);
    std::vector<uint32_t> hexValues;
    std::string           fileContents, word;

    if(!inputFile.is_open())
    {
        std::cerr << "Error opening file: " << fileName << std::endl;
        return "";
    }

    // Read the entire file into a string
    std::stringstream buffer;
    buffer << inputFile.rdbuf();
    fileContents = buffer.str();

    std::stringstream ss(fileContents);

    // Process each 8-character hex word separated by spaces
    while(ss >> word)
    {
        try
        {
            // Convert hex word to uint32_t
            uint32_t value = hexStringToUint32(word);

            // Add the value to the vector
            hexValues.push_back(value);
        }
        catch(const std::invalid_argument& e)
        {
            std::cerr << "Invalid hex word: " << word << std::endl;
            continue;
        }
    }

    inputFile.close();

    if(numberOfWords == 0) numberOfWords = hexValues.size() / 16;

    std::cout << __PRETTY_FUNCTION__ << " [" << __LINE__ << "] total banks = " << numberOfWords << std::endl;

    std::vector<std::array<uint32_t, 16>> theWordArray(numberOfWords);

    for(size_t wordNumber = 0; wordNumber < numberOfWords; ++wordNumber)
    {
        for(size_t hexWord = 0; hexWord < 16; ++hexWord) { theWordArray[wordNumber][hexWord] = hexValues[wordNumber * 16 + hexWord]; }
    }

    auto invert = [](std::array<uint32_t, 16>& theWord)
    {
        std::array<uint32_t, 16> theNewWord;
        for(size_t word256 = 0; word256 < 2; ++word256)
        {
            for(size_t word32 = 0; word32 < 8; ++word32) { theNewWord[word256 * 8 + word32] = theWord[word256 * 8 + (word32 + 4) % 8]; }
        }

        theWord = theNewWord;
    };

    for(auto& word: theWordArray) { invert(word); }

    std::array<uint32_t, 16> theStartWord;
    for(auto& thehexWord: theStartWord) thehexWord = 0xaaaaaaaa;
    theStartWord[0] = 0xFAB10FAB;
    theStartWord[1] = 0x10FAB10a;
    theStartWord[7] = 0xaaaaaaaf;
    theStartWord[8] = 0xFAB10FAB;
    theStartWord[9] = 0x10FAB10a;

    auto searchForFAB10 = [](const std::array<uint32_t, 16>& theWord)
    {
        if(theWord[0] != 0xFAB10FAB) return false;
        if((theWord[1] & 0xfffffff0) != 0x10FAB100) return false;
        if(theWord[8] != 0xFAB10FAB) return false;
        if((theWord[9] & 0xfffffff0) != 0x10FAB100) return false;
        return true;
    };

    auto searchStart = [&theStartWord](const std::array<uint32_t, 16>& theWord)
    {
        for(size_t i = 0; i < 16; i++)
        {
            if(theWord[i] != theStartWord[i]) return false;
        }
        return true;
    };

    auto decodeHybrid = [](const std::array<uint32_t, 16>& theWord, std::vector<std::bitset<196>>& theDecodedHydrid0Data, std::vector<std::bitset<196>>& theDecodedHydrid1Data)
    {
        std::stringstream theBitStream;

        for(size_t stubDataWord = 0; stubDataWord < 16; ++stubDataWord)
        {
            std::bitset<32> theBits(theWord[stubDataWord]);
            theBitStream << theBits.to_string();
        }

        theDecodedHydrid0Data.push_back(std::bitset<196>(theBitStream.str().substr(256 + 60, 196)));
        theDecodedHydrid1Data.push_back(std::bitset<196>(theBitStream.str().substr(60, 196)));
    };

    std::vector<std::vector<std::bitset<196>>> theDecodedHydridData(2);

    bool startWordFound = false;
    for(size_t wordNumber = 0; wordNumber < numberOfWords; ++wordNumber)
    {
        // if(wordNumber>10) abort();
        // printWord(theWordArray[wordNumber]);
        // printWord(theStartWord);
        // std::cout << "1kb word number " << wordNumber << std::endl;
        // printWord(theWordArray[wordNumber]);
        if(!startWordFound)
        {
            if(searchStart(theWordArray[wordNumber]))
            {
                startWordFound = true;
                // std::cout << "Package start pattern found at 1kb word number " << wordNumber << std::endl;
            }
            continue;
        }
        if(!searchForFAB10(theWordArray[wordNumber]))
        {
            std::cout << "Missing badbdabad pattern at 1kb word number " << wordNumber << std::endl;
            continue;
        }

        // std::cout << "Hybrid 0" << std::endl;
        // decodeHybrid(theWordArray[wordNumber], 20, theDecodedHydrid0Data);

        // std::cout << "Hybrid 1 - word number = " << wordNumber << std::endl;

        decodeHybrid(theWordArray[wordNumber], theDecodedHydridData[0], theDecodedHydridData[1]);
    }

    std::string outputFileName = fileName.substr(0, fileName.length() - 4) + ".root";
    TFile       theFile(outputFileName.c_str(), "RECREATE");

    for(size_t hybrid = 0; hybrid < 2; ++hybrid)
    {
        theFile.mkdir(Form("Hybrid_%i", hybrid));
        theFile.cd(Form("Hybrid_%i", hybrid));

        // size_t numberOfSkip = 0;
        // std::bitset<48> startPattern    ("100000000000000000000000100000000000000000000000");
        // std::bitset<48> startPatternMask("111111111100000000000011111100000000000000000000");
        // // some times the start pattern is saved, sometimes is not
        // std::bitset<48> counterStartPattern    ("100000000000000000000000100000000011111111111111");
        // std::bitset<48> counterStartPatternMask("111111111100000000000011111100000011111111111111");
        // for(auto theBXword: theDecodedHydridData[hybrid])
        // {
        //     if((theBXword & counterStartPatternMask) == counterStartPattern)
        //     {
        //         std::cout<< __PRETTY_FUNCTION__ << " [" << __LINE__ << "] FOUND start pattern!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!" << std::endl;
        //         numberOfSkip += 8;
        //         break;
        //     }
        //     if((theBXword & startPatternMask) == startPattern) break;
        //     ++numberOfSkip;
        // }
        // theDecodedHydridData[hybrid].erase(theDecodedHydridData[hybrid].begin(), theDecodedHydridData[hybrid].begin() + numberOfSkip);

        std::vector<TH2I*> theMPAhistogramList;
        for(size_t chip = 0; chip < 8; ++chip)
        {
            theMPAhistogramList.push_back(new TH2I(Form("Hybrid_%i_MPA%i", hybrid, chip), Form("Hybrid %i MPA %i", hybrid, chip), 120, -0.5, 119.5, 16, -0.5, 16.5));
            theMPAhistogramList.back()->SetStats(false);
        }

        std::vector<TH1I*> theSSAhistogramList;
        for(size_t chip = 0; chip < 8; ++chip)
        {
            theSSAhistogramList.push_back(new TH1I(Form("Hybrid_%i_SSA%i", hybrid, chip), Form("Hybrid %i SSA %i", hybrid, chip), 120, -0.5, 119.5));
            theSSAhistogramList.back()->SetStats(false);
        }

        size_t numberOfBanks = theDecodedHydridData[hybrid].size();
        std::cout << __PRETTY_FUNCTION__ << " [" << __LINE__ << "] numberOfBanks = " << numberOfBanks << std::endl;

        uint16_t bxOffset = 8;
        for(size_t bankNumber = 0; bankNumber < numberOfBanks; ++bankNumber)
        {
            if(bankNumber >= 17 * 120) break;
            uint16_t pixelCol = bankNumber % 120;
            uint16_t pixelRow = bankNumber / 120;
            if(bankNumber >= 16 * 120)
            {
                pixelRow = 16;
                pixelCol = bankNumber - 16 * 120;
            }
            std::string fullWord = theDecodedHydridData[hybrid][bankNumber].to_string();

            uint16_t numberOfStubs = std::bitset<6>(fullWord.substr(22, 6)).to_ulong();
            if(numberOfStubs != 8)
            {
                // std::cout<< __PRETTY_FUNCTION__ << " [" << __LINE__ << "] Event " << bankNumber << " without 8 stubs, read out " << numberOfStubs << std::endl;
                // std::cout<< __PRETTY_FUNCTION__ << " [" << __LINE__ << "] " << fullWord.substr(0, 1) << std::endl;
                // std::cout<< __PRETTY_FUNCTION__ << " [" << __LINE__ << "] " << fullWord.substr(1, 9) << std::endl;
                // std::cout<< __PRETTY_FUNCTION__ << " [" << __LINE__ << "] " << fullWord.substr(10, 12) << std::endl;
                // std::cout<< __PRETTY_FUNCTION__ << " [" << __LINE__ << "] " << fullWord.substr(22, 6) << std::endl;
                // for(size_t stubPosition=0; stubPosition<16; ++stubPosition)
                // {
                //     std::string theStubString = fullWord.substr(28 + stubPosition*21, 21);
                //     std::cout<< __PRETTY_FUNCTION__ << " [" << __LINE__ << "] " << theStubString.substr(0, 3) << " " << theStubString.substr(3, 3) << " " << theStubString.substr(6, 15) <<
                //     std::endl;
                // }
                // std::cout<< __PRETTY_FUNCTION__ << " [" << __LINE__ << "] " << fullWord.substr(364, 20) << std::endl;
            }

            for(size_t chipPosition = 0; chipPosition < 8; ++chipPosition)
            {
                std::string chipData = fullWord.substr(28 + chipPosition * 21, 21);
                // if(bxOffset == 8) bxOffset = std::bitset<3>(chipData.substr(0, 3)).to_ulong();
                // else if(bxOffset != std::bitset<3>(chipData.substr(0, 3)).to_ulong())
                // {
                //     std::cout<< __PRETTY_FUNCTION__ << " [" << __LINE__ << "] BX offset changed in a run!!!!!!!!!!!!!!!!" << std::endl;
                //     std::cout<< __PRETTY_FUNCTION__ << " [" << __LINE__ << "] expectd " << +bxOffset << " found " << +std::bitset<3>(chipData.substr(0, 3)).to_ulong() << std::endl;
                //     std::cout<< __PRETTY_FUNCTION__ << " [" << __LINE__ << "] chipData = " << chipData << std::endl;
                // }
                uint8_t  chipId       = std::bitset<3>(chipData.substr(3, 3)).to_ulong();
                uint16_t counterLow   = std::bitset<7>(chipData.substr(6, 7)).to_ulong();
                uint16_t counterHigh  = std::bitset<7>(chipData.substr(14, 7)).to_ulong() << 7;
                uint16_t totalCounter = counterLow + counterHigh;
                if(totalCounter-- == 0) continue; // not a real counter since they are always at least 1;
                if(pixelRow == 16)
                {
                    // if(totalCounter > 0) std::cout<< __PRETTY_FUNCTION__ << " [" << __LINE__ << "] FOUND!!!" << std::endl;
                    theSSAhistogramList[chipId]->SetBinContent(pixelCol + 1, totalCounter);
                }
                else
                    theMPAhistogramList[chipId]->SetBinContent(pixelCol + 1, pixelRow + 1, totalCounter);
                if(totalCounter > 255)
                {
                    std::cout << "MPA" << chipId << " - " << pixelCol << " - " << pixelRow << " count " << totalCounter << std::endl;
                    std::cout << fullWord << std::endl;
                }
            }
        }

        // std::cout<< __PRETTY_FUNCTION__ << " [" << __LINE__ << "] BX offset = " << bxOffset << std::endl;

        TCanvas* theMPAcanvas = new TCanvas();
        theMPAcanvas->SetName(Form("Hybrid_%i_AllMPA", hybrid));
        theMPAcanvas->Divide(2, 4);

        for(size_t chip = 0; chip < 8; ++chip)
        {
            theMPAcanvas->cd(chip + 1);
            theMPAhistogramList[chip]->Draw("colz");
            theMPAhistogramList[chip]->Write();
        }

        theMPAcanvas->Write();

        TCanvas* theSSAcanvas = new TCanvas();
        theSSAcanvas->SetName(Form("Hybrid_%i_AllSSA", hybrid));

        theSSAcanvas->Divide(2, 4);

        for(size_t chip = 0; chip < 8; ++chip)
        {
            theSSAcanvas->cd(chip + 1);
            theSSAhistogramList[chip]->Draw();
            theSSAhistogramList[chip]->Write();
        }

        theSSAcanvas->Write();
    }

    theFile.Close();

    return outputFileName;
}

int main(int argc, char* argv[])
{
    // Check if the file name is provided as an argument
    if(argc < 2 || argc > 3)
    {
        std::cerr << "Usage: " << argv[0] << " <folderName> [numberOfWords]" << std::endl;
        return 1;
    }

    size_t numberOfWords = 0;
    if(argc == 3) numberOfWords = atoi(argv[2]);

    uint16_t numberOfLinks = 1;

    // uint16_t stripThresholdStart = 94;
    // uint16_t pixelThresholdStart = 229;
    // uint16_t totalThresholdOffset = 0;

    uint16_t stripThresholdStart  = 65;
    uint16_t pixelThresholdStart  = 200;
    uint16_t totalThresholdOffset = 30;

    std::string folderName = argv[1];

    std::string ouputFileName = folderName + "/SCurve.root";
    TFile       theSCurveFile(ouputFileName.c_str(), "RECREATE");

    for(size_t link = 0; link < numberOfLinks; ++link)
    {
        theSCurveFile.mkdir(Form("OpticalGroup_%i", link));

        std::map<size_t, std::vector<TH2I*>> theMPAhistogramList;
        std::map<size_t, std::vector<TH2I*>> theSSAhistogramList;
        for(size_t hybrid = 0; hybrid < 2; ++hybrid)
        {
            theSCurveFile.mkdir(Form("OpticalGroup_%i/Hybrid_%i", link, hybrid));
            theSCurveFile.cd(Form("OpticalGroup_%i/Hybrid_%i", link, hybrid));
            for(size_t chip = 0; chip < 8; ++chip)
            {
                theMPAhistogramList[hybrid].push_back(new TH2I(Form("SCurve_OpticalGroup%i_Hybrid%i_MPA%i", link, hybrid, chip),
                                                               Form("SCurve_OpticalGroup%i_Hybrid%i MPA %i", link, hybrid, chip),
                                                               120 * 16,
                                                               -0.5,
                                                               120 * 16 - 0.5,
                                                               totalThresholdOffset + 1,
                                                               pixelThresholdStart - 0.5,
                                                               pixelThresholdStart + totalThresholdOffset + 0.5));
                theMPAhistogramList[hybrid].back()->SetStats(false);
            }

            for(size_t chip = 0; chip < 8; ++chip)
            {
                theSSAhistogramList[hybrid].push_back(new TH2I(Form("SCurve_OpticalGroup%i_Hybrid%i_SSA%i", link, hybrid, chip),
                                                               Form("SCurve_OpticalGroup%i_Hybrid%i SSA %i", link, hybrid, chip),
                                                               120,
                                                               -0.5,
                                                               120 - 0.5,
                                                               totalThresholdOffset + 1,
                                                               stripThresholdStart - 0.5,
                                                               stripThresholdStart + totalThresholdOffset + 0.5));
                theSSAhistogramList[hybrid].back()->SetStats(false);
            }
        }

        for(uint16_t thrOffset = 0; thrOffset <= totalThresholdOffset; ++thrOffset)
        {
            uint16_t    stripThreshold = stripThresholdStart + thrOffset;
            uint16_t    pixelThreshold = pixelThresholdStart + thrOffset;
            std::string fileName       = folderName + "/fastCounter_StripTh_" + std::to_string(stripThreshold) + "_PixelTh_" + std::to_string(pixelThreshold) + "_link" + std::to_string(link) + ".txt";
            std::string parsedFileName = parseCounterRaw(fileName, numberOfWords);
            if(parsedFileName == "") continue;

            TFile parsedFile(parsedFileName.c_str());
            for(size_t hybrid = 0; hybrid < 2; ++hybrid)
            {
                for(size_t chip = 0; chip < 8; ++chip)
                {
                    auto theMPAoccupancyPlot = (TH2I*)(parsedFile.Get(Form("Hybrid_%i/Hybrid_%i_MPA%i", hybrid, hybrid, chip)));
                    for(uint16_t col = 0; col < 120; ++col)
                    {
                        for(uint16_t row = 0; row < 16; ++row)
                        {
                            theMPAhistogramList[hybrid][chip]->SetBinContent(col + row * 120 + 1, thrOffset + 1, theMPAoccupancyPlot->GetBinContent(col + 1, row + 1));
                        }
                    }

                    auto theSSAoccupancyPlot = (TH1I*)(parsedFile.Get(Form("Hybrid_%i/Hybrid_%i_SSA%i", hybrid, hybrid, chip)));
                    for(uint16_t col = 0; col < 120; ++col) { theSSAhistogramList[hybrid][chip]->SetBinContent(col + 1, thrOffset + 1, theSSAoccupancyPlot->GetBinContent(col + 1)); }
                }
            }
            parsedFile.Close();
        }

        for(size_t hybrid = 0; hybrid < 2; ++hybrid)
        {
            TCanvas* theMPAcanvas = new TCanvas();
            theMPAcanvas->SetName(Form("OpticalGroup_%i/Hybrid_%i_AllMPA", link, hybrid));
            theMPAcanvas->Divide(2, 4);

            theSCurveFile.cd();

            for(size_t chip = 0; chip < 8; ++chip)
            {
                theMPAcanvas->cd(chip + 1);
                theMPAhistogramList[hybrid][chip]->Draw("colz");
                theSCurveFile.cd(Form("OpticalGroup_%i/Hybrid_%i", link, hybrid));
                theMPAhistogramList[hybrid][chip]->Write();
            }
            theMPAcanvas->Write();

            TCanvas* theSSAcanvas = new TCanvas();
            theSSAcanvas->SetName(Form("OpticalGroup_%i/Hybrid_%i_AllSSA", link, hybrid));
            theSSAcanvas->Divide(2, 4);

            theSCurveFile.cd();

            for(size_t chip = 0; chip < 8; ++chip)
            {
                theSSAcanvas->cd(chip + 1);
                theSSAhistogramList[hybrid][chip]->Draw("colz");
                theSCurveFile.cd(Form("OpticalGroup_%i/Hybrid_%i", link, hybrid));
                theSSAhistogramList[hybrid][chip]->Write();
            }

            theSSAcanvas->Write();
        }
    }

    theSCurveFile.Close();

    return 0;
}
