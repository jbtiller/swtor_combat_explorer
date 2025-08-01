#include <chrono>
#include <string>

#include "timestamps.hpp"
#include "logging.hpp"

namespace sc = std::chrono;

const std::string Timestamps::LOG_FILENAME_TIMESTAMP_FORMAT {"%Y-%m-%d_%H_%M_%S"};
const std::string Timestamps::LOG_ENTRY_TIME_FORMAT {"%T"};

// class Timestamps
// {
//   public:
//     Timestamps(const std::string init_timestamp);
// 
//     // Return the format string suitable for parsing via std::chrono::parse() the timestamp
//     // embedded in the log's filename.
//     auto log_filename_timestamp_format() -> std::string_view;
// 
//     // Return the time of day format string used to parse the time associated with each log entry.
//     auto log_entry_time_format() -> std::string_view;
// 
//     // Updates the current state based on the log entry time  string. Returns a reference to this
//     // instance to use for chaining.
//     auto update_from_log_entry(std::string_view log_entry_time) -> Timestamps&;
// 
//     auto current_log_timestamp() {
// 	return m_curr_log_entry_ts;
//     }
// 
// private:
//     // Ignores the microseconds portion, "...%S_<micro>"
//     static constexpr std::string_view LOG_FILENAME_TIMESTAMP_FORMAT  = "%Y-%m-%d_%H_%M_%S";
//     // Ignores the millisecond portion, "%T.<milli>"
//     static constexpr std::string_view LOG_ENTRY_TIME_FORMAT = "%T";
// 
//     // Extracted from the log filename
//     const std::chrono::time_point<std::chrono::system_clock> m_log_creation_ts;
// 
//     // Beginning of the current day of the most recent timestamp seen.
//     std::chrono::time_point<std::chrono::system_clock> m_curr_day_start_ts;
// 
//     std::chrono::time_point<std::chrono::system_clock> m_curr_log_entry_ts;
// };

template<class Unit, class Tp>
auto tp_to_count(Tp tp) -> unsigned long
{
    return sc::floor<Unit>(tp).time_since_epoch().count();
}

auto Timestamps::parse_logfile_timestamp(const std::string& init_timestamp) -> Timestamps::timestamp
{
    timestamp creation_ts;
    std::string_view init_ts_no_microseconds_v {init_timestamp};
    std::string_view init_ts_microseconds_v {init_timestamp};

    init_ts_no_microseconds_v.remove_suffix(7);
    init_ts_microseconds_v.remove_prefix(init_ts_microseconds_v.length() - 6);
    BLT(info) << "Initial stripped log timestamp string from filename: " << std::quoted(init_ts_no_microseconds_v);
    BLT(info) << "Initial stripped timestamp microseconds from filename: " << std::quoted(init_ts_microseconds_v);
    std::istringstream ts_stream {std::string {init_ts_no_microseconds_v}};
    ts_stream >> sc::parse(LOG_FILENAME_TIMESTAMP_FORMAT, creation_ts);
    auto init_microseconds = sc::microseconds(std::stol(std::string(init_ts_microseconds_v)));
    BLT(info) << "Initial microseconds from filename: " << init_microseconds;
    creation_ts += init_microseconds;
    auto creation_ts_seconds = tp_to_count<sc::seconds>(creation_ts);
    BLT(info) << "seconds past epoch of log creation timestamp: " << creation_ts_seconds;
    BLT(info) << "seconds since log was created: "
	      << tp_to_count<sc::seconds>(sc::system_clock::now()) - creation_ts_seconds;

    BLT(info) << "creation ts = " << creation_ts;
    BLT(info) << "creation ts day = " << sc::floor<sc::days>(creation_ts);
    BLT(info) << "creation ts time only = " << creation_ts - sc::floor<sc::days>(creation_ts);

    return creation_ts;
}

Timestamps::Timestamps()
    : m_log_creation_ts(sc::system_clock::now())
{
}

Timestamps::Timestamps(const std::string& init_timestamp)
    : m_log_creation_ts {parse_logfile_timestamp(init_timestamp)}
{
}

auto Timestamps::update_from_log_entry(std::string_view log_entry_time) -> Timestamps&
{
    // OMG TODO: Handle errors.
    const auto evt_time_ms_str = std::string {log_entry_time.end() - 3, log_entry_time.end()}; // ms is 3 digits
    const auto evt_time_no_ms_str = std::string(log_entry_time).substr(0, 8); // len(HH:MM:SS) == 8
    const auto evt_time_only_ms = sc::milliseconds(std::stol(evt_time_ms_str));

    BLT(info) << "Event time str without ms: " << evt_time_no_ms_str;
    BLT(info) << "Event time str ms only: " << evt_time_ms_str;
    BLT(info) << "Event time ms only: " << evt_time_only_ms;

    sc::milliseconds evt_time_no_ms;
    std::istringstream evt_time_in {evt_time_no_ms_str}; 
    evt_time_in >> sc::parse("%T", evt_time_no_ms);

    auto evt_time_ms = evt_time_no_ms + evt_time_only_ms;

    BLT(info) << "Event time of day without ms: " << evt_time_no_ms;
    BLT(info) << "Event time of day only ms: " << evt_time_ms;
    BLT(info) << "Event time of day in ms: " << evt_time_ms;

    if (!m_curr_log_entry_ts) {
	m_curr_log_entry_ts.emplace(m_log_creation_ts);
    }

    auto prev_log_entry_time_ms = sc::duration_cast<sc::milliseconds>((*m_curr_log_entry_ts - sc::floor<sc::days>(*m_curr_log_entry_ts)));
    BLT(info) << "Previous event time of day in ms: " << evt_time_ms;
    
    *m_curr_log_entry_ts = sc::floor<sc::days>(*m_curr_log_entry_ts) + evt_time_ms;
    if (prev_log_entry_time_ms > evt_time_ms) {
	*m_curr_log_entry_ts = *m_curr_log_entry_ts + sc::days(1);
    }
	    
    BLT(info) << "Event timestamp: " << *m_curr_log_entry_ts;

    return *this;
}
