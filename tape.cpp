#include "tape.h"
#include "parser.h"

#include "constants.h"
#include <parlay/parallel.h>
#include <parlay/primitives.h>

namespace ParaJson {

    static const size_t kMaxDepth = 1024;

    [[noreturn]] void _error(const char *expected, const char *input, char encountered, size_t index) {
        std::stringstream stream;
        char _encounter[3];
        size_t len = 0;
        __error_maybe_escape(_encounter, &len, encountered);
        _encounter[len] = 0;
        stream << "expected " << expected << " at index " << index << ", but encountered '" << _encounter << "'";
        ParaJson::__error(stream.str(), input, index);
    }

    struct TapeStack {
        size_t depth, extra_closing_count;
        void *ret_address[kMaxDepth];
        size_t scope_offset[kMaxDepth];
        size_t extra_closing_offset[kMaxDepth];  // offsets of extra closing brackets

        TapeStack() : depth(0), extra_closing_count(0) {}

        inline void push(size_t offset, void *address) {
            scope_offset[depth] = offset;
            ret_address[depth] = address;
            ++depth;
        }
    };

#define peek_char() ({             \
        idx = indices[idx_offset]; \
        ch = input[idx];           \
    })

    // void Tape::_thread_parse_str(size_t pid, char *input, const size_t *idx_ptr, size_t structural_size, size_t num_chunks) {
    //     size_t idx;
    //     size_t begin = pid * structural_size / num_chunks;
    //     size_t end = (pid + 1) * structural_size / num_chunks;
    //     if (end > structural_size) end = structural_size;
    //     for (size_t i = begin; i < end; ++i) {
    //         idx = idx_ptr[i];
    //         char *dest = input + idx + 1;
    //         if (input[idx] == '"') {
    //             parse_str(input, dest, nullptr, idx + 1);
    //         }
    //     }
    // }

    void Tape::__parse_and_write_number_backoff(const char *input, size_t offset, size_t tape_idx, size_t numeric_idx) {
        bool is_decimal;
        auto ret = parse_number(input, &is_decimal, offset);
        if (is_decimal) {
            tape[tape_idx] = TYPE_DEC | numeric_idx;
            numeric[numeric_idx] = *reinterpret_cast<uint64_t *>(&ret);
        } else {
            tape[tape_idx] = TYPE_INT | numeric_idx;
            numeric[numeric_idx] = *reinterpret_cast<uint64_t *>(&ret);
        }
    }

    void Tape::__parse_and_write_number(const char *input, size_t offset, size_t tape_idx, size_t numeric_idx) {
        const char *s = input + offset;
        uint64_t integer = 0ULL;
        bool negative = false;
        int64_t exponent = 0LL;
        if (*s == '-') {
            ++s;
            negative = true;
        }
        if (*s == '0') {
            ++s;
            if (*s >= '0' && *s <= '9')
                ParaJson::__error("numbers cannot have leading zeros", input, offset);
        } else {
            if (*s < '0' || *s > '9')
                ParaJson::__error("numbers must have integer parts", input, offset);
            do {
                integer = integer * 10 + (*s++ - '0');
            } while (*s >= '0' && *s <= '9');
        }
        if (*s == '.') {
            const char *const base = ++s;
            while (*s >= '0' && *s <= '9')
                integer = integer * 10 + (*s++ - '0');
            exponent = base - s;
            if (exponent == 0)
                __error("excessive characters at end of number", input, s - input - 1);
        }
        if (s - input - offset >= 18) {
            // use the slower back-off method
            __parse_and_write_number_backoff(input, offset, tape_idx, numeric_idx);
            return;
        }
        if (*s == 'e' || *s == 'E') {
            ++s;
            bool negative_exp = false;
            if (*s == '-') {
                negative_exp = true;
                ++s;
            } else if (*s == '+') ++s;
            int64_t expo = 0LL;
            if (*s < '0' || *s > '9')
                ParaJson::__error("numbers must not have null exponents", input, s - input);
            do {
                expo = expo * 10 + (*s++ - '0');
            } while (*s >= '0' && *s <= '9');
            exponent += negative_exp ? -expo : expo;
        }
        if (!kStructuralOrWhitespace[*s])
            __error("excessive characters at end of number", input, s - input);
        if (exponent == 0) {
            tape[tape_idx] = TYPE_INT | numeric_idx;
            numeric[numeric_idx] = negative ? -integer : integer;
        } else {
            if (exponent < -308 || exponent > 308)
                ParaJson::__error("decimal exponent out of range", input, offset);
            double decimal = negative ? -integer : integer;
            decimal *= kPowerOfTen[308 + exponent];
            tape[tape_idx] = TYPE_DEC | numeric_idx;
            numeric[numeric_idx] = plain_convert(decimal);
        }
    }

    void Tape::_parse_and_write_number(const char *input, size_t offset, size_t tape_idx, size_t numeric_idx) {
        __parse_and_write_number(input, offset, tape_idx, numeric_idx);
    }

    void Tape::_thread_parse_num_parlay(size_t i, char *input, const size_t *idx_ptr) {
        size_t idx = idx_ptr[i];
        char *dest = input + idx + 1;
        size_t offset = idx_ptr[idx];
        if (!(input[offset] == '-' || (input[offset] >= '0' && input[offset] <= '9'))) return;
        size_t tape_idx = numeric[idx];
        std::cout << "i: " << i << "tape_idx :" << tape_idx << std::endl;
        __parse_and_write_number(input, offset, tape_idx, idx);
    }

    void Tape::_thread_parse_str_parlay(size_t i, char *input, const size_t *idx_ptr) {
        size_t idx = idx_ptr[i];
        char *dest = input + idx + 1;
        if (input[idx] == '"') {
            parse_str(input, dest, nullptr, idx + 1);
        }
    }

    size_t Tape::_parse_str(char *input, size_t idx) {
        return idx + 1;
    }

