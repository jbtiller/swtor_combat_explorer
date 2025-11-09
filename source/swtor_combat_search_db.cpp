#include <getopt.h>
#include <iostream>

#include <gflags/gflags.h>
#include "db_populator.hpp"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wall"
#include <pqxx/pqxx>
#pragma GCC diagnostic pop

DEFINE_string(abilities_for_class,      "",    "Show abilities for class in form \"style,discipline\"");
DEFINE_bool(all_abilities,              false, "All abilities");
DEFINE_bool(all_actions,                false, "Show all actions");
DEFINE_bool(all_action_verbs,           false, "Show just the unique verbs in actions");
DEFINE_bool(all_class_abilities,        false, "Show all abilities per class");
DEFINE_bool(all_classes,                false, "Show all classes");
// TODO
DEFINE_bool(all_class_unique_abilities, false, "Show unique abilities for each class");
DEFINE_bool(all_combats,                false, "Show all combats");
DEFINE_bool(all_action_events,          false, "Show all nouns/details with the 'Event' verb");
// TODO
DEFINE_string(class_unique_abilities,   "",    "Show unique abilities for a specific class in the form \"style,discipline\"");
DEFINE_bool(duplicate_name_counts,      false, "Show how many names have the same string but different id");
DEFINE_bool(pcs_in_combats,             false, "Show all PCs in all combats");

auto main(int argc, char* argv[]) -> int {
    gflags::SetUsageMessage("Extract information from the SW:ToR combat database");
    gflags::ParseCommandLineFlags(&argc, &argv, true);

    auto cx = pqxx::connection{"dbname = swtor_combat_explorer   user = jason   password = jason"};
    auto tx = pqxx::nontransaction{cx};

    if (!FLAGS_abilities_for_class.empty()) {
        auto comma_pos = std::find(FLAGS_abilities_for_class.begin(), FLAGS_abilities_for_class.end(), ',');
        if (   comma_pos == FLAGS_abilities_for_class.end()
            || FLAGS_abilities_for_class.starts_with(',') 
            || FLAGS_abilities_for_class.ends_with(',')) {
            std::cerr << "Supplied class style/discipline, " << std::quoted(FLAGS_abilities_for_class)
                      << " is incorrectly formatted\n";
            return 1;
        }
        auto style = std::string_view(FLAGS_abilities_for_class.begin(), comma_pos);
        auto discipline = std::string_view(comma_pos + 1, FLAGS_abilities_for_class.end());
        auto res = tx.exec(" SELECT DISTINCT ab.name_id,ab.name FROM Event"
                           "     JOIN Actor as act ON Event.source = act.id"
                           "     JOIN Advanced_Class AS ac ON act.class = ac.id"
                           "     JOIN Name as sn ON ac.style = sn.id"
                           "     JOIN Name as dn ON ac.class = dn.id"
                           "     JOIN Name AS ab ON Event.ability = ab.id"
                           " WHERE Event.source IS NOT NULL"
                           "   AND act.type = 'pc'"
                           "   AND Event.ability IS NOT NULL"
                           "   AND sn.name = $1"
                           "   AND dn.name = $2"
                           " ORDER BY ab.name", pqxx::params(style, discipline));
        std::cout << "ability_id,ability_name\n";
        for (auto row : res) {
            auto [ability_id, ability_name] = row.as<uint64_t,std::string_view>();
            std::cout << ability_id << "," << ability_name << "\n";
        }
        std::cout << res.size() << " rows\n";
    }

    if (FLAGS_all_abilities) {
        std::cout << "You requested all abilities.\n";
        auto res = tx.exec(" SELECT DISTINCT ab.name_id, ab.name FROM Event"
                           "     JOIN Name as ab ON Event.ability = ab.id"
                           " WHERE Event.ability IS NOT NULL"
                           " ORDER BY ab.name");
        std::cout << "ability_id,ability_name\n";
        for (auto row : res) {
            auto [ab_id, ab_name] = row.as<uint64_t, std::string_view>();
            std::cout << ab_id << "," << ab_name << "\n";
        }
        std::cout << res.size() << " rows\n";
    }

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
        std::cout << res.size() << " rows\n";
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
        std::cout << res.size() << " rows\n";
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
        std::cout << res.size() << " rows\n";
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
        std::cout << res.size() << " rows\n";
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
        std::cout << res.size() << " rows\n";
    }
    if (FLAGS_all_action_events) {
        std::cout << "You requested the nouns and details associated with the 'Event' action verb.\n";
        auto res = tx.exec(" SELECT DISTINCT an.name_id, an.name, ad.name_id, ad.name FROM Action"
                           "     JOIN Name AS an ON Action.noun = an.id"
                           "     JOIN Name AS ad ON Action.detail = ad.id"
                           " WHERE Action.verb = (SELECT id FROM Name WHERE name = 'Event')"
                           " ORDER BY an.name");
        std::cout << "noun_id,noun_name,detail_id,detail_name\n";
        for (auto row : res) {
            auto [noun_id, noun_name, detail_id, detail_name] = row.as<uint64_t,std::string_view,uint64_t,std::string_view>();
            std::cout << noun_id << "," << noun_name << "," << detail_id << "," << detail_name << "\n";
        }
        std::cout << res.size() << " rows\n";
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
            auto [combat_id, area_name, difficulty_name, pc_name] = row.as<int, std::string, std::string, std::string>();
            std::cout << combat_id << "," << area_name << ", " << difficulty_name << ", " << pc_name << "\n";
        }
        std::cout << res.size() << " rows\n";
    }

    if (FLAGS_duplicate_name_counts) {
        std::cout << "You requested a count of how many times each Name string is duplicated.\n";
        auto res = tx.exec(" SELECT Name.name, COUNT(*) as num_duplicates FROM Name"
                           " GROUP BY Name.name"
                           " HAVING COUNT(*) > 1"
                           " ORDER BY num_duplicates, Name.name");
        std::cout << "name,num_duplicates\n";
        for (auto row : res) {
            auto [name, num_duplicates] = row.as<std::string_view, int>();
            std::cout << name << "," << num_duplicates << "\n";
        }
        std::cout << res.size() << " rows\n";
    }

    return 0;
}
