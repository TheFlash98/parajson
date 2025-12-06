#include <iostream>
#include <stdio.h>
#include <string.h>
#include <bitset>
#include <getopt.h>
#include <chrono>
#include "simdjson.h"

#include "utils.h"
#include "parser.h"
#include "tape.h"

double stage_1_time = 0.0;
double stage_2_time = 0.0;

void simd_parse(char *input_path) {
    simdjson::ondemand::parser parser;
    simdjson::padded_string json = simdjson::padded_string::load(input_path);
    simdjson::ondemand::document parsed_json = parser.iterate(json);
}

void parajson_parse(char *input_path, bool verbose=false) {
    size_t size;
    char *buf = read_file(input_path, &size);
    if (verbose)
        std::cout << "File size: " << size << " bytes\n";
    // std::cout << "File content:\n" << buf << "\n";
    
    char *input = aligned_malloc(size + 2 * kAlignmentSize);
    memcpy(input, buf, size + 1);

    auto json = ParaJson::JSON(input, size);
    if (verbose)
        std::cout << "Size of JSON object: " << json.input_len << " bytes\n";
    ParaJson::Tape tape(size, size);

    auto stage_1_start = std::chrono::high_resolution_clock::now();
    json.exec_stage_1();
    auto stage_1_end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> stage_1_diff = stage_1_end - stage_1_start;
    stage_1_time += stage_1_diff.count();

    if (verbose)
        std::cout << "Number of indices: " << json.num_indices << "\n";
    
    auto stage_2_start = std::chrono::high_resolution_clock::now();
    tape.state_machine(const_cast<char *>(json.input), json.indices, json.num_indices);
    auto stage_2_end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> stage_2_diff = stage_2_end - stage_2_start;
    stage_2_time += stage_2_diff.count();

    std::cout << "Stage 1 Time: " << stage_1_diff.count() << " s\n";
    std::cout << "Stage 2 Time: " << stage_2_diff.count() << " s\n\n";
}

int main(int argc, char **argv) {
    int c;
    enum Implementation {SIMD, PARAJSON};
    Implementation impl = PARAJSON;
    char *input_path = NULL;
    int num_iterations = 1;
    int warmups = 1;

    int option_index = 0;
    static struct option long_options[] = {
        {"implementation", required_argument, NULL, 'i'},
        {"file", required_argument, NULL, 'f'},
        {"repeats", required_argument, NULL, 'r'},
        {"warmups", required_argument, NULL, 'w'},
        {NULL, 0, NULL, 0}
    };

    while ((c = getopt_long(argc, argv, "i:f:r:w:", long_options, &option_index)) != -1) {
        switch (c) {
            case 'i':
                if (strcmp(optarg, "simd") == 0) {
                    impl = SIMD;
                } else if (strcmp(optarg, "parajson") == 0) {
                    impl = PARAJSON;
                } else {
                    std::cerr << "Invalid implementation: " << optarg << std::endl;
                    return 1;
                }
                break;
            case 'f':
                input_path = optarg;
                // std::cout << "Input JSON: " << input_path << std::endl;
                break;
            case 'r':
                num_iterations = atoi(optarg);
                break;
            case 'w':
                warmups = atoi(optarg);
                break;
            default:
                return 1;
        }
    }

    if (!input_path) {
        std::cerr << "Input file is required." << std::endl;
        return 1;
    }

    auto warmup_start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < warmups; ++i) {
        std::cout << "--------------------------------------------------\n";
        std::cout << "Warmup " << i + 1 << "\n";
        std::cout << "--------------------------------------------------\n";
        impl == SIMD ? simd_parse(input_path) : parajson_parse(input_path, i == 0);
    }
    auto warmup_end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> warmup_diff = warmup_end - warmup_start;
    std::cout << "Warmup Stats: \n";
    std::cout << "Average Time: " << warmup_diff.count() / warmups << " s\n";
    std::cout << "Average Stage 1 Time: " << stage_1_time / warmups << " s\n";
    std::cout << "Average Stage 2 Time: " << stage_2_time / warmups << " s\n\n";
    
    stage_1_time = 0.0;
    stage_2_time = 0.0;
    auto iterations_start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < num_iterations; ++i) {
        std::cout << "--------------------------------------------------\n";
        std::cout << "Iteration " << i + 1 << "\n";
        std::cout << "--------------------------------------------------\n";
        impl == SIMD ? simd_parse(input_path) : parajson_parse(input_path);
    }

    auto iterations_end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> iterations_diff = iterations_end - iterations_start;
    std::cout << "Iterations Stats: \n";
    std::cout << "Average Time: " << iterations_diff.count() / num_iterations << " s\n";
    std::cout << "Average Stage 1 Time: " << stage_1_time / num_iterations << " s\n";
    std::cout << "Average Stage 2 Time: " << stage_2_time / num_iterations << " s\n";
}