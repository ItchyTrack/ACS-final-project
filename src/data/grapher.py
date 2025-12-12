from pathlib import Path
import pandas as pd
import matplotlib.pyplot as plt

paths = [
    (str((Path(__file__).parent / "data1kb.txt").resolve()), 1),
    (str((Path(__file__).parent / "data4kb.txt").resolve()), 4)
]

# Global collections for computing consistent axis limits
all_load = []
all_lookup = []
all_insert = []
all_comp = []

# Store right-axis objects so we don't create duplicates
right_axes = []

# Create a single figure with two stacked subplots
fig, axes = plt.subplots(2, 1, figsize=(11, 8), sharex=True)

for idx, path in enumerate(paths):
    data = open(path[0]).readlines()

    insertThroughputs = []
    lookupThroughputs = []
    compressionRatios = []
    loadFactors = []

    for i in range(0, len(data), 4):
        insertThroughputs.append(float(data[i].split(" ")[2]) / 1_000_000)
        lookupThroughputs.append(float(data[i + 1].split(" ")[2]) / 1_000_000)
        compressionRatios.append(float(data[i + 2].split(" ")[2]))
        loadFactors.append(float(data[i + 3].split(" ")[2]))

    # Sort everything by load factor
    combined = list(zip(loadFactors, lookupThroughputs, insertThroughputs, compressionRatios))
    combined.sort(key=lambda x: x[0])
    loadFactors, lookupThroughputs, insertThroughputs, compressionRatios = zip(*combined)

    # Collect global min/max
    all_load.extend(loadFactors)
    all_lookup.extend(lookupThroughputs)
    all_insert.extend(insertThroughputs)
    all_comp.extend(compressionRatios)

    ax1 = axes[idx]

    # Left Y-axis (throughput)
    ax1.set_yscale("log")
    l1 = ax1.plot(loadFactors, lookupThroughputs, '-o', label="Lookup Throughput")
    l2 = ax1.plot(loadFactors, insertThroughputs, '-s', label="Insert Throughput")
    ax1.set_ylabel("Throughput (M ops/s)")
    ax1.grid(True, which="both", linestyle="--", linewidth=0.5)
    ax1.set_title(f"S_bk = {path[1]} KB", loc='left')

    # Right Y-axis (compression ratio)
    ax2 = ax1.twinx()
    right_axes.append(ax2)  # store this axis
    l3 = ax2.plot(loadFactors, compressionRatios, '-^', color="green",
                  label="Memory Space Saving")
    ax2.set_ylabel("Memory Space Saving")

# ---- Apply consistent scales across subplots with padding ----
xmin, xmax = min(all_load), max(all_load)
xpad = (xmax - xmin) * 0.05  # 5% padding
xmin, xmax = xmin - xpad, xmax + xpad

ymin_tp, ymax_tp = min(all_lookup + all_insert), max(all_lookup + all_insert)
ypad_tp = (ymax_tp - ymin_tp) * 0.05  # 5% padding
ymin_tp, ymax_tp = ymin_tp - ypad_tp, ymax_tp + ypad_tp

ymin_comp, ymax_comp = min(all_comp), max(all_comp)
ypad_comp = (ymax_comp - ymin_comp) * 0.05  # 5% padding
ymin_comp, ymax_comp = ymin_comp - ypad_comp, ymax_comp + ypad_comp

for ax in axes:
    ax.set_xlim(xmin, xmax)
    ax.set_ylim(ymin_tp, ymax_tp)

axes[-1].set_xlabel("Load Factor (α)")

for ax2 in right_axes:
    ax2.set_ylim(ymin_comp, ymax_comp)

lines = l1 + l2 + l3
labels = [line.get_label() for line in lines]
axes[0].legend(lines, labels, loc="upper center", ncol=3, bbox_to_anchor=(0.5, 1.25))

# plt.xlabel("Load Factor (α)")
plt.tight_layout()
plt.show()
