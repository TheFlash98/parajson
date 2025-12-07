#include <immintrin.h>
#include <bitset>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <sstream>
#include <math.h>
#include "utils.h"
#include "parser.h"
#include "constants.h"

namespace ParaJson {
    
    static const uint32_t kEvenMask32 = 0x55555555U;
    static const uint32_t kOddMask32 = ~kEvenMask32;

    // @formatter:off
    uint32_t extract_escape_mask(const Warp &raw, uint32_t *prev_odd_backslash_ending_mask) {
        uint32_t backslash_mask = __cmpeq_mask(raw, '\\');
        uint32_t start_backslash_mask = backslash_mask & ~(backslash_mask << 1U);

        uint32_t even_start_backslash_mask = (start_backslash_mask & kEvenMask32) ^ *prev_odd_backslash_ending_mask;
        uint32_t even_carrier_backslash_mask = even_start_backslash_mask + backslash_mask;
        uint32_t even_escape_mask = (even_carrier_backslash_mask ^ backslash_mask) & kOddMask32;

        uint32_t odd_start_backslash_mask = (start_backslash_mask & kOddMask32) ^ *prev_odd_backslash_ending_mask;
        uint32_t odd_carrier_backslash_mask = odd_start_backslash_mask + backslash_mask;
        uint32_t odd_escape_mask = (odd_carrier_backslash_mask ^ backslash_mask) & kEvenMask32;

        uint32_t odd_backslash_ending_mask = odd_carrier_backslash_mask < odd_start_backslash_mask;
        *prev_odd_backslash_ending_mask = odd_backslash_ending_mask;

        return even_escape_mask | odd_escape_mask;
    }

    uint32_t extract_literal_mask(
            const Warp &raw, uint32_t escape_mask, uint32_t *prev_literal_ending, uint32_t *quote_mask) {
        uint32_t _quote_mask = __cmpeq_mask(raw, '"') & ~escape_mask;
        uint32_t literal_mask = _mm_cvtsi128_si32(
                _mm_clmulepi64_si128(_mm_set_epi64x(0ULL, _quote_mask), _mm_set1_epi8(0xFF), 0));
        literal_mask ^= *prev_literal_ending;
        *quote_mask = _quote_mask;
        *prev_literal_ending = static_cast<uint32_t>(static_cast<uint32_t>(literal_mask) >> 31);
        return literal_mask;
    }

    const size_t kStructuralUnrollCount = 4;
    inline void construct_structural_character_pointers(
            uint32_t pseudo_structural_mask, size_t offset, size_t *indices, size_t *base) {
        size_t next_base = *base + __builtin_popcountll(pseudo_structural_mask);
        while (pseudo_structural_mask) {
            for (size_t i = 0; i < kStructuralUnrollCount; ++i) {
                size_t bit = __builtin_ctzll(pseudo_structural_mask);     // tzcnt
                indices[*base + i] = offset + bit;
                pseudo_structural_mask &= (pseudo_structural_mask - 1);   // blsr
            }
            *base += kStructuralUnrollCount;
        }
        *base = next_base;
    }


