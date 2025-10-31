#include <chrono>
#include <filesystem>
#include <string>

#include "timestamps.hpp"
#include "logging.hpp"

namespace sc = std::chrono;

const std::string Timestamps::LOG_FILENAME_TIMESTAMP_FORMAT {"%Y-%m-%d_%H_%M_%S"};
const std::string Timestamps::LOG_ENTRY_TIME_FORMAT {"%T"};

namespace {
    const std::string FILENAME_PREFIX {"combat_"};
    const std::string FILENAME_SUFFIX {".txt"};
    const std::string::size_type FILENAME_MICROSECONDS_LEN {6};
    const std::string::size_type FILENAME_MICROSECONDS_SUFFIX_LEN {7};
} // namespace

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

    init_ts_no_microseconds_v.remove_suffix(FILENAME_MICROSECONDS_SUFFIX_LEN);
    init_ts_microseconds_v.remove_prefix(init_ts_microseconds_v.length() - FILENAME_MICROSECONDS_LEN);
    BLT(trace) << "Initial stripped log timestamp string from filename: " << std::quoted(init_ts_no_microseconds_v);
    BLT(trace) << "Initial stripped timestamp microseconds from filename: " << std::quoted(init_ts_microseconds_v);
    std::istringstream ts_stream {std::string {init_ts_no_microseconds_v}};
    ts_stream >> sc::parse(LOG_FILENAME_TIMESTAMP_FORMAT, creation_ts);
    auto init_microseconds = sc::microseconds(std::stol(std::string(init_ts_microseconds_v)));
    BLT(trace) << "Initial microseconds from filename: " << init_microseconds;
    creation_ts += init_microseconds;
    auto creation_ts_seconds = tp_to_count<sc::seconds>(creation_ts);
    BLT(trace) << "seconds past epoch of log creation timestamp: " << creation_ts_seconds;
    BLT(trace) << "seconds since log was created: "
	      << tp_to_count<sc::seconds>(sc::system_clock::now()) - creation_ts_seconds;

    BLT(info) << "creation ts = " << creation_ts;
    BLT(trace) << "creation ts day = " << sc::floor<sc::days>(creation_ts);
    BLT(trace) << "creation ts time only = " << creation_ts - sc::floor<sc::days>(creation_ts);

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

Timestamps::Timestamps(std::optional<std::string> init_timestamp) {
    if (init_timestamp) {
        m_log_creation_ts = parse_logfile_timestamp(*init_timestamp);
    } else {
        BLT(warning) << "No initial timestamp provided - using 'now' as creation timestamp.";
        m_log_creation_ts = sc::system_clock::now();
    }
}

auto Timestamps::log_file_creation_time(const std::string& log_filename) -> std::optional<std::string> {
    auto name_only = std::filesystem::path(log_filename).filename().string();
    std::string_view lf {name_only};
    // Get the creation timestamp from the filename.
    if (!lf.starts_with(FILENAME_PREFIX)) {
        BLT(warning) << "Log filename " << std::quoted(lf) << " has unexpected format - should start with 'combat_'";
        return {};
    }
    if (!lf.ends_with(FILENAME_SUFFIX)) {
        BLT(warning) << "Log filename " << std::quoted(lf) << " has unexpected format - should end with '.txt'";
        return {};
    }
    lf.remove_prefix(FILENAME_PREFIX.length());
    lf.remove_suffix(FILENAME_SUFFIX.length());
    return std::string {lf.begin(), lf.end()};
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
