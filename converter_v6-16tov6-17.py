#!/usr/bin/env python3
"""
Scan a directory for files ending with 'v6-16' (optionally followed by .root),
create exact copies with the ending changed to 'v6-17', and for files whose
basename starts with 'PS', update the S-curve fits IN-PLACE inside the copied
v6-17 ROOT file. Only the fitting part is applied
and results are written back into the existing SSA/MPA directories without
altering the file's directory structure.

Usage:
  python3 converter_v6-16tov6-17.py   --data-dir ./DataDir [--dry-run]

"""

import argparse
import os
import re
import shutil
import sys
import math

# Import ROOT for optical-group detection and fitting
try:
	import ROOT
except Exception as e:
	print(f"Error: PyROOT not available: {e}")
	print("Please ensure ROOT with Python bindings is installed and configured (PyROOT).")
	sys.exit(1)

ROOT.gErrorIgnoreLevel = ROOT.kError 
ROOT.gROOT.SetBatch(True)

# ------------------------ Utilities ------------------------

def MyErf(x, par):
	x0 = par[0]
	width = par[1]
	if x[0] < x0:
		return 0.5 * math.erfc((x[0] - x0) / (math.sqrt(2.0) * width))
	else:
		return 0.5 + 0.5 * math.erf((x0 - x[0]) / (math.sqrt(2.0) * width))



def find_version_candidates(data_dir: str, version: str):
	pattern = re.compile(rf"^(?P<stem>.+?){re.escape(version)}(?P<ext>\.root)?$")
	for entry in os.scandir(data_dir):
		if not entry.is_file():
			continue
		name = entry.name
		m = pattern.match(name)
		if m:
			yield entry.path, m.group("stem"), (m.group("ext") or "")