    void extract_structural_whitespace_characters(
            const Warp &raw, uint32_t literal_mask, uint32_t *structural_mask, uint32_t *whitespace_mask) {
        const __m128i upper_lookup = _mm_setr_epi8(8, 0, 17, 2, 0, 4, 0, 4, 0, 0, 0, 0, 0, 0, 0, 0);
        const __m128i lower_lookup = _mm_setr_epi8(16, 0, 0, 0, 0, 0, 0, 0, 0, 8, 10, 4, 1, 12, 0, 0);

        __m128i hi_upper_index = _mm_shuffle_epi8(upper_lookup, _mm_and_si128(_mm_srli_epi16(raw.hi, 4),
                                                                                    _mm_set1_epi8(0x7F)));
        __m128i hi_lower_index = _mm_shuffle_epi8(lower_lookup, raw.hi);
        __m128i hi_character_label = _mm_and_si128(hi_upper_index, hi_lower_index);

        __m128i lo_upper_index = _mm_shuffle_epi8(upper_lookup, _mm_and_si128(_mm_srli_epi16(raw.lo, 4),
                                                                                    _mm_set1_epi8(0x7F)));
        __m128i lo_lower_index = _mm_shuffle_epi8(lower_lookup, raw.lo);
        __m128i lo_character_label = _mm_and_si128(lo_upper_index, lo_lower_index);

        __m128i hi_whitespace_mask = _mm_and_si128(hi_character_label, _mm_set1_epi8(0x18));
        __m128i lo_whitespace_mask = _mm_and_si128(lo_character_label, _mm_set1_epi8(0x18));
        *whitespace_mask = ~(__cmpeq_mask(hi_whitespace_mask, lo_whitespace_mask, 0) | literal_mask);

        __m128i hi_structural_mask = _mm_and_si128(hi_character_label, _mm_set1_epi8(0x7));
        __m128i lo_structural_mask = _mm_and_si128(lo_character_label, _mm_set1_epi8(0x7));
        *structural_mask = ~(__cmpeq_mask(hi_structural_mask, lo_structural_mask, 0) | literal_mask);
    }

    uint32_t extract_pseudo_structural_mask(
            uint32_t structural_mask, uint32_t whitespace_mask, uint32_t quote_mask, uint32_t literal_mask,
            uint32_t *prev_pseudo_structural_end_mask) {
        uint32_t st_ws = structural_mask | whitespace_mask;
        structural_mask |= quote_mask;
        uint32_t pseudo_structural_mask = (st_ws << 1U) | *prev_pseudo_structural_end_mask;
        *prev_pseudo_structural_end_mask = st_ws >> 31U;
        pseudo_structural_mask &= (~whitespace_mask) & (~literal_mask);
        structural_mask |= pseudo_structural_mask;
        structural_mask &= ~(quote_mask & ~literal_mask);
        return structural_mask;
    }

    JSON::JSON(char *document, size_t size) {
        input = document;
        input_len = size;
        // this->document = nullptr;

        idx_ptr = indices = aligned_malloc<size_t>(size);
        num_indices = 0;
    }

    void print_mask(parlay::sequence<bool> mask) {
        for (size_t i = 0; i < mask.size(); ++i) {
            printf("%d", mask[i]);
        }
        printf("\n");
    }

    void print_mask(parlay::sequence<uint32_t> mask) {
        for (size_t i = 0; i < mask.size(); ++i) {
            printf("%d", mask[i]);
        }
        printf("\n");
    }

    void print_mask(parlay::sequence<int> mask) {
        for (size_t i = 0; i < mask.size(); ++i) {
            printf("%d", mask[i]);
        }
        printf("\n");
    }

    inline bool is_structural_char(char c) {
        return c == '{' || c == '}' || c == '[' || c == ']' || 
            c == ':' || c == ',';
    }

