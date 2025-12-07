#ifndef PARAJSON_PARSER_H
#define PARAJSON_PARSER_H

#include <cstdint>
#include <iostream>
#include <immintrin.h>
#include <parlay/sequence.h>
#include <parlay/primitives.h>

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
    inline parlay::sequence<bool> __cmpeq_mask_parlay(char* input, size_t input_size, char c) {
       parlay::sequence<bool> result = parlay::tabulate(input_size, [&](size_t i) {
           return (input[i] == c);
       });
       return result;
    }


    class JSON {
    public:
        char *input;
        size_t input_len, num_indices;
        size_t *indices;
        const size_t *idx_ptr;

        void exec_stage_1();
        void exec_stage_1_parlay();
        
        JSON(char *document, size_t size);
        ~JSON();
    };

    bool parse_true(const char *s, size_t offset = 0U);
    bool parse_false(const char *s, size_t offset = 0U);
    void parse_null(const char *s, size_t offset = 0U);
    long long int parse_number(const char *s, bool *is_decimal, size_t offset = 0U);
}


#endif