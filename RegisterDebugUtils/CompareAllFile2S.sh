#!/bin/bash

# Check if the correct number of arguments is provided
if [ "$#" -ne 1 ]; then
    echo "Usage: $0 <RunNumber>"
    exit 1
fi

g++ -o ${PH2ACF_BASE_DIR}/RegisterDebugUtils/RegisterDifference ${PH2ACF_BASE_DIR}/RegisterDebugUtils/RegisterDifference.cc
g++ -o ${PH2ACF_BASE_DIR}/RegisterDebugUtils/BoardRegisterDifference ${PH2ACF_BASE_DIR}/RegisterDebugUtils/BoardRegisterDifference.cc -lpugixml

run_number="$1"

echo "Comparing Board files"
${PH2ACF_BASE_DIR}/RegisterDebugUtils/CompareBoardFilesWithDefault.sh ${PH2ACF_BASE_DIR}/settings/BeBoardFiles/uDTC_registers.xml ${run_number}
echo
echo
echo

echo "Comparing LpGBT files"
${PH2ACF_BASE_DIR}/RegisterDebugUtils/CompareFilesWithDefault.sh ${PH2ACF_BASE_DIR}/settings/lpGBTFiles/lpGBT_v1_2S.txt ${run_number} "BE*_OG*_lpGBT*_lpGBT_v1_2S.txt" LpGBT
echo
echo
echo

echo "Comparing CIC files"
${PH2ACF_BASE_DIR}/RegisterDebugUtils/CompareFilesWithDefault.sh ${PH2ACF_BASE_DIR}/settings/CicFiles/CIC2_2S.txt ${run_number} "BE*_OG*_FE*_CIC2_2S.txt" CIC
echo
echo
echo

echo "Comparing CBC files"
${PH2ACF_BASE_DIR}/RegisterDebugUtils/CompareFilesWithDefault.sh ${PH2ACF_BASE_DIR}/settings/CbcFiles/CBC3_default.txt ${run_number} "BE*_OG*_FE*_Chip*.txt" CBC
echo
echo
echo