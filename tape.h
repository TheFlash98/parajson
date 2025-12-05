#include <stdio.h>
#include <atomic>

#include "parser.h"
#include "utils.h"
#include "parsestring.h"

namespace ParaJson {

    class Tape {
        uint64_t *tape;
        // Numerals are also stored off-tape, in the `numeric` array, at the same offset as the structural character.
        // When using multi-threaded number parsing, during the main parsing algorithm, tape offsets for each number
        // are stored in `numeric`. This offset is then used in number parsing threads to write the number type.
        uint64_t *numeric;
        char *literals;
        size_t tape_size, literals_size, numeric_size;

        // void _thread_parse_str(size_t pid, char *input, const size_t *idx_ptr, size_t structural_size);
        void _thread_parse_str(size_t pid, char *input, const size_t *idx_ptr, size_t structural_size, size_t num_chunks);
        void _thread_parse_str_parlay(size_t i, char *input, const size_t *idx_ptr);

    public:
        Tape(size_t string_size, size_t structural_size) {
            tape = aligned_malloc<uint64_t>(structural_size);
            numeric = aligned_malloc<uint64_t>(structural_size);
            tape_size = 0;
            literals_size = 0;
            numeric_size = 0;
        }

        ~Tape() {
            aligned_free(tape);
            aligned_free(numeric);
        }

        void state_machine(char *input, size_t *idx_ptr, size_t structural_size);
    };
}