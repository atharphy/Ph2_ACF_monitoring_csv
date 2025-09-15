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
def update_hybrid_hist(hyb_hist, hist_name_pattern):
	hyb_dir = hyb_hist.GetDirectory()
	hyb_hist.Reset()
	subdirs = [] # define on which subdirectory to loop to fill the hist
	if hist_name_pattern == "_StripChannelNoise_" or hist_name_pattern == "_StripNoiseDistribution_":
		for key in hyb_dir.GetListOfKeys():
			obj = key.ReadObj()
			if obj.InheritsFrom("TDirectory") and "SSA" in obj.GetName():
				subdirs.append(obj)
	elif hist_name_pattern == "_PixelChannelNoise_" or hist_name_pattern == "_PixelNoiseDistribution_":
		for key in hyb_dir.GetListOfKeys():
			obj = key.ReadObj()
			if obj.InheritsFrom("TDirectory") and "MPA" in obj.GetName():
				subdirs.append(obj)


	# Loop over Chips
	for chip_key in subdirs:
		chip_dir = hyb_dir.Get(chip_key.GetName())  # SSA_X or MPA_X
		print(" chip_dir ",chip_dir)
		if not isinstance(chip_dir, ROOT.TDirectory):
			continue

		# Inside Channel directory
		channel_dir = chip_dir.Get("Channel")
		if not channel_dir:
			continue

		# Loop over histograms in Channel
		hist_names = [k.GetName() for k in channel_dir.GetListOfKeys() if isinstance(channel_dir.Get(k.GetName()), ROOT.TH1)]
		for name in hist_names:
			row, col, noise, noise_error, pulseheight, pulseheight_error = 	getScurvesFitParameters(channel_dir, name)

			# hist = channel_dir.Get(name)
			# if "SCurve" not in name:
			# 	continue

			# print("Processing", name)
			# m = re.search(r"Row\((\d+)\)_Col\((\d+)\)", name)
			# if m:
			# 	row = int(m.group(1))
			# 	col = int(m.group(2))

			# fit_name = f"SCurveFit"
			# fit = hist.GetFunction(fit_name)
			# if fit:
			# 	noise = fit.GetParameter(1)
			# 	noise_error = fit.GetParError(1)
			# 	print(" noise ", noise)

			if "Channel" in hist_name_pattern:
				if "SSA" in chip_dir.GetName():
					theBin = linearizeRowAndColumns(row, col) + COLUMNS * int(chip_dir.GetName().split("_")[-1])
				if "MPA" in chip_dir.GetName(): 
					theBin = linearizeRowAndColumns(row, col) + COLUMNS * 16 * (int(chip_dir.GetName().split("_")[-1]) - 8)
				print(" theBin ", theBin)
				hyb_hist.SetBinContent(theBin, noise)
				hyb_hist.SetBinError(theBin, noise_error)
			else:
				hyb_hist.Fill(noise)
    
	hyb_hist.Write()
 
def getScurvesFitParameters(channel_dir, name):	
	hist = channel_dir.Get(name)

	row = -1
	col = -1
	noise = 0
	noise_error = 0
	pulseheight = 0
	pulseheight_error = 0

	print("Processing", name)
	m = re.search(r"Row\((\d+)\)_Col\((\d+)\)", name)
	if m:
		row = int(m.group(1))
		col = int(m.group(2))
	fit_name = f"SCurveFit"
	fit = hist.GetFunction(fit_name)
	if fit:
		noise = fit.GetParameter(1)
		noise_error = fit.GetParError(1)
		print(" noise ", noise)
		pulseheight = fit.GetParameter(0)
		pulseheight_error = fit.GetParError(0)
	
	return row, col, noise, noise_error, pulseheight, pulseheight_error
 
 
