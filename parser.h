#ifndef PARAJSON_PARSER_H
#define PARAJSON_PARSER_H

#include <immintrin.h>

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

    inline uint64_t __cmpeq_mask(const SIMDPair &raw, char c) {
        return __cmpeq_mask(raw.first, raw.second, c);
    }
}


#endif