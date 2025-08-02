// -*- fil-column: 120; indent-tabs-mode: nil -*-
#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <cstdint>
#include <string_view>

#include "log_parser.hpp"
#include <boost/log/trivial.hpp>
#include "log_parser_types.hpp"
#include "logging.hpp"
#include "timestamps.hpp"

using sv = std::string_view;

auto LogParser::parse_line(sv line, int line_num, Timestamps& ts_parser) -> std::optional<LogParserTypes::ParsedLogLine> {
    m_lph.set_line_num(line_num);
    // Still need to keep the line num here. ::sigh::
    BLT_LINE(error, line_num) << "Parsing log line " << std::quoted(line);

    // There are some special case log entries that it might be worth
    // it to segregate and handle in a non-standard way. AreaEntered
    // is an obvious one, since it has no ability but does have an
    // action.

    uint64_t dist_beyond_field_delimiter {};
    auto ts_field = m_lph.get_next_field(line, '[', ']', &dist_beyond_field_delimiter);
    if (!ts_field) {
        BLT_LINE(fatal, line_num) << "Unable to extract timestamp field (#1) from the log line. Skipping.";
        return {};
    }
    // Currently doesn't handle errors, lol.
    // Looks like ts field is always present and has a fixed format.
    ts_parser.update_from_log_entry(*ts_field);
    auto ts = ts_parser.current_log_timestamp();
    if (!ts) {
        BLT_LINE(fatal, line_num) << "Unable to parse timestamp string into valid timestamp. Skipping.";
        return {};
    }
    // BLT_LINE(error, line_num) << "ts field = " << std::quoted(*ts_field);
    line.remove_prefix(dist_beyond_field_delimiter);

    // source field is always present and has 4 formats:
    // Empty/PC/NPC/Comp. The non-empty have the same trailing subfields: location health
    auto source_field = m_lph.get_next_field(line, '[', ']', &dist_beyond_field_delimiter);
    if (!source_field) {
        BLT_LINE(fatal, line_num) << "Unable to extract the source field (#2) from the log line. Skipping.";
        return {};
    }
    decltype(LogParserTypes::ParsedLogLine::source) source;
    if (source_field->empty()) {
        BLT_LINE(info, line_num) << "Source is empty. Continuing.";
    } else {
        source = m_lph.parse_source_target_field(*source_field);
        if (!source) {
            BLT_LINE(fatal, line_num) << "Unable to parse source field. Skipping.";
            return {};
        }
    }

    // BLT_LINE(error, line_num) << "source field = " << std::quoted(*source_field);
    line.remove_prefix(dist_beyond_field_delimiter);

    // target field is always present and is either empty, the character =, or a full source/target specification.
    auto target_field = m_lph.get_next_field(line, '[', ']', &dist_beyond_field_delimiter);
    if (!target_field) {
        BLT_LINE(fatal, line_num) << "Unable to extract target field (#3) from the log line. Skipping.";
        return {};
    }
    // BLT_LINE(error, line_num) << "target_field = " << std::quoted(*target_field);

    decltype(source) target;
    if (*target_field == "=") {
        BLT_LINE(info, line_num) << "target is the same as the source.";
        target = source;
    } else if (target_field->empty()) {
        BLT_LINE(info, line_num) << "empty (no) target specified.";
    } else {
        target = m_lph.parse_source_target_field(*target_field);
        if (!target) {
            BLT_LINE(fatal, line_num) << "Unable to parse target field. Skipping.";
            return {};
        }
    }
    line.remove_prefix(dist_beyond_field_delimiter);

    // ability is always present but can be in 3 forms: empty, name/ID, and empty name with ID. WTF?
    // appears to be empty for these actions:
    // AreaEntered
    // DisciplineChanged
    // TargetSet
    // TargetCleared
    // EnterCombat
    // Restore (ex is focus point)
    // LeaveCover
    // Revived
    // FallingDamage
    // ExitCombat
    // Spend (ex is focus point)
    // Taunt
    // AbilityInterrupt
    // Death
    auto ability_field = m_lph.get_next_field(line, '[', ']', &dist_beyond_field_delimiter);
    if (!ability_field) {
        BLT_LINE(fatal, line_num) << "Unable to extract ability field (#4) from log line. Skipping.";
        return {};
    }
    decltype(m_lph.parse_name_and_id(*ability_field)) ability {};
    if (ability_field == "") {
        BLT_LINE(warning, line_num) << "Ability field is empty. Ignoring and continuing.";
        ability = std::make_optional<LogParserTypes::NameId>({.name = "", .id = 0});
    } else {
        ability = m_lph.parse_name_and_id(*ability_field);
        if (!ability) {
            BLT_LINE(fatal, line_num) << "Unable parse ability field. Skipping.";
            return {};
        }
    }
    line.remove_prefix(dist_beyond_field_delimiter);

    auto action_field = m_lph.get_next_field(line, '[', ']', &dist_beyond_field_delimiter);
    if (!action_field) {
        BLT_LINE(fatal, line_num) << "Unable to extract action field (#5) from log line. Skipping.";
        return {};
    }
    auto action = m_lph.parse_action_field(*action_field);
    if (!action) {
        BLT_LINE(fatal, line_num) << "Unable to parse action field from log line. Skipping.";
        return {};
    }
    BLT_LINE(trace, line_num) << "Action: verb=" << action->verb.ref().name << ", noun=" << action->noun.ref().name;
    line.remove_prefix(dist_beyond_field_delimiter);
    BLT_LINE(warning, line_num) << "Line after action: " << std::quoted(line);

    LogParserTypes::ParsedLogLine ret;
    ret.ts = *ts;
    ret.source = source;
    ret.target = target;
    ret.ability = *ability;
    ret.action = *action;
    
    auto value_field = m_lph.get_next_field(line, '(', ')', &dist_beyond_field_delimiter);
    if (!value_field) {
        BLT_LINE(warning, line_num) << "Optional value field (#6) not present in log line. Ignoring.";
    } else {
        ret.value = m_lph.parse_value_field(*value_field);
        BLT_LINE(info, line_num) << "parse_value_field returns '" << (ret.value ? "true'" : "false'");
        if (!ret.value) {
            BLT_LINE(fatal, line_num) << "Value field (#6) present but could not be parsed. Ignoring.";
        }
        line.remove_prefix(dist_beyond_field_delimiter);
    }
    BLT_LINE(warning, line_num) << "Line after value: " << std::quoted(line);

    auto threat_field = m_lph.get_next_field(line, '<', '>', &dist_beyond_field_delimiter);
    if (!threat_field) {
        BLT_LINE(warning, line_num) << "Optional threat field (#7) not present in log line. Ignoring.";
    } else {
        ret.threat = m_lph.parse_threat_field(*threat_field);
        if (!ret.threat) {
            BLT_LINE(fatal, line_num) << "Threat field (#7) present but could not be parsed. Ignoring.";
        }
    }

    return ret;
}
