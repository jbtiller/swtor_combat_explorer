// -*- fil-column: 120; indent-tabs-mode: nil -*-
#include <cmath>
#include <cstdlib>
#include <iomanip>

#include "log_parser_helpers.hpp"
#include <boost/log/trivial.hpp>
#include "log_parser_types.hpp"
#include "logging.hpp"

#define LL(lev) BLT_LINE(lev, m_line_num)
using sv = std::string_view;

constexpr int dec_radix = 10;
constexpr std::string expected_mitigation_type {"absorbed"};

// Just to make typing a bit easier.
using Health = LogParserTypes::Health;
using Action = LogParserTypes::Action;

auto LogParserHelpers::str_to_uint64(sv field, uint64_t* first_non_int_char) const -> std::optional<uint64_t> {
    LL(trace) << "Parsing string " << std::quoted(field) << " as an uint64_t.";

    std::string ul_str = std::string(field);
    char* endptr {nullptr};
    errno = 0;
    uint64_t ret = strtoul(ul_str.data(), &endptr, dec_radix);
    decltype(errno) local_errno = errno;
    if (local_errno != 0) {
        LL(warning) << "String " << std::quoted(ul_str) << " failed to convert to ulong. errno=" << errno
                    << " " << std::quoted(std::strerror(local_errno));
        return {};
    }
    if (ret == ULONG_MAX) {
        LL(warning) << "String " << std::quoted(ul_str) << " doesn't represent a valid ulong. Skipping.";
        return {};
    }
    if (endptr == ul_str.data()) {
	LL(warning) << "Did not encounter an integer character.";
	return {};
    }
    if (first_non_int_char != nullptr) {
        *first_non_int_char = static_cast<uint64_t>(std::distance(ul_str.data(), endptr));
    }
    return ret;
}

auto LogParserHelpers::str_to_double(sv field, uint64_t* first_non_double_char) const -> std::optional<double> {
    LL(trace) << "Parsing string " << std::quoted(field) << " as a double.";

    if (field.empty()) {
	LL(warning) << "Empty string is not a valid uint64.";
	return {};
    }

    std::string field_s = std::string(field);
    auto endptr = field_s.data();
    errno = 0;
    double ret = strtod(field_s.data(), &endptr);
    decltype(errno) local_errno = errno;
#   pragma GCC diagnostic push
#   pragma GCC diagnostic ignored "-Wfloat-equal"
    // This is safe because strtod is guaranteed to return 0.0 or HUGE_VAL if an error occurs.
    if ((ret == 0.0 || ret == HUGE_VAL) && local_errno != 0) {
        LL(warning) << "String " << std::quoted(field_s) << " isn't a valid double. Skipping.";
        return {};
    }
#   pragma GCC diagnostic pop
    if (endptr == field_s.data()) {
	LL(warning) << "Did not encounter a double character. Skipping.";
	return {};
    }
    if (*std::prev(endptr) == '.') {
	LL(warning) << "Double has no integer beyond the decimal point. Skipping.";
	return {};
    }

    if (first_non_double_char != nullptr) {
        *first_non_double_char = static_cast<uint64_t>(std::distance(field_s.data(), endptr));
    }
    return ret;
}

auto LogParserHelpers::lstrip(sv field, sv to_remove) const -> sv {
    auto beg = std::find_if(field.begin(), field.end(), [&to_remove] (auto c) {
        return to_remove.find(c) == sv::npos;
    });
    field.remove_prefix(static_cast<sv::size_type>(std::distance(field.begin(), beg)));
    return field;
}

auto LogParserHelpers::rstrip(sv field, sv to_remove) const -> sv {
    auto end = std::find_if(field.rbegin(), field.rend(), [&to_remove] (auto c) {
        return to_remove.find(c) == sv::npos;
    });
    field.remove_suffix(static_cast<sv::size_type>(std::distance(field.rbegin(), end)));
    return field;
}

auto LogParserHelpers::strip(sv field, sv to_remove) const -> sv {
    return lstrip(rstrip(field, to_remove), to_remove);
}

