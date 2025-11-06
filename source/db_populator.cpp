// -*- fil-column: 120; indent-tabs-mode: nil -*-

#include <cstdio>
#include <filesystem>
#include <format>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wall"
#include <pqxx/pqxx>
#pragma GCC diagnostic pop

#include "db_custom_types.hpp"
#include "db_populator.hpp"
#include "log_parser_types.hpp"
#include "logging.hpp"
#include "timestamps.hpp"

namespace lpt = LogParserTypes;

#include <time.h>

ScopeRuns::ScopeRuns(const std::string& function_name)
    : m_func_name(function_name) {
}

auto ScopeRuns::enter() -> void {
    clock_gettime(CLOCK_MONOTONIC, &m_enter_time);
}

auto ScopeRuns::exit() -> void {
    m_num_calls++;
    timespec exit_time;
    clock_gettime(CLOCK_MONOTONIC, &exit_time);
    int64_t sec_diff = exit_time.tv_sec - m_enter_time.tv_sec;
    int64_t ns_diff = exit_time.tv_nsec - m_enter_time.tv_nsec;
    int64_t time_in_func = sec_diff * 1000000000 + ns_diff;
    m_total_time_in_func += time_in_func;
}

ScopeRuns measure_add_name_id("DbPopulator::add_name_id");
ScopeRuns measure_add_pc_class("DbPopulator::add_pc_class");
ScopeRuns measure_add_action("DbPopulator::add_action");
ScopeRuns measure_add_pc_actor("DbPopulator::add_pc_actor");
ScopeRuns measure_add_npc_actor("DbPopulator::add_npc_actor");
ScopeRuns measure_add_companion_actor("DbPopulator::add_companion_actor");

class MeasureScope {
  public:
    MeasureScope(ScopeRuns& sr)
        : m_sr(sr) {
        m_sr.enter();
    }
    ~MeasureScope() {
        m_sr.exit();
    }

    ScopeRuns& m_sr;
};

template <typename T>
auto append_or_null(pqxx::params& params, std::optional<T>& maybe_val) -> void {
    if (maybe_val) {
        params.append(*maybe_val);
    } else {
        params.append();
    }
}

template <typename T>
auto append_or_null(pqxx::params& params, const std::optional<T>& maybe_val) -> void {
    if (maybe_val) {
        params.append(*maybe_val);
    } else {
        params.append();
    }
}

DbPopulator::DbPopulator(const DbPopulator::ConnStr& conn_str,
                         const DbPopulator::LogfileFilename& logfile_filename,
                         Timestamps::timestamp logfile_ts,
                         ExistingLogfileBehavior existing_logfile_behavior) {
    BLT(info) << "DbPopulator: Connecting to database using conn_str: " << std::quoted(conn_str.cref());
    m_cx = std::make_unique<pqxx::connection>(conn_str.val());
    BLT(info) << "DbPopulator: Successfully connected to db: " << std::quoted(m_cx->dbname());

    m_tx = std::make_unique<pqxx::nontransaction>(*m_cx);

    m_db_version = m_tx->query_value<std::string>("SELECT id FROM Version");
    BLT(info) << "DbPopulator: Database version: " << std::quoted(m_db_version);
    
    const auto lfn = std::filesystem::path(logfile_filename.val()).filename().string();
    auto logfile_id = m_tx->query01<int, bool>("SELECT id, fully_parsed FROM Log_File WHERE filename = $1",
                                         pqxx::params(lfn));
    if (logfile_id) {
        auto lf_id = std::get<0>(*logfile_id);
        auto finished = std::get<1>(*logfile_id);
        BLT(info) << "DbPopulator: DB has logfile " << std::quoted(lfn) << " with id=" << m_logfile_id
                     << ", fully_parsed=" << finished;
        auto always_delete = (existing_logfile_behavior == ExistingLogfileBehavior::DELETE_ON_EXISTING);
        auto delete_if_unfinished = (existing_logfile_behavior == ExistingLogfileBehavior::DELETE_ON_EXISTING_UNFINISHED);
        if (always_delete || (delete_if_unfinished && !finished)) {
            BLT(info) << "DbPopulator: Requested behavior is to delete.";

            BLT(info) << "DbPopulator: Deleting Event entries corresponding to existing logfile.";
            m_tx->exec("DELETE FROM Event WHERE logfile = $1", pqxx::params(lf_id));

            BLT(info) << "DbPopulator: Deleting Combats that came from existing logfile.";
            m_tx->exec("DELETE FROM Combat WHERE logfile = $1", pqxx::params(lf_id));

            BLT(info) << "DbPopulator: Deleting duplicate Log_File entry.";
            m_tx->exec("DELETE FROM Log_File WHERE id = $1", pqxx::params(lf_id));
        } else {
            BLT(fatal) << "DbPopulator: Requested behavior is to throw if the existing logfile is not fully parsed.";
            throw duplicate_logfile{"DbPopulator: Duplicate logfile in database", finished};
        }
    } else {
        BLT(debug) << "DbPopulator: No exiting logfile " << std::quoted(lfn);
    }
    
    BLT(info) << "Add new Log_File entry to database.";
    m_parsing_finished = false;
    auto logfile_creation_ms = Timestamps::timestamp_to_ms_past_epoch(logfile_ts);
    m_logfile_id = m_tx->query_value<int>("INSERT INTO Log_File (filename, creation_ts, fully_parsed) VALUES \
                                             ($1, $2, $3) RETURNING id",
                                          pqxx::params(/*1*/lfn, /*2*/logfile_creation_ms, /*3*/false));
}

