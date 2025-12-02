#ifndef PARAJSON_PARSER_H
#define PARAJSON_PARSER_H

#include <immintrin.h>
#include <cstdint>
#include <iostream>

namespace ParaJson {

    struct SIMDPair {
        __m256i first, second;

        SIMDPair(const __m256i &h, const __m256i &l) : first(h), second(l) {}

        explicit SIMDPair(const char *address) {
            first = _mm256_loadu_si256(reinterpret_cast<const __m256i *>(address));
            second = _mm256_loadu_si256(reinterpret_cast<const __m256i *>(address + 32));
        }
    };

    inline uint64_t __cmpeq_mask(const __m256i raw_hi, const __m256i raw_lo, char c) {
        const __m256i vec_c = _mm256_set1_epi8(c);
        uint64_t hi = static_cast<uint32_t>(_mm256_movemask_epi8(_mm256_cmpeq_epi8(raw_hi, vec_c)));
        uint64_t lo = static_cast<uint32_t>(_mm256_movemask_epi8(_mm256_cmpeq_epi8(raw_lo, vec_c)));
        return (hi << 32U) | lo;
    }

    // inline uint64_t __cmpeq_mask(const SIMDPair &raw, char c) {
    //     return __cmpeq_mask(raw.first, raw.second, c);
    // }

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
        
        JSON(char *document, size_t size, bool manual_construct = false);
        ~JSON();
    };
}


#endif