def update_chip_hist(chip_hist, hist_name_pattern):
	chip_dir = chip_hist.GetDirectory()
	chip_hist.Reset()

	# Inside Channel directory
	channel_dir = chip_dir.Get("Channel")

	# Loop over histograms in Channel
	hist_names = [k.GetName() for k in channel_dir.GetListOfKeys() if isinstance(channel_dir.Get(k.GetName()), ROOT.TH1)]
	for name in hist_names:
		row, col, noise, noise_error, pulseheight, pulseheight_error = 	getScurvesFitParameters(channel_dir, name)

		if "Channel" in hist_name_pattern:
			# if "SSA" in chip_dir.GetName():
			# 	theBin = linearizeRowAndColumns(row, col) + COLUMNS * int(chip_dir.GetName().split("_")[-1])
			# if "MPA" in chip_dir.GetName(): 
			# 	theBin = linearizeRowAndColumns(row, col) + COLUMNS * 16 * (int(chip_dir.GetName().split("_")[-1]) - 8)
			if "Noise" in hist_name_pattern:
				if "2D" in hist_name_pattern:
					chip_hist.SetBinContent(col +1, row + 1, noise)
					chip_hist.SetBinError(col +1, row + 1, noise_error)
				else:
					theBin = linearizeRowAndColumns(row, col)
					print(" theBin ", theBin)
					chip_hist.SetBinContent(theBin, noise)
					chip_hist.SetBinError(theBin, noise_error)
			else:
				theBin = linearizeRowAndColumns(row, col)
				chip_hist.SetBinContent(theBin, pulseheight)
				chip_hist.SetBinError(theBin, pulseheight_error)
		else:
			chip_hist.Fill(noise)
    
	chip_hist.Write()
 
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

def refit_scurves_obj(hist):
	name = hist.GetName()
		
	TheChannelDir = hist.GetDirectory()
	TheChipDir = hist.GetDirectory().GetMotherDir()
	root_path = TheChipDir.GetMotherDir().GetMotherDir().GetMotherDir().GetMotherDir().GetMotherDir()
	print(" root_path name ", root_path.GetName())
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

	# fit initial parameters
	# cChannelPedestal = h_chip_channel_pulseheight_summary.GetBinContent(col +1, row + 1)
	# cChannelNoise = h_chip_channel_noise_summary.GetBinContent(col +1, row + 1)

	if(TheChipDir.GetName().find("SSA")):
		cChannelPedestal = 30.0
		cChannelNoise = 3.0
	elif(TheChipDir.GetName().find("MPA")):
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
		result = hist.Fit(newfit, "SRQ")
		
		# if not result:
		# 	print(" not result ", hist.GetName())
		# 	# print("bad fit hybrid ", hyb_dir, " chip ", chip_dir)
		# 	with open(root_path.GetName().replace(".root","_Irene.txt"), "a") as textfile:
		# 		textfile.write(hist.GetName()+" \n")
		# else:
		# 	if int(result) != 0: # or not result.IsValid():
		# 		print("bad fit ", hist.GetName())
		# 		with open(root_path.GetName().replace(".root","_Irene.txt"), "a") as textfile:
		# 			textfile.write(hist.GetName()+" \n")
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