DbPopulator::~DbPopulator() {
    // Nothing special to do here, just ensure destructor implementation has access to full definition of pqxx objects -
    // we include pqxx in this file.
}

auto DbPopulator::mark_fully_parsed(void) -> void {
    BLT(info) << "mark_fully_parsed";
    m_tx->exec("UPDATE Log_File SET fully_parsed = TRUE WHERE id = $1", pqxx::params(m_logfile_id));
}

// A simple but inefficient implementation - see if the name_id is in the database and if not populate it.
auto DbPopulator::add_name_id(const lpt::NameId& name_id) -> int {
    MeasureScope meas(measure_add_name_id);

    // Store a reference to the key's mapped value.
    auto& row_id = m_names[name_id.id];
    if (row_id != int{}) { // operator[] inserts a new key with default initialized value
        // Name is in cache.
        return row_id;
    }

    // name is not in cache. Is it in the database?
    auto maybe_row_id = m_tx->query01<int>("SELECT id FROM Name WHERE name_id = $1", pqxx::params(name_id.id));
    if (maybe_row_id) {
        // Name is in database. Add to cache.
        row_id = std::get<0>(*maybe_row_id);
        return row_id;
    }
    
    // Name isn't in database. Add to database and cache.
    row_id = m_tx->query_value<int>("INSERT INTO Name (name_id, name) VALUES ($1, $2) RETURNING id",
                                    pqxx::params(name_id.id, name_id.name));
    return row_id;
}

auto DbPopulator::add_pc_class(const DbPopulator::PcClass& pc_class) -> int {
    MeasureScope meas(measure_add_pc_class);

    BLT(info) << "add_pc_class: style.name=" << std::quoted(pc_class.style.cref().name)
              << ", advanced_class.name=" << std::quoted(pc_class.advanced_class.cref().name);

    auto key = std::tuple<uint64_t,uint64_t>(pc_class.style.val().id, pc_class.advanced_class.val().id);
    auto& row_id = m_classes[key];
    if (row_id != int{}) {
        return row_id;
    }
    pqxx::params params {/*1*/pc_class.style.val().id, /*2*/pc_class.advanced_class.val().id};
    auto cid = m_tx->query01<int>("SELECT Advanced_Class.id FROM Advanced_Class \
                                    JOIN Name AS n1 ON Advanced_Class.style = n1.id \
                                    JOIN Name AS n2 ON Advanced_Class.class = n2.id \
                                    WHERE (n1.name_id, n2.name_id) = ($1, $2)", params);
    if (cid) {
        row_id = std::get<0>(*cid);
        return row_id;
    }

    auto style_id = add_name_id(pc_class.style.val());
    auto advanced_class_id = add_name_id(pc_class.advanced_class.val());
    row_id = m_tx->query_value<int>("INSERT INTO Advanced_Class (style, class) VALUES ($1, $2) RETURNING id",
                                              pqxx::params{style_id, advanced_class_id});
    return row_id;
}

