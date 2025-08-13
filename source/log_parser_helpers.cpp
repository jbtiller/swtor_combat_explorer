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
        auto const pc_comp_sep = std::find(field.begin(), field.end(), '/');
        // The PC is either part of a companion or just a PC.
        auto const pc_end = std::min(pc_comp_sep, field.end());
        sv name;
        uint64_t pc_id {};
        if (pc_name_id_sep == field.end()) {
            LL(warning) << "PC s/t is missing name/id separator, '#'. Checking for UNKNOWN.";
            if (field.starts_with("UNKNOWN")) {
                LL(warning) << "PC is UNKNOWN with no ID.";
                name = sv {field.begin(), pc_end};
                field.remove_prefix(name.length());
            } else {
                LL(error) << "PC is missing '#' and is not UNKNOWN. Skipping.";
                return {};
            }
        } else {
            name = sv(field.begin(), pc_name_id_sep);
            // +1 to move beyond the name '#' id separator
            field.remove_prefix(static_cast<sv::size_type>(std::distance(field.begin(), pc_name_id_sep)) + 1);

            uint64_t dist_to_first_non_uint64_char {};
            auto pc_id_maybe = str_to_uint64(field, &dist_to_first_non_uint64_char);
            if (!pc_id_maybe) {
                LL(warning) << "PC ID string to ulong conversion failed. Skipping.";
                return {};
            }
            pc_id = *pc_id_maybe;
            field.remove_prefix(dist_to_first_non_uint64_char);
        }

        LogParserTypes::PcSubject pcs {.name = std::string(name), .id = pc_id};
        
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

auto LogParserHelpers::parse_mitigation_effect(std::string_view field) const -> std::optional<LogParserTypes::MitigationEffect> {
    LL(info) << "Parsing mitigation effect from field " << std::quoted(field);

    LogParserTypes::MitigationEffect effect;
    uint64_t idx_just_beyond {};
    auto eff_value = str_to_double(field, &idx_just_beyond);
    if (eff_value) {
        effect.value = static_cast<uint64_t>(*eff_value);
        field.remove_prefix(idx_just_beyond);
        field = lstrip(field);
    }

    if (!field.empty()) {
        effect.effect = parse_name_and_id(field);
    }

    return effect;
}

auto LogParserHelpers::parse_value_field(std::string_view field) const -> std::optional<LogParserTypes::Value> {
    LL(info) << "Parsing value from field " << std::quoted(field);

    if (field.starts_with("he")) {
        LL(trace) << "Value is the unique sentinel " << std::quoted(field);
        return LogParserTypes::LogInfoValue {.info = {field.begin(), field.end()}};
    }

    uint64_t steps_past_subfield {};
    auto base_value = str_to_double(field, &steps_past_subfield);
    if (!base_value) {
        LL(error) << "Unable to parse value field's base value as number. Skipping.";
        return {};
    }
    field.remove_prefix(steps_past_subfield);

    LogParserTypes::RealValue ret {};
    ret.base_value = static_cast<uint64_t>(*base_value);

    if (field.empty()) {
        return ret;
    }

    if (field[0] == '*') {
        ret.crit = true;
        field.remove_prefix(1);
    }
    field = strip(field);
    if (field.empty()) {
        return ret;
    }

    if (field[0] == '~') {
        field.remove_prefix(1);
        auto eff_value = str_to_double(field, &steps_past_subfield);
        if (!eff_value) {
            LL(error) << "Effective value sentinel (~) found but can't parse as a number. Skipping.";
            return {};
        }
        ret.effective = eff_value;
        field.remove_prefix(steps_past_subfield);
        field = lstrip(field);
    }

    // Now we might encounter any of:
    //
    // NameId for the value type
    // '-' mitigation_reason
    // '(' mitigation_effect ')'

    // If any string contains a dash, we're screwed.
    auto minus_pos = std::find(field.begin(), field.end(), '-');
    auto paren_pos = std::find(field.begin(), field.end(), '(');
    auto closer = std::min(minus_pos, paren_pos);

    auto type_subf = rstrip(std::string_view(field.begin(), closer));
    auto reason_subf = rstrip(std::string_view(minus_pos, paren_pos));
    auto effect_subf = rstrip(std::string_view(paren_pos, field.end()));
    LL(trace) << "type_subf = " << std::quoted(type_subf);
    LL(trace) << "reason_subf = " << std::quoted(reason_subf);
    LL(trace) << "effect_subf = " << std::quoted(effect_subf);
    
    if (!type_subf.empty()) {
        ret.type = parse_name_and_id(type_subf);
    }
    // >1 accounts for the '-' sentinel that starts the field. If there's nothing after the sentinel, who cares?
    if (reason_subf.length() > 1) {
        reason_subf.remove_prefix(1);
        ret.mitigation_reason = parse_name_and_id(reason_subf);
    }

    if (effect_subf.empty()) {
        return ret;
    }
    auto maybe_effect = get_next_field(field, '(', ')');
    if (!maybe_effect) {
        return ret;
    }
    ret.mitigation_effect = parse_mitigation_effect(*maybe_effect);

    return ret;
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
