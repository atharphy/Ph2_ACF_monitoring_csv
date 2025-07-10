import pandas as pd
import glob
import os

# Root folder that contains subdirectories with CSVs
root_folder = "settings/MPAFiles/calibrations/"
output_file = "settings/MPAFiles/combined_MPA2_calibration.csv"

# Find all CSV files in all subdirectories
csv_files = glob.glob(os.path.join(root_folder, "**", "NB*.csv"), recursive=True)

print("CSV FILES:\n",csv_files)
# Load and concatenate
df_list = [pd.read_csv(file) for file in csv_files]
print("DF list:\n",df_list)
merged_df = pd.concat(df_list, ignore_index=True)

# Save merged result
merged_df.to_csv(output_file, index=False)

print(f"Merged {len(csv_files)} CSV files from subdirectories into '{output_file}'")
