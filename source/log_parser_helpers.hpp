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
     * Convert a string to a unsigned int
     *
     * 
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
    // auto str_to_uint(std::string_view field, unsigned* dist_to_first_non_int_char=nullptr) const -> std::optional<unsigned> ;

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
     * Name/ID/Instance is a combination used to identify actors that can have multiple copies - the combination
     * identifies the unique actor in the combat. This generally means an actor is a non-unique enemy or a player
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
     * Parse the actor of a source/target field
     *
     * This parses the name and identifier(s) of the actor of a source field or, technically, the object of a target
     * field. There are three possible formats to this field, one each for a PC, NPC, and PC's companion.
     *
     * <ol>
     * <li> PC: `"@name#id"`
     * <li> Companion: `"PC/comp_name {comp_id}:comp_instance"`. `PC` is as per the `PC` case, although when part of a
     *      Companion, it may be `@UNKNOWN`. I don't know why.
     * <li> NPC: `"name {id}:instance}"`
     * </ol>
     *
     * For a PC, `name` is a string and `id` as parsed by `parse_name_and_id()`.
     *
     * For a PC's companion, `pc_name` and `pc_id` are parsed as a PC and "comp_name {comp_id}:comp_instance" is parsed
     * as per `parse_name_id_instance()`.
     *
     * For an NPC, "name {id}:instance" is parsed per `parse_name_id_instance()`.
     *
     * @param[in] field view to be parsed
     *
     * @returns On success, an optional populated with an actor object; on failure, an empty optional.
     */
    auto parse_source_target_actor(std::string_view field) const -> std::optional<LogParserTypes::Actor>;

    /**
     * Parse a source or target (s/t) field
     *
     * A s/t field has the following format (BNF):
     *
     * actor '|(' location ')|(' health ')'
     *
     * Where:
     *
     * <ul>
     * <li> `actor` is in the format parsed by `parse_source_target_actor()`
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
     * Parse the mitigation effect subfield of the value field
     *
     * The mitigation effect has this form:
     *
     * '(' effect_value? effect_name_id? ')'
     *
     * The parentheses should NOT be included in view parameter
     *
     * <ul>
     * <li> `effect_value`: amount mitigated
     * <li> `effect_name_id`: the type of mitigation effect, such as "absorbed" or "reflected"
     * </ul>
     *
     * @param[in] field view to parse
     *
     * @returns On success, a true-evaluating optional populated with a MitigationEffect; on failure, a false-evaluating
     * optional.
     */
    auto parse_mitigation_effect(std::string_view field) const -> std::optional<LogParserTypes::MitigationEffect>;

    /**
     * Parse the value field
     *
     * The value field is optional. It's present when the action has some associated value, like damage, healing,
     * gaining/losing charges, etc. This is the most complicated field in the log but it has these basic concepts:
     *
     * <ul>
     * <li> Some value is changing, such as damage taken, heals received, charges expended, etc.
     * <li> The value might be as the result of a critical hit or heal
     * <li> Some of the damage or heal might not have been applied, perhaps due to absorption or overhealing
     * <li> Damage might have an associated modality, like kinetic or internal
     * <li> Damage might mitigated for some reason
     * <li> Damage mitigation might have another effect, like reflect
     * </ul>
     *
     * @note: There is a special log entry that's used when the PC enters an area that is unlike any other entry
     * format. In this case, the value will be something like the string "he3001." I believe this might stand for "hero
     * engine 3.0.0.1," but that's just a guess. Therefore, the return type is a std::variant to handle this one-off case
     * and then the more standard value format.
     *
     * Many concepts are combined in this single field so that almost all parts of it are optional. Here are the components:
     *
     * <ol>
     * <li> Base value: This is the only required field, something like the amount of damage or healing. This may not,
     *      however, be the value that's applied to the target.
     * <li> Critical: This indicates that the base value was a critical hit.
     * <li> Effective value: This is the value that was actually applied to the target after the result of mitigation.
     * <li> Value type: One type of value, damage, has various types that describe what kind of damage it is, such as
     *      "elemental" or "internal" or "kinetic." The value type triggers some behaviors, such as an ability proc'ing
     *      based on damage type or perhaps damage being mitigated based on type. If this isn't present for a damage value, 
     *      then the damage is called "typeless."
     * <li> Mitigation reason: The damage might be reduced and if so this reason describes why. This typically refers to
     *      an ability the target has to reduce damage, such as a "shield" or maybe a "parry."
     * <li> Mitigation effect: This is what happened when the damage was mitigated. The most common forms are
     *      absorption, meaning the damage was mitigated by some ability (shield, sage bubble, etc.), or reflection,
     *      where the damage was reflect (applied) back to the source. The mitigation effect can contain the amount of
     *      the effect (the effect's value) and what the effect was.
     * </ol>
     *
     * Here is a Python-compatible regex that describes the entire field, with named subfields:
     *
     * `(?P<base_val>...)\*? (~(?P<eff_val>...))? (-(?P<mit_reason>...)?)? (\(?P<mit_effect>...)\)?`
     *
     * @note: An effective value is always prefixed by a tilde `~`. The mitigation reason must be prefixed by a dash
     * `-`-. The mitigation effect must be surrounded by parentheses `()`. 
     *
     * Details:
     *
     * <ul>
     * <li> name: \w+
     * <li> id: \d+
     * <li> fp: A restricted form of a floating-point: `\d+(\.\d+)?`
     * <li> Base value: `fp`
     * <li> Effective value: `fp`
     * <li> Mitigation reason: `name? {id}`
     * <li> Mitigation effect: `(P<mit_eff_value>fp)? (P<mit_eff_type>name? {id})?`
     * </ul>
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
