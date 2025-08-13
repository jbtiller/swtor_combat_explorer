// -*- fil-column: 120; indent-tabs-mode: nil -*-
#include <variant>
#include <fstream>
#include <string>
#include <climits>
#include <array>
#include <boost/log/trivial.hpp>
#include <boost/log/expressions.hpp>
#include <unistd.h>
#include <cstdlib>
#include <iomanip>
#include <chrono>
#include <memory>
#include <map>

#include "generator.hpp"
#include "lib.hpp"
#include "log_parser_types.hpp"
#include "timestamps.hpp"
#include "logging.hpp"
#include "log_parser.hpp"

std::unique_ptr<Timestamps> timestamps;

// Extract and parse log creation timestamp embedded in log filename. Initialize global timestamp manager.
auto parse_combat_log_filename_timestamp(std::string_view log_filename) -> void {
    namespace sc = std::chrono;
    
    std::array<char, PATH_MAX> cwd {};
    auto now_time = sc::system_clock::now();
    
    getcwd(cwd.data(), PATH_MAX);
    BLT(info) << "Current path: " << cwd.data();
    BLT(info) << "Log path: " << log_filename;

    auto now_sec = sc::duration_cast<sc::seconds>(now_time.time_since_epoch()).count();
    BLT(info) << "seconds past the epoch as of now: " << now_sec;

    const char* fn_prefix = "combat_";
    const char* fn_suffix = ".txt";
    
    // Get the creation timestamp from the filename.
    if (auto prefix_start = log_filename.find(fn_prefix); prefix_start != std::string::npos) {
        if (log_filename.ends_with(".txt")) {
            log_filename.remove_prefix(prefix_start + strlen(fn_prefix));
            log_filename.remove_suffix(strlen(fn_suffix));
            timestamps = std::make_unique<Timestamps>(std::string(log_filename));
        } else {
            BLT(warning) << "Log filename " << std::quoted(log_filename) << " has unexpected format - should end with .txt'";
            BLT(warning) << "Unable to determine date and time of log start. Will use now instead.";
            timestamps = std::make_unique<Timestamps>();
        }
    } else {
        BLT(warning) << "Log filename " << std::quoted(log_filename) << " has unexpected format - should include 'combat_'";
        BLT(warning) << "Unable to determine date and time of log start. Will use now instead.";
        timestamps = std::make_unique<Timestamps>();
    }
}

auto log_source_target(const LogParserTypes::SourceOrTarget& st, int line_num, std::string_view st_str) -> void {
    if (std::holds_alternative<LogParserTypes::PcSubject>(st.subject)) {
        const auto& pc = std::get<LogParserTypes::PcSubject>(st.subject);
        BLT_LINE(error, line_num) << "PC " << st_str << ": name=" << std::quoted(pc.name) << ", id=" << pc.id;
    } else if (std::holds_alternative<LogParserTypes::NpcSubject>(st.subject)) {
        const auto& npc = std::get<LogParserTypes::NpcSubject>(st.subject);
        BLT_LINE(error, line_num) << "NPC " << st_str << ": name=" << std::quoted(npc.name_id.name) << ", id=" << npc.name_id.id;
    } else {
        const auto& comp = std::get<LogParserTypes::CompanionSubject>(st.subject);
        BLT_LINE(error, line_num) << "Comp " << st_str << ": pc_name=" << std::quoted(comp.pc.name) << ", id=" << comp.pc.id
                                  << ", comp_name=" << std::quoted(comp.companion.name_id.name)
                                  << ", comp_id=" << comp.companion.name_id.id
                                  << ", comp_inst=" << comp.companion.instance;
    }
}

auto file_reader(std::ifstream& ifs) -> Generator<std::string> {
    std::string line;
    getline(ifs, line);
    while(ifs.good()) {
        co_yield line;
        getline(ifs, line);
    }
}

