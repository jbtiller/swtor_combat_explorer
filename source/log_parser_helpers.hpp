// -*- fil-column: 120; indent-tabs-mode: nil -*-
#pragma once

#include <string_view>
#include <optional>
#include <cstdint>

#include "log_parser_types.hpp"

class LogParserHelpers {
public:
    auto set_line_num(int line_num) -> void {
	m_line_num = line_num;
    }

    /**
     * Convert a string to a uint64
     *
     * Relies on cstdlib `strtoul()` with additional checks to trigger failure if no integer is found.
     *
     * @param[in] field String to be parsed for an integer
     * @param[out] dist_to_first_non_int_char [optional] On success, populated with the number of steps to reach the
     *     first character past the integer, aka the index of the first non-integer character.
     * 
     * @returns Optional that wraps a uint64 on success or empty optional in the following conditions: <ol>
     *   <li> `strtoul()` sets errno to non-zero
     *   <li> `strtoul()` returns ULONG_MAX
     *   <li> `strtoul()` sets its out-param `endptr` to the beginning of the input `field`. This is
     *   an additional check to catch emptry strings or strings only of whitespace.
     * </ol>
     */
    auto str_to_uint64(std::string_view field, uint64_t* dist_to_first_non_int_char=nullptr) const -> std::optional<uint64_t> ;

    /**
     * Convert a string to a double
     *
     * Relies on cstdlib strtod() with additional checks to trigger failure if no double is found.
     *
     * @param[in] field String to be parsed for an double
     * @param[out] dist_to_first_non_double_char [optional] On success, populated with the number of steps to reach the
     *     first character past the double, aka the index of the first non-double character.
     * 
     * @returns Optional that wraps a double on success or empty optional in the following conditions: <ol>
     *   <li> `strtod` sets errno to non-zero and returns 0.0
     *   <li> `strtod` sets errno to non-zero and returns HUGE_VAL
     *   <li> `strtod` sets its out-param `endptr` to the beginning of the input `field`. This is
     *       an additional check to catch emptry strings or strings only of whitespace.
     *   <li> the double has nothing beyond the decimal point (e.g. "1."). This form is not useful in our context.
     * </ol>
     */
    auto str_to_double(std::string_view field, uint64_t* dist_to_first_non_double_char=nullptr) const -> std::optional<double> ;

    /**
     * Remove characters from the beginning of a string
     *
     * @param[in] field strip this string
     * @param[in] to_remove strip each of these characters. If not provided, remove spaces and tabs.
     *
     * @returns A {@code string_view} into the input {@code field} with the initial matching characters removed. This
     *     means that the input and return both depend on the original string.
     */
    auto lstrip(std::string_view field, std::string_view to_remove = " 	") const -> std::string_view;

    /**
     * Remove characters from the end of a string
     *
     * @param[in] field strip this string
     * @param[in] to_remove strip each of these characters. If not provided, remove spaces and tabs.
     *
     * @returns A {@code string_view} into the input {@code field} with the trailing matching characters removed. This
     *     means that the input and return both depend on the original string.
     */
    auto rstrip(std::string_view field, std::string_view to_remove = " 	") const -> std::string_view;

    /**
     * Remove characters from the beginning and end of a string
     *
     * @param[in] field strip this string
     * @param[in] to_remove strip each of these characters. If not provided, remove spaces and tabs.
     *
     * @returns A {@code string_view} into the input {@code field} with the initial and trailing matching characters
     *     removed. This means that the input and return both depend on the original string.
     */
    auto strip(std::string_view field, std::string_view to_remove = " 	") const -> std::string_view;