#define expect(__char) ({                                    \
        if (ch != (__char)) _error(#__char, input, ch, idx); \
    })


    void Tape::_thread_state_machine(char *input, const size_t *indices, size_t idx_begin, size_t idx_end,
                                     TapeStack *stack, size_t *tape_end, bool start_unknown) {

#define next_char() ({                           \
        if (idx_offset == idx_end) goto succeed; \
        idx = indices[idx_offset++];             \
        ch = input[idx];                         \
    })

        size_t idx;  // index in input
        char ch;  // current character
        size_t left_tape_idx, right_tape_idx;
        size_t idx_offset = idx_begin;
        // We keep a local counter for `tape_size` for the part of tape that the current thread writes to.
        // Our implementation only guarantees that tape size is no greater than the number of structural characters,
        // since commas (,) and colons (:) are not stored, and numerals and literals are stored off-tape.
        size_t tape_pos = idx_begin;

        if (start_unknown) {
            goto unknown_start;
        } else {
            goto start_value;
        }

#define PARSE_VALUE(continue_address) ({                                             \
            switch (ch) {                                                            \
                case '"':                                                             \
                    write_str(tape_pos++, _parse_str(input, idx));                   \
                    break;                                                           \
                case 't':                                                            \
                    parse_true(input, idx);                                          \
                    write_true(tape_pos++);                                          \
                    break;                                                           \
                case 'f':                                                            \
                    parse_false(input, idx);                                         \
                    write_false(tape_pos++);                                         \
                    break;                                                           \
                case 'n':                                                            \
                    parse_null(input, idx);                                          \
                    write_null(tape_pos++);                                          \
                    break;                                                           \
                case '0':                                                            \
                case '1':                                                            \
                case '2':                                                            \
                case '3':                                                            \
                case '4':                                                            \
                case '5':                                                            \
                case '6':                                                            \
                case '7':                                                            \
                case '8':                                                            \
                case '9':                                                            \
                case '-': {                                                          \
                    _parse_and_write_number(input, idx, tape_pos++, idx_offset - 1); \
                    break;                                                           \
                }                                                                    \
                case '[': {                                                          \
                    write_array(tape_pos);                                           \
                    stack->push(tape_pos++, continue_address);                       \
                    goto array_begin;                                                \
                }                                                                    \
                case '{': {                                                          \
                    write_object(tape_pos);                                          \
                    stack->push(tape_pos++, continue_address);                       \
                    goto object_begin;                                               \
                }                                                                    \
                default:                                                             \
                    goto fail;                                                       \
            }                                                                        \
        })


start_value:
        next_char();
        PARSE_VALUE(&&start_continue);
        goto succeed;
start_continue:
//        next_char();  // strip off the extra closing bracket at end
        goto succeed;

unknown_start:
        next_char();
        switch (ch) {
            case '"':
                write_str(tape_pos++, _parse_str(input, idx));
                peek_char();
                if (ch == ':') goto object_key_state;
                break;
            case ']':
                goto array_end;
            case '}':
                goto object_end;
            case ':':
                --idx_offset;
                goto object_key_state;
            case ',':
                goto unknown_2nd_value;
            default:
                PARSE_VALUE(&&unknown_continue);
        }
