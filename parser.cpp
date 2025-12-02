#include "utils.h"
#include <arm_neon.h>

#include "parser.h"

namespace ParaJson {
    
    static const uint64_t kEvenMask64 = 0x5555555555555555U;
    static const uint64_t kOddMask64 = ~kEvenMask64;

    // @formatter:off
    uint64_t extract_escape_mask(char* raw, uint64_t *prev_odd_backslash_ending_mask) {
        uint64_t backslash_mask = __cmpeq_mask(raw, '\\');
        uint64_t start_backslash_mask = backslash_mask & ~(backslash_mask << 1U);

        uint64_t even_start_backslash_mask = (start_backslash_mask & kEvenMask64) ^ *prev_odd_backslash_ending_mask;
        uint64_t even_carrier_backslash_mask = even_start_backslash_mask + backslash_mask;
        uint64_t even_escape_mask = (even_carrier_backslash_mask ^ backslash_mask) & kOddMask64;

        uint64_t odd_start_backslash_mask = (start_backslash_mask & kOddMask64) ^ *prev_odd_backslash_ending_mask;
        uint64_t odd_carrier_backslash_mask = odd_start_backslash_mask + backslash_mask;
        uint64_t odd_escape_mask = (odd_carrier_backslash_mask ^ backslash_mask) & kEvenMask64;

        uint64_t odd_backslash_ending_mask = odd_carrier_backslash_mask < odd_start_backslash_mask;
        *prev_odd_backslash_ending_mask = odd_backslash_ending_mask;

        return even_escape_mask | odd_escape_mask;
    }

    uint64_t extract_literal_mask(
            const Warp &raw, uint64_t escape_mask, uint64_t *prev_literal_ending, uint64_t *quote_mask) {
        uint64_t _quote_mask = __cmpeq_mask(raw, '"') & ~escape_mask;
        uint64_t literal_mask = _mm_cvtsi128_si64(
                _mm_clmulepi64_si128(_mm_set_epi64x(0ULL, _quote_mask), _mm_set1_epi8(0xFF), 0));
        literal_mask ^= *prev_literal_ending;
        *quote_mask = _quote_mask;
        *prev_literal_ending = static_cast<uint64_t>(static_cast<int64_t>(literal_mask) >> 63);
        return literal_mask;
    }


    JSON::JSON(char *document, size_t size,  bool manual_construct) {
        input = document;
        input_len = size;
        // this->document = nullptr;

        idx_ptr = indices = aligned_malloc<size_t>(size);
        num_indices = 0;


        if (!manual_construct) {
            exec_stage_1();
        }
    }

    void JSON::exec_stage_1() {
        uint64_t prev_escape_mask = 0;
        uint64_t prev_quote_mask = 0;
        uint64_t prev_pseudo_mask = 1;  // initial value set to 1 to allow literals at beginning of input
        uint64_t quote_mask, structural_mask, whitespace_mask;
        uint64_t pseudo_mask = 0;
        size_t offset = 0;
        for (; offset < input_len; offset += 64) {
            char* warp = (input + offset);
            uint64_t escape_mask = extract_escape_mask(warp, &prev_escape_mask);
            uint64_t literal_mask = extract_literal_mask(warp, escape_mask, &prev_quote_mask, &quote_mask);
            std::cout << std::bitset<64>(literal_mask) << "\n";
            // // Dump pointers for *previous* iteration.
            // construct_structural_character_pointers(pseudo_mask, offset - 64, indices, &num_indices);

            // extract_structural_whitespace_characters(warp, literal_mask, &structural_mask, &whitespace_mask);
            // pseudo_mask = extract_pseudo_structural_mask(
            //         structural_mask, whitespace_mask, quote_mask, literal_mask, &prev_pseudo_mask);
        }
        // Dump pointers for the final iteration.
        // construct_structural_character_pointers(pseudo_mask, offset - 64, indices, &num_indices);
        // if (num_indices == 0 || input[indices[num_indices - 1]] != '\0')  // Ensure '\0' is added to indices.
        //     indices[num_indices++] = offset - 64 + strlen(input + offset - 64);
        // if (prev_quote_mask != 0)
        //     throw std::runtime_error("unclosed quotation marks");
    }

    // void JSON::exec_stage_1() {
    //     // Implementation of stage 1 parsing
    //     uint64_t mask = __cmpeq_mask(input, '"');
    //     std::cout << std::bitset<64>(mask) << "\n";
    // }

    JSON::~JSON() {
        if (indices != nullptr) aligned_free(indices);
    }
}