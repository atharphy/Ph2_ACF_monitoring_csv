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
${PH2ACF_BASE_DIR}/RegisterDebugUtils/CompareFilesWithDefault.sh ${PH2ACF_BASE_DIR}/settings/lpGBTFiles/lpGBT_v1_PS.txt ${run_number} "BE*_OG*_lpGBT*_lpGBT_v1_PS.txt" LpGBT
echo
echo
echo

echo "Comparing CIC files"
${PH2ACF_BASE_DIR}/RegisterDebugUtils/CompareFilesWithDefault.sh ${PH2ACF_BASE_DIR}/settings/CicFiles/CIC2_PS.txt ${run_number} "BE*_OG*_FE*_CIC2_PS.txt" CIC
echo
echo
echo

echo "Comparing SSA files"
${PH2ACF_BASE_DIR}/RegisterDebugUtils/CompareFilesWithDefault.sh ${PH2ACF_BASE_DIR}/settings/SSAFiles/SSA2.txt ${run_number} "BE*_OG*_FE*_Chip*SSA.txt" SSA
echo
echo
echo


echo "Comparing MPA files"
${PH2ACF_BASE_DIR}/RegisterDebugUtils/CompareFilesWithDefault.sh ${PH2ACF_BASE_DIR}/settings/MPAFiles/MPA2.txt ${run_number} "BE*_OG*_FE*_Chip*MPA.txt" MPA
echo
echo
echo