import argparse
import subprocess
import re
import os
import matplotlib.pyplot as plt

def benchmark_file(filename):
    print(f"Benchmarking JSON processing for file: {filename}")
    results = {}
    for i in [1, 2, 4, 8, 16, 32, 64]:
        command = [
            "./build/parajson",
            "-f",
            filename,
            "-w",
            "10",
            "-r",
            "100",
            "-c",
            "1500"
        ]
        env = os.environ.copy()
        env["PARLAY_NUM_THREADS"] = str(i)
        result = subprocess.run(command, capture_output=True, text=True, env=env)
        
        # Extract the last 3 average times from the output
        output = result.stdout
        
        # Find all occurrences of the average times
        avg_time_matches = re.findall(r'Average Time: ([\d.e-]+) s', output)
        avg_stage1_matches = re.findall(r'Average Stage 1 Time: ([\d.e-]+) s', output)
        avg_stage2_matches = re.findall(r'Average Stage 2 Time: ([\d.e-]+) s', output)
        
        # Get the last 3 values (or fewer if not available)
        if avg_time_matches:
            print(f"\nLast Average Time: {avg_time_matches[-1]} s")
        if avg_stage1_matches:
            print(f"Last Average Stage 1 Time: {avg_stage1_matches[-1]} s")
        if avg_stage2_matches:
            print(f"Last Average Stage 2 Time: {avg_stage2_matches[-1]} s")
        results[i] = {
            "average_time": float(avg_time_matches[-1]) if avg_time_matches else None,
            "average_stage1_time": float(avg_stage1_matches[-1]) if avg_stage1_matches else None,
            "average_stage2_time": float(avg_stage2_matches[-1]) if avg_stage2_matches else None,
            "average_stage1+2_time": (float(avg_stage1_matches[-1]) + float(avg_stage2_matches[-1])) if avg_stage1_matches and avg_stage2_matches else None
        }
    dir_name = "benchmark_results/%s" % os.path.basename(filename)
    os.makedirs(dir_name, exist_ok=True)

    # Plotting the results
    thread_counts = list(results.keys())
    avg_times = [results[t]["average_time"] for t in thread_counts]
    speedups = [avg_times[0] / t if t else None for t in avg_times]
    plt.figure(figsize=(10, 6))
    plt.plot(thread_counts, speedups, marker='o')
    plt.xscale('log', base=2)
    plt.xlabel('Number of Threads (log scale)')
    plt.ylabel('Self speedup')
    plt.title('JSON Processing Benchmark')
    plt.grid(True, which="both", ls="--")
    plt.xticks(thread_counts, thread_counts)
    plt.savefig('%s/json_benchmark_%s.png' % (dir_name, os.path.basename(filename)))

    stage_1_times = [results[t]["average_stage1_time"] for t in thread_counts]
    stage_2_times = [results[t]["average_stage2_time"] for t in thread_counts]
    plt.figure(figsize=(10, 6))
    stage_1_speedups = [stage_1_times[0] / t if t else None for t in stage_1_times]
    stage_2_speedups = [stage_2_times[0] / t if t else None for t in stage_2_times]
    plt.plot(thread_counts, stage_1_speedups, marker='o', label='Stage 1 Time')
    plt.plot(thread_counts, stage_2_speedups, marker='o', label='Stage 2 Time')
    plt.xscale('log', base=2)
    plt.xlabel('Number of Threads (log scale)')
    plt.ylabel('Self speedup')
    plt.title('Stage 1 and Stage 2 Times')
    plt.grid(True, which="both", ls="--")
    plt.xticks(thread_counts, thread_counts)
    plt.legend()
    plt.savefig('%s/json_stages_benchmark_%s.png' % (dir_name, os.path.basename(filename)))

    stage_1_2_times = [results[t]["average_stage1+2_time"] for t in thread_counts]
    plt.figure(figsize=(10, 6))
    stage_1_2_speedups = [stage_1_2_times[0] / t if t else None for t in stage_1_2_times]
    plt.plot(thread_counts, stage_1_2_speedups, marker='o', color='purple')
    plt.xscale('log', base=2)
    plt.xlabel('Number of Threads (log scale)')
    plt.ylabel('Self speedup')
    plt.title('Stage 1 + Stage 2 Times')
    plt.grid(True, which="both", ls="--")
    plt.xticks(thread_counts, thread_counts)
    plt.savefig('%s/json_stage1_2_benchmark_%s.png' % (dir_name, os.path.basename(filename)))  
    
    print("\nFinal Results:")
    for threads, times in results.items():
        print(f"Threads: {threads}, Times: {times}")
    # save data in csv
    with open('%s/json_benchmark_%s.csv' % (dir_name, os.path.basename(filename)), 'w') as f:
        f.write("Threads,Average_Time,Average_Stage_1_Time,Average_Stage_2_Time,Average_Stage_1+2_Time\n")
        for threads, times in results.items():
            f.write(f"{threads},{times['average_time']},{times['average_stage1_time']},{times['average_stage2_time']},{times['average_stage1+2_time']}\n")


def main():
    args = parser.parse_args()


    # check if dir then iterate over files
    if os.path.isdir(args.file):
        for filename in os.listdir(args.file):
            if filename.endswith(".json"):
                filepath = os.path.join(args.file, filename)
                print(f"\nProcessing file: {filepath}")
                benchmark_file(filepath)
        return
    benchmark_file(args.file)

    
if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Benchmark JSON processing libraries.")
    parser.add_argument(
        "--file", "-f", type=str, required=True, help="Path to the JSON file to be processed."
    )
    
    main()