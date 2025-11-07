// -*- fil-column: 120; indent-tabs-mode: nil -*-

#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>

#include "generator.hpp"
#include "log_parser.hpp"
#include "logging.hpp"
#include "db_populator.hpp"
#include "timestamps.hpp"

auto dump_scope_measurements(const ScopeRuns& sr) -> void {
    std::cout << "Scope: " << sr.m_func_name << "\n"
              << "    # calls: " << sr.m_num_calls
              << ", total ns of all calls: " << sr.m_total_time_in_func
              << ", ns/call: " << sr.m_total_time_in_func / sr.m_num_calls
              << "\n";
}

auto file_reader(std::ifstream& ifs) -> Generator<std::string> {
    using std::getline;
    std::string line;
    getline(ifs, line);
    while(ifs.good()) {
        co_yield line;
        getline(ifs, line);
    }
}

auto main(int argc, char* argv[]) -> int {
    set_log_filter();

    const std::string conn_str {"dbname = swtor_combat_explorer   user = jason   password = jason"};

    auto parse_time = ScopeRuns("parse_line");
    auto populate_time = ScopeRuns("populate_from_entry");

    for (int i = 1; i < argc; i++) {
        const std::string lfn {argv[i]};
        BLT(info) << "Parsing logfile " << std::quoted(lfn);
        auto log_creation_time = Timestamps::log_file_creation_time(lfn);
        Timestamps ts {log_creation_time};

        std::ifstream log_in {lfn};
        
        if (log_in.fail()) {
            BLT(fatal) << "Error reading logfile " << std::quoted(lfn) << ". Skipping this logfile.";
            continue;
        }
        
        DbPopulator db {
            DbPopulator::ConnStr(conn_str),
            DbPopulator::LogfileFilename(lfn),
            ts.log_creation_timestamp(),
            DbPopulator::ExistingLogfileBehavior::DELETE_ON_EXISTING
        };

        BLT(info) << "Database version: " << std::quoted(db.db_version());

        int line_num = 0;
        LogParser lp;
        for(const auto& line : file_reader(log_in)) {
            line_num += 1;
        
            std::string_view linev(line);
            linev.remove_suffix(1); // Chomp final character, which is always a CR.
        
            if (linev.empty()) {
              continue;
            }
        
            parse_time.enter();
            auto log_entry = lp.parse_line(linev, line_num, ts);
            parse_time.exit();
            if (!log_entry) {
                BLT(fatal) << "Error parsing log line: " << std::quoted(linev) << ". Skipping.";
                continue;
            }
        
            populate_time.enter();
            db.populate_from_entry(*log_entry);
            populate_time.exit();
        }

        db.mark_fully_parsed();

        dump_scope_measurements(measure_add_name_id);
        dump_scope_measurements(measure_add_pc_class);
        dump_scope_measurements(measure_add_action);
        dump_scope_measurements(measure_add_pc_actor);
        dump_scope_measurements(measure_add_npc_actor);
        dump_scope_measurements(measure_add_companion_actor);
        dump_scope_measurements(parse_time);
        dump_scope_measurements(populate_time);
    }

    BLT(info) << "All logfiles processed. Exiting.";

    return 0;
}
