#pragma once

// Contains declarations for pqxx-specific custom SQL type converters for setting and getting values of custom
// types. Custom type converters are presented to pqxx with template classes that contain specific member functions that
// pqxx knows how to use.

#include <cmath>
#include <iostream>
#include <string_view>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wall"
#include <pqxx/pqxx>
#pragma GCC diagnostic pop

#include "log_parser_types.hpp"

// This tells pqxx how to handle the "Location" type.
namespace pqxx {
    // Provide a name for error messages and the like.
    template<> inline const std::string_view type_name<LogParserTypes::Location> {"Actor location"};
    // There is no natural NULL value for this type, so we just use the SQL NULL.
    template<> struct nullness<LogParserTypes::Location> : no_null<LogParserTypes::Location> {
        [[nodiscard]] inline static constexpr bool is_null(const LogParserTypes::Location&) noexcept { return false; }
    };
    // This class contains the functions and variables that pqxx uses to convert the type to/from SQL strings.
    template<> struct string_traits<LogParserTypes::Location> {
        // Yes, this type can be converted to/from a string.
        static constexpr bool converts_to_string {true};
        static constexpr bool converts_from_string {true};
        // Allocate a string of this many bytes to hold the conversion.
        static constexpr auto size_buffer(const LogParserTypes::Location&) noexcept -> size_t {
            //           1         2
            // 012345678901234567890123456789
            // (-111.1,-222.2,-333.3,-444.4)
            return 35;
        }

        // Convert the C++ type into a SQL string. Populate buffer and return zview into the buffer. The final character
        // of the zview must be followed by a null terminator.
        static auto to_buf(char* begin, char* end, const LogParserTypes::Location& loc) -> zview {
            char* beyond = into_buf(begin, end, loc);
            // Constructs a std::string_view from a C-string.
            return begin;
        }

        // Convert the C++ type into a SQL string. Populated buffer must be null-terminated. Must return a pointer that
        // points to the character beyond the null terminator. Similar to to_buf() but more strict.
        //
        // Note: There are many conflicting sources about how the string form should appear. For example, "ROW(...)" and
        // "(...)" are supposedly synonymous, the latter being syntactic sugar for the former, but this isn't allowed in
        // the string form in libpqxx - it triggers a "malformed literal" error and complains about not seeing the
        // opening parenthesis. In addition, appending the datatype via "::Location" is shown in multiple places but
        // also isn't allowed and also produces a malformed literal error complaining about "junk" after the right
        // parenthesis. So, ignore all documentation - just do this: (v1,v2,v3,...,vN).
        static auto into_buf(char* begin, char* end, const LogParserTypes::Location& loc) -> char* {
            assert((end - begin) >= size_buffer(loc));
            auto len = snprintf(begin, size_buffer(loc), "(%.1f,%.1f,%.1f,%.1f)",
                                loc.x.val(), loc.y.val(), loc.z.val(), loc.rot.val());
            if (len < 0) {
                throw conversion_overrun("Overflow buffer for converting Location type");
            }
            return begin + len + 1;
        }

        // Convert the SQL string into the C++ type.
        //
        // Note that in the results we get back from a query of this type, we'll never see the explicit row constructor
        // (ROW::) or the appended data type (::Location).
        static auto from_string(std::string_view loc_str) -> LogParserTypes::Location {
            if (loc_str.front() != '(' || loc_str.back() != ')') {
                throw conversion_error("string_traits<Location>::from_string: Value not surrounded by parentheses");
            }
            if (std::count(loc_str.begin(), loc_str.end(), ',') != 3) {
                throw conversion_error("string_traits<Location>::from_string: Need 3 comma separators");
            }
            LogParserTypes::Location loc;
            double* comps[4] = {&loc.x, &loc.y, &loc.z, &loc.rot};
            std::string_view::size_type comp_pos[4];
            comp_pos[0] = 1;
            comp_pos[1] = loc_str.find(',', comp_pos[0] + 1) + 1;
            comp_pos[2] = loc_str.find(',', comp_pos[1] + 1) + 1;
            comp_pos[3] = loc_str.find(',', comp_pos[2] + 1) + 1;
            for (int i = 0; i < 4; ++i) {
                char* end {};
                const char* pos = &loc_str[comp_pos[i]];
                auto l = std::strtod(pos, &end);
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wfloat-equal"
                // This comparison is acceptable because strtod returns this exactly HUGE_VAL in this error condition.
                if (l == HUGE_VAL) {
                    throw conversion_error("string_traits<Location>::from_string: Component is outside allowable range");
                }
                // This comparison is acceptable because strtod returns exactly 0.0 in this error condition.
                else if (l == 0.0 && end == pos) {
#pragma GCC diagnostic pop
                    throw conversion_error("string_traits<Location>::from_string: Component is not a valid float");
                } else {
                    *comps[i] = l;
                }
            }
            return loc;
        }
    };

