import os
from collections import defaultdict

def check_lines_in_other(fileA, fileB):
	print("file ", fileA)
	# Read lines and strip whitespace/newlines
	with open(fileA) as f:
		linesA = {line.strip() for line in f if line.strip()}
	with open(fileB) as f:
		linesB = {line.strip() for line in f if line.strip()}
	newtxt = fileA.replace("_updated.txt","_compare.txt")
	print("newfile ",newtxt)
	with open(newtxt, "w") as textfile:
		# Find missing lines
		missing = []
		for a in linesA:
			if not any(a in b for b in linesB):
				missing.append(a)
				# print(a)
				textfile.write(a+" \n")
		textfile.write("Failed fits in old: "+str(len(linesB))+" \n")
		textfile.write("Failed fits in new: "+str(len(linesA) - len(missing))+" \n")
		textfile.write("New unresolved fits: "+str(len(missing))+" \n")
	if missing:
		print("❌ These lines from fileA were not found inside fileB lines:")
		# for m in missing:
		# 	print("   ", m)
	else:
		print("✅ All lines from fileA were found somewhere in fileB.")


def find_and_check_pairs(directory):
	print(" directory ", directory)
	groups = defaultdict(list)
	# group files by stem (everything except last two parts)
	for f in os.listdir(directory):
		if not f.endswith(".txt"):
			continue
		stem = "_".join(f.split("_")[:-1])
		print("stem ", stem)
		groups[stem].append(f)

	for stem, files in groups.items():
		fileA = [f for f in files if f.endswith("_updated.txt")]
		fileB = [f for f in files if f.endswith("_old.txt")]


		if len(fileA) == 1 and len(fileB) == 1:
			pathA = os.path.join(directory, fileA[0])
			pathB = os.path.join(directory, fileB[0])
			print(f"Checking {fileA[0]} vs {fileB[0]}")
			result = check_lines_in_other(pathA, pathB)
			print("✔ All lines match" if result else "❌ Some lines not found")

# Example usage
find_and_check_pairs("./TestOutputs")