auto LogParserHelpers::get_next_field(sv line, char begin_delim, char end_delim, uint64_t* dist_beyond_field_char) const -> std::optional<sv> {
    // Procedure (ignoring errors):
    //
    // Find opening delimiter
    // Set nesting to 1
    // While true
    //     Find next delimiter
    //     If delimiter not found: return indicating error
    //     If delimiter is an opening: increase nesting by 1
    //     If delimiter is a closing : decrease nesting by 1
    //         If nesting == 0: return field

    const auto beg_field = std::find(line.begin(), line.end(), begin_delim);
    if (beg_field == line.end()) {
        BLT_LINE(warning, m_line_num) << "Line did not contain beginning delimiter '" << begin_delim << "'.  Skipping.";
        return {};
    }
    BLT_LINE(trace, m_line_num) << "In " << std::quoted(line) << " found '" << begin_delim << "' at pos "
                                << std::distance(line.begin(), beg_field);

    int nesting = 1;
    auto curr_start = std::next(beg_field);
    while (true) {
        const auto next_delim = std::find_if(curr_start, line.end(), [begin_delim, end_delim] (auto c) {
            return begin_delim == c || end_delim == c; });
        if (next_delim == line.end()) {
            BLT_LINE(warning, m_line_num) << "Unbalanced opening delimiter - did not find ending delimiter '" << end_delim << "'.";
            return {};
        }
        BLT_LINE(trace, m_line_num) << "In " << std::quoted(line) << " found '" << *next_delim << "' at pos "
                                    << std::distance(line.begin(), next_delim);
        if (*next_delim == begin_delim) {
            nesting++;
        }
        if (*next_delim == end_delim) {
            if (--nesting == 0) {
                auto to_ret = sv(std::next(beg_field), next_delim);
                if (dist_beyond_field_char != nullptr) {
                    *dist_beyond_field_char = static_cast<uint64_t>(std::distance(line.begin(), next_delim) + 1);
                }
                return to_ret;
            }
        }
        curr_start = std::next(next_delim);
    }
}

auto LogParserHelpers::parse_st_location(sv field) const -> std::optional<LogParserTypes::Location> {
    LL(trace) << "Parsing s/t location from string " << std::quoted(field);
    if (std::count(field.begin(), field.end(), ',') != 3) {
        LL(warning) << "Did not find all components (x,y,z,rot) in the location string. Skipping.";
        return {};
    }

    LogParserTypes::Location ret {};
    const std::array<double*,4> to_store {&ret.x, &ret.y, &ret.z, &ret.rot};
    auto curr_store = to_store.begin();
    auto beg_pos = field.begin();

    while (true) {
        auto sep_pos = std::find(beg_pos, field.end(), ',');
        auto double_str = sv(beg_pos, sep_pos);
        auto val = str_to_double(double_str);
        if (!val) {
	    LL(warning) << "Location component " << std::quoted(double_str) << " (#" << std::distance(to_store.begin(), curr_store)
			<< ") could not be converted to a double.";
            return {};
        }

        **curr_store = *val;
        curr_store = std::next(curr_store);
        if (curr_store == to_store.end()) {
            break;
        }
        beg_pos = std::next(sep_pos);
    }

    return ret;
}

auto LogParserHelpers::parse_st_health(sv field) const -> std::optional<Health> {
    LL(trace) << "Parsing s/t health from string " << std::quoted(field);

    const auto health_sep = std::find(field.begin(), field.end(), '/');
    if (health_sep == field.end()) {
        LL(warning) << "s/t health field missing '/' separator. Skipping.";
        return {};
    }

    auto curr_health = str_to_uint64({field.begin(), health_sep});
    if (!curr_health) {
        LL(warning) << "s/t current health field is not a valid integer. Skipping.";
        return {};
    }

    auto total_health = str_to_uint64({std::next(health_sep), field.end()});
    if (!total_health) {
        LL(warning) << "s/t total health field is not a valid integer. Skipping.";
        return {};
    }

    return std::make_optional<Health>(Health::Current(*curr_health), Health::Total(*total_health));
}