    void JSON::exec_stage_1_parlay() {        
        // Compute backslash masks in parallel
        // printf("Computing backslash masks using parlay for len %i\n", input_len);
        // parlay::sequence<bool> backslash_mask_seq = __cmpeq_mask_parlay(input, input_len, '\\');
        // parlay::sequence<uint32_t> last_run_start = parlay::tabulate(input_len, [&](size_t i) {
        //     if (input[i] == '\\') {
        //         if (i == 0 || input[i - 1] != '\\') {
        //             return static_cast<uint32_t>(i);
        //         } else {
        //             return 0U;
        //         }
        //     } else {
        //         return 0U;
        //     }
        // });
        // // print_mask(last_run_start);
        // auto op = [](uint32_t a, uint32_t b) {
        //     return b > 0 ? b : a;
        // };
        // // Inclusive scan (probably what you want)
        // auto prefix_sum = parlay::scan(
        //     last_run_start, parlay::binary_op(op, 0)
        // );        
        // auto escape_mask_seq = parlay::tabulate(input_len, [&](size_t i) {
        //     if (i == 0) {
        //         return false;
        //     }
        //     uint32_t last_start = prefix_sum.first[i];
        //     if (backslash_mask_seq[i-1]) {
        //         size_t run_length = i - last_start + 1;
        //         return (run_length % 2 == 0);
        //     } else {
        //         return false;
        //     }
        // });
        // print_mask(escape_mask_sseq);
        
        // Identify backslashes
        // parlay::sequence<bool> backslash_mask_seq = __cmpeq_mask_parlay(input, input_len, '\\');

        // Count consecutive backslashes ending at each position
        parlay::sequence<uint8_t> backslash_counts = parlay::tabulate(input_len, [&](size_t i) {
            return (input[i] == '\\') ? uint8_t(1) : uint8_t(0);
        });
        // But the scan result still needs to be uint32_t
        auto op = parlay::binary_op([](uint32_t a, uint32_t b) { return b > 0 ? a + b : 0U; }, 0U);
        auto counts = parlay::scan_inclusive(
            backslash_counts, 
            op   
        );
        // print_mask(counts);
        // A character is escaped if preceded by an odd number of backslashes
        auto escape_mask_seq = parlay::tabulate(input_len, [&](size_t i) {
            return (i > 0) && (counts[i-1] % 2 == 1);
        });
        // print_mask(escape_mask_seq);
        // auto true_quote_mask_seq = parlay::tabulate(input_len, [&](size_t i) {
        //     return (input[i] == '"') && !escape_mask_seq[i];
        // });
        // print_mask(true_quote_mask_seq);
        // auto op2 = parlay::binary_op([](uint32_t a, bool b) { return a + (b ? 1U : 0U); }, 0U);
        // auto quote_count = parlay::scan(true_quote_mask_seq, op2);
        // print_mask(quote_count.first);
        // auto literal_mask_seq = parlay::tabulate(input_len, [&](size_t i) {
        //     if (true_quote_mask_seq[i]) {
        //         return (quote_count.first[i] % 2 == 0);
        //     }
        //     return (quote_count.first[i] % 2 == 1);
        // });
        // print_mask(literal_mask_seq);
        auto literal_mask_seq = parlay::scan_inclusive(
            parlay::delayed_tabulate(input_len, [&](size_t i) {
                return (input[i] == '"' && !escape_mask_seq[i]) ? 1U : 0U;
            }),
            parlay::binary_op([](uint32_t a, uint32_t b) { 
                return (a + b) % 2; 
            }, 0U)
        );
        // print_mask(literal_mask_seq);
        parlay::sequence<bool> whitespace_mask_seq = parlay::tabulate(input_len, [&](size_t i) {
            char c = input[i];
            return c == ' ' || c == '\n' || c == '\r' || c == '\t';
        });
        parlay::sequence<bool> quote_mask_seq = parlay::tabulate(input_len, [&](size_t i) {
            return input[i] == '"';
        });
        // print_mask(quote_mask_seq);
        auto structural_mask = parlay::tabulate(input_len, [&](size_t i) {
            bool is_struct = is_structural_char(input[i]) && !literal_mask_seq[i];
            bool is_ws = whitespace_mask_seq[i] && !literal_mask_seq[i];
            bool is_quote = quote_mask_seq[i] && !escape_mask_seq[i];
            bool in_literal = literal_mask_seq[i];

            bool p = is_struct || is_ws || is_quote;
            is_struct = is_struct || is_quote;
            bool is_struct_prev = (i > 0) ? is_structural_char(input[i-1]) && !literal_mask_seq[i-1] : p;
            bool is_ws_prev = (i > 0) ? whitespace_mask_seq[i-1] && !literal_mask_seq[i-1] : p;
            bool is_quote_prev = (i > 0) ? quote_mask_seq[i-1] && !escape_mask_seq[i-1] : p;
            bool prev_p = is_struct_prev || is_ws_prev || is_quote_prev;
            bool is_candidate = (i > 0) ? prev_p || whitespace_mask_seq[i-1] : p;
            
            is_candidate = is_candidate && !is_ws && !in_literal;
            is_struct = is_struct || is_candidate;
            is_struct = is_struct && !(is_quote && !in_literal);
            return is_struct;
        });
        auto indices_parlay = parlay::pack_index<uint32_t>(
            parlay::delayed_tabulate(input_len, [&](size_t i) {
                return structural_mask[i];
            })
        );
        num_indices = indices_parlay.size() + 1;  // +1 for null terminator
        this->indices = aligned_malloc<size_t>(num_indices);
        parlay::parallel_for(0, indices_parlay.size(), [&](size_t i) {
            this->indices[i] = indices_parlay[i];
        });
        this->indices[num_indices - 1] = input_len;  // null terminator

        // for (size_t i = 0; i < num_indices; ++i) {
        //     std::cout << "Index " << i << ": " << indices[i] << " ('" << input[indices[i]] << "')\n";
        // }
        
    }   