unknown_continue:
        next_char();
        switch (ch) {
            case ',':
                goto unknown_2nd_value;
            case ']':
                goto array_end;
            case '}':
                goto object_end;
            default:
                goto fail;
        }
unknown_2nd_value:
        next_char();
        if (ch == '"') {
            write_str(tape_pos++, _parse_str(input, idx));
            peek_char();
            if (ch == ':') goto object_key_state;
            else goto array_continue;
        } else {
            goto array_value;
        }

object_begin:
        next_char();
        switch (ch) {
            case '"':
                write_str(tape_pos++, _parse_str(input, idx));
                goto object_key_state;
            case '}':
                goto object_end;
            default:
                goto fail;
        }
object_key_state:
        next_char();
        expect(':');
        next_char();
        PARSE_VALUE(&&object_continue);
object_continue:
        next_char();
        switch (ch) {
            case ',':
                next_char();
                expect('"');
                write_str(tape_pos++, _parse_str(input, idx));
                goto object_key_state;
            case '}':
                goto object_end;
            default:
                goto fail;
        }
object_end:
        if (stack->depth == 0) {
            // Extra closing curly bracket in current segment.
            stack->extra_closing_offset[stack->extra_closing_count++] = tape_pos;
            write_object(tape_pos);
            append_content(tape_pos, idx_offset - 1);
            ++tape_pos;
            goto unknown_continue;
        } else {
            --stack->depth;
            left_tape_idx = stack->scope_offset[stack->depth];
            right_tape_idx = tape_pos++;
            write_object(left_tape_idx, right_tape_idx);
            //@formatter:off
            goto *stack->ret_address[stack->depth];
            //@formatter:on
        }

array_begin:
        next_char();
        if (ch == ']') goto array_end;
array_value:
        PARSE_VALUE(&&array_continue);
array_continue:
        next_char();
        switch (ch) {
            case ',':
                next_char();
                goto array_value;
            case ']':
                goto array_end;
            default:
                goto fail;
        }
array_end:
        if (stack->depth == 0) {
            // Extra closing square bracket in current segment.
            stack->extra_closing_offset[stack->extra_closing_count++] = tape_pos;
            write_array(tape_pos);
            append_content(tape_pos, idx_offset - 1);
            ++tape_pos;
            goto unknown_continue;
        } else {
            --stack->depth;
            left_tape_idx = stack->scope_offset[stack->depth];
            right_tape_idx = tape_pos++;
            write_array(left_tape_idx, right_tape_idx);
            //@formatter:off
            goto *stack->ret_address[stack->depth];
            //@formatter:on
        }

fail:
        ParaJson::__error("unexpected character when parsing value", input, idx);
succeed:
        if (idx_offset != idx_end) ParaJson::__error("excessive characters at end of input", input, idx);
        *tape_end = tape_pos;

