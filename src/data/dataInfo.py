from pathlib import Path
import matplotlib.pyplot as plt
import numpy as np

def parse_data_file(filename):
    data = {
        'load_factor': [],
        'insert_throughput': [],
        'lookup_throughput': [],
        'compression_ratio': [],
        'memory_space_saving': []
    }

    with open(filename, 'r') as f:
        lines = f.readlines()

    for i in range(0, len(lines), 5):
        data['insert_throughput'].append(float(lines[i].split()[2]) / 1_000_000)
        data['lookup_throughput'].append(float(lines[i + 1].split()[2]) / 1_000_000)
        data['compression_ratio'].append(float(lines[i + 2].split()[2]))
        data['memory_space_saving'].append(float(lines[i + 3].split()[3]))
        data['load_factor'].append(float(lines[i + 4].split()[2]))

    return data

def get_value_at_load_factor(data, target_lf):
    """Interpolate or find closest value at a specific load factor."""
    idx = min(range(len(data['load_factor'])),
              key=lambda i: abs(data['load_factor'][i] - target_lf))
    return idx

def calculate_degradation_rate(data, metric_key):
    """Calculate how quickly performance degrades with load factor."""
    lf = np.array(data['load_factor'])
    metric = np.array(data[metric_key])

    # Linear regression to find degradation rate
    if len(lf) > 1:
        coeffs = np.polyfit(lf, metric, 1)
        return coeffs[0]  # slope
    return 0