def refit_scurves(channel_dir_src, channel_dir_dst, parent_dir):
	# Loop over histograms in Channel
	hist_names = [k.GetName() for k in channel_dir_src.GetListOfKeys() if isinstance(channel_dir_src.Get(k.GetName()), ROOT.TH1)]
	for name in hist_names:
		
		hist = channel_dir_src.Get(name)
		if "SCurve" not in name:
			continue
		
		# print("Processing", name)
		m = re.search(r"Row\((\d+)\)_Col\((\d+)\)", name)
		if m:
			row = int(m.group(1))
			col = int(m.group(2))
			# print("Row:", row, "Col:", co
		# Delete any existing fit objects for this channel
		fit_name = f"SCurveFit"
		fit = hist.GetFunction(fit_name)
		if fit:
			hist.GetListOfFunctions().Remove(fit)
			fit.Delete
		# fit initial parameters
		# cChannelPedestal = h_chip_channel_pulseheight_summary.GetBinContent(col +1, row + 1)
		# cChannelNoise = h_chip_channel_noise_summary.GetBinContent(col +1, row + 1)
		
		if(parent_dir.find("SSA")):
			cChannelPedestal = 30.0
			cChannelNoise = 3.0
		elif(parent_dir.find("MPA")):
			cChannelPedestal = 125.0
			cChannelNoise = 5
		# Edge search
		lastOneIndex = -1
		firstZeroIndex = -1
		oneThreshold = 0.9
		zeroThreshold = 0.1
		maxNoise = 15.0
		noiseTolerance = 2.0
		bins = hist.GetNbinsX()
		for l in range(bins):
			currentbin = l+1
			if hist.GetBinContent(currentbin) > oneThreshold and hist.GetBinContent(currentbin + 1) < hist.GetBinContent(currentbin):
				lastOneIndex = l
				break
		for l in range(bins+1, 0, -1):
			if hist.GetBinContent(l) < zeroThreshold and hist.GetBinContent(l - 1) > hist.GetBinContent(l):
				firstZeroIndex = l
				break
			
		if firstZeroIndex != -1 and lastOneIndex != -1:
			# print("firstZeroIndex != -1 and lastOneIndex != -1, hybrid ", hyb_dir, " chip ", chip_dir, " row ", row, " col ", co
			cChannelPedestal = (lastOneIndex + firstZeroIndex) / 2.0
			cChannelNoise = (firstZeroIndex - lastOneIndex) / 2.0
			if cChannelNoise > maxNoise:
				cChannelNoise = maxNoise
				
			rangeMinus = cChannelPedestal - (cChannelNoise * noiseTolerance)
			rangePlus = cChannelPedestal + (cChannelNoise * noiseTolerance)
		elif lastOneIndex == -1 and firstZeroIndex != -1:
			lastOneIndex = hist.GetMaximumBin() # bin with highest content
			cChannelPedestal = (lastOneIndex + firstZeroIndex) / 2.0
			cChannelNoise = (firstZeroIndex - lastOneIndex) / 2.0
			if cChannelNoise > maxNoise:
				cChannelNoise = maxNoise
				
			rangeMinus = cChannelPedestal - (cChannelNoise * noiseTolerance)
			rangePlus = cChannelPedestal + (cChannelNoise * noiseTolerance)
			# print("firstZeroIndex != -1 and lastOneIndex == -1, hybrid ", hyb_dir, " chip ", chip_dir, " row ", row, " col ", col)
			# print(" range ", rangeMinus, " ", rangePlus, " zero ",firstZeroIndex, " one ",lastOneIndex, " pedestal ", cChannelPedestal, " noise ", cChannelNoise) 
		elif firstZeroIndex == -1 and lastOneIndex != -1:
			# print("firstZeroIndex == -1 and lastOneIndex != -1, hybrid ", hyb_dir, " chip ", chip_dir, " row ", row, " col ", col
			firstZeroIndex = hist.GetMinimumBin() # bin with lowest contentfirstZeroIndex == -1:
			cChannelPedestal = (lastOneIndex + firstZeroIndex) / 2.0
			cChannelNoise = (firstZeroIndex - lastOneIndex) / 2.0
			if cChannelNoise > maxNoise:
				cChannelNoise = maxNoise
			rangeMinus = cChannelPedestal - (cChannelNoise * noiseTolerance)
			rangePlus = cChannelPedestal + (cChannelNoise * noiseTolerance) 
			# print(" range ", rangeMinus, " ", rangePlus, " zero ",firstZeroIndex, " one ",lastOneIndex) 
		elif firstZeroIndex == -1 and lastOneIndex == -1:
			# print("firstZeroIndex == -1 and lastOneIndex == -1, hybrid ", hyb_dir, " chip ", chip_dir, " row ", row, " col ", co
			firstZeroIndex = hist.GetMinimumBin()
			lastOneIndex = hist.GetMaximumBin()
			cChannelPedestal = (lastOneIndex + firstZeroIndex) / 2.0
			cChannelNoise = (firstZeroIndex - lastOneIndex) / 2.0
			if cChannelNoise > maxNoise:
				cChannelNoise = maxNoise
			rangeMinus = cChannelPedestal - (cChannelNoise * noiseTolerance)
			rangePlus = cChannelPedestal + (cChannelNoise * noiseTolerance)
			# print(" range ", rangeMinus, " ", rangePlus, " zero ",firstZeroIndex, " one ",lastOneIndex) 
		# print(" hybrid ", hyb_dir, " chip ", chip_dir, " row ", row, " col ", col)
		# print(" range ", rangeMinus, " ", rangePlus, " zero ",firstZeroIndex, " one ",lastOneIndex)   
		# fit.SetRange(rangeMinus, rangePlus)
		newfit = ROOT.TF1(fit_name, MyErf, rangeMinus, rangePlus, 2)
		newfit.SetNpx(100)
		newfit.SetParameter(0, cChannelPedestal)
		newfit.SetParameter(1, cChannelNoise)
		newfit.SetParLimits(1, 1, maxNoise*noiseTolerance)
		if hist.GetMean() != 0:
			hist.Fit(newfit, "RQ+")
			hist.Fit(newfit, "RQ+")
			result = hist.Fit(newfit, "SRQ+")
			
			# if not result:
			# 	print(" not result ", hist.GetName())
			# 	# print("bad fit hybrid ", hyb_dir, " chip ", chip_dir)
			# 	with open(root_path.replace(".root","_Irene.txt"), "a") as textfile:
			# 		textfile.write(hist.GetName()+" \n")
			# else:
			# if int(result) != 0: # or not result.IsValid():
				# print("bad fit ", hist.GetName())
				# print("bad fit hybrid ", hyb_dir, " chip ", chip_dir)
				# with open(root_path.replace(".root","_Irene.txt"), "a") as textfile:
				# 	textfile.write(hist.GetName()+" \n")
		newfit.SetRange(rangeMinus, rangePlus)
		# channel_dir.WriteTObject(hist, hist.GetName(), ROOT.TObject.kOverwrite)
		channel_dir_dst.cd()  # temporarily move into that directory
		newfit.Write()
		# hist.Write(hist.GetName(), ROOT.TObject.kOverwrite)