def copy_dir(src_dir, dst_dir, original_filename, updating, verbose=True, depth=0):	
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
			copy_dir(obj, newdir, original_filename, updating, verbose=verbose, depth=depth+1)
		elif obj.InheritsFrom("TObjString"):  # Special handling for TObjString
			dst_dir.cd()  # Move to the current directory in the output file
			stringToCopy = ROOT.TObjString(obj.GetName())
			stringToCopy.Write(key.GetName())
		else:
			dst_dir.cd()
			if updating == "final_hists" and "LpGBT_EyeOpeningScan_Power_0.333333" in name:
				originalname = obj.GetName()
				newname = originalname.replace("0.333333", "1.000000")
				obj.SetName(newname)
				originaltitle = obj.GetName()
				newtitle = originalname.replace("0.333333", "1.000000")
				obj.SetTitle(newtitle)
				obj.Write(newname, ROOT.TObject.kOverwrite)
			elif updating == "final_hists" and "LpGBT_EyeOpeningScan_Power_1.000000" in name:
				originalname = obj.GetName()
				newname = originalname.replace("1.000000", "0.333333")
				obj.SetName(newname)
				originaltitle = obj.GetName()
				newtitle = originalname.replace("0.333333", "1.000000")
				obj.SetTitle(newtitle)
				obj.Write(newname, ROOT.TObject.kOverwrite)
			elif updating == "final_hists" and "_StripChannelNoise_" in name and os.path.basename(original_filename).startswith("PS"):
				update_hybrid_hist(obj, "_StripChannelNoise_" )
			elif updating == "final_hists" and "_StripNoiseDistribution_" in name and os.path.basename(original_filename).startswith("PS"):
				update_hybrid_hist(obj, "_StripNoiseDistribution_" )
			elif updating == "final_hists" and "_PixelChannelNoise_" in name and os.path.basename(original_filename).startswith("PS"):
				update_hybrid_hist(obj, "_PixelChannelNoise_" )
			elif updating == "final_hists" and "_PixelNoiseDistribution_" in name and os.path.basename(original_filename).startswith("PS"):
				update_hybrid_hist(obj, "_PixelNoiseDistribution_" )
			elif updating == "final_hists" and "_ChannelNoise_" in name and os.path.basename(original_filename).startswith("PS"):
				update_chip_hist(obj, "_ChannelNoise_" )
			elif updating == "final_hists" and "_2DChannelNoise_" in name and os.path.basename(original_filename).startswith("PS"):
				update_chip_hist(obj, "_2DChannelNoise_" )
			elif updating == "final_hists" and "_NoiseDistribution_" in name and os.path.basename(original_filename).startswith("PS"):
				update_chip_hist(obj, "_NoiseDistribution_" )
			elif updating == "final_hists" and "_ChannelPulseHeight_" in name and os.path.basename(original_filename).startswith("PS"):
				update_chip_hist(obj, "_ChannelPulseHeight_" )
			elif updating == "final_hists" and "_PulseHeightDistribution_" in name and os.path.basename(original_filename).startswith("PS"):
				update_chip_hist(obj, "_PulseHeightDistribution_" )
			elif updating == "PSscurves_only" and isinstance(obj, ROOT.TH1) and "SCurve_Row" in obj.GetName() and os.path.basename(original_filename).startswith("PS"):
				refit_scurves_obj(obj)
			else:
				obj.Write() #name, ROOT.TObject.kOverwrite)


def copy_file_version(src_path: str, stem: str, vupdate: str, ext: str, perdirectory: bool  = False, dry_run: bool = False) -> str:
	new_name = f"{stem}{vupdate}{ext}"
	destination_path = os.path.join(os.path.dirname(src_path), new_name)
	if dry_run:
		print(f"[DRY-RUN] Would copy: {src_path} -> {destination_path}")
		return destination_path
	print(f"Copy: {src_path} -> {destination_path}")

	f_in = ROOT.TFile.Open(src_path)
	f_out = ROOT.TFile.Open(destination_path, "RECREATE") #, " ", 9) # set compression level to 9

	updating = "final_hists"
	if "temp" in vupdate:
		updating = "PSscurves_only"
	copy_dir(f_in, f_out, destination_path, updating)
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
		# Make the v6-17 temp copy and update Scurves fits for PS modules
		destination_path = copy_file_version(src_path, stem, "v6-17temp", ext, dry_run=args.dry_run)
	found_any = False
	for src_path, stem, ext in find_version_candidates(data_dir, "v6-17temp"):
		found_any = True
		# Make the v6-17 copy and update LpGBT eye opening plots for all modules 
		# and summary plots using Scurves fits for PS modules
		destination_path = copy_file_version(src_path, stem, "v6-17", ext, dry_run=args.dry_run)


if __name__ == "__main__":
	main()