#include "parser.h"

namespace ParaJson {
    
    static const uint64_t kEvenMask64 = 0x5555555555555555U;
    static const uint64_t kOddMask64 = ~kEvenMask64;

    // @formatter:off
    // uint64_t extract_escape_mask(const SIMDPair &raw, uint64_t *prev_odd_backslash_ending_mask) {
    //     uint64_t backslash_mask = __cmpeq_mask(raw, '\\');
    //     uint64_t start_backslash_mask = backslash_mask & ~(backslash_mask << 1U);

    //     uint64_t even_start_backslash_mask = (start_backslash_mask & kEvenMask64) ^ *prev_odd_backslash_ending_mask;
    //     uint64_t even_carrier_backslash_mask = even_start_backslash_mask + backslash_mask;
    //     uint64_t even_escape_mask = (even_carrier_backslash_mask ^ backslash_mask) & kOddMask64;

    //     uint64_t odd_start_backslash_mask = (start_backslash_mask & kOddMask64) ^ *prev_odd_backslash_ending_mask;
    //     uint64_t odd_carrier_backslash_mask = odd_start_backslash_mask + backslash_mask;
    //     uint64_t odd_escape_mask = (odd_carrier_backslash_mask ^ backslash_mask) & kEvenMask64;

    //     uint64_t odd_backslash_ending_mask = odd_carrier_backslash_mask < odd_start_backslash_mask;
    //     *prev_odd_backslash_ending_mask = odd_backslash_ending_mask;

    //     return even_escape_mask | odd_escape_mask;
    // }
}