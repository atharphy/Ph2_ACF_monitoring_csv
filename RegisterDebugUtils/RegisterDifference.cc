#include <iostream>
#include <fstream>
#include <string>
#include <unordered_map>
#include <sstream>
#include <regex>
#include <map>

enum class ChipType{LpGBT = 0, CIC, MPA, SSA, CBC};

std::map<std::string, ChipType> theChipTypeMap {{"LpGBT", ChipType::LpGBT}, {"CIC", ChipType::CIC}, {"MPA", ChipType::MPA}, {"SSA", ChipType::SSA}, {"CBC", ChipType::CBC}};

std::map<ChipType, std::vector<std::regex>> listOfFreeRegistersMap;

void initializeFreeRegisters()
{
    listOfFreeRegistersMap[ChipType::LpGBT] =
    {
        std::regex("^ConfigPins$"),
        std::regex("^I2CSlaveAddress$"),
        std::regex("^EPRX[0-6]Locked$"),
        std::regex("^EPRX[0-6]CurrentPhase[13][02]$"),
        std::regex("^EPRXEcCurrentPhase$"),
        std::regex("^EPRX[0-6]DLLStatus$"),
        std::regex(R"(^I2CM[0-2](?!Config$).*)"),
        std::regex("^I2CM[0-2]Data[0-3]$"),
        std::regex("^I2CM[0-2]Cmd$"),
        std::regex("^PSStatus$"),
        std::regex("^PIOIn[HL]$"),
        std::regex("^FUSE.*"),
        std::regex("^FuseMagic$"),
        std::regex("^ProcessMonitorStatus$"),
        std::regex("^PMFreq[A-C]$"),
        std::regex("^SEUCount[HL]$"),
        std::regex("^CLKGStatus[0-9]$"),
        std::regex("^DLDPFecCorrectionCount[0-3]$"),
        std::regex("^ADCStatus[HL]$"),
        std::regex("^EOMStatus$"),
        std::regex("^EOMCounterValue[HL]$"),
        std::regex("^EOMCounter40M[HL]$"),
        std::regex("^BERTStatus$"),
        std::regex("^BERTResult[0-4]$"),
        std::regex("^ROM$"),
        std::regex("^PORBOR$"),
        std::regex("^PUSM.*"),
        std::regex("^CRCValue[0-3]$"),
        std::regex("^FailedCRC$"),
        std::regex("^TOValue$"),
        std::regex("^SCStatus$"),
        std::regex("^FAState$"),
        std::regex("^FAHeader.*"),
        std::regex("^FALossOfLockCount$"),
        std::regex("^ConfigErrorCounter[HL]$"),
        std::regex("^POWERUP2$"),
        std::regex("^EPRX[0-6]DllStatus$")
    };

    listOfFreeRegistersMap[ChipType::CIC] =
    {
        std::regex("^EfuseValue[0-3]$")
    };

    listOfFreeRegistersMap[ChipType::CBC] =
    {
        std::regex("^ChipIDFuse[1-3]$")
    };

    listOfFreeRegistersMap[ChipType::MPA] =
    {
        std::regex("^Mask$"),
        std::regex("^EfuseProg[0-3]$"),
        std::regex(".*_ALL"),
        std::regex("^EfuseValue[0-3]$")
    };

    listOfFreeRegistersMap[ChipType::SSA] =
    {
        std::regex("^mask_strip$"),
        std::regex("^mask_peri_[AD]$"),
        std::regex("^Fuse_Prog_b[0-3]$"),
        std::regex("^ENFLAGS$"),
        std::regex("^StripControl2$"),
        std::regex("^THTRIMMING$"),
        std::regex("^DigCalibPattern_[LH]$"),
        std::regex("^AC_ReadCounter[LM]SB$"),
        std::regex("^SEUcnt$"),
        std::regex("^Ring_oscillator$"),
        std::regex("^ADC_out$"),
        std::regex("^bist_output$"),
        std::regex("^AC_ReadCounter$"),
        std::regex("^status_reg$"),
        std::regex("^Fuse_Value_b[0-3]$")
    };
}

bool matchWithPatternList(ChipType theChipType, std::string registerName)
{
    for(const auto pattern : listOfFreeRegistersMap[theChipType])
    {
        if(std::regex_search(registerName, pattern)) 
        {
            return true;
        }
    }
    return false;
}

// Function to extract the last hex number from a line
std::pair<std::string, uint8_t> getRegNameAndValue(const std::string& line, ChipType theChipType) {

    std::istringstream input(line);
    std::string name, pageString, addressString, defValueString, valueString;
    input >> name >> pageString >> addressString >> defValueString >> valueString;
    if(matchWithPatternList(theChipType, name)) return std::make_pair<std::string, uint8_t>("DUMMY", 0);
    uint8_t value = (theChipType == ChipType::LpGBT) ? strtoul(defValueString.c_str(), 0, 16) : strtoul(valueString.c_str(), 0, 16);
    return std::make_pair(name, value);
}

bool isValidLine(const std::string& line) {
    // Check if the line starts with '*' or '#'
    return (line.empty() || line[0] != '*' && line[0] != '#');
}

int main(int argc, char* argv[]) {
    if (argc != 4) {
        std::cerr << "Usage: " << argv[0] << " <file1_path> <file2_path> <chipType>" << std::endl;
        return 1;
    }

    std::cout<< "Comparing file " << argv[1] << " with file " << argv[2] << std::endl;

    std::ifstream file1(argv[1]);
    std::ifstream file2(argv[2]);
    std::string chipTypeName(argv[3]);

    ChipType theChipType;
    try
    {
        theChipType = theChipTypeMap.at(chipTypeName);
    }
    catch(const std::exception& e)
    {
        std::cerr << "Cannot recognize chip type " << chipTypeName << '\n';
        return 0;
    }
    
    if (!file1.is_open() || !file2.is_open()) {
        std::cerr << "Error opening input files." << std::endl;
        return 1;
    }

    initializeFreeRegisters();

    // Map to store last hex values for each register from both files
    std::unordered_map<std::string, uint8_t> registerMap;

    std::string line;
    while (std::getline(file1, line)) {
        if (isValidLine(line)) {
            registerMap.emplace(getRegNameAndValue(line, theChipType));
        }
    }

    while (std::getline(file2, line)) {
        if (isValidLine(line)) {
            auto nameAndValue = getRegNameAndValue(line, theChipType);
            auto name = nameAndValue.first;
            auto value = nameAndValue.second;

            // Check if the register exists in file1
            if (registerMap.find(name) != registerMap.end()) {
                // Compare the last hex values
                if (registerMap[name] != value) {
                    // Print the result
                    std::cout << std::hex << "Modified register: " << name << " 0x" << +registerMap[name]
                              << " -> 0x" << +value <<  std::dec << std::endl;
                }
            } else {
                std::cerr << "Register " << name << " not found in file1." << std::endl;
            }
        }
    }

    file1.close();
    file2.close();


    std::cout<< "End of comparison \n" << std::endl;

    return 0;
}