    /**
     * Find next delimited field
     *
     * Search input to find a string surrounded by the provided delimiters. Will return the outermost string in the
     * case of nested delimiters. Ensures nested delimiters are balanced.
     *
     * @param[in] line search in here for field
     * @param[in] begin_delim opening delimiter
     * @param[in] end_delim closing delimiter
     * @param[out] dist_beyond_field_char If return evaluates to true, populated with the number of steps to reach the
     *     first character after the closing delimiter; otherwise unchanged.
     *
     * @returns optional populated with view into {@code line} that is the found field, not including delimiters;
     *     optional evaluates to false if opening or closing delimiter is missing or if delimiters aren't balanced in
     *     the case of nesting.
     *
     * @note An empty field is considered success.
     */
    auto get_next_field(std::string_view line, char begin_delim, char end_delim,
			uint64_t* dist_beyond_field_char=nullptr) const -> std::optional<std::string_view>;

    // More to do here - pass in timestamp handling class. not sure what ot return...
    // auto parse_timestamp(std::string_view field) -> std::optional

    /**
     * Parse source/target location
     *
     * Location has this form: "(x,y,z,w)" and represents a 3D spatial position and 1D rotation for facing. Coordinates
     * are doubles and are parsed using str_to_double(). Assumes the outer surrounding field delimiters have been
     * removed.
     *
     * @param[in] field view containing location to parse
     *
     * @returns If input view is in correct format, an optional populated with Location corresponding to the parsed
     *     location coordinates. Else returns an uninitialized optional.
     *
     * @note Ignores leading whitespace and all non-double characters trailing the 'w' component.
     */
    auto parse_st_location(std::string_view field) const -> std::optional<LogParserTypes::Location>;

    /**
     * Parse source/target health
     *
     * Health has this form: "(current/total)". Both are integers and are parsed using str_to_uint64(). Assumes
     * surrounding delimiteres have been removed.
     *
     * @param[in] field view containing health to parse
     *
     * @returns If input view is in correct format, an optional populated with Health corresponding to the parsed
     *     health values. Else returns an uninitialized optional.
     *
     * @note Ignores leading whitespace and all non-double characters trailing the 'total' component.
     */
    auto parse_st_health(std::string_view field) const -> std::optional<LogParserTypes::Health>;

    /**
     * Parse a name string and its associated numeric identifier from the given view
     *
     * The name and ID are assumed to be in this form: `"name {id}"`.
     *
     * `name` is the string that starts at the beginning of the input and ends before the opening identifier delimiter;
     * surrounding whitespace will be stripped. An empty `name` is allowed.
     *
     * `id` is an integer identifier parsed from the string between the delimiters using `str_to_uint64()`. Unlike `name`,
     * it is an error if `id` is missing.
     *
     * @param[in] field view to be parsed
     * @param[out] dist_beyond_field_delim If non-null will be populated with the number of steps to move to the
     *     character past the closing delimiter, essentially the index of the first character following the delimiter.
     *     Unchanged if return evaluates to false.
     *
     * @returns An optional populated with a valid NameId instance else an empty optional. 
     */
    auto parse_name_and_id(std::string_view field, uint64_t* dist_beyond_field_delim=nullptr) const -> std::optional<LogParserTypes::NameId>;

    /**
     * Parse a string name, numeric ID, and numeric instance from a string
     *
     * Name/ID/Instance is a combination used to identify subjects that can have multiple copies - the combination
     * identifies the unique subject in the combat. This generally means a subject is a non-unique enemy or a player
     * companion.
     *
     * The format of this is: `"name {id}:instance"`. `name` and `id` are parsed as in `parse_name_and_id()`; `instance`
     * is parsed as `str_to_uint64()`. For the parse to be successful, `name` may be absent but `id` and `instance` are
     * required.
     *
     * @param[in] field view to be parsed
     *
     * @returns On success, an optional populated with a NameIdInstance object; on failure, an empty optional.
     */
    auto parse_name_id_instance(std::string_view field) const -> std::optional<LogParserTypes::NameIdInstance>;