def copy_dir(src_dir, dst_dir, original_filename, verbose=True, depth=0, path_so_far=None):
    if path_so_far is None:
        path_so_far = []  # initialize only once
    """Recursively copy all objects from src_dir to dst_dir."""
    indent = "  " * depth
    keys = list(src_dir.GetListOfKeys())  # take a snapshot
    for key in keys:
        name = key.GetName()
        obj = key.ReadObj()
        if not obj:
            if verbose: print(f"{indent}(skip) could not read {name}")
            continue

        cls = obj.ClassName()
        if verbose: print(f"{indent}copy: {name} [{cls}]")

        # Recurse into subdirectories
        if obj.InheritsFrom("TDirectory"):
            dst_dir.cd()
            newdir = dst_dir.mkdir(name, obj.GetTitle() if hasattr(obj,"GetTitle") else "")
            
            
            # If this is the "Channel" directory *and* source file starts with "PS" perform Scurves fit again
			# I also need to know if it is SSA or MPA
			# Build new path context
            new_path_so_far = path_so_far + [name]
            if name == "Channel" and os.path.basename(original_filename).startswith("PS"):
                print(f"Editing contents of Channel in {dst_dir}")
                print(new_path_so_far)
                parent_dir = new_path_so_far[-2] if len(new_path_so_far) >= 2 else None
                refit_scurves(obj, newdir, parent_dir)
            else:
            	copy_dir(obj, newdir, original_filename, path_so_far=new_path_so_far, verbose=verbose, depth=depth+1)

        # Special case: TTree → use CloneTree
        elif obj.InheritsFrom("TTree"):
            dst_dir.cd()
            newtree = obj.CloneTree(-1, "fast")
            newtree.SetName(name)
            newtree.Write(name, ROOT.TObject.kOverwrite)

        # Everything else: generic TObject
        else:
            dst_dir.cd()
            # detach histograms/graphs/etc. from the input directory
            if hasattr(obj, "SetDirectory"):
                try:
                    obj.SetDirectory(0)
                except Exception:
                    pass
            if "LpGBT_EyeOpeningScan_Power_0.333333" in name:
                originalname = obj.GetName()
                newname = originalname.replace("0.333333", "1.000000")
                obj.SetName(newname)
                originaltitle = obj.GetName()
                newtitle = originalname.replace("0.333333", "1.000000")
                obj.SetTitle(newtitle)
                obj.Write(newname, ROOT.TObject.kOverwrite)
            elif "LpGBT_EyeOpeningScan_Power_1.000000" in name:
                originalname = obj.GetName()
                newname = originalname.replace("1.000000", "0.333333")
                obj.SetName(newname)
                originaltitle = obj.GetName()
                newtitle = originalname.replace("0.333333", "1.000000")
                obj.SetTitle(newtitle)
                obj.Write(newname, ROOT.TObject.kOverwrite)
            else:
	            obj.Write(name, ROOT.TObject.kOverwrite)


def copy_file_version(src_path: str, stem: str, vupdate: str, ext: str, perdirectory: bool  = False, dry_run: bool = False) -> str:
	new_name = f"{stem}{vupdate}{ext}"
	destination_path = os.path.join(os.path.dirname(src_path), new_name)
	if dry_run:
		print(f"[DRY-RUN] Would copy: {src_path} -> {destination_path}")
		return destination_path
	print(f"Copy: {src_path} -> {destination_path}")

	f_in = ROOT.TFile.Open(src_path)
	f_out = ROOT.TFile.Open(destination_path, "RECREATE", " ", 9) # set compression level to 9

	copy_dir(f_in, f_out, destination_path)
	f_out.Write("", ROOT.TObject.kOverwrite)
	f_out.Close()
	f_in.Close()

	return destination_path

# ------------------------ Main ------------------------

def main() -> None:
	parser = argparse.ArgumentParser(description="Copy v6-16 files to v6-17 and update S-curve fits in-place for PS files (SSA + MPA), without altering file structure.")
	parser.add_argument("--data-dir", required=True, help="Directory to scan for files")
	parser.add_argument("--dry-run", action="store_true", help="Only print actions without performing them")
	args = parser.parse_args()

	data_dir = os.path.abspath(args.data_dir)
	if not os.path.isdir(data_dir):
		print(f"Error: data directory not found: {data_dir}")
		sys.exit(1)

	found_any = False
	for src_path, stem, ext in find_version_candidates(data_dir, "v6-16"):
		found_any = True
		# Make the v6-17 temp copy
		destination_path = copy_file_version(src_path, stem, "v6-17", ext, dry_run=args.dry_run)


if __name__ == "__main__":
	main()