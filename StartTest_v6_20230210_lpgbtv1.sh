
#!/bin/bash
while getopts s:h:o:t: flag
do
    case "${flag}" in
        s) slot=${OPTARG};;
        h) hybridID=${OPTARG};;
        o) operator=${OPTARG};;
        t) testcardID=${OPTARG};;
    esac
done
if [ $slot -lt 0 ] || [ $slot -gt 3 ]
then
    printf "Error: Slot = \"%s\" is not a valid choice. \nEnter -s [0,1,2,3]\n" $slot
    exit -1
fi
if [ $hybridID -z ]
then
    printf "Error: Enter a hybrid ID\n"
    exit -1
fi

cd /home/DAQUser/Felix/Ph2_ACF
source setup.sh

TIME=`date +"%Y-%m-%d_%H-%M-%S"`
mkdir Results/SEH_${hybridID}_${TIME}
mux_setup -f settings/mux_setup.xml --mux_configure 0,$slot | tee -a logs/Logfile_${hybridID}_${TIME}.log

SEHTest -f settings/2S_SEH.xml --measure-input-iv --test-external-pattern 202 --fcmd-pattern 202 --test-fcmd --test-clock -c -a -r --test-efficiency --test-ext-leak 1000 --test-ext-bias -v --i2c 10000 -b --hybridId ${hybridID}_noLoad --output Results/SEH_${hybridID}_${TIME}/ | tee -a logs/Logfile_noLoad_${hybridID}_${TIME}.log

mux_setup -f settings/mux_setup.xml --mux_configure 0,$slot | tee -a logs/Logfile_${hybridID}_${TIME}.log

SEHTest -f settings/2S_SEH.xml --test-external-pattern 202 --fcmd-pattern 170 --test-fcmd --test-clock -c -a -r -v --i2c 10000 -b --hybridId ${hybridID}_withLoad --output Results/SEH_${hybridID}_${TIME}/ --leftLoad 2000 --rightLoad 2000 | tee -a logs/Logfile_withLoad_${hybridID}_${TIME}.log

mux_setup -f settings/mux_setup.xml --mux_disconnect 0,$slot | tee -a logs/Logfile_${hybridID}_${TIME}.log

RESULTDIR=`ls Results -t | head -1`


printf "[General]\n" | tee -a Results/${RESULTDIR}/RunInformation.txt
printf "Slot = \"%s\"\n" $slot | tee -a Results/${RESULTDIR}/RunInformation.txt
printf "HybridID = \"%s\"\n" $hybridID | tee -a Results/${RESULTDIR}/RunInformation.txt
printf "Operator = \"%s\"\n" ${operator:="unknown"} | tee -a Results/${RESULTDIR}/RunInformation.txt
printf "TestcardID = \"%s\"\n" ${testcardID:="unknown"} | tee -a Results/${RESULTDIR}/RunInformation.txt
echo "Add a comment: "  
read comment
printf "Comment = \"%s\"\n" "$comment" | tee -a Results/${RESULTDIR}/RunInformation.txt
printf "Temperature = \"20\"\n" | tee -a Results/${RESULTDIR}/RunInformation.txt
for d in Results/$RESULTDIR/*/; do cp Results/$RESULTDIR/RunInformation.txt "$d"; done
cp -r Results/${RESULTDIR} ../Data/