auto LogParserHelpers::parse_name_and_id(sv field, uint64_t* dist_beyond_field_delim) const -> std::optional<LogParserTypes::NameId> {
    LL(trace) << "Parsing Name/ID from string " << std::quoted(field);
    
    // Everything up to the '{' is the name string subfield.
    const auto name_end = std::find(field.begin(), field.end(), '{');
    if (name_end == field.end()) {
        LL(warning) << "Did not find delimiter between name and ID. Skipping.";
        return {};
    }

    const auto dist_to_delim = std::distance(field.begin(), name_end);

    if (dist_to_delim == 1) {
        LL(warning) << "Name string is empty";
    } else if (*std::prev(name_end) != ' ') {
        LL(warning) << "Name substring does not have trailing space.";
    }

    auto name = strip({field.begin(), name_end}, " ");
    LL(trace) << "Name is " << std::quoted(name);

    uint64_t dist_to_first_char_beyond_field {};
    auto id_str = get_next_field(field, '{', '}', &dist_to_first_char_beyond_field);
    if (!id_str) {
        LL(warning) << "Failed to extract numeric ID field from name/id string. Skipping.";
        return {};
    }

    auto id = str_to_uint64(*id_str);
    if (!id) {
        LL(warning) << "Failed to parse ID string as an integer. Skipping.";
        return {};
    }

    LL(trace) << "ID is " << *id;

    if (dist_beyond_field_delim != nullptr) {
        *dist_beyond_field_delim = dist_to_first_char_beyond_field;
    }

    return LogParserTypes::NameId {.name = std::string(name), .id = *id};
}

auto LogParserHelpers::parse_name_id_instance(sv field) const -> std::optional<LogParserTypes::NameIdInstance> {
    LL(trace) << "Parsing name/id/instance from field " << std::quoted(field);

    const auto name_id_inst_sep = std::find(field.begin(), field.end(), ':');
    if (name_id_inst_sep == field.end()) {
        LL(warning) << "Field does not contain the name_id/inst separator, ':'";
        return {};
    }
    
    uint64_t dist_to_first_char_beyond_id {};
    auto name_id = parse_name_and_id({field.begin(), name_id_inst_sep}, &dist_to_first_char_beyond_id);
    if (!name_id) {
        LL(warning) << "Failed to parse name and id. Skipping.";
        return {};
    }

    // +1 to move beyond the name/id ':' instance separator.
    field.remove_prefix(static_cast<sv::size_type>(std::distance(field.begin(), name_id_inst_sep)) + 1);;

    LL(trace) << "Parsing instance string " << std::quoted(field) << " as an uint64_t";

    uint64_t dist_to_first_non_int_char {};
    auto inst = str_to_uint64(field, &dist_to_first_non_int_char);
    if (!inst) {
        LL(warning) << "Instance string " << std::quoted(field) << " is not a valid uint64_t. Skipping.";
        return {};
    }

    return LogParserTypes::NameIdInstance {.name_id = *name_id, .instance = *inst};
}

