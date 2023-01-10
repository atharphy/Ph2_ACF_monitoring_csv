# IP-environment variables are set by user/eudet/misc/environments/setup_eudaq2_aida-tlu.sh 
# Define port
RPCPORT=44000
RUNCONTROLIP="192.168.21.1"

printf '\033[22;33m\t Cleaning up first...  \033[0m \n'

sh KILLRUN

printf '\033[22;31m\t End of killall \033[0m \n'

# Start Run Control
xterm -T "Run Control" -e 'euRun' &
sleep 2 

# Start Logger
xterm -T "Log Collector" -e 'euLog -r tcp://${RUNCONTROLIP}' &
sleep 2


#xterm -T "Data Collector TLU" -e 'euCliCollector -n TriggerIDSyncDataCollector -t the_dc -r tcp://${RUNCONTROLIP}:${RPCPORT}' &
xterm -T "Data Collector" -e 'euCliCollector -n EventIDSyncDataCollector -t the_dc -r tcp://${RUNCONTROLIP}:${RPCPORT}' &

sleep 2

xterm -T "Online Monitor" -e 'StdEventMonitor -t StdEventMonitor -r tcp://${RUNCONTROLIP}:${RPCPORT}' & 

printf '\033[1;32;48m \t STARTING DAQ \033[0m \n'
echo $(date)