#undef next_char
#undef PARSE_VALUE
    }

    size_t Tape::print_json(size_t tape_idx, size_t indent) {
        uint64_t section = tape[tape_idx];
        switch (section & TYPE_MASK) {
            case TYPE_NULL:
                printf("null");
                return 1;
            case TYPE_FALSE:
                printf("false");
                return 1;
            case TYPE_TRUE:
                printf("true");
                return 1;
            case TYPE_STR:
                printf("\"%s\"", literals + (section & VALUE_MASK));
                return 1;
            case TYPE_INT:
                printf("%lld", static_cast<long long int>(numeric[section & VALUE_MASK]));
                return 1;
            case TYPE_DEC:
                printf("%.10lf", plain_convert(static_cast<long long int>(numeric[section & VALUE_MASK])));
                return 1;
            case TYPE_ARR: {
                size_t elem_idx = tape_idx + 1;
                bool first = true;
                printf("[");
                assert((section & VALUE_MASK) > tape_idx);
                while (elem_idx < (section & VALUE_MASK)) {
                    if (first) first = false; else printf(",");
                    printf("\n");
                    print_indent(indent + 2);
                    elem_idx += print_json(elem_idx, indent + 2);
                    if ((tape[elem_idx] & TYPE_MASK) == TYPE_JUMP) {
                        // Skip jumps at the end of each value.
                        // Otherwise, this case will fail:  [ value JUMP ]
                        elem_idx += tape[elem_idx] & VALUE_MASK;
                    }
                }
                assert(elem_idx == (section & VALUE_MASK) && tape_idx == (tape[elem_idx] & VALUE_MASK)
                       && (tape[elem_idx] & TYPE_MASK) == TYPE_ARR);
                printf("\n");
                print_indent(indent);
                printf("]");
                return elem_idx + 1 - tape_idx;
            }
            case TYPE_OBJ: {
                size_t elem_idx = tape_idx + 1;
                bool first = true;
                printf("{");
                assert((section & VALUE_MASK) > tape_idx);
                while (elem_idx < (section & VALUE_MASK)) {
                    if (first) first = false; else printf(",");
                    printf("\n");
                    print_indent(indent + 2);
                    elem_idx += print_json(elem_idx, indent + 2);
                    printf(": ");
                    elem_idx += print_json(elem_idx, indent + 2);
                    if ((tape[elem_idx] & TYPE_MASK) == TYPE_JUMP) {
                        // Skip jumps at the end of each value.
                        // Otherwise, this case will fail:  { str : value JUMP }
                        elem_idx += tape[elem_idx] & VALUE_MASK;
                    }
                }
                assert(elem_idx == (section & VALUE_MASK) && tape_idx == (tape[elem_idx] & VALUE_MASK)
                       && (tape[elem_idx] & TYPE_MASK) == TYPE_OBJ);
                printf("\n");
                print_indent(indent);
                printf("}");
                return elem_idx + 1 - tape_idx;
            }
            case TYPE_JUMP: {
                size_t offset = (section & VALUE_MASK);
                return print_json(tape_idx + offset, indent) + offset;
            }
            default:
                throw std::runtime_error("unexpected element on tape");
        }
    }

    void Tape::print_tape() {
        for (size_t i = 0; i < tape_size; ++i) {
            printf("[%3lu] ", i);
            uint64_t section = tape[i];
            switch (section & TYPE_MASK) {
                case TYPE_NULL:
                    printf("null\n");
                    break;
                case TYPE_FALSE:
                    printf("false\n");
                    break;
                case TYPE_TRUE:
                    printf("true\n");
                    break;
                case TYPE_STR:
                    printf("string: \"%s\"\n", literals + (section & VALUE_MASK));
                    break;
                case TYPE_INT:
                    printf("integer: %lld\n", static_cast<long long int>(numeric[section & VALUE_MASK]));
                    break;
                case TYPE_DEC:
                    printf("decimal: %lf\n", plain_convert(static_cast<long long int>(numeric[section & VALUE_MASK])));
                    break;
                case TYPE_ARR:
                    printf("array: %llu\n", (section & VALUE_MASK));
                    break;
                case TYPE_OBJ:
                    printf("object: %llu\n", (section & VALUE_MASK));
                    break;
                case TYPE_JUMP:
                    printf("jump offset: %llu\n", (section & VALUE_MASK));
                    break;
                default:
                    printf("unknown: type = %llu, value = %llu\n", (section & TYPE_MASK), (section & VALUE_MASK));
//                    throw std::runtime_error("unexpected element on tape");
                    break;
            }
        }
    }

    inline bool __is_opening_bracket(char ch) {
        return ch == '{' || ch == '[';
    }

    inline bool __is_closing_bracket(char ch) {
        return ch == '}' || ch == ']';
    }

    inline bool __is_separator(char ch) {
        return ch == ',' || ch == ':';
    }
    
    inline bool __is_non_structural(char ch) {
        return !(__is_opening_bracket(ch) || __is_closing_bracket(ch) || __is_separator(ch));
    }

    void Tape::state_machine(char *input, size_t *idx_ptr, size_t structural_size, int chunk_size) {
        if (structural_size == 1)
            __error("emtpy string is not valid JSON", input, 0);
        
        parlay::parallel_for(0, structural_size, [&](int i) {
            Tape::_thread_parse_str_parlay(i, input, idx_ptr);
        });
        literals = input;

        size_t num_chunks = (structural_size + chunk_size - 1) / chunk_size;
        std::cout << "num_chunks: " << num_chunks << std::endl;
        TapeStack stack[num_chunks];
        size_t tape_ends[num_chunks];
        size_t idx_splits[num_chunks + 1];

        parlay::parallel_for(0, num_chunks + 1, [&](int i) {
            idx_splits[i] = (structural_size - 1) * i / num_chunks;
        });

        parlay::parallel_for(0, num_chunks, [&](int i) {
            size_t idx_begin = idx_splits[i];
            size_t idx_end = idx_splits[i + 1];
            Tape::_thread_state_machine(input, idx_ptr, idx_begin, idx_end, &stack[i], &tape_ends[i], i != 0);
        });

        size_t merge_stack[kMaxDepth];
        size_t top = 0;

        for (int i = 0; i < stack[0].depth; ++i)
            merge_stack[top++] = stack[0].scope_offset[i];

        for (int pid = 1; pid < num_chunks; ++pid) {
            // Verify grammar correctness checks for cross-boundary input.
            // This is to make sure structural characters that are not stored on tape (, and :) are properly inserted
            // between segments, i.e. the following cases should fail when running with 2 threads:
            //  No.  Reason                    1st Thread         2nd Thread
            //   1.  Missing colon (:)         { "1": 2, "3"      4, "5": 6 }
            //   2.  Missing comma (,)         [ 1, 2             3, 4]
            //   3.  Missing comma (,)         [ [ 1, 2 ]         [ 3, 4 ] ]
            //   4.  Extra colon (:)           { "1": 2, "3":     : 4, "5": 6 }
            //   5.  Extra comma (,)           [ 1, 2,            , 3, 4 ]
            //   6.  Extra kv-pair in object   { "1": 2, "3":     "4": 5, "5": 6 }
            //   7.  Array value in object     { "1": 2,          "3", "5": 6 }
            size_t pos = idx_splits[pid];
            size_t idx = idx_ptr[pos];
            if (pos >= 1 && pos < structural_size) {
                char left_char = input[idx_ptr[pos - 1]], right_char = input[idx];
                if ((__is_non_structural(left_char) || __is_closing_bracket(left_char))
                    && (__is_non_structural(right_char) || __is_opening_bracket(right_char)))  // case 1, 2, 3
                    ParaJson::__error("expected separator", input, idx);
                if (__is_separator(left_char) && __is_separator(right_char))  // cases 4 & 5
                    ParaJson::__error("extra separator", input, idx);
            }
            if (top > 0) {
                bool in_object = (tape[merge_stack[top - 1]] & TYPE_MASK) == TYPE_OBJ;
                for (size_t right_pos = pos; right_pos <= pos + 1; ++right_pos) {
                    if (right_pos >= 2 && right_pos < structural_size) {
                        char left_char = input[idx_ptr[right_pos - 2]], right_char = input[idx_ptr[right_pos]];
                        if (left_char == ':' && right_char == ':')  // case 6
                            ParaJson::__error("extra colon (:)", input, idx_ptr[right_pos]);
                        if (in_object && left_char == ',' && right_char == ',')
                            ParaJson::__error("non key-value pair in object", input, idx_ptr[right_pos]);
                    }
                }
            }

            TapeStack &cur_stack = stack[pid];
            for (int i = 0; i < cur_stack.extra_closing_count; ++i) {
                size_t right_tape_idx = cur_stack.extra_closing_offset[i];
                size_t right_input_idx = idx_ptr[tape[right_tape_idx] & VALUE_MASK];
                if (top == 0) ParaJson::__error("unmatched closing bracket", input, right_input_idx);
                size_t left_tape_idx = merge_stack[--top];
                if ((tape[left_tape_idx] & TYPE_MASK) != (tape[right_tape_idx] & TYPE_MASK))
                    ParaJson::__error("matching brackets have different types", input, right_input_idx);
                write_content(right_tape_idx, left_tape_idx);
                write_content(left_tape_idx, right_tape_idx);
            }
            for (int i = 0; i < cur_stack.depth; ++i)
                merge_stack[top++] = cur_stack.scope_offset[i];
        }

        if (top > 0) throw std::runtime_error("unmatched opening brackets");
        if (size_t pos = idx_ptr[idx_splits[num_chunks] - 1]; input[pos] == ',')
            ParaJson::__error("extra separator", input, pos);
        for (int i = 0; i < num_chunks - 1; ++i) {
            size_t idx_begin_next = idx_splits[i + 1];
            if (tape_ends[i] < idx_begin_next) write_jump(tape_ends[i], idx_begin_next);
        }
        tape_size = tape_ends[num_chunks - 1];

        // parlay::parallel_for(0, structural_size, [&](int i) {
        //     Tape::_thread_parse_num_parlay(i, input, idx_ptr);
        // });
        
        print_tape();
        print_json();
        printf("\n");
    }
}
