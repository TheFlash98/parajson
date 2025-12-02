#include <iostream>
#include <stdio.h>
#include "utils.h"

#include "parser.h"

int main(void) {
    size_t size;
    char *buf = read_file("sample.json", &size);
    std::cout << "File size: " << size << " bytes\n";
    std::cout << "File content:\n" << buf << "\n";
    
    char *input = aligned_malloc(size + 2 * kAlignmentSize);
    memcpy(input, buf, size + 1);

    ParaJson::SIMDPair simd_pair(input);
    uint64_t mask = ParaJson::__cmpeq_mask(simd_pair, '{');
    std::cout << "Mask for '{': " << std::hex << mask << std::dec << "\n";
    
}