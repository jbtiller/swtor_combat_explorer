#pragma once

#include <string>
#include <chrono>
#include <optional>

/// Timestamps - handle time-related information in combat logs
///
/// Assumptions:
///
/// 1.  The creation timestamp is "YYYY-MM-DD_HH_mm_SS_micros" where all are integer and use the typical
///     restrictions as well as being 0-prefixed. "micros" is six digits.
///
/// 2.  The log entry time is "HH_mm_SS.millis" where this is only the time of day and millis is 3-digits
///     long
///
/// 3.  The timestamp supplied in the constructor is used as the base day for all log entry times, which
///     means that you should use only use this class for log entries in the same file.
///
/// 4.  The first log entry is assumed to have occurred within 24 hours of when the log was created.
///
class Timestamps
{
  public:
    using timestamp = std::chrono::time_point<std::chrono::system_clock>;
    
    // Initialize using the current moment
    Timestamps();

    // Initialize using the timestamp in the supplied string.
    explicit Timestamps(const std::string& init_timestamp);

    explicit Timestamps(std::optional<std::string> init_timestamp);

    // Extract the timestamp from the combat event log filename.
    auto static log_file_creation_time(const std::string& log_filename) -> std::optional<std::string>;

    // Return the format string suitable for parsing via std::chrono::parse() the timestamp
    // embedded in the log's filename.
    auto static log_filename_timestamp_format() {
	return LOG_FILENAME_TIMESTAMP_FORMAT;
    }

    // Return the time of day format string used to parse the time associated with each log entry.
    auto static log_entry_time_format() {
	return LOG_ENTRY_TIME_FORMAT;
    }

    // Updates the current state based on the log entry time  string. Returns a reference to this
    // instance to use for chaining.
    auto update_from_log_entry(std::string_view log_entry_time) -> Timestamps&;

    auto current_log_timestamp() {
	return m_curr_log_entry_ts;
    }

    auto log_creation_timestamp() {
	return m_log_creation_ts;
    }

    static auto diff_ms(const timestamp& from, const timestamp&  to) {
	return std::chrono::duration_cast<std::chrono::milliseconds>(to - from);
    }

    auto diff_curr_timestamp_ms(const timestamp& to) {
	return diff_ms(*m_curr_log_entry_ts, to);
    }

    static auto timestamp_to_ms_past_epoch(const timestamp& ts) {
        return std::chrono::duration_cast<std::chrono::milliseconds>(ts.time_since_epoch()).count();
    }

    static auto parse_logfile_timestamp(const std::string& init_timestamp) -> timestamp;

private:
    // Ignores the microseconds portion, "...%S_<micro>"
    static const std::string LOG_FILENAME_TIMESTAMP_FORMAT;
    // Ignores the millisecond portion, "%T.<milli>"
    static const std::string LOG_ENTRY_TIME_FORMAT;

    // Extracted from the log filename
    timestamp m_log_creation_ts;

    // These two won't be filled in until we're called for the first log entry's time, which is why
    // they're held in optionals.

    // Beginning of the current day of the most recent timestamp seen.
    std::optional<timestamp> m_curr_log_entry_ts;
};