    /**
     * Parse the subject of a source/target field
     *
     * This parses the name and identifier(s) of the subject of a source field or, technically, the object of a target
     * field. There are three possible formats to this field, one each for a PC, NPC, and PC's companion.
     *
     * <ol>
     * <li> PC: `"@name#id"`
     * <li> Companion: `"@pc_name#pc_id/comp_name {comp_id}:comp_instance"`
     * <li> NPC: `"name {id}:instance}"`
     * </ol>
     *
     * For a PC, `name` is a string and `id` is parsed as `str_to_uint64()`. `name` can be empty.
     *
     * For a PC's companion, `pc_name` and `pc_id` are parsed as a PC and "comp_name {comp_id}:comp_instance" is parsed
     * as per `parse_name_id_instance()`. `pc_name` can be empty.
     *
     * For an NPC, "name {id}:instance" is parsed per `parse_name_id_instance()`.
     *
     * @param[in] field view to be parsed
     *
     * @returns On success, an optional populated with a Subject object; on failure, an empty optional.
     */
    auto parse_source_target_subject(std::string_view field) const -> std::optional<LogParserTypes::Subject>;

    /**
     * Parse a source or target (s/t) field
     *
     * A s/t field has the following format (BNF):
     *
     * subject '|(' location ')|(' health ')'
     *
     * Where:
     *
     * <ul>
     * <li> `subject` is in the format parsed by `parse_source_target_subject()`
     * <li> `location` is in the format parsed by `parse_st_location()`
     * <li> `health` is in the format parsed by `parse_st_health()`
     * </ul>
     *
     * All three fields must be present.
     *
     * @param field view to parse
     *
     * @returns An optional that evalutes to true, populated with SourceOrTarget, if parsed successfully, else an
     *     optional that evaluates to false.
     *
     * @note All three fields must be present for the "source" field. However, the caller should handle 3 allowable
     *     conditions when they're parsing the "target" field before calling this function:
     *
     * @note <ol>
     * <li> The field is empty. This indicates the target isn't relevant.
     * <li> The field is exactly `"="`. This indicates that the target is the same as the source and is just a
     *     shorthand.
     * <li> The field is parsed as the "source" described above.
     * </ol>
     *
     * @note This function assumes that the field is parsed per the "source" requirements and doesn't handle the other
     * conditions.
     */
    auto parse_source_target_field(std::string_view field) const -> std::optional<LogParserTypes::SourceOrTarget>;

    /**
     * Parse the ability field
     *
     * The ability field has the same format as name and ID (NameId): `"name {id}"`.
     *
     * @param[in] view to be parsed
     *
     * @returns On success, an optional that evaluates to true populated with an Ability object; on failure, an optional
     *     that evaluates to false.
     */
    auto parse_ability_field(std::string_view field) const -> std::optional<LogParserTypes::Ability>;

    /**
     * Parse the action field
     *
     * The action field has multiple formats and a combation of name/id not seen anywhere else:
     *
     * <ol>
     * <li> Empty. This case is not handled here - handle it in the caller. It's an error here.
     * <li> `name {id}: name {id}`. This is not a name/id/instanced combination - it's two name/id separated by a colon.
     * <li> `name {id}: name {id}/name {id}`. Generally used to indicate a PC has a certain class and spec.
     * <li> `name {id}: name {id} name {id}`. Generally used to indicate a PC has entered an area that has a difficulty
     *     rating associated with it.
     * </ol>
     *
     * Examples:
     *
     * `ApplyEffect {123}: Heal {234}`
     * `DisciplineChanged {123}: Guardian {234}/Defense {345}`
     * `AreaEntered {123}: R-4 Anomaly {234} 8 Player Story {345}`
     *
     * Expressing it as a pseudo-regex:
     *
     * `verb: noun([/ ]detail)?`
     *
     * Thus, the `verb` (`name {id}`) and `noun` (`name {id}`) are always present, but the `detail` (also `name {id}`)
     * is optional. This function doesn't interpret the meaning of the verb/noun, so it doesn't assume the `detail` is
     * required for any specific `verb`; the caller should enforce this logic.
     *
     * @param[in] view to be parsed
     *
     * @returns If properly formatted, an optional that evaluates to true and is populated with an Action instance; else
     *     an empty optional that evaluates to false.
     */
    auto parse_action_field(std::string_view field) const -> std::optional<LogParserTypes::Action>;