def print_detailed_analysis(data_1kb, data_4kb):
    print(f"\n{'='*80}")
    print("DETAILED COMPARATIVE ANALYSIS FOR PAPER")
    print(f"{'='*80}")

    # Key load factor points for analysis
    load_factors = [0.2, 0.3, 0.4, 0.5]

    print("\n1. THROUGHPUT AT KEY LOAD FACTORS")
    print("-" * 80)
    print(f"{'Load Factor':<15} {'1KB Insert':<20} {'4KB Insert':<20} {'Ratio':<15}")
    print(f"{'':15} {'(M ops/s)':<20} {'(M ops/s)':<20} {'(4KB/1KB)':<15}")
    print("-" * 80)

    for lf in load_factors:
        idx_1kb = get_value_at_load_factor(data_1kb, lf)
        idx_4kb = get_value_at_load_factor(data_4kb, lf)

        insert_1kb = data_1kb['insert_throughput'][idx_1kb]
        insert_4kb = data_4kb['insert_throughput'][idx_4kb]
        ratio = insert_4kb / insert_1kb if insert_1kb > 0 else 0

        print(f"{lf:<15.2f} {insert_1kb:<20.3f} {insert_4kb:<20.3f} {ratio:<15.3f}")

    print(f"\n{'Load Factor':<15} {'1KB Lookup':<20} {'4KB Lookup':<20} {'Ratio':<15}")
    print(f"{'':15} {'(M ops/s)':<20} {'(M ops/s)':<20} {'(4KB/1KB)':<15}")
    print("-" * 80)

    for lf in load_factors:
        idx_1kb = get_value_at_load_factor(data_1kb, lf)
        idx_4kb = get_value_at_load_factor(data_4kb, lf)

        lookup_1kb = data_1kb['lookup_throughput'][idx_1kb]
        lookup_4kb = data_4kb['lookup_throughput'][idx_4kb]
        ratio = lookup_4kb / lookup_1kb if lookup_1kb > 0 else 0

        print(f"{lf:<15.2f} {lookup_1kb:<20.3f} {lookup_4kb:<20.3f} {ratio:<15.3f}")

    # Performance degradation rates
    print("\n2. PERFORMANCE DEGRADATION RATES")
    print("-" * 80)

    insert_deg_1kb = calculate_degradation_rate(data_1kb, 'insert_throughput')
    insert_deg_4kb = calculate_degradation_rate(data_4kb, 'insert_throughput')
    lookup_deg_1kb = calculate_degradation_rate(data_1kb, 'lookup_throughput')
    lookup_deg_4kb = calculate_degradation_rate(data_4kb, 'lookup_throughput')

    print(f"Insert Throughput Degradation (M ops/s per load factor increase):")
    print(f"  1KB blocks: {insert_deg_1kb:.3f}")
    print(f"  4KB blocks: {insert_deg_4kb:.3f}")
    print(f"  Relative stability (4KB/1KB): {abs(insert_deg_4kb/insert_deg_1kb):.3f}x")

    print(f"\nLookup Throughput Degradation (M ops/s per load factor increase):")
    print(f"  1KB blocks: {lookup_deg_1kb:.3f}")
    print(f"  4KB blocks: {lookup_deg_4kb:.3f}")
    print(f"  Relative stability (4KB/1KB): {abs(lookup_deg_4kb/lookup_deg_1kb):.3f}x")

    # Compression efficiency analysis
    print("\n3. COMPRESSION EFFICIENCY ANALYSIS")
    print("-" * 80)

    for lf in load_factors:
        idx_1kb = get_value_at_load_factor(data_1kb, lf)
        idx_4kb = get_value_at_load_factor(data_4kb, lf)

        cr_1kb = data_1kb['compression_ratio'][idx_1kb]
        cr_4kb = data_4kb['compression_ratio'][idx_4kb]

        saving_1kb = data_1kb['memory_space_saving'][idx_1kb]
        saving_4kb = data_4kb['memory_space_saving'][idx_4kb]

        print(f"\nAt load factor {lf:.2f}:")
        print(f"  Compression Ratio - 1KB: {cr_1kb:.3f}, 4KB: {cr_4kb:.3f}")
        print(f"  Memory Saving - 1KB: {saving_1kb:.2%}, 4KB: {saving_4kb:.2%}")
        print(f"  4KB compresses {cr_4kb/cr_1kb:.3f}x better than 1KB")

    # Peak performance analysis
    print("\n4. PEAK PERFORMANCE METRICS")
    print("-" * 80)

    max_insert_1kb = max(data_1kb['insert_throughput'])
    max_insert_4kb = max(data_4kb['insert_throughput'])
    max_lookup_1kb = max(data_1kb['lookup_throughput'])
    max_lookup_4kb = max(data_4kb['lookup_throughput'])

    idx_max_insert_1kb = data_1kb['insert_throughput'].index(max_insert_1kb)
    idx_max_insert_4kb = data_4kb['insert_throughput'].index(max_insert_4kb)

    print(f"Peak Insert Throughput:")
    print(f"  1KB: {max_insert_1kb:.3f} M ops/s at load factor {data_1kb['load_factor'][idx_max_insert_1kb]:.3f}")
    print(f"  4KB: {max_insert_4kb:.3f} M ops/s at load factor {data_4kb['load_factor'][idx_max_insert_4kb]:.3f}")
    print(f"  4KB achieves {max_insert_4kb/max_insert_1kb:.3f}x the peak of 1KB")

    idx_max_lookup_1kb = data_1kb['lookup_throughput'].index(max_lookup_1kb)
    idx_max_lookup_4kb = data_4kb['lookup_throughput'].index(max_lookup_4kb)

    print(f"\nPeak Lookup Throughput:")
    print(f"  1KB: {max_lookup_1kb:.3f} M ops/s at load factor {data_1kb['load_factor'][idx_max_lookup_1kb]:.3f}")
    print(f"  4KB: {max_lookup_4kb:.3f} M ops/s at load factor {data_4kb['load_factor'][idx_max_lookup_4kb]:.3f}")
    print(f"  4KB achieves {max_lookup_4kb/max_lookup_1kb:.3f}x the peak of 1KB")

    # Overall averages
    print("\n5. OVERALL PERFORMANCE SUMMARY")
    print("-" * 80)

    avg_insert_1kb = np.mean(data_1kb['insert_throughput'])
    avg_insert_4kb = np.mean(data_4kb['insert_throughput'])
    avg_lookup_1kb = np.mean(data_1kb['lookup_throughput'])
    avg_lookup_4kb = np.mean(data_4kb['lookup_throughput'])
    avg_compression_1kb = np.mean(data_1kb['compression_ratio'])
    avg_compression_4kb = np.mean(data_4kb['compression_ratio'])

    print(f"Average across all load factors:")
    print(f"  Insert - 1KB: {avg_insert_1kb:.3f} M ops/s, 4KB: {avg_insert_4kb:.3f} M ops/s")
    print(f"  Lookup - 1KB: {avg_lookup_1kb:.3f} M ops/s, 4KB: {avg_lookup_4kb:.3f} M ops/s")
    print(f"  4KB averages {avg_insert_4kb/avg_insert_1kb:.3f}x insert throughput of 1KB")
    print(f"  4KB averages {avg_lookup_4kb/avg_lookup_1kb:.3f}x lookup throughput of 1KB")
    print(f"\n  Compression - 1KB: {avg_compression_1kb:.3f}, 4KB: {avg_compression_4kb:.3f}")
    print(f"  4KB compresses {avg_compression_4kb/avg_compression_1kb:.3f}x better on average")

    # Memory efficiency
    print("\n6. MEMORY EFFICIENCY METRICS")
    print("-" * 80)

    best_saving_1kb = max(data_1kb['memory_space_saving'])
    best_saving_4kb = max(data_4kb['memory_space_saving'])
    idx_best_1kb = data_1kb['memory_space_saving'].index(best_saving_1kb)
    idx_best_4kb = data_4kb['memory_space_saving'].index(best_saving_4kb)

    print(f"Best memory savings:")
    print(f"  1KB: {best_saving_1kb:.2%} at load factor {data_1kb['load_factor'][idx_best_1kb]:.3f}")
    print(f"  4KB: {best_saving_4kb:.2%} at load factor {data_4kb['load_factor'][idx_best_4kb]:.3f}")

    avg_saving_1kb = np.mean(data_1kb['memory_space_saving'])
    avg_saving_4kb = np.mean(data_4kb['memory_space_saving'])

    print(f"\nAverage memory savings:")
    print(f"  1KB: {avg_saving_1kb:.2%}")
    print(f"  4KB: {avg_saving_4kb:.2%}")


# Main execution
data_1kb = parse_data_file(Path(__file__).parent / 'data1kb.txt')
data_4kb = parse_data_file(Path(__file__).parent / 'data4kb.txt')

# Print detailed analysis for paper
print_detailed_analysis(data_1kb, data_4kb)
