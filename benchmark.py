import os
import sys
import subprocess
import glob
import argparse
import time


BENCHMARKS_PATH = "./benchmarks"
EXECUTABLE = "./build/parajson"

def parse_args():
    parser = argparse.ArgumentParser(description="Run Benchmarks for JSON Parsing")
    parser.add_argument("--implementation", "-i", required=True, help="simd | parajson", choices=['simd', 'parajson'])
    parser.add_argument("--file", "-f", help="Specific JSON file to parse")
    parser.add_argument("--warmups", "-w", type=int, default=1, help="Number of warmup runs")
    parser.add_argument("--iter", "-n", type=int, default=1, help="Number of iterations to run")
    parser.add_argument("--suppress", action='store_true', help="Suppress output of JSON parsing")
    parsed_args = parser.parse_args()
    return parsed_args

def run_executable(executable, exec_args, suppress_output):
    cmd = [executable] + exec_args
    if suppress_output:
        subprocess.run(cmd, check=True, stdout = subprocess.DEVNULL)
    else:
        subprocess.run(cmd, check=True)

def run_benchmarks(implementation, warmups, iterations, suppress_output = False, input_file = None):
    files = []
    if input_file:
        files.append(input_file)
    else:
        for f in os.listdir(BENCHMARKS_PATH):
            if os.path.isfile(os.path.join(BENCHMARKS_PATH, f)):
                files.append(os.path.join(BENCHMARKS_PATH, f))

    if not files:
        print(f"ERROR: No files found in {BENCHMARKS_PATH}")
        sys.exit(1)

    for file_path in files:
        total_time = 0
        print(f"Input File: {file_path}\n")

        for w in range(warmups):
            print(f"[Warmup {w+1}/{warmups}]")
            start = time.perf_counter()
            run_executable(EXECUTABLE, ["--implementation", implementation, "--file", file_path], suppress_output)
            end = time.perf_counter()
            print(f"Time Taken: {end - start:.6f} seconds\n")

        for i in range(iterations):
            print(f"[Iteration {i+1}/{iterations}]")
            start = time.perf_counter()
            run_executable(EXECUTABLE, ["--implementation", implementation, "--file", file_path], suppress_output)
            end = time.perf_counter()
            time_taken = end - start
            total_time += time_taken
            print(f"Time Taken: {time_taken:.6f} seconds\n")

        print(f"Average Time Taken: {total_time/iterations:.6f} seconds\n")
        print("--------------------------------------------------------------------------------------\n")

def main():
    args = parse_args()
    run_benchmarks(args.implementation, args.warmups, args.iter, args.suppress, args.file)
    print("All benchmarks completed successfully.")

if __name__ == "__main__":
    main()
