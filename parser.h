#ifndef PARAJSON_PARSER_H
#define PARAJSON_PARSER_H

#include <cstdint>
#include <iostream>
#include <immintrin.h>

namespace ParaJson {

    struct Warp {
        __m128i hi, lo;

        Warp(const __m128i &h, const __m128i &l) : hi(h), lo(l) {}

        explicit Warp(const char *address) {
            lo = _mm_loadu_si128(reinterpret_cast<const __m128i *>(address));
            hi = _mm_loadu_si128(reinterpret_cast<const __m128i *>(address + 16));
        }
    };

    inline uint32_t __cmpeq_mask(const __m128i raw_hi, const __m128i raw_lo, char c) {
        const __m128i vec_c = _mm_set1_epi8(c);
        uint32_t hi = static_cast<uint32_t>(_mm_movemask_epi8(_mm_cmpeq_epi8(raw_hi, vec_c)));
        uint32_t lo = static_cast<uint32_t>(_mm_movemask_epi8(_mm_cmpeq_epi8(raw_lo, vec_c)));
        return (hi << 16U) | lo;
    }

    inline uint32_t __cmpeq_mask(const Warp &raw, char c) {
        return __cmpeq_mask(raw.hi, raw.lo, c);
    }

    void __error_maybe_escape(char *context, size_t *length, char ch);
    [[noreturn]] void __error(const std::string &message, const char *input, size_t offset);

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