    void JSON::exec_stage_1() {
        uint32_t prev_escape_mask = 0;
        uint32_t prev_quote_mask = 0;
        uint32_t prev_pseudo_mask = 1;  // initial value set to 1 to allow literals at beginning of input
        uint32_t quote_mask, structural_mask, whitespace_mask;
        uint32_t pseudo_mask = 0;
        size_t offset = 0;
        for (; offset < input_len; offset += 32) {
            Warp warp(input + offset);
            uint32_t escape_mask = extract_escape_mask(warp, &prev_escape_mask);
            uint32_t literal_mask = extract_literal_mask(warp, escape_mask, &prev_quote_mask, &quote_mask);
            
            // Dump pointers for *previous* iteration.
            construct_structural_character_pointers(pseudo_mask, offset - 32, indices, &num_indices);

            extract_structural_whitespace_characters(warp, literal_mask, &structural_mask, &whitespace_mask);
            pseudo_mask = extract_pseudo_structural_mask(structural_mask, whitespace_mask, quote_mask, literal_mask, &prev_pseudo_mask);
        }
        // Dump pointers for the final iteration.
        construct_structural_character_pointers(pseudo_mask, offset - 32, indices, &num_indices);
        if (num_indices == 0 || input[indices[num_indices - 1]] != '\0')  // Ensure '\0' is added to indices.
            indices[num_indices++] = offset - 32 + strlen(input + offset - 32);
        if (prev_quote_mask != 0)
            throw std::runtime_error("unclosed quotation marks");
        // for (size_t i = 0; i < num_indices; ++i) {
        //     std::cout << "Index " << i << ": " << indices[i] << " ('" << input[indices[i]] << "')\n";
        // }
    }

    void __error_maybe_escape(char *context, size_t *length, char ch) {
        if (ch == '\0') {
            context[(*length)++] = '"';
        } else if (ch == '\t' || ch == '\n' || ch == '\b') {
            context[(*length)++] = '\\';
            switch (ch) {
                case '\t':
                    context[(*length)++] = 't';
                    break;
                case '\n':
                    context[(*length)++] = 'n';
                    break;
                case '\b':
                    context[(*length)++] = 'b';
                    break;
                default:
                    break;
            }
        } else {
            context[(*length)++] = ch;
        }
    }

