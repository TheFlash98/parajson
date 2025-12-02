#include <iostream>
#include <stdio.h>
#include <string.h>
#include <bitset>
#include <getopt.h>
#include "simdjson.h"

#include "utils.h"
#include "parser.h"

void simd_parse(char *input_path) {
    simdjson::ondemand::parser parser;
    simdjson::padded_string json = simdjson::padded_string::load(input_path);
    simdjson::ondemand::document tweets = parser.iterate(json);
    std::cout << uint64_t(tweets["age"]) << " results." << std::endl;
}

void parajson_parse(char *input_path) {
    size_t size;
    char *buf = read_file(input_path, &size);
    std::cout << "File size: " << size << " bytes\n";
    std::cout << "File content:\n" << buf << "\n";
    
    char *input = aligned_malloc(size + 2 * kAlignmentSize);
    memcpy(input, buf, size + 1);

    auto json = ParaJson::JSON(input, size);
    std::cout << "size of JSON object: " << json.input_len << " bytes\n";
}

int main(int argc, char **argv) {
    int c;
    enum Implementation {SIMD, PARAJSON};
    Implementation impl = PARAJSON;
    char *input_path = NULL;

    int option_index = 0;
    static struct option long_options[] = {
        {"implementation", required_argument, NULL, 'i'},
        {"file", required_argument, NULL, 'f'},
        {NULL, 0, NULL, 0}
    };

    while ((c = getopt_long(argc, argv, "i:f:", long_options, &option_index)) != -1) {
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
                std::cout << "Input JSON: " << input_path << std::endl;
                break;
            default:
                return 1;
        }
    }

    if (!input_path) {
        std::cerr << "Input file is required." << std::endl;
        return 1;
    }

    impl == SIMD ? simd_parse(input_path) : parajson_parse(input_path);
}