auto DbPopulator::add_npc_actor(const lpt::NpcActor& npc_actor) -> int {
    MeasureScope meas(measure_add_npc_actor);
    auto key = std::tuple<uint64_t, uint64_t>(npc_actor.name_id.id, npc_actor.instance);
    auto& row_id = m_npcs[key];
    if (row_id != int{}) {
        return row_id;
    }

    auto npc_name_id = add_name_id(npc_actor.name_id);

    pqxx::params params {/*1*/"npc", /*2*/npc_name_id, /*3*/npc_actor.instance};
    auto o_row_id = m_tx->query01<int>("SELECT id FROM Actor WHERE (type, name, instance) = ($1, $2, $3)", params);
    if (o_row_id) {
        row_id = std::get<0>(*o_row_id);
        return row_id;
    }
    row_id = m_tx->query_value<int>("INSERT INTO Actor (type, name, instance) VALUES ($1, $2, $3) RETURNING id", params);
    return row_id;
}

// Possible state on entry:
//
// 1. There IS/IS NOT A PC Actor row with pc_actor's name_id with the "unknown" class.
// 2. There IS/IS NOT an entry in m_pcs for pc_actor's name_id.
//
// Input constraints:
//
// None.
//
// Output constraints:
//
// 1. m_pcs has an entry that relates the pc_actor's name_id to both an PC Actor row matching the pc_actor's name_id and
//    the advanced class ID in the Actor row.
auto DbPopulator::add_pc_actor(const lpt::PcActor& pc_actor) -> int {
    MeasureScope meas(measure_add_pc_actor);
    BLT(info) << "add_pc_actor: pc_actor name.id = " << pc_actor.id;

    if (m_pcs.contains(pc_actor.id)) {
        BLT(info) << "add_pc_actor: PC name_id found in m_pcs cache.";
        return m_pcs[pc_actor.id].row_id;
    }
    BLT(info) << "add_pc_actor: PC name_id not found in m_pcs cache.";

    auto name_row_id = add_name_id(pc_actor);
    BLT(info) << "add_pc_actor: name_row_id = " << name_row_id;

    auto params = pqxx::params(ACTOR_PC_CLASS_TYPE_NAME, name_row_id, UNKNOWN_CLASS_ROW_ID);

    auto res = m_tx->query01<int>("SELECT id FROM Actor WHERE (type, name, class) = ($1, $2, $3)", params);
    if (res) {
        auto actor_id = std::get<0>(*res); 
        BLT(info) << "add_pc_actor: Found row id=" << actor_id << " for PC matching name with unknown class."
                  << " Add m_pcs cache entry.";
        m_pcs[pc_actor.id] = ActorRowInfo {.row_id = actor_id, .class_id = UNKNOWN_CLASS_ROW_ID};
        return actor_id;
    }
    BLT(info) << "add_pc_actor: Did not find Actor row for PC name with 'unknown' class. Add new one.";

    auto id = m_tx->query_value<int>("INSERT INTO Actor (type, name, class) VALUES ($1, $2, $3) RETURNING id",
                                     params);
    BLT(info) << "add_pc_actor: New actor row id=" << id;
    m_pcs[pc_actor.id] = ActorRowInfo {.row_id = id, .class_id = UNKNOWN_CLASS_ROW_ID};
    return id;
}

auto DbPopulator::add_companion_actor(const lpt::CompanionActor& comp_actor) -> int {
    MeasureScope meas(measure_add_companion_actor);
    auto comp_name_row_id = add_name_id(comp_actor.companion.name_id);
    auto pc_actor_row_id = add_pc_actor(comp_actor.pc);
    BLT(info) << "add_companion_actor: comp_name_row_id = " << comp_name_row_id
              << ", pc_actor_row_id = " << pc_actor_row_id;

    pqxx::params params(DbPopulator::ACTOR_COMPANION_CLASS_TYPE_NAME,
                        comp_name_row_id,
                        pc_actor_row_id,
                        comp_actor.companion.instance);
    auto maybe_comp_id = m_tx->query01<int>("SELECT id FROM Actor WHERE (type, name, pc, instance) = ($1, $2, $3, $4)",
                                            params);
    if (maybe_comp_id) {
        auto comp_id = std::get<0>(*maybe_comp_id);
        BLT(info) << "add_companion_actor: Row for companion actor found, id = " << comp_id;
        return comp_id;
    }
    BLT(info) << "add_companion_actor: Row for companion actor not found";

    BLT(info) << std::format("INSERT INTO Actor (type, name, pc, instance) VALUES ({}, {}, {}, {}) RETURNING id",
                              DbPopulator::ACTOR_COMPANION_CLASS_TYPE_NAME,
                              comp_name_row_id,
                              pc_actor_row_id,
                              comp_actor.companion.instance);
    auto comp_row_id = m_tx->query_value<int>("INSERT INTO Actor (type, name, pc, instance) VALUES ($1, $2, $3, $4) \
                                                   RETURNING id", params);
    BLT(info) << "add_companion_actor: Insert new row for companion actor at id = " << comp_row_id;
    return comp_row_id;
}

