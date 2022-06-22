/*!
  \file                  RD53A.h
  \brief                 RD53A description class
  \author                Mauro DINARDO and Alkiviadis PAPADOPOULOS
  \version               1.0
  \date                  28/06/22
  Support:               email to mauro.dinardo@cern.ch
  Support:               email to alkiviadis.papadopoulos@cern.ch
*/

#ifndef RD53A_H
#define RD53A_H

#include "RD53.h"

namespace Ph2_HwDescription
{
class RD53A : public RD53
{
  public:
    static constexpr size_t NROWS = 192; // Total number of rows
    static constexpr size_t NCOLS = 400; // Total number of columns

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

    static constexpr FrontEnd SYNC = {"SYNC", "VTH_SYNC", "IBIAS_KRUM_SYNC", 0, 0, 127};
    static constexpr FrontEnd LIN  = {"LIN", "Vthreshold_LIN", "KRUM_CURR_LIN", 16, 128, 263};
    static constexpr FrontEnd DIFF = {"DIFF", "VTH1_DIFF", "VFF_DIFF", 31, 264, 399};
    static const FrontEnd*    frontEnds[];

    std::unique_ptr<ChannelGroupBase> getChannelGroup() const override { return new ChannelGroup<RD53A::NROWS, RD53A::NCOLS>; }
    ChannelGroupBase* getChannelGroupAll() const override { return new RD53ChannelGroupHandler::RD53ChannelGroupAll<RD53A::NROWS, RD53A::NCOLS>; }
    ChannelGroupBase* getChannelGroupPattern(uint8_t hitPerCol) const override { return new RD53ChannelGroupHandler::RD53ChannelGroupPattern<RD53A::NROWS, RD53A::NCOLS>(hitPerCol); }
    size_t                            getNRows() const override { return RD53A::NROWS; }
    size_t                            getNCols() const override { return RD53A::NCOLS; }
    const FrontEnd*                   getMajorityFE(size_t colStart, size_t colStop) const override;

    struct HitData
    {
        HitData(uint16_t row, uint16_t col, uint8_t tot) : row(row), col(col), tot(tot) {}

        uint16_t row;
        uint16_t col;
        uint8_t  tot;
    };

    struct Event
    {
        Event(const uint32_t* data, size_t n);

        uint16_t             trigger_id;
        uint16_t             trigger_tag;
        uint16_t             bc_id;
        std::vector<HitData> hit_data;

        uint16_t eventStatus;

      private:
        void DecodeQuad(uint32_t data);
    };
};

} // namespace Ph2_HwDescription

#endif
