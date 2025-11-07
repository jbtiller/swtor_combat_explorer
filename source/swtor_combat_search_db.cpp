#include <getopt.h>
#include <iostream>

#include <gflags/gflags.h>
#include "db_populator.hpp"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wall"
#include <pqxx/pqxx>
#pragma GCC diagnostic pop

const uint64_t APPLY_EFFECT_ID  {836045448945477ULL};
const uint64_t REMOVE_EFFECT_ID {836045448945478ULL};

DEFINE_bool(all_actions,         false, "Show all actions");
DEFINE_bool(all_action_verbs,    false, "Show just the unique verbs in actions");
DEFINE_bool(all_class_abilities, false, "Show all abilities per class");
DEFINE_bool(all_classes,         false, "Show all classes");
DEFINE_bool(all_combats,         false, "Show all combats");
DEFINE_bool(all_effects,         false, "Show all effects");
DEFINE_bool(num_events,          false, "Show number of Events");
DEFINE_bool(num_logfiles,        false, "Show number of Log_Files");
DEFINE_bool(pcs_in_combats,      false, "Show all PCs in all combats");

auto main(int argc, char* argv[]) -> int {
    gflags::SetUsageMessage("Extract information from the SW:ToR combat database");
    gflags::ParseCommandLineFlags(&argc, &argv, true);

    auto cx = pqxx::connection{"dbname = swtor_combat_explorer   user = jason   password = jason"};
    auto tx = pqxx::nontransaction{cx};

    if (FLAGS_all_action_verbs) {
        std::cout << "You requested all unique verbs in actions.\n";
        auto res = tx.exec(" SELECT DISTINCT _v.name FROM Action"
                           "     JOIN Name as _v ON Action.verb = _v.id"
                           " ORDER BY _v.name");
        std::cout << "verb\n";
        for (auto row : res) {
            auto [verb] = row.as<std::string_view>();
            std::cout << verb << "\n";
        }
        std:: cout << "rows=" << res.size() << "\n";
    }
    if (FLAGS_all_actions) {
        std::cout << "You requested all actions.\n";
        auto res = tx.exec(" SELECT DISTINCT _v.name, _n.name, _d.name FROM Action AS _a"
                           "     JOIN Name AS _v ON _a.verb = _v.id"
                           "     JOIN Name AS _n ON _a.noun = _n.id"
                           "     JOIN Name AS _d ON _a.detail = _d.id"
                           " ORDER BY _v.name, _n.name, _d.name");
        std::cout << "verb,noun,detail\n";
        for (auto row : res) {
            auto [verb, noun, detail] = row.as<std::string_view,std::string_view, std::string_view>();
            std::cout << verb << "," << noun << "," << detail << "\n";
        }
        std:: cout << "rows=" << res.size() << "\n";
    }
    if (FLAGS_all_class_abilities) {
        std::cout << "You requested all abilitities for all classes.\n";
        auto res = tx.exec(" SELECT DISTINCT _style.name, _discipline.name, _ability.name FROM Event"
                           "     JOIN Actor ON Event.source = Actor.id"
                           "     JOIN Name AS _ability ON Event.ability = _ability.id"
                           "     JOIN Advanced_Class AS ac ON Actor.class = ac.id"
                           "       JOIN Name AS _style ON ac.style = _style.id"
                           "       JOIN Name AS _discipline ON ac.class = _discipline.id"
                           " WHERE Event.source IS NOT NULL"
                           "   AND Actor.type = $1"
                           "   AND Event.ability IS NOT NULL"
                           "   AND Actor.class != $2"
                           " ORDER BY _style.name, _discipline.name, _ability.name",
                           pqxx::params(DbPopulator::ACTOR_PC_CLASS_TYPE_NAME,
                                        DbPopulator::UNKNOWN_CLASS_ROW_ID));
        std::cout << "style,discipline,ability\n";
        for (auto row : res) {
            auto [style, discipline, ability] = row.as<std::string_view,std::string_view, std::string_view>();
            std::cout << style << "," << discipline << "," << ability << "\n";
        }
        std:: cout << "rows=" << res.size() << "\n";
    }
    if (FLAGS_all_classes) {
        std::cout << "You requested all classes.\n";
        auto res = tx.exec(" SELECT style_name.name, discipline_name.name FROM Advanced_Class AS ac" 
                           "     JOIN Name AS style_name ON ac.style = style_name.id"
                           "     JOIN Name AS discipline_name ON ac.class = discipline_name.id"
                           " WHERE ac.id != $1"
                           " ORDER BY style_name.name, discipline_name.name",
                           pqxx::params(DbPopulator::UNKNOWN_CLASS_ROW_ID));
        std::cout << "style,discipline\n";
        for (auto row : res) {
            auto [style, discipline] = row.as<std::string_view,std::string_view>();
            std::cout << style << "," << discipline << "\n";
        }
        std:: cout << "rows=" << res.size() << "\n";
    }
    if (FLAGS_all_combats) {
        std::cout << "You requested all combats.\n";
        auto res = tx.exec("SELECT ts_begin, an.name, lf.filename FROM Combat"
                           "    JOIN Area as ar  ON Combat.area = ar.id" 
                           "      JOIN Name as an  ON ar.area = an.id"
                           "    JOIN Log_File as lf ON combat.logfile = lf.id"
                           "  ORDER BY an.name");
        std::cout << "begin_ts,area,logfile\n";
        for (auto row : res) {
            auto [begin_ts, area, logfile] = row.as<uint64_t,std::string_view,std::string_view>();
            std::cout << begin_ts << "," << area << "," << logfile << "\n";
        }
        std:: cout << "rows=" << res.size() << "\n";
    }
    if (FLAGS_pcs_in_combats) {
        std::cout << "You requested the PCs for all combats.\n";
        auto res = tx.exec("SELECT DISTINCT"
                           "  combat as combat_id"
                           ", area_name.name as area_name"
                           ", difficulty_name.name as difficulty_name"
                           ", pc_name.name as pc_name"
                           "    FROM Event"
                           "     JOIN Combat on Event.combat = Combat.id"
                           "     JOIN Actor on Event.source = Actor.id"
                           "       JOIN Name as pc_name ON Actor.name = pc_name.id"
                           "     JOIN Area on Combat.area = Area.id"
                           "       JOIN Name as area_name ON area.area = area_name.id"
                           "       JOIN Name as difficulty_name ON area.difficulty = difficulty_name.id"
                           " WHERE Actor.type = 'pc' AND combat IS NOT NULL"
                           " GROUP BY combat_id, area_name, difficulty_name, pc_name"
                           " ORDER BY combat_id, pc_name");
        std::cout << "combat, area, area, pc\n";
        for (auto row : res) {
            auto [combat_id, area_name, difficulty_name, pc_name] =
                row.as<int, std::string, std::string, std::string>();
            std::cout << combat_id << "," << area_name << ", " << difficulty_name << ", " << pc_name << "\n";
        }
        std:: cout << "rows=" << res.size() << "\n";
    }
    if (FLAGS_all_effects) {
        std::cout << "You requested all of the unique effects in the Action table (no effect has details).\n";
        auto res = tx.exec(" SELECT DISTINCT en.name FROM Action as act"
                           "     JOIN Name AS en ON act.noun = en.id"
                           "     JOIN Name AS aen ON aen.name_id = $1"
                           "     JOIN Name AS ren ON ren.name_id = $2"
                           " WHERE act.verb = aen.id OR act.verb = ren.id",
                           pqxx::params(APPLY_EFFECT_ID, REMOVE_EFFECT_ID));
        std::cout << "effect\n";
        for (auto row : res) {
            auto [eff] = row.as<std::string>();
            std::cout << eff << "\n";
        }
        std:: cout << "rows=" << res.size() << "\n";
    }
    if (FLAGS_num_events) {
        std::cout << "You requested all of the number of events in the database.\n";
        std::cout << "rows=" << tx.query_value<int>("SELECT COUNT(*) FROM Event") << "\n";
    }
    if (FLAGS_num_logfiles) {
        std::cout << "You requested all of the number of Log_Files in the database.\n";
        std::cout << "rows=" << tx.query_value<int>("SELECT COUNT(*) FROM Log_File") << "\n";
    }
    return 0;
}