auto DbPopulator::add_class_to_pc_actor(const lpt::PcActor& pc_actor, const DbPopulator::PcClass& pc_class) -> int {
    BLT(info) << "add_class_to_pc_actor: pc_actor.name=" << std::quoted(pc_actor.name)
              << ", pc_class.style.name=" << std::quoted(pc_class.style.cref().name)
              << ", pc_class.advanced_class.name = << " << std::quoted(pc_class.advanced_class.cref().name);
    auto class_id = add_pc_class(pc_class);

    // Simplest case. The pc/class Actor already exists and we know about it.
    if (class_id == m_pcs[pc_actor.id].class_id) {
        return m_pcs[pc_actor.id].row_id;
    }

    // Get all PC Actors that have the same name as `pc_actor`.
    auto rows = m_tx->exec("SELECT act.id, act.class FROM Actor AS act"
                           "  JOIN Name as actn ON act.name = actn.id"
                           " WHERE (type, actn.name_id) = ($1, $2)", pqxx::params(ACTOR_PC_CLASS_TYPE_NAME, pc_actor.id));
    std::map<int, int> m_class_to_actor;
    for (auto row : rows) {
        m_class_to_actor[row[1].as<int>()] = row[0].as<int>();
    }

    if (m_class_to_actor.contains(class_id)) {
        // A row for `pc_actor` already exists with class `pc_class`. Use that row.
        m_pcs[pc_actor.id] = ActorRowInfo {.row_id = m_class_to_actor[class_id], .class_id = class_id};
        return m_pcs[pc_actor.id].row_id;
    }

    if (m_class_to_actor.contains(UNKNOWN_CLASS_ROW_ID)) {
        // A row for `pc_actor` with the "unknown" class exists. Update that row with `pc_class`.
        auto row_id = m_class_to_actor[UNKNOWN_CLASS_ROW_ID];
        m_tx->exec("UPDATE Actor SET class = $1 WHERE id = $2",
                   pqxx::params(class_id, row_id));
        m_pcs[pc_actor.id] = ActorRowInfo {.row_id = row_id, .class_id = class_id};
        return row_id;
    }

    // The database contains neither a row with the same `pc_class` nor a row with the "unknown" class. Add a new row
    // for our `pc_actor`/`pc_class` combination.
    auto actor_name_id = m_tx->query_value<int>("SELECT id FROM Name WHERE name_id = $1", pqxx::params(pc_actor.id));
    auto row_id = m_tx->query_value<int>("INSERT INTO Actor (type, name, class) VALUES ($1, $2, $3) RETURNING id",
                                         pqxx::params(ACTOR_PC_CLASS_TYPE_NAME, actor_name_id, class_id));
    m_pcs[pc_actor.id] = ActorRowInfo {.row_id = row_id, .class_id = class_id};
    return row_id;
}

auto DbPopulator::add_action(const lpt::Action& action) -> int {
    MeasureScope meas(measure_add_action);
    auto key = std::tuple<uint64_t, uint64_t, uint64_t>(action.verb.cref().id,
                                                        action.noun.cref().id,
                                                        action.detail.cref() ? action.detail.cref()->id : NOT_APPLICABLE_ROW_ID);
    auto& row_id = m_actions[key];
    if (row_id != int{}) {
        return row_id;
    }
    auto verb_row_id = add_name_id(action.verb);
    auto noun_row_id = add_name_id(action.noun);
    auto detail_row_id = action.detail.cref() ? add_name_id(*action.detail.cref()) : NOT_APPLICABLE_ROW_ID;
    pqxx::params params(verb_row_id, noun_row_id, detail_row_id);

    auto maybe_action = m_tx->query01<int>("SELECT id FROM Action WHERE (verb, noun, detail) = ($1, $2, $3)", params);
    if (maybe_action) {
        row_id = std::get<0>(*maybe_action);
        return row_id;
    }

    row_id = m_tx->query_value<int>("INSERT INTO Action (verb, noun, detail) VALUES ($1, $2, $3) RETURNING id", params);
    return row_id;
}

