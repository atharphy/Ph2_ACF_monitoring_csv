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

COLUMNS = 120
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
def linearizeRowAndColumns(row, col):
	return col + row * COLUMNS

def find_and_copy_hist1D(hist_name, location, hist_type):
	hist = None
	for key in location.GetListOfKeys():
		obj = location.Get(key.GetName())
		if isinstance(obj, ROOT.TH1) and hist_name in obj.GetName():
			hist = obj
			break
	if hist is None:
		print("No "+hist_name+" histogram found in", location.GetName())
		# sys.exit()
 
	hist_copy = hist_type 
	hist.Copy(hist_copy)
	hist_copy.SetDirectory(0) 
	hist_copy.Reset()
	return hist_copy, hist


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


def refit_scurves(channel_dir_src, channel_dir_dst, dir_stack):
	# print(" dir_stack ", dir_stack)
	for d in dir_stack:
		print(d.GetName())
	parent_dir = dir_stack[-1] if len(dir_stack) >= 1 else None
	# print(" parent_dir ", parent_dir, " ", parent_dir.GetName())

	hyb_dir = dir_stack[-2] if len(dir_stack) >= 2 else None
	# print(" hyb_dir ", hyb_dir, " ", hyb_dir.GetName())
	# Loop over histograms in Channel
	h_hybrid_strip_channel_noise_summary_copy, dummy = find_and_copy_hist1D("StripChannelNoise", hyb_dir, ROOT.TH1F())
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
		
		if(parent_dir.GetName().find("SSA")):
			cChannelPedestal = 30.0
			cChannelNoise = 3.0
		elif(parent_dir.GetName().find("MPA")):
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
		noise = newfit.GetParameter(1)
		noise_error = newfit.GetParError(1)
		pulseheight = newfit.GetParameter(0)
		pulseheight_error = newfit.GetParError(0)
		# channel_dir.WriteTObject(hist, hist.GetName(), ROOT.TObject.kOverwrite)
		channel_dir_dst.cd()  # temporarily move into that directory
		# newfit.Write()
		# Hybrid summary hists
		if "SSA" in parent_dir.GetName():
			theBin = linearizeRowAndColumns(row, col) + COLUMNS * int(parent_dir.GetName().split("_")[-1])
			h_hybrid_strip_channel_noise_summary_copy.SetBinContent(theBin, noise)
			h_hybrid_strip_channel_noise_summary_copy.SetBinError(theBin, noise_error)
  
		hist.Write() #hist.GetName(), ROOT.TObject.kOverwrite)
	return h_hybrid_strip_channel_noise_summary_copy


def refit_scurves_obj(hist, dir_stack):
	name = hist.GetName()
	# print(" dir_stack ", dir_stack)
	for d in dir_stack:
		print(d.GetName())
	parent_dir = dir_stack[-1] if len(dir_stack) >= 1 else None
	# print(" parent_dir ", parent_dir, " ", parent_dir.GetName())

	hyb_dir = dir_stack[-2] if len(dir_stack) >= 2 else None
	# print(" hyb_dir ", hyb_dir, " ", hyb_dir.GetName())
	# Loop over histograms in Channel
	# h_hybrid_strip_channel_noise_summary_copy, dummy = find_and_copy_hist1D("StripChannelNoise", hyb_dir, ROOT.TH1F())
	
		
	# print("Processing", name)
	m = re.search(r"Row\((\d+)\)_Col\((\d+)\)", name)
	if m:
		row = int(m.group(1))
		col = int(m.group(2))
		# print("Row:", row, "Col:", co
	# Delete any existing fit objects for this channel
	hist.GetListOfFunctions().Clear()
	fit_name = f"SCurveFit"
	# fit = hist.GetFunction(fit_name)
	# hist.GetListOfFunctions().Clear()
	# if fit:
	# 	hist.GetListOfFunctions().Remove(fit)
		# fit.Delete
	# fit initial parameters
	# cChannelPedestal = h_chip_channel_pulseheight_summary.GetBinContent(col +1, row + 1)
	# cChannelNoise = h_chip_channel_noise_summary.GetBinContent(col +1, row + 1)
	
	if(parent_dir.GetName().find("SSA")):
		cChannelPedestal = 30.0
		cChannelNoise = 3.0
	elif(parent_dir.GetName().find("MPA")):
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
		# hist.Fit(newfit, "RQ+")
		# hist.Fit(newfit, "RQ+")
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
	noise = newfit.GetParameter(1)
	noise_error = newfit.GetParError(1)
	pulseheight = newfit.GetParameter(0)
	pulseheight_error = newfit.GetParError(0)
	# channel_dir.WriteTObject(hist, hist.GetName(), ROOT.TObject.kOverwrite)
	# channel_dir_dst.cd()  # temporarily move into that directory
	# newfit.Write()
	# Hybrid summary hists
	# if "SSA" in parent_dir.GetName():
	# 	theBin = linearizeRowAndColumns(row, col) + COLUMNS * int(parent_dir.GetName().split("_")[-1])
	# 	h_hybrid_strip_channel_noise_summary_copy.SetBinContent(theBin, noise)
	# 	h_hybrid_strip_channel_noise_summary_copy.SetBinError(theBin, noise_error)
  
	hist.Write() #hist.GetName(), ROOT.TObject.kOverwrite)


