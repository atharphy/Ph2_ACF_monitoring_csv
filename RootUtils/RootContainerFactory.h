/*

        \file                          RootContainerFactory.h
        \brief                         Container factory for DQM
        \author                        Fabio Ravera, Lorenzo Uplegger
        \version                       1.0
        \date                          14/06/19
        Support :                      mail to : fabio.ravera@cern.ch

 */

#ifndef __ROOTCONTAINERFACTORY_H__
#define __ROOTCONTAINERFACTORY_H__

#include "../RootUtils/PlotContainer.h"
#include "../Utils/Container.h"
#include "../Utils/DataContainer.h"

#include "TFile.h"
#include <iostream>
#include <map>
#include <vector>

namespace RootContainerFactory
{
namespace details
{
inline void createAndOpenRootFileFolder(TFile* theOutputFile, std::string& folderName)
{
    if(theOutputFile->GetDirectory(folderName.data()) == nullptr) theOutputFile->mkdir(folderName.data());
    theOutputFile->cd(folderName.data());
}

template <typename T, typename std::enable_if<!std::is_base_of<PlotContainer, T>::value, int>::type = 0>
std::string getPlotName(T* plot)
{
    return "NULL";
}

template <typename T, typename std::enable_if<std::is_base_of<PlotContainer, T>::value, int>::type = 0>
std::string getPlotName(T* plot)
{
    return plot->getName();
}

template <typename T, typename std::enable_if<!std::is_base_of<PlotContainer, T>::value, int>::type = 0>
std::string getPlotTitle(T* plot)
{
    return "NULL";
}

template <typename T, typename std::enable_if<std::is_base_of<PlotContainer, T>::value, int>::type = 0>
std::string getPlotTitle(T* plot)
{
    return plot->getTitle();
}

template <typename T, typename std::enable_if<!std::is_base_of<PlotContainer, T>::value, int>::type = 0>
void initializePlot(T* plot, std::string name, std::string title, const T* reference)
{
    ;
}

template <typename T, typename std::enable_if<std::is_base_of<PlotContainer, T>::value, int>::type = 0>
void initializePlot(T* plot, std::string name, std::string title, const T* reference)
{
    plot->initialize(name, title, reference);
}
} // namespace details

using namespace details;

template <typename T, typename SC, typename SM, typename SO, typename SB, typename SD>
void bookHistogramsFromStructure(TFile*                   theOutputFile,
                                 const DetectorContainer& original,
                                 DetectorDataContainer&   copy,
                                 const T&                 channel,
                                 const SC&                chipSummary,
                                 const SM&                hybridSummary,
                                 const SO&                opticalGroupSummary,
                                 const SB&                boardSummary,
                                 const SD&                detectorSummary)
{
    copy.reset();

    std::string detectorFolder = "Detector";
    createAndOpenRootFileFolder(theOutputFile, detectorFolder);
    std::string channelHistogramGenericName             = getPlotName(&channel);
    std::string chipSummaryHistogramGenericName         = getPlotName(&chipSummary);
    std::string hybridSummaryHistogramGenericName       = getPlotName(&hybridSummary);
    std::string opticalGroupSummaryHistogramGenericName = getPlotName(&opticalGroupSummary);
    std::string boardSummaryHistogramGenericName        = getPlotName(&boardSummary);
    std::string detectorSummaryHistogramGenericName     = getPlotName(&detectorSummary);

    std::string channelHistogramGenericTitle             = getPlotTitle(&channel);
    std::string chipSummaryHistogramGenericTitle         = getPlotTitle(&chipSummary);
    std::string hybridSummaryHistogramGenericTitle       = getPlotTitle(&hybridSummary);
    std::string opticalGroupSummaryHistogramGenericTitle = getPlotTitle(&opticalGroupSummary);
    std::string boardSummaryHistogramGenericTitle        = getPlotTitle(&boardSummary);
    std::string detectorSummaryHistogramGenericTitle     = getPlotTitle(&detectorSummary);

    copy.initialize<SD, SB>();

    SD theDetectorSummary;
    initializePlot(&theDetectorSummary, Form("%s_Detector", detectorSummaryHistogramGenericName.data()), Form("%s Detector", detectorSummaryHistogramGenericTitle.data()), &detectorSummary);
    copy.getSummary<SD, SB>() = std::move(theDetectorSummary);

    // Boards
    for(const BoardContainer* board: original)
    {
        std::string boardFolder     = "/Board_" + std::to_string(board->getId());
        std::string fullBoardFolder = detectorFolder + boardFolder;
        createAndOpenRootFileFolder(theOutputFile, fullBoardFolder);

        BoardDataContainer* copyBoard = copy.addBoardDataContainer(board->getId());
        copyBoard->initialize<SB, SM>();

        SB theBoardSummary;
        initializePlot(&theBoardSummary,
                       Form("D_%s_Board_(%d)", boardSummaryHistogramGenericName.data(), board->getId()),
                       Form("D_%s_Board(%d)", boardSummaryHistogramGenericTitle.data(), board->getId()),
                       &boardSummary);
        copyBoard->getSummary<SB, SO>() = std::move(theBoardSummary);

        // OpticalGroups
        for(const OpticalGroupContainer* opticalGroup: *board)
        {
            std::string opticalGroupFolder     = "/OpticalGroup_" + std::to_string(opticalGroup->getId());
            std::string fullOpticalGroupFolder = detectorFolder + boardFolder + opticalGroupFolder;
            createAndOpenRootFileFolder(theOutputFile, fullOpticalGroupFolder);

            OpticalGroupDataContainer* copyOpticalGroup = copyBoard->addOpticalGroupDataContainer(opticalGroup->getId());
            copyOpticalGroup->initialize<SO, SM>();

            SO theOpticalGroupSummary;
            initializePlot(&theOpticalGroupSummary,
                           Form("D_%s_OpticalGroup_(%d)", opticalGroupSummaryHistogramGenericName.data(), opticalGroup->getId()),
                           Form("D_%s_OpticalGroup(%d)", opticalGroupSummaryHistogramGenericTitle.data(), opticalGroup->getId()),
                           &opticalGroupSummary);
            copyOpticalGroup->getSummary<SO, SM>() = std::move(theOpticalGroupSummary);

            // Hybrids
            for(const HybridContainer* hybrid: *opticalGroup)
            {
                std::string hybridFolder     = "/Hybrid_" + std::to_string(hybrid->getId());
                std::string fullHybridFolder = detectorFolder + boardFolder + opticalGroupFolder + hybridFolder;
                createAndOpenRootFileFolder(theOutputFile, fullHybridFolder);

                HybridDataContainer* copyHybrid = copyOpticalGroup->addHybridDataContainer(hybrid->getId());
                copyHybrid->initialize<SM, SC>();

                SM theHybridSummary;
                initializePlot(&theHybridSummary,
                               Form("D_B(%d)_O(%d)_%s_Hybrid(%d)", board->getId(), opticalGroup->getId(), hybridSummaryHistogramGenericName.data(), hybrid->getId()),
                               Form("D_B(%d)_O(%d)_%s_Hybrid(%d)", board->getId(), opticalGroup->getId(), hybridSummaryHistogramGenericTitle.data(), hybrid->getId()),
                               &hybridSummary);
                copyHybrid->getSummary<SM, SC>() = std::move(theHybridSummary);

                // Chips
                for(const ChipContainer* chip: *hybrid)
                {
                    std::string chipFolder     = "/Chip_" + std::to_string(chip->getId());
                    std::string fullChipFolder = detectorFolder + boardFolder + opticalGroupFolder + hybridFolder + chipFolder;
                    createAndOpenRootFileFolder(theOutputFile, fullChipFolder);

                    ChipDataContainer* copyChip = copyHybrid->addChipDataContainer(chip->getId(), chip->getNumberOfRows(), chip->getNumberOfCols());
                    copyChip->initialize<SC, T>();

                    SC theChipSummary;
                    initializePlot(&theChipSummary,
                                   Form("D_B(%d)_O(%d)_H(%d)_%s_Chip(%d)", board->getId(), opticalGroup->getId(), hybrid->getId(), chipSummaryHistogramGenericName.c_str(), chip->getId()),
                                   Form("D_B(%d)_O(%d)_H(%d)_%s_Chip(%d)", board->getId(), opticalGroup->getId(), hybrid->getId(), chipSummaryHistogramGenericTitle.c_str(), chip->getId()),
                                   &chipSummary);
                    copyChip->getSummary<SC, T>() = std::move(theChipSummary);

                    // Channels
                    std::string channelFolder     = "/Channel";
                    std::string fullChannelFolder = detectorFolder + boardFolder + opticalGroupFolder + hybridFolder + chipFolder + channelFolder;
                    createAndOpenRootFileFolder(theOutputFile, fullChannelFolder);

                    for(uint32_t row = 0; row < chip->getNumberOfRows(); ++row)
                    {
                        for(uint32_t col = 0; col < chip->getNumberOfCols(); ++col)
                        {
                            if(channelHistogramGenericName != "NULL")
                            {
                                T           theChannel;
                                std::string histogramName;
                                std::string histogramTitle;
                                if(chip->getNumberOfCols() == 1)
                                {
                                    histogramName = Form(
                                        "D_B(%d)_O(%d)_H(%d)_C(%d)_%s_Channel(%d)", board->getId(), opticalGroup->getId(), hybrid->getId(), chip->getId(), channelHistogramGenericName.data(), row);
                                    histogramTitle = Form(
                                        "D_B(%d)_O(%d)_H(%d)_C(%d)_%s_Channel(%d)", board->getId(), opticalGroup->getId(), hybrid->getId(), chip->getId(), channelHistogramGenericTitle.data(), row);
                                }
                                else
                                {
                                    histogramName  = Form("D_B(%d)_O(%d)_H(%d)_C(%d)_%s_Row(%d)_Col(%d)",
                                                         board->getId(),
                                                         opticalGroup->getId(),
                                                         hybrid->getId(),
                                                         chip->getId(),
                                                         channelHistogramGenericName.data(),
                                                         row,
                                                         col);
                                    histogramTitle = Form("D_B(%d)_O(%d)_H(%d)_C(%d)_%s_Row(%d)_Col(%d)",
                                                          board->getId(),
                                                          opticalGroup->getId(),
                                                          hybrid->getId(),
                                                          chip->getId(),
                                                          channelHistogramGenericTitle.data(),
                                                          row,
                                                          col);
                                }
                                initializePlot(&theChannel, histogramName, histogramTitle, &channel);
                                copyChip->getChannel<T>(row, col) = std::move(theChannel);
                            }
                        }
                    }
                }
            }
        }
    }

    theOutputFile->cd();
}

template <typename T>
void bookHistogramsFromStructure(TFile* theOutputFile, const DetectorContainer& original, DetectorDataContainer& copy, const T& channel)
{
    bookHistogramsFromStructure<T, T, T, T, T, T>(theOutputFile, original, copy, channel, channel, channel, channel, channel, channel);
}

template <typename T, typename S>
void bookHistogramsFromStructure(TFile* theOutputFile, const DetectorContainer& original, DetectorDataContainer& copy, const T& channel, const S& summay)
{
    bookHistogramsFromStructure<T, S, S, S, S, S>(theOutputFile, original, copy, channel, summay, summay, summay, summay, summay);
}

template <typename T>
void bookChannelHistograms(TFile* theOutputFile, const DetectorContainer& original, DetectorDataContainer& copy, const T& channel)
{
    EmptyContainer theEmpty;
    bookHistogramsFromStructure<T, EmptyContainer, EmptyContainer, EmptyContainer, EmptyContainer, EmptyContainer>(
        theOutputFile, original, copy, channel, theEmpty, theEmpty, theEmpty, theEmpty, theEmpty);
}

template <typename T>
void bookChipHistograms(TFile* theOutputFile, const DetectorContainer& original, DetectorDataContainer& copy, const T& chipSummary)
{
    EmptyContainer theEmpty;
    bookHistogramsFromStructure<EmptyContainer, T, EmptyContainer, EmptyContainer, EmptyContainer, EmptyContainer>(
        theOutputFile, original, copy, theEmpty, chipSummary, theEmpty, theEmpty, theEmpty, theEmpty);
}

template <typename T>
void bookHybridHistograms(TFile* theOutputFile, const DetectorContainer& original, DetectorDataContainer& copy, const T& hybridSummary)
{
    EmptyContainer theEmpty;
    bookHistogramsFromStructure<EmptyContainer, EmptyContainer, T, EmptyContainer, EmptyContainer, EmptyContainer>(
        theOutputFile, original, copy, theEmpty, theEmpty, hybridSummary, theEmpty, theEmpty, theEmpty);
}

template <typename T>
void bookOpticalGroupHistograms(TFile* theOutputFile, const DetectorContainer& original, DetectorDataContainer& copy, const T& opticalGroupSummary)
{
    EmptyContainer theEmpty;
    bookHistogramsFromStructure<EmptyContainer, EmptyContainer, EmptyContainer, T, EmptyContainer, EmptyContainer>(
        theOutputFile, original, copy, theEmpty, theEmpty, theEmpty, opticalGroupSummary, theEmpty, theEmpty);
}

template <typename T>
void bookBoardHistograms(TFile* theOutputFile, const DetectorContainer& original, DetectorDataContainer& copy, const T& boardSummary)
{
    EmptyContainer theEmpty;
    bookHistogramsFromStructure<EmptyContainer, EmptyContainer, EmptyContainer, EmptyContainer, T, EmptyContainer>(
        theOutputFile, original, copy, theEmpty, theEmpty, theEmpty, theEmpty, boardSummary, theEmpty);
}

template <typename T>
void bookDetectorHistograms(TFile* theOutputFile, const DetectorContainer& original, DetectorDataContainer& copy, const T& detectorSummary)
{
    EmptyContainer theEmpty;
    bookHistogramsFromStructure<EmptyContainer, EmptyContainer, EmptyContainer, EmptyContainer, EmptyContainer, T>(
        theOutputFile, original, copy, theEmpty, theEmpty, theEmpty, theEmpty, theEmpty, detectorSummary);
}

} // namespace RootContainerFactory

#endif