auto DbPopulator::add_actor(const lpt::Actor& actor) -> int {
    int actor_row_id {};
    if (std::holds_alternative<lpt::NpcActor>(actor)) {
        actor_row_id = add_npc_actor(std::get<lpt::NpcActor>(actor));
    } else if (std::holds_alternative<lpt::PcActor>(actor)) {
        actor_row_id =  add_pc_actor(std::get<lpt::PcActor>(actor));
    } else if (std::holds_alternative<lpt::CompanionActor>(actor)) {
        actor_row_id = add_companion_actor(std::get<lpt::CompanionActor>(actor));
    } else {
        throw std::logic_error("DbPopulator::add_actor(): Unknown actor type - not NPC, PC, or Companion.");
    }

    return actor_row_id;
}

auto DbPopulator::record_area_entered(DbPopulator::AreaName area, std::optional<DbPopulator::DifficultyName> difficulty) -> int {
    auto area_id = add_name_id(area);
    auto difficulty_id = difficulty ? add_name_id(*difficulty) : DIFFICULTY_NONE_ROW_ID;
    BLT(info) << "record_area_entered: area.name=" << std::quoted(area.cref().name);

    auto row_id = m_tx->query01<int>("SELECT id FROM Area WHERE (area, difficulty) = ($1, $2)",
                                     pqxx::params(area_id, difficulty_id));
    if (row_id) {
        m_area_id = std::get<0>(*row_id);
        return *m_area_id;
    }
    pqxx::params params(area_id, difficulty_id);

    m_area_id = m_tx->query_value<int>("INSERT INTO Area (area, difficulty) VALUES ($1, $2) RETURNING id", params);
    return *m_area_id;
}

auto DbPopulator::record_enter_combat(const Timestamps::timestamp& combat_begin) -> int {
    const auto begin_ms = Timestamps::timestamp_to_ms_past_epoch(combat_begin);
    BLT(info) << "record_enter_combat: ts=" << begin_ms;
    m_combat_id = m_tx->query_value<int>("INSERT INTO Combat (ts_begin, area, logfile) VALUES ($1, $2, $3) RETURNING id",
                                         pqxx::params(begin_ms, *m_area_id, m_logfile_id));
    return *m_combat_id;
}

auto DbPopulator::record_exit_combat(const Timestamps::timestamp& combat_end) -> int {
    if (!m_combat_id) {
        BLT(error) << "record_exit_combat: NOT currently in combat. Ignoring.";
        return 0;
    }
    const auto end_ms = Timestamps::timestamp_to_ms_past_epoch(combat_end);
    BLT(info) << "record_exit_combat: ts=" << end_ms;
    m_tx->exec("UPDATE Combat SET ts_end = $1 WHERE id = $2", pqxx::params(end_ms, *m_combat_id));
    auto ret = *m_combat_id;
    m_combat_id.reset();
    return ret;
}


