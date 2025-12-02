#ifndef PARAJSON_PARSER_H
#define PARAJSON_PARSER_H

#include <cstdint>
#include <iostream>

namespace ParaJson {

    inline uint64_t __cmpeq_mask(char* input, char c) {
        std::cout << "Using fallback __cmpeq_mask\n";
        uint64_t mask = 0;
        for (int i = 0; i < 64; ++i) {
            char val = input[i];
            if (val == c) mask |= (1ULL << i);
        }
        return mask;
    }


    class JSON {
    public:
        char *input;
        size_t input_len, num_indices;
        size_t *indices;
        const size_t *idx_ptr;

        void exec_stage_1();
        
        JSON(char *document, size_t size, bool manual_construct = false);
        ~JSON();
    };
}


#endif