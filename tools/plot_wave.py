import sys
import json
import argparse
import matplotlib.pyplot as plt


def main():
    parser = argparse.ArgumentParser(description="Plot PCM-like JSON data (frame vs sample).")
    parser.add_argument("--limit", type=int, default=None, help="Number of entries to process (default: all)")
    args = parser.parse_args()

    # Read everything from stdin
    input_str = sys.stdin.read().strip()

    # Try parsing as JSON array, otherwise wrap into []
    try:
        data = json.loads(input_str)
    except json.JSONDecodeError:
        input_str = "[" + input_str.rstrip(",") + "]"
        data = json.loads(input_str)

    # Apply limit
    if args.limit is not None:
        data = data[: args.limit]

    # Extract frame and sample values
    frames = [point["frame"] for point in data]
    samples = [point["sample"] for point in data]

    # Normalize samples to range [-1, 1]
    min_val = -(2**15)
    max_val = 2**15
    if max_val != min_val:
        samples = [2 * (s - min_val) / (max_val - min_val) - 1 for s in samples]
    else:
        samples = [0 for _ in samples]

    # Plot
    plt.figure(figsize=(10, 5))
    plt.plot(frames, samples, linewidth=0.8)
    plt.xlabel("Frame")
    plt.ylabel("Normalized Sample")
    plt.title(f"Frame vs Sample ({len(samples)} entries, normalized)")
    plt.grid(True)
    plt.show()


if __name__ == "__main__":
    main()
