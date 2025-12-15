from pathlib import Path
import matplotlib.pyplot as plt

# File paths and block sizes
data_files = [
    (Path(__file__).parent / "data1kb.txt", 1),
    (Path(__file__).parent / "data4kb.txt", 4)
]

# Configure plot styling
plt.rcParams.update({
    "font.size": 14,
    "axes.titlesize": 14,
    "axes.labelsize": 18,
    "xtick.labelsize": 17,
    "ytick.labelsize": 17,
    "axes.labelweight": "bold",
    "axes.titleweight": "bold",
    "legend.fontsize": 15
})

# Create figure with two stacked subplots
fig, axes = plt.subplots(2, 1, figsize=(11, 8), sharex=True)

# Add shared Y-axis labels
fig.text(0.02, 0.5, "Throughput (M ops/s)", va="center", rotation="vertical",
         fontsize=18, fontweight="bold")
fig.text(0.96, 0.5, "Measured Memory Space Saving", va="center", rotation=-90,
         fontsize=18, fontweight="bold")

# Collect all data for consistent axis limits
all_data = {"load": [], "lookup": [], "insert": [], "memory": []}

# Parse and plot data for each configuration
for idx, (filepath, block_size) in enumerate(data_files):
    # Read and parse data file
    lines = open(filepath).readlines()
    load_factors, lookup_tp, insert_tp, memory_saving = [], [], [], []

    for i in range(0, len(lines), 5):
        insert_tp.append(float(lines[i].split()[2]) / 1_000_000)
        lookup_tp.append(float(lines[i + 1].split()[2]) / 1_000_000)
        memory_saving.append(float(lines[i + 3].split()[3]))
        load_factors.append(float(lines[i + 4].split()[2]))

    # Sort by load factor
    data = sorted(zip(load_factors, lookup_tp, insert_tp, memory_saving))
    load_factors, lookup_tp, insert_tp, memory_saving = zip(*data)

    # Accumulate for global axis limits
    all_data["load"].extend(load_factors)
    all_data["lookup"].extend(lookup_tp)
    all_data["insert"].extend(insert_tp)
    all_data["memory"].extend(memory_saving)

    # Plot throughput on left axis
    ax1 = axes[idx]
    ax1.set_yscale("log")
    l1 = ax1.plot(load_factors, lookup_tp, '-o', label="Lookup Throughput", linewidth=2)
    l2 = ax1.plot(load_factors, insert_tp, '-s', label="Insert Throughput", linewidth=2)
    ax1.grid(True, which="both", linestyle="--", linewidth=0.5, alpha=0.7)
    ax1.set_title(f"S_bk = {block_size} KB", loc='left')

    # Plot memory saving on right axis
    ax2 = ax1.twinx()
    l3 = ax2.plot(load_factors, memory_saving, '-^', color="green",
                  label="Memory Space Saving", linewidth=2)

    # Store legend lines from first subplot
    if idx == 0:
        legend_lines = l1 + l2 + l3
        legend_labels = [line.get_label() for line in legend_lines]

# Set consistent axis limits with padding
def add_padding(values, factor=0.05):
    vmin, vmax = min(values), max(values)
    pad = (vmax - vmin) * factor
    return vmin - pad, vmax + pad

def add_log_padding(values, factor=1.2):
    """For log scale, use multiplicative padding"""
    vmin, vmax = min(values), max(values)
    return vmin / factor, vmax * factor

x_limits = add_padding(all_data["load"])
y_left_limits = add_log_padding(all_data["lookup"] + all_data["insert"])
y_right_limits = add_padding(all_data["memory"])

# Store twin axes references
twin_axes = []

# Apply limits to both subplots and their twin axes
for ax in axes:
    ax.set_xlim(x_limits)
    ax.set_ylim(y_left_limits)

# Set right axis limits on the actual twin axes created earlier
for ax in fig.get_axes():
    if ax not in axes:  # This is a twin axis
        ax.set_ylim(y_right_limits)

# Add x-axis label and legend
axes[-1].set_xlabel("Load Factor (α)")
axes[0].legend(legend_lines, legend_labels, loc="upper center", ncol=3,
               bbox_to_anchor=(0.5, 1.3), frameon=True)

plt.tight_layout(rect=[0.03, 0, 0.97, 1])
plt.show()

doSave = input("Save (Y/N):")
if (doSave == "Y"):
    fig.savefig("writeup/imgs/tieredMapMemSaving.png")