    long long int parse_number(const char *input, bool *is_decimal, size_t offset) {
        const char *s = input + offset;
        long long int integer = 0LL;
        double decimal = 0.0;
        bool negative = false, _is_decimal = false;
        if (*s == '-') {
            ++s;
            negative = true;
        }
        if (*s == '0') {
            ++s;
            if (*s >= '0' && *s <= '9')
                __error("numbers cannot have leading zeros", input, offset);
        } else {
            if (*s < '0' || *s > '9')
                ParaJson::__error("numbers must have integer parts", input, offset);
            while (*s >= '0' && *s <= '9')
                integer = integer * 10 + (*s++ - '0');
        }
        if (s - input - offset > 18)
            __error("integer part too large", input, offset);
        if (*s == '.') {
            _is_decimal = true;
            decimal = integer;
            double multiplier = 0.1;
            ++s;
            while (*s >= '0' && *s <= '9') {
                decimal += (*s++ - '0') * multiplier;
                multiplier *= 0.1;
            }if (multiplier == 0.1)
                __error("excessive characters at end of number", input, s - input - 1);
        }
        if (*s == 'e' || *s == 'E') {
            if (!_is_decimal) {
                _is_decimal = true;
                decimal = integer;
            }
            ++s;
            bool negative_exp = false;
            if (*s == '-') {
                negative_exp = true;
                ++s;
            } else if (*s == '+') ++s;
            double exponent = 0.0;
            if (*s < '0' || *s > '9')
                ParaJson::__error("numbers must not have null exponents", input, s - input);
            do {
                exponent = exponent * 10.0 + (*s++ - '0');
            } while (*s >= '0' && *s <= '9');
            if (negative_exp) exponent = -exponent;
            if (exponent < -308 || exponent > 308)
                __error("decimal exponent out of range", input, offset);
            decimal *= pow(10.0, exponent);
        }
        if (!kStructuralOrWhitespace[*s])
            __error("excessive characters at end of number", input, s - input);
        *is_decimal = _is_decimal;
        if (negative) {
            if (_is_decimal) return plain_convert(-decimal);
            else return -integer;
        } else {
            if (_is_decimal) return plain_convert(decimal);
            else return integer;
        }
    }

    bool parse_true(const char *s, size_t offset) {
        uint32_t literal;
        memcpy(&literal, s + offset, 4);
        uint32_t target = 0x65757274;
        if (target != literal || !kStructuralOrWhitespace[s[offset + 4]])
            __error("invalid true value", s, offset);
        return true;
    }

    bool parse_false(const char *s, size_t offset) {
        uint64_t literal;
        memcpy(&literal, s + offset, 5);
        uint64_t target = 0x00000065736c6166;
        uint64_t mask = 0x000000ffffffffff;
        if (target != (literal & mask) || !kStructuralOrWhitespace[s[offset + 5]])
            __error("invalid false value", s, offset);
        return false;
    }

    void parse_null(const char *s, size_t offset) {
        uint32_t literal;
        memcpy(&literal, s + offset, 4);
        uint32_t target = 0x6c6c756e;
        if (target != literal || !kStructuralOrWhitespace[s[offset + 4]])
            __error("invalid null value", s, offset);
    }
    
    [[noreturn]] void __error(const std::string &message, const char *input, size_t offset) {
        static const size_t context_len = 20;
        char *context = new char[(2 * context_len + 1) * 4];  // add space for escaped chars
        size_t length = 0;
        if (offset > context_len) {
            context[0] = context[1] = context[2] = '.';
            length = 3;
        }
        for (size_t i = offset > context_len ? offset - context_len : 0U; i < offset; ++i)
            __error_maybe_escape(context, &length, input[i]);
        size_t left = length;
        bool end = false;
        for (size_t i = offset; i < offset + context_len; ++i) {
            if (input[i] == '\0') {
                end = true;
                break;
            }
            __error_maybe_escape(context, &length, input[i]);
        }
        if (!end) {
            context[length] = context[length + 1] = context[length + 2] = '.';
            length += 3;
        }
        context[length] = 0;
        std::stringstream stream;
        stream << message << std::endl;
        stream << "context: " << context << std::endl;
        delete[] context;
        stream << "         " << std::string(left, ' ') << "^";
        throw std::runtime_error(stream.str());
    }

    JSON::~JSON() {
        if (indices != nullptr) aligned_free(indices);
    }
}