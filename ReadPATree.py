from ROOT import ROOT, gROOT, TCanvas, TF1, TTree, TFile
import os

def all_subdirs_of(b='.'):
    result = []
    for d in os.listdir(b):
        bd = os.path.join(b, d)
        if os.path.isdir(bd): result.append(bd)
    return result


latest_subdir = max(all_subdirs_of("Results/"), key=os.path.getmtime)

print(latest_subdir)


#Reading ROOT file
myFile = TFile(latest_subdir + "/Hybrid.root", "READ")

tree = myFile.PATree

nEntries = tree.GetEntries()

data = {}

#Reading the summary tree from the ROOT file
for i in range(nEntries):
    tree.GetEntry(i)
    parameter = str(tree.Parameter)
    value = str(tree.Value)
    # print(parameter)
    # print(value)
    if f'{parameter[-3]}_{parameter[-1]}' in data:
        data[f'{parameter[-3]}_{parameter[-1]}' ].append(value)
    else:
        data[f'{parameter[-3]}_{parameter[-1]}' ] = [value];


for key in data:
    print(f"PhyPort {key}")
    for i in range( len(data[key]) ):
        print(f"Phase {i}:  {data[key][i]}")
    print()