auto main(int argc, char** argv) -> int {
    if (const char* bl_level = std::getenv("BL_LEVEL")) {
        std::map<std::string,boost::log::trivial::severity_level> sevs {
            {"trace",   boost::log::trivial::trace},
            {"debug",   boost::log::trivial::debug},
            {"info",    boost::log::trivial::info},
            {"warning", boost::log::trivial::warning},
            {"error",   boost::log::trivial::error},
            {"fatal",   boost::log::trivial::fatal},
        };
        if (auto levit = sevs.find(bl_level); levit != sevs.end()) {
            auto lev = levit->second;
            boost::log::core::get()->set_filter(
                boost::log::trivial::severity >= lev
            );
        }
    }

    auto const lib = library {};

    BLT(info) << "Hello '" << lib.name << "' World!";

    for (int li = 1; li < argc; ++li) {
        const auto log_path = std::string(argv[li]);
    // {"../../test/logs/combat_2025-05-15_13_31_56_714109.txt"};
    // const std::string log_path {"../../test/logs/combat_2025-04-20_19_52_15_159478.txt"};

    parse_combat_log_filename_timestamp(log_path);

    std::ifstream log_in {log_path};
    if (!log_in.good()) {
        BLT(error) << "Failed to open " << std::quoted(log_path) << "for reading. Skipping.";
        continue;
    }
    BLT(info) << "Successfully opened "  << std::quoted(log_path) << " for reading.";

    int line_num = 0;
    LogParser lp;

    for(auto& line : file_reader(log_in)) {
        line_num += 1;

        std::string_view linev(line);
        linev.remove_suffix(1); // Chomp final character, which is always a CR.

        BLT_LINE(info, line_num) << line;

        // skip blank lines.
        if (linev.empty()) {
          BLT_LINE(info, line_num) << "Line empty.  Skipping.";
          continue;
        }

        auto log_entry = lp.parse_line(linev, line_num, *timestamps);
        if (log_entry) {
            BLT_LINE(error, line_num) << linev;
            BLT_LINE(error, line_num) << "ts = " << log_entry->ts;

            if (!log_entry->source) {
                BLT_LINE(error, line_num) << "Source field is empty.";
            } else {
                log_source_target(*log_entry->source, line_num, "source");
            }

            if (!log_entry->target) {
                BLT_LINE(error, line_num) << "Target field is empty.";
            } else {
                log_source_target(*log_entry->target, line_num, "target");
            }

            const auto& ability = log_entry->ability;
            BLT_LINE(error, line_num) << "Ability: name=" << std::quoted(ability.name) << ", id=" << ability.id;

            auto& action = log_entry->action;
            auto has_detail = action.detail.ref().has_value();
            BLT_LINE(error, line_num) << "Action: verb=" << action.verb.ref().name << ", noun=" << action.noun.ref().name
                                      << ", detail=" << (has_detail? action.detail.ref()->name : "none");
            if (!log_entry->value) {
                BLT_LINE(error, line_num) << "No value field present.";
            } else {
                auto& value = *log_entry->value;
                if (std::holds_alternative<LogParserTypes::LogInfoValue>(value)) {
                    BLT_LINE(error, line_num) << "Value: info=" << std::get<LogParserTypes::LogInfoValue>(value).info;
                } else if (std::holds_alternative<LogParserTypes::RealValue>(value)) {
                    auto& rv = std::get<LogParserTypes::RealValue>(value);
                    bool has_type = rv.type.has_value();
                    std::string type = has_type ? rv.type->name : "n/p";
                    bool has_eff = rv.effective.has_value();
                    std::string eff = has_eff ? std::to_string(*rv.effective) : "n/p";
                    bool has_mit_reas = rv.mitigation_reason.has_value();
                    std::string mit_reas = has_mit_reas ? rv.mitigation_reason->name : "n/p";
                    bool has_mit_eff = rv.mitigation_effect.has_value();
                    bool has_mit_eff_val = has_mit_eff && rv.mitigation_effect->value.has_value();
                    std::string mit_eff_val = has_mit_eff_val ? std::to_string(*rv.mitigation_effect->value) : "n/p";
                    bool has_mit_eff_eff = has_mit_eff && rv.mitigation_effect->effect.has_value();
                    std::string mit_eff_eff = has_mit_eff_eff ? rv.mitigation_effect->effect->name : "n/p";
                    BLT_LINE(error, line_num) << "Real value: base=" << rv.base_value << ", crit=" << rv.crit
                                              << ", eff=" << eff << ", type=" << type << ", mit_reas=" << mit_reas
                                              << ", mit_eff_val=" << mit_eff_val << "mit_eff_eff=" << mit_eff_eff;
                }
            }

            if (!log_entry->threat) {
                BLT_LINE(error, line_num) << "No threat field present.";
            } else if (std::holds_alternative<double>(*log_entry->threat)) {
                auto threat = std::get<double>(*log_entry->threat);
                BLT_LINE(error, line_num) << "Threat: threat=" << threat;
            } else {
                auto threat = std::get<std::string_view>(*log_entry->threat);
                BLT_LINE(error, line_num) << "Threat: threat=" << std::quoted(threat);
            }
        }
    }

    BLT(info) << "Done parsing file " << std::quoted(log_path);
    }
    return 0;
}