auto LogParserHelpers::parse_source_target_subject(sv field) const -> std::optional<LogParserTypes::Subject> {
    LL(trace) << "Parsing " << std::quoted(field) << " as a source/target (s/t) subject.";
    field = strip(field, " ");
    if (field.empty()) {
        LL(warning) << "source/target subject field is empty. Skipping.";
        return {};
    }

    if (field[0] == '@') {
        field.remove_prefix(1);
        LL(trace) << "s/t is a PC or a PC's companion";
        auto const pc_name_id_sep = std::find(field.begin(), field.end(), '#');
        if (pc_name_id_sep == field.end()) {
            LL(warning) << "PC s/t is missing name/id separator, '#'. Skipping.";
            return {};
        }
        auto name = sv(field.begin(), pc_name_id_sep);
        // +1 to move beyond the name '#' id separator
        field.remove_prefix(static_cast<sv::size_type>(std::distance(field.begin(), pc_name_id_sep)) + 1);
        uint64_t dist_to_first_non_uint64_char {};
        auto pc_id = str_to_uint64(field, &dist_to_first_non_uint64_char);
        if (!pc_id) {
            LL(warning) << "PC ID string to ulong conversion failed. Skipping.";
            return {};
        }
        field.remove_prefix(dist_to_first_non_uint64_char);

	LogParserTypes::PcSubject pcs {.name = std::string(name), .id = *pc_id};
        
        auto const pc_comp_sep = std::find(field.begin(), field.end(), '/');
        if (pc_comp_sep == field.end()) {
            LL(trace) << "s/t is a PC.";
            return pcs;
        }
        field.remove_prefix(static_cast<sv::size_type>(std::distance(field.begin(), pc_comp_sep)));

        LL(trace) << "s/t is a PC's companion.";
        auto pc_comp = parse_name_id_instance({std::next(pc_comp_sep), field.end()});
        if (!pc_comp) {
            LL(warning) << "Failed to parse companion's name, id, and instance. Skipping.";
            return {};
        }

        return LogParserTypes::CompanionSubject {.pc = pcs, .companion = *pc_comp};
    }

    LL(trace) << "s/t is a NPC.";
    auto npc = parse_name_id_instance({field.begin(), field.end()});
    if (!npc) {
        LL(warning) << "Failed to parse NPC's name/ID and instance. Skipping.";
        return {};
    }


    return npc;
}

auto LogParserHelpers::parse_source_target_field(sv field) const -> std::optional<LogParserTypes::SourceOrTarget> {
    LL(trace) << "parsing source/target (s/t) from field " << std::quoted(field);

    const auto name_loc_sep = std::find(field.begin(), field.end(), '|');
    if (name_loc_sep == field.end()) {
        LL(warning) << "field missing source/location separator, '|'. Skipping.";
            return {};
    }
    auto subject_field = sv(field.begin(), name_loc_sep);
    field.remove_prefix(static_cast<sv::size_type>(std::distance(field.begin(), name_loc_sep) + 1));

    const auto loc_health_sep = std::find(field.begin(), field.end(), '|');
    if (loc_health_sep == field.end()) {
        LL(warning) << "field missing location/health separator, '|'. Skipping.";
            return {};
    }
    const auto location_field = sv(field.begin(), loc_health_sep);
    auto health_field = sv(std::next(loc_health_sep), field.end());

    auto subject = parse_source_target_subject(subject_field);
    if (!subject) {
        LL(warning) << "Failed to parse s/t subject subfield. Skipping.";
        return {};
    }

    auto loc_str = get_next_field(location_field, '(', ')');
    if (!loc_str) {
        LL(warning) << "Location delimiters '()' not found. Skipping.";
        return {};
    }
    auto location = parse_st_location(*loc_str);
    if (!location) {
        LL(warning) << "Failed to parse location subfield. Skipping.";
        return {};
    }

    auto health_str = get_next_field(health_field, '(', ')');
    if (!health_str) {
        LL(warning) << "Health field delimiters '()' not found. Skipping.";
        return {};
    }

    auto health = parse_st_health(*health_str);
    if (!health) {
        LL(warning) << "Failed to parse s/t health subfield. Skipping..";
        return {};
    }

    return LogParserTypes::SourceOrTarget {.subject = *subject, .loc = *location, .health = *health};
}

auto LogParserHelpers::parse_ability_field(std::string_view field) const -> std::optional<LogParserTypes::Ability> {
    LL(trace) << "parsing ability from field " << std::quoted(field);

    return parse_name_and_id(field);
}

