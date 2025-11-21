# import uproot
# import awkward as ak

# # open the file
# file = uproot.open("Results/Run_5312/run_005312_Board000.root")

# # open the tree
# tree = file["Events"]

# # read all branches
# data = tree.arrays(library="ak")

# # access simple branch
# event_id = data["eventId"]

# # access fields inside the HybridL1EventInfo object
# err_code   = data["HybridL1EventInfo.fErrorCode"]
# hybrid_id  = data["HybridL1EventInfo.fHybridId"]
# numberOfPixelClusters  = data["HybridL1EventInfo.fNumberOfPixelClusters"]
# numberOfStripClusters  = data["HybridL1EventInfo.fNumberOfStripClusters"]
# chip_id    = data["HybridL1EventInfo.fChipId"]
# cbcHitList  = data["fCBCHits"]


# # print first entry
# print("eventId:", event_id[0])
# print("Error code:", err_code[0])
# print("numberOfPixelClusters:", numberOfPixelClusters[0])
# print("numberOfStripClusters:", numberOfStripClusters[0])
# print("cbcHitList", cbcHitList)
import ROOT

# MUST load your dictionary!!!
ROOT.gSystem.Load("lib/libPh2_EventDict.so")

f = ROOT.TFile.Open("../Results/Run_5312/run_005312_Board000.root")
t = f.Get("Events")

for evt in t:
    print("eventId =", evt.eventId)

    hybrid_list = evt.HybridL1EventPS
    print("  nHybrid =", len(hybrid_list))

    for h, hybrid in enumerate(hybrid_list):
        print("   Hybrid", h, "HybridId =", hybrid.fHybridL1EventInfo.fHybridId)

        for pair in hybrid.fCBCHits:
            chip = pair.first
            clusters = pair.second
            print("     Chip", chip.fChipId, "clusters:", len(clusters))
