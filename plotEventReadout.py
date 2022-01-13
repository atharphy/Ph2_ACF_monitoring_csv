import numpy as np
import matplotlib.pyplot as plt


#ROOT
from ROOT import ROOT, gROOT, TTree, TFile

data = []
dataindex = 0

myFile = TFile("Results/FEH_PS_xxxx_Electron_04-02-21_21h22m58/Hybrid.root", "READ")
st = myFile.summaryTree
nEntries = st.GetEntries()

for i in range(nEntries):
	st.GetEntry(i)
	parameter = str(st.Parameter)
	if parameter == "Empty event":
		if st.Value > len(data)+1:
			for j in range(int(st.Value) - len(data)+1):
				data.append(0)
		else:
			data.append(1)

print(data)		
y_axis = []
for i in range(len(data)):
    	y_axis.append(i)

y_pos = np.arange(len(y_axis))
 
# Create bars
plt.bar(y_pos, data)
 
# Create names on the x-axis
plt.xticks(y_pos, y_axis)
 
# Show graphic
plt.show()