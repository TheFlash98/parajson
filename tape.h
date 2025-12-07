#include <stdio.h>
#include <atomic>

#include "parser.h"
#include "utils.h"
#include "parsestring.h"

namespace ParaJson {

    class Tape {
        static const uint64_t TYPE_MASK = 0xf000000000000000;
        static const uint64_t VALUE_MASK = ~TYPE_MASK;
        static const uint64_t TYPE_JUMP = 0x8000000000000000;

        uint64_t *tape;
        // Numerals are also stored off-tape, in the `numeric` array, at the same offset as the structural character.
        // When using multi-threaded number parsing, during the main parsing algorithm, tape offsets for each number
        // are stored in `numeric`. This offset is then used in number parsing threads to write the number type.
        uint64_t *numeric;
        char *literals;
        size_t tape_size, literals_size, numeric_size;

        inline void write_true(size_t offset) {
            std::cout << "Write true at offset: " << offset << std::endl;
            tape[offset] = TYPE_TRUE;
        }
        inline void write_false(size_t offset) {
            std::cout << "Write false at offset: " << offset << std::endl;
            tape[offset] = TYPE_FALSE;
        }
        inline void write_null(size_t offset) {
            std::cout << "Write null at offset: " << offset << std::endl;
            tape[offset] = TYPE_NULL;
        }
        inline void write_str(size_t offset, uint64_t literal_idx) {
            std::cout << "Write str at offset: " << offset << " with literal_idx: " << literal_idx << std::endl;
            tape[offset] = TYPE_STR | literal_idx;
        }
        inline void write_object(size_t offset) {
            std::cout << "Write object at offset: " << offset << std::endl;
            tape[offset] = TYPE_OBJ;
        }
        inline void write_object(size_t idx1, size_t idx2) {
            std::cout << "Write objects at idx1: " << idx1 << " and idx2: " << idx2 << std::endl;
            tape[idx1] = TYPE_OBJ | idx2;
            tape[idx2] = TYPE_OBJ | idx1;
        }
        inline void write_array(size_t idx1, size_t idx2) {
            std::cout << "Write arrays at idx1: " << idx1 << " and idx2: " << idx2 << std::endl;
            tape[idx1] = TYPE_ARR | idx2;
            tape[idx2] = TYPE_ARR | idx1;
        }
        inline size_t write_array() {
            std::cout << "Write array" << std::endl;
            write_array(tape_size);
            return tape_size++;
        }
        inline void write_array(size_t offset) {
            std::cout << "Write array at offset: " << offset << std::endl;
            tape[offset] = TYPE_ARR;
        }
        inline void write_jump(size_t offset, size_t dest) {
            std::cout << "Write jump at offset: " << offset << " with dest: " << dest << std::endl;
            tape[offset] = TYPE_JUMP | (dest - offset);
        }
        inline void write_content(size_t offset, uint64_t content) {
            std::cout << "Write content at offset: " << offset << " with content: " << content << std::endl;
            tape[offset] = (tape[offset] & TYPE_MASK) | content;
        }
        inline void append_content(size_t offset, uint64_t content) {
            std::cout << "Append content at offset: " << offset << " with content: " << content << std::endl;
            tape[offset] |= content;
        }

        // void _thread_parse_str(size_t pid, char *input, const size_t *idx_ptr, size_t structural_size, size_t num_chunks);
        void _parse_and_write_number(const char *input, size_t offset, size_t tape_idx, size_t numeric_idx);
        void __parse_and_write_number_backoff(const char *input, size_t offset, size_t tape_idx, size_t numeric_idx);
        void __parse_and_write_number(const char *input, size_t offset, size_t tape_idx, size_t numeric_idx);
        void _thread_parse_str_parlay(size_t i, char *input, const size_t *idx_ptr);
        void _thread_parse_num_parlay(size_t i, char *input, const size_t *idx_ptr);

        size_t _parse_str(char *input, size_t idx);
        void _thread_state_machine(char *input, const size_t *indices, size_t idx_begin, size_t idx_end,
                                   struct TapeStack *stack, size_t *tape_end, bool start_unknown = false);

    public:

        static const uint64_t TYPE_NULL = 0xf000000000000000;
        static const uint64_t TYPE_FALSE = 0x1000000000000000;
        static const uint64_t TYPE_TRUE = 0x2000000000000000;
        static const uint64_t TYPE_STR = 0x3000000000000000;
        static const uint64_t TYPE_INT = 0x4000000000000000;
        static const uint64_t TYPE_DEC = 0x5000000000000000;
        static const uint64_t TYPE_OBJ = 0x6000000000000000;
        static const uint64_t TYPE_ARR = 0x7000000000000000;
        
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

        friend class TapeWriter;

        void state_machine(char *input, size_t *idx_ptr, size_t structural_size, int chunk_size);
        void print_tape();
    };

    class TapeWriter {
        Tape *tape;
        const char *input;
        const size_t *indices;
        size_t idx_offset;

        void _parse_value();
        size_t _parse_str(size_t idx);
        size_t _parse_array();
        size_t _parse_object();

    public:
        TapeWriter(Tape *tape, const char *input, size_t *indices) : tape(tape), input(input), indices(indices) {}

        inline void parse_value() {
            _parse_value();
        }
    };
}