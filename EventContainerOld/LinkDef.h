// LinkDef.h
#ifdef __CLING__
#pragma link off all globals;
#pragma link off all classes;
#pragma link off all functions;

// Simple structs
#pragma link C++ struct HybridL1EventInfo+;
#pragma link C++ struct ChipL1EventInfo+;
#pragma link C++ struct StripClusterPS+;

// Containers used inside fCBCHits
#pragma link C++ class std::vector<StripClusterPS>+;
#pragma link C++ class std::pair<ChipL1EventInfo, std::vector<StripClusterPS>>+;
#pragma link C++ class std::vector<std::pair<ChipL1EventInfo, std::vector<StripClusterPS>>>+;

// The main class you store in the TTree
#pragma link C++ class HybridL1EventPS+;
#pragma link C++ class std::vector<HybridL1EventPS>+;

#endif
