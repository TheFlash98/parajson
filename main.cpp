#include <iostream>
#include <stdio.h>
#include <string.h>
#include <bitset>


#include "utils.h"
#include "parser.h"

int main(void) {
    size_t size;
    char *buf = read_file("../sample.json", &size);
    std::cout << "File size: " << size << " bytes\n";
    std::cout << "File content:\n" << buf << "\n";
    
    char *input = aligned_malloc(size + 2 * kAlignmentSize);
    memcpy(input, buf, size + 1);

    auto json = ParaJson::JSON(input, size);
    std::cout << "size of JSON object: " << json.input_len << " bytes\n";
}