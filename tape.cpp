#include "tape.h"
#include <parlay/parallel.h>
#include <parlay/primitives.h>
#include <chrono>

namespace ParaJson {

    void Tape::_thread_parse_str(size_t pid, char *input, const size_t *idx_ptr, size_t structural_size, size_t num_chunks) {
        size_t idx;
        size_t begin = pid * structural_size / num_chunks;
        size_t end = (pid + 1) * structural_size / num_chunks;
        if (end > structural_size) end = structural_size;
        for (size_t i = begin; i < end; ++i) {
            idx = idx_ptr[i];
            char *dest = input + idx + 1;
            if (input[idx] == '"') {
                parse_str(input, dest, nullptr, idx + 1);
            }
        }
    }

    void Tape::_thread_parse_str_parlay(size_t i, char *input, const size_t *idx_ptr) {
        size_t idx = idx_ptr[i];
        char *dest = input + idx + 1;
        if (input[idx] == '"') {
            parse_str(input, dest, nullptr, idx + 1);
        }
    }

    void Tape::state_machine(char *input, size_t *idx_ptr, size_t structural_size) {
        if (structural_size == 1)
            __error("emtpy string is not valid JSON", input, 0);
        
        auto start = std::chrono::high_resolution_clock::now();

        parlay::parallel_for(0, structural_size, [&](int i) {
            Tape::_thread_parse_str_parlay(i, input, idx_ptr);
        });

        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> diff = end - start;
        std::cout << "String parsing time: " << diff.count() << " s\n";
        literals = input;
    }
}
