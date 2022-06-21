/*!
  \file                  RD53B.h
  \brief                 RD53B description class, config of the RD53B
  \author                Mauro DINARDO and Alkiviadis PAPADOPOULOS
  \version               1.0
  \date                  28/06/22
  Support:               email to mauro.dinardo@cern.ch
  Support:               email to alkiviadis.papadopoulos@cern.ch
*/

#ifndef RD53B_H
#define RD53B_H

#include "RD53.h"

namespace Ph2_HwDescription
{
class RD53B : public RD53
{
  public:
    static constexpr size_t NROWS = 336; // Total number of rows
    static constexpr size_t NCOLS = 432; // Total number of columns

    // ########################################
    // # Support for different FrontEnd types #
    // ########################################
    struct FrontEnd
    {
        const char* name;
        const char* thresholdReg;
        const char* gainReg;
        size_t      nTDACvalues;
        size_t      colStart;
        size_t      colStop;
    };

    static constexpr FrontEnd CROC = {"CROC", "DAC_GDAC_M_LIN", "DAC_KRUM_CURR_LIN", 32, 0, RD53B::NCOLS-1};

    std::unique_ptr<ChannelGroupBase> getChannelGroup() const override { return new ChannelGroup<RD53B::NROWS, RD53B::NCOLS>; }
    size_t getNRows() const override { return RD53B::NROWS; }
    size_t getNCols() const override { return RD53B::NCOLS; }
    const FrontEnd* getMajorityFE(size_t colStart, size_t colStop) const override { return RD53B::CROC; }
};

} // namespace Ph2_HwDescription

#endif