auto DbPopulator::populate_from_entry(const lpt::ParsedLogLine& entry) -> int {
    pqxx::params params;

    // 1 - timestamp
    // 2 - combat
    // 3 - source actor
    // 4 - source_location
    // 5 - source_health
    // 6 - target actor
    // 7 - target_location
    // 8 - target_health

    // Fill in event row in the order of the SQL declaration. We'll pass in everything, so we'll set NULLs for fields we
    // don't have.
    params.append(/*1*/Timestamps::timestamp_to_ms_past_epoch(entry.ts));

    // These actions have handling that must occur before we can populate the event values. Specifically, if this is
    // event enters combat, we need to know the ID of the current Combat row.
    if (entry.action.noun.cref().id == ENTER_COMBAT_ID) {
        record_enter_combat(entry.ts);
    } else if (entry.action.noun.cref().id == EXIT_COMBAT_ID) {
        record_exit_combat(entry.ts);
    }

    std::optional<int> src_actor_id {};

    append_or_null(params, /*2*/m_combat_id);

    int source_pc_actor_row_id {};
    if (entry.source) {
        source_pc_actor_row_id = add_actor(entry.source->actor);
        params.append(/*3*/source_pc_actor_row_id);
        params.append(/*4*/entry.source->loc);
        params.append(/*5*/entry.source->health);
    } else {
        params.append(/*3*/);
        params.append(/*4*/);
        params.append(/*5*/);
    }

    if (entry.target) {
        params.append(/*6*/add_actor(entry.target->actor));
        params.append(/*7*/entry.target->loc);
        params.append(/*8*/entry.target->health);
    } else {
        params.append(/*6*/);
        params.append(/*7*/);
        params.append(/*8*/);
    }
        
    // 9 - ability
    // 10- action

    int ability_row_id {};
    if (entry.ability) {
        ability_row_id = add_name_id(*entry.ability);
        params.append(/*9*/ability_row_id);
    } else {
        params.append(/*9*/);
    }

    params.append(/*10*/add_action(entry.action));

    // Handle special cases which require tables other than Event to be updated. These also manage some local state.
    if (entry.action.verb.cref().id == DISCIPLINE_CHANGED_ID) {
        auto& actor = std::get<LogParserTypes::PcActor>(entry.source->actor);
        auto pc_class = PcClass {CombatStyle(entry.action.noun.cref()), AdvancedClass(*entry.action.detail.cref())};
        // This must be done *after* we've encountered the actor's name (as a source or target).
        add_class_to_pc_actor(actor, pc_class);
    } else if (entry.action.verb.cref().id == AREA_ENTERED_ID) {
        record_area_entered(AreaName(entry.action.noun.cref()),
                            std::optional<DifficultyName>{entry.action.detail.val()});
    } 
    
    // 11- value_version
    // 12- value_base
    // 13- value_crit
    // 14- value_effective
    // 15- value_type
    // 16- value_mitigation_reason
    // 17- value_mitigation_effect_value
    // 18- value_mitigation_effect_value_name

    if (entry.value) {
        if (std::holds_alternative<LogParserTypes::LogInfoValue>(*entry.value)) {
            params.append(/*11*/std::get<LogParserTypes::LogInfoValue>(*entry.value).info);
            params.append(/*12*/);
            params.append(/*13*/);
            params.append(/*14*/);
            params.append(/*15*/);
            params.append(/*16*/);
            params.append(/*17*/);
            params.append(/*18*/);
        } else {
            params.append(/*11*/);
            auto& v = std::get<LogParserTypes::RealValue>(*entry.value);
            params.append(/*12*/v.base_value);
            params.append(/*13*/v.crit);
            append_or_null(params, /*14*/(v.effective));
            /*15*/ if (v.type) params.append(add_name_id(*v.type)); else params.append();
            /*16*/ if (v.mitigation_reason) params.append(add_name_id(*v.mitigation_reason)); else params.append();
            if (v.mitigation_effect) {
                append_or_null(params, /*17*/v.mitigation_effect->value);
                if (v.mitigation_effect->effect) {
                    params.append(/*18*/add_name_id(*v.mitigation_effect->effect));
                } else {
                    params.append(/*18*/);
                }
            } else {
                params.append(/*17*/);
                params.append(/*18*/);
            }
        }
    } else {
        params.append(/*11*/);
        params.append(/*12*/);
        params.append(/*13*/);
        params.append(/*14*/);
        params.append(/*15*/);
        params.append(/*16*/);
        params.append(/*17*/);
        params.append(/*18*/);
    }
        
    // 19- threat_val
    // 20- threat_str

    if (entry.threat) {
        if (std::holds_alternative<double>(*entry.threat)) {
            params.append(/*19*/static_cast<int>(std::get<double>(*entry.threat)));
            params.append(/*20*/);
        } else {
            params.append(/*19*/);
            params.append(/*20*/std::get<std::string>(*entry.threat));
        }
    } else {
        params.append(/*19*/);
        params.append(/*20*/);
    }        

    // 21- logfile
    params.append(m_logfile_id);

    auto ins = "INSERT INTO Event "
        /*1 */"(ts"
        /*2 */",combat"
        /*3 */",source"
        /*4 */",source_location"
        /*5 */",source_health"
        /*6 */",target"
        /*7 */",target_location"
        /*8 */",target_health"
        /*9 */",ability"
        /*10*/",action"
        /*11*/",value_version"
        /*12*/",value_base"
        /*13*/",value_crit"
        /*14*/",value_effective"
        /*15*/",value_type"
        /*16*/",value_mitigation_reason"
        /*17*/",value_mitigation_effect_value"
        /*18*/",value_mitigation_effect_value_name"
        /*19*/",threat_val"
        /*20*/",threat_str"
        /*21*/",logfile) VALUES"
        "($1"
        ",$2"
        ",$3"
        ",$4"
        ",$5"
        ",$6"
        ",$7"
        ",$8"
        ",$9"
        ",$10"
        ",$11"
        ",$12"
        ",$13"
        ",$14"
        ",$15"
        ",$16"
        ",$17"
        ",$18"
        ",$19"
        ",$20"
        ",$21) RETURNING id";

    auto event_id = m_tx->query_value<int>(ins, params);

    return event_id;
}