// Actions:
// AreaEntered (2 formats) AreaEntered {id}: place {id} for areas with no difficulty level associated (planets, etc.)
//                         AreaEntered {id}: place {id} difficulty {id} for ops, FPs, etc.
// DisciplineChanged (1 format): DisciplineChanged {id}: class {id}/spec {id}
// ApplyEffect (1 format) ApplyEffect {id}: effect_name {id}
// Event (1 format) Event {id}: event_name {id}
//     Events:
//     AbilityActivate
//     AbilityDeactivate
//     TargetSet
//     AbilityCancel
//     TargetCleared
//     EnterCombat
//     Crouch
//     ModifyThreat
//     LeaveCover
//     Death
//     FailedEffect
//     ExitCombat
//     AbilityInterrupt
//     Taunt
//     Revived
//     FallingDamage
// RemoveEffect (1 format) RemoveEffect {id}: effect_name {id}
// Restore (1 format) Restore {id}: resource_name {id}
// ModifyCharges (1 format) ModifyCharges {id}: resource_name {id}
// Spend (1 format): Spend {id}: resource_name {id}
auto LogParserHelpers::parse_action_field(sv field) const -> std::optional<Action> {
    LL(trace) << "parsing action verb from field " << std::quoted(field);

    auto action_verb = parse_name_and_id(field);
    if (!action_verb) {
        LL(warning) << "Unable to parse action verb from field. Skipping.";
        return {};
    }
    
    auto colon = std::find(field.begin(), field.end(), ':');
    if (colon == field.end()) {
        LL(warning) << "Missing separator (:) between action verb and action noun. Skipping.";
        return {};
    }
    field.remove_prefix(static_cast<sv::size_type>(std::distance(field.begin(), colon) + 1));

    uint64_t dist_to_first_char_after_id {};
    LL(trace) << "parsing action noun from field " << std::quoted(field);
    auto action_noun = parse_name_and_id(field, &dist_to_first_char_after_id);
    if (!action_noun) {
        LL(warning) << "Unable to parse action noun from field. Skipping.";
        return {};
    }
    field.remove_prefix(dist_to_first_char_after_id);
    
    Action ret {.verb = Action::Verb(*action_verb), .noun = Action::Noun(*action_noun)};

    if (field.empty()) {
        LL(trace) << "Action noun has no additional details.";
        return ret;
    }

    if (field[0] != ' ' && field[0] != '/') {
        LL(warning) << "Remaining field to be parsed: " << std::quoted(field);
        LL(warning) << "Action noun details subfield separator (' ' or '/') not found - "
                    << "got " << field[0] << "' instead? Ignoring and return.";
        return ret;
    }
    field.remove_prefix(1);

    auto noun_details = parse_name_and_id(field);
    if (!noun_details) {
        LL(warning) << "Found details delimiter but unable to parse action noun details from field. Skipping.";
        return {};
    }

    ret.detail = Action::Detail(noun_details);
    return ret;
}