    // This tells pqxx how to handle the "Health" type.

    // Provide a name for error messages and the like.
    template<> inline const std::string_view type_name<LogParserTypes::Health> {"Actor health"};
    // There is no natural NULL value for this type, so we just use the SQL NULL.
    template<> struct nullness<LogParserTypes::Health> : pqxx::no_null<LogParserTypes::Health> {
        [[nodiscard]] inline static constexpr bool is_null(const LogParserTypes::Health&) { return false; }
    };
    // This class contains the functions and variables that pqxx uses to convert the type to/from SQL strings.
    template<> struct string_traits<LogParserTypes::Health> {
        // Yes, this type can be converted to/from a string.
        static constexpr bool converts_to_string {true};
        static constexpr bool converts_from_string {true};
        // Allocate a string of this many bytes to hold the conversion.
        static constexpr auto size_buffer(const LogParserTypes::Health&) noexcept -> size_t {
            //           1    
            // 012345678901234
            // (-111.1,-222.2)
            return 25;
        }

        // Convert the C++ type into a SQL string. Populate buffer and return zview into the buffer. The final character
        // of the zview must be followed by a null terminator.
        static auto to_buf(char* begin, char* end, const LogParserTypes::Health& loc) -> zview {
            char* beyond = into_buf(begin, end, loc);
            // Constructs a std::string_view from a C-string.
            return begin;
        }

        // Convert the C++ type into a SQL string. Populated buffer must be null-terminated. Must return a pointer that
        // points to the character beyond the null terminator. Similar to to_buf() but more strict.
        static auto into_buf(char* begin, char* end, const LogParserTypes::Health& h) -> char* {
            assert((end - begin) >= size_buffer(h));
            auto len = snprintf(begin, size_buffer(h), "(%u,%u)", h.current.val(), h.total.val());
                                
            if (len < 0) {
                throw conversion_overrun("Overflow buffer for converting Health type");
            }
            return begin + len + 1;
        }

        // Convert the SQL string into the C++ type.
        static auto from_string(std::string_view h_str) -> LogParserTypes::Health {
            if (h_str.front() != '(' || h_str.back() != ')') {
                throw conversion_error("string_traits<Health>::from_string: Value not surrounded by parentheses");
            }
            if (std::count(h_str.begin(), h_str.end(), ',') != 1) {
                throw conversion_error("string_traits<Health>::from_string: Need 1 comma separators");
            }
            LogParserTypes::Health h;
            unsigned* comps[2] = {&h.current, &h.total};
            std::string_view::size_type comp_pos[2];
            comp_pos[0] = 1;
            comp_pos[1] = h_str.find(',', comp_pos[0] + 1) + 1;
            for (int i = 0; i < 2; ++i) {
                char* end {};
                const char* pos = &h_str[comp_pos[i]];
                errno = 0;
                auto l = std::strtoul(pos, &end, 10);
                auto en = errno;
                errno = 0;
                // This comparison is acceptable because strtod returns this exactly HUGE_VAL in this error condition.
                if ((l == 0) && en) {
                    throw conversion_error("string_traits<Location>::from_string: Component is not a valid integer");
                } else if ((l == 0) && (end == pos)) {
                    throw conversion_error("string_traits<Location>::from_string: No integer found");
                } else if ((l == ULONG_MAX) && en) {
                    throw conversion_error("string_traits<Location>::from_string: Component is outside allowable range");
                } else {
                    *comps[i] = static_cast<unsigned>(l);
                }
            }
            return h;
        }
    };


} //namespace pqxx
