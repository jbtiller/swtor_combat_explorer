// -*- fil-column: 120; indent-tabs-mode: nil -*-
#pragma once

#include <string_view>
#include <optional>

#include "log_parser_types.hpp"
#include "log_parser_helpers.hpp"
#include "timestamps.hpp"

class LogParser {
public:
    LogParser() = default;

    // The line number is used to populate logging messages.
    auto parse_line(std::string_view line, int line_num, Timestamps& ts_parser) -> std::optional<LogParserTypes::ParsedLogLine>;

private:
    LogParserHelpers m_lph;
};