    /**
     * Parse the value field
     *
     * The value field is optional. It's present when the action has some associated value, like damage, healing,
     * charges, etc. This is the most complicated field in the log but it has these basic concepts:
     *
     * <ul>
     * <li> Some value is changing, such as damage taken, heals received, charges expended, etc.
     * <li> The value might be as the result of a critical hit or heal
     * <li> Some of the damage or heal might not have been applied, perhaps due to absorption or overhealing
     * <li> Damage might have an associated modality, like kinetic or internal
     * <li> Damage might be avoided entirely due to an ability
     * <li> Damage might simply be 0
     * </ul>
     *
     * Many concepts are combined in this single field, which is why it has so many formats:
     *
     * <ol>
     * <li> `"he3001"`. No idea what this is but it's only present on AreaEntered, like a version
     * <li> `"9608"`. A simple value
     * <li> `"0"`. Special case - 0 damage hit or perhaps overhealing at full health.
     * <li> `"3107*"`. A critical hit or heal
     * <li> `3.0`. A double, but always representing an integer. Something to do with charges, etc.
     * <li> `5894 energy {123}`. For damage, this represents the type of damage. Can also be various kinds of points or
     *      charges, like "1 charges {123}". Any combination of base damage, crit, or mitigation can have a type
     *      associated with it.
     * <li> `13315 ~9902`. Some action had a base value of `13315` but only `9902` was applied due to mitigation
     *      (absorb, etc.). This might also be a critical hit (`13315*`).
     * <li> `248 ~0 energy {123} -`. Damage received was absorbed by something but it can't tell us. Probably a bug in
     *      the logging code, to be honest.
     * <li> `4012 ~2809 energy {123} (1204 absorbed {234}`. Damage received was absorbed by something other than a
     *      shield.
     * <li> `25708 kinetic {123}(reflected {234})`. I think this means the damage to the target was fully reflected back
     *      to the target? Such as: "[pc] [enemy] [saber reflect {...}] [apply: dmg] (10 kinetic {...}(reflected...)"
     * <li> `4012 ~2809 energy {123} -shield {234} (1204 absorbed {345}`. Damage received was absorbed by the target's
     *      shield.
     * <li> `0 -parry {123}`. Incoming damage was completely mitigated, in this case by a "parry," most likely a passive
     *      ability.
     * <li> `0 -`. Incoming damage was completely mitigated, but no reason given.
     *
     * The return value is also somewhat complex, unfortunately, but it really boils down to holding this information:
     *
     * <ol>
     * <li> The base value and if it's a critical hit
     * <li> The effective value actually applied due to mitigation
     * <li> The modality (`detail`) of the damage
     * <li> How much damage was absorbed
     * <li> Why the damage was absorbed
     * <li> Mitigation reason if all damage was avoided
     * </ol>
     *
     * There is a special log entry that's used logged when the PC enters an area that is unlike any other entry
     * format. Therefore, the return type is a std::variant to handle this one-off case and then the more standard value
     * format.
     *
     * @param[in] view to be parsed
     *
     * @returns On success, an optional that evaluates to false and is populated with a `Value` object; on failure, an
     *     optional that evaluates to false.
     */
    auto parse_value_field(std::string_view field) const -> std::optional<LogParserTypes::Value>;

    /**
     * Parse the threat field
     *
     * The threat field, if present, almost always consists only of a double surrounded by angle brackets. However, in
     * the special case of "AreaEntered", this field is repurposed to act as some kind of string version.
     *
     * <ol>
     * <li> `45.0`. The current threat.
     * <li> `v7.0.0b`. Some random version, perhaps of the log format.
     * </ol>
     *
     * @param[in] field view to parse. Should not include delimiters.
     *
     * @returns If parse succeeds, the current threat that the source has towards the target or some version string;
     * else, the optional evaluates to false.
     */
    auto parse_threat_field(std::string_view field) const -> std::optional<LogParserTypes::Threat>;

private:
    // Used by all parsing functions to populate logging messages.
    int m_line_num {0};
}; // class LogParserHelpers