def copy_dir(src_dir, dst_dir, original_filename, verbose=True, depth=0, dir_stack=None):
	
	h_hybrid_strip_channel_noise_summary_copy = None
	
	# print(" copy_dir dir_stack ",dir_stack)
	if dir_stack is None:
		dir_stack = []  # initialize only once
	# Push current directory object onto the stack
	dir_stack.append(src_dir)
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
			# mainDir = dir_stack[-6] if len(dir_stack) >= 6 else None
			# if name == "Channel" and os.path.basename(original_filename).startswith("PS") and mainDir.GetName() != "MonitorDQM":
			# 	print(f"Editing contents of Channel in {dst_dir}")
			# 	h_hybrid_strip_channel_noise_summary_copy = refit_scurves(obj, newdir, dir_stack)
			# else:
			# 	copy_dir(obj, newdir, original_filename, verbose=verbose, depth=depth+1, dir_stack=dir_stack)
			copy_dir(obj, newdir, original_filename, verbose=verbose, depth=depth+1, dir_stack=dir_stack)

		# Special case: TTree → use CloneTree
		# elif obj.InheritsFrom("TTree"):
		# 	dst_dir.cd()
		# 	newtree = obj.CloneTree(-1, "fast")
		# 	newtree.SetName(name)
		# 	newtree.Write(name, ROOT.TObject.kOverwrite)

		# Everything else: generic TObject
		elif obj.InheritsFrom("TObjString"):  # Special handling for TObjString
			dst_dir.cd()  # Move to the current directory in the output file
			stringToCopy = ROOT.TObjString(obj.GetName())
			stringToCopy.Write(key.GetName())
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
			# elif "StripChannelNoise" in name and h_hybrid_strip_channel_noise_summary_copy:
			# 	h_hybrid_strip_channel_noise_summary_copy.Write(h_hybrid_strip_channel_noise_summary_copy.GetName(), ROOT.TObject.kOverwrite)
			elif isinstance(obj, ROOT.TH1) and "SCurve" in obj.GetName() and os.path.basename(original_filename).startswith("PS"):
				refit_scurves_obj(obj, dir_stack)
			else:
				obj.Write() #name, ROOT.TObject.kOverwrite)
	# Pop when leaving the directory
	dir_stack.pop()


def copy_file_version(src_path: str, stem: str, vupdate: str, ext: str, perdirectory: bool  = False, dry_run: bool = False) -> str:
	new_name = f"{stem}{vupdate}{ext}"
	destination_path = os.path.join(os.path.dirname(src_path), new_name)
	if dry_run:
		print(f"[DRY-RUN] Would copy: {src_path} -> {destination_path}")
		return destination_path
	print(f"Copy: {src_path} -> {destination_path}")

	f_in = ROOT.TFile.Open(src_path)
	f_out = ROOT.TFile.Open(destination_path, "RECREATE") #, " ", 9) # set compression level to 9

	copy_dir(f_in, f_out, destination_path)
	# f_out.Write("", ROOT.TObject.kOverwrite)
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
		destination_path = copy_file_version(src_path, stem, "v6-17temp", ext, dry_run=args.dry_run) # update Scurves 


if __name__ == "__main__":
	main()