auto LogParserHelpers::parse_value_field(sv field) const -> std::optional<LogParserTypes::Value> {
    LL(info) << "Parsing value from field " << std::quoted(field);

    // he3001
    // .info = "he3001"
    if (field == "he3001") {
        LL(trace) << "Value is the unique sentinel " << std::quoted(field);
        return LogParserTypes::LogInfoValue {.info = "he3001"};
    }

    if (field[0] == '0') {
        // 0: "0"
        // .base_value = 0
        // .crit = false
        if (field.size() == 1) {
            LL(trace) << "Value is just 0 - fully mitigated.";
            return LogParserTypes::FullyMitigatedValue {};
        }
        field.remove_prefix(1);
        field = strip(field, " ");

        if (field[0] == '-') {
            if (field.size() == 1) {
                // 0 -: "0 -". Fully mitigated but no reason - I think this is probably a bug.
                LL(trace) << "Value is \"0 -\" - fully mitigated, most likely a logging bug.";
                return LogParserTypes::FullyMitigatedValue {};
            }
            field.remove_prefix(1);
            field = lstrip(field, " ");

            // 0 -name {id}: "0 -parry {836045448945503}"
            // .damage_avoided_reason = {.name = parry, .id = 836045448945503}
            LL(trace) << "Damage was completely mitigated by some form of defensive ability.";
            auto damage_avoided_reason = parse_name_and_id(field);
            if (!damage_avoided_reason) {
                LL(warning) << "Unable to parse name/id of mitigation reason.";
                return {};
            }
            // This is the end of this particular branch of the parse tree.
            return LogParserTypes::FullyMitigatedValue {.damage_avoided_reason = damage_avoided_reason};
        }
    }

    uint64_t dist_to_first_non_int_char {};
    auto maybe_base_value = str_to_uint64(field, &dist_to_first_non_int_char);
    if (!maybe_base_value) {
        LL(warning) << "Could not convert initial base value to uint64_t. Skipping.";
        return {};
    }
    auto base_value = *maybe_base_value;
    LL(trace) << "base val = " << base_value;

    // 2.  INT: "9608"
    //     .base_value = 9608
    //     .crit = false
    if (dist_to_first_non_int_char == field.size()) {
        LL(trace) << "Value is " << base_value << ", a plain uint64_t.";
        return LogParserTypes::UnmitigatedValue {.base_value = base_value};
    }

    // 4.  INT.0: "3.0"
    //     .base_value = 3107
    //     .crit = false
    if (field[dist_to_first_non_int_char] == '.') {
        uint64_t dist_to_last_double_char {};
        auto double_val = str_to_double(field, &dist_to_last_double_char);
        if (double_val) {
            auto value = static_cast<uint64_t>(*double_val);
            if (dist_to_last_double_char == field.size()) {
                LL(trace) << "Value is just " << value;
            } else {
                field.remove_prefix(dist_to_last_double_char + 1);
                LL(warning) << "Value has unknown format: decimal is good but fraction has unexpected form: " << std::quoted(field);
                LL(warning) << "Ignoring suffix and returning value.";
            }
            return LogParserTypes::UnmitigatedValue {.base_value = value};
        }
    }

    // Move beyond base value.
    LL(trace) << dist_to_first_non_int_char << " steps to move past base value integer.";
    field.remove_prefix(dist_to_first_non_int_char);
    LL(trace) << "First char after base value is " << field[0];

    auto crit = false;
    if (field[0] == '*') {
        LL(trace) << "Value is a crit.";
        crit = true;
        field.remove_prefix(1);
    }
    // Eat up spaces - technically there will only be one for complex values, but we'll let it slide this time.
    field = lstrip(field, " ");

    // 3.  INT*: "3107*"
    //     .base_value = 3107
    //     .crit = true
    if (field.length() == 0) {
        LL(trace) << "Value is a simple crit with base value = " << base_value;
        return LogParserTypes::UnmitigatedValue {.base_value = base_value, .crit = crit};
    }

    // 7.  INT* ~INT: "17110* ~8973"
    //     .base_value = 17110
    //     .crit = true
    //     .effective = 8973
    // 8.  INT ~INT: "13315 ~9902"
    //     .base_value = 13315
    //     .crit = false
    //     .effective = 9902
    // NOTE: This means a toon's name should never start with a tilde.
    decltype(LogParserTypes::UnmitigatedValue::effective) effective {};
    if (field[0] == '~') {
        LL(trace) << "Have effective value - base value is mitigated in some way.";
        field.remove_prefix(1);
        effective = str_to_uint64(field, &dist_to_first_non_int_char);
        if (!effective) {
            LL(warning) << "Unable to parse effective value as an uint64_t. Skipping.";
            return {};
        }
        // Move beyond effective value.
        field.remove_prefix(dist_to_first_non_int_char);
        field = lstrip(field, " ");

        if (field.length() == 0) {
            LL(trace) "Value field has base=" << base_value << " and effective=" << *effective << " values only.";
            return LogParserTypes::UnmitigatedValue {.base_value = base_value, .crit = crit, .effective = effective};
        }
    }

    // At this point we're beyond the base and effective values and pointing to the first character of the
    // next string, generally a name/id pair.

    //        -v- we're here
    // 5.  INT name {id}: "5824 energy {836045448940874}"
    //     .base_value = 5824
    //     .crit = false
    //     .detail = {.name = energy, .id = 836045448940874
    // 6.  INT* name {id}: "5894* internal {836045448940876}"
    //     .base_value = 5894
    //     .crit = true
    //     .detail = {.name = internal, .id = 836045448940876
    // 12. INT ~INT name {id}: "13315 ~9902 energy {...}"
    //     .base_value = 13315
    //     .crit = false
    //     .detail = {.name = energy, .id = ...}
    //     .effective = 9902
    auto maybe_detail = parse_name_and_id(field, &dist_to_first_non_int_char);
    if (!maybe_detail) {
        LL(warning) << "Expected name/id details for value but could not parse. Skipping";
        return {};
    }
    field.remove_prefix(dist_to_first_non_int_char);
    field = lstrip(field, " ");
    if (field.length() == 0) {
        LL(info) << "Value is not mitigated.";
        return LogParserTypes::UnmitigatedValue {
            .base_value = base_value, .crit = crit, .detail = maybe_detail, .effective = effective
        };
    }

    // Now we're past the base, effective, and damage type.

    //      We'ere here -v-
    // 10. INT name {id} -name {id} (INT name {id}): "2749 energy {...874} -shield {...509} (19364 absorbed {...511})"
    //     .value = 2749
    //     .crit = false
    //     .detail = {.name = energy, .id = ...874}
    //     .absorbed = 19364
    //     .absorbed_reason = {.name = shield, .id = {...509}
    // XX. INT name {id} -: "2749 ~0 energy {...874} -"
    //     .value = 2749
    //     .crit = false
    decltype(LogParserTypes::AbsorbedValue::absorbed_reason) absorbed_reason;
    if (field[0] == '-') {
        LL(info) << "Damage was partially absorbed by something.";
        field.remove_prefix(1);
        if (field.empty()) {
            LL(warning) << "no reason provided, only the sentinel '-'.";
            return LogParserTypes::AbsorbedValue {
                .base_value = base_value,
                .crit = crit,
                .effective = effective,
                .absorbed = 0
            };
        }
        uint64_t dist_to_first_char_past_delim {};
        absorbed_reason = parse_name_and_id(field, &dist_to_first_char_past_delim);
        if (!absorbed_reason) {
            LL(warning) << "Unable to parse name/id for damage absorbed reason. Skipping.";
            return {};
        }
        field.remove_prefix(dist_to_first_char_past_delim);
        field = lstrip(field, " ");
    }

    // We've now handled the special case of a reason for the absorbed damage.
    //                                                      we'ere here -v-
    // 9.  INT ~INT name {id} (INT name {id}): 4012 ~2809 energy {...874} (1204 absorbed {...511})"
    //     .value = 4012
    //     .crit = false
    //     .detail = {.name = energy, .id = ...874}
    //     .effective = 2809
    //     .absorbed = 1024
    auto abs_field = get_next_field(field, '(', ')');
    if (!abs_field) {
        LL(warning) << "Could not extract damage amount absorbed field. Skipping.";
        return {};
    }

    auto maybe_absorbed = str_to_uint64(*abs_field, &dist_to_first_non_int_char);
    if (!maybe_absorbed) {
        LL(warning) << "Unable to parse damage absorbed amount. Skipping.";
        return {};
    }
    auto absorbed = *maybe_absorbed;
    abs_field->remove_prefix((dist_to_first_non_int_char));
    abs_field = lstrip(*abs_field, " ");
    
    auto abs_info = parse_name_and_id(*abs_field);
    if (!abs_info) {
        LL(warning) << "Unable to parse absorption name. Continuing.";
    } else if (abs_info->name != expected_mitigation_type) {
        LL(warning) << "Expected mitigation type is " << std::quoted(expected_mitigation_type)
                    << " but got " << std::quoted(abs_info->name) << " instead. Continuing.";
    }

    return LogParserTypes::AbsorbedValue {
        .base_value = base_value,
        .crit = crit,
        .effective = effective,
        .absorbed = absorbed,
        .detail = maybe_detail,
        .absorbed_reason = absorbed_reason
    };
}

auto LogParserHelpers::parse_threat_field(std::string_view field) const -> std::optional<LogParserTypes::Threat> {
    LL(trace) << "parsing threat from field " << std::quoted(field);

    if (field.empty()) {
        return {};
    }

    auto thr_dub = str_to_double(field);
    if (thr_dub) {
        return *thr_dub;
    }

    return field;
}
