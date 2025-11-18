#include <iostream>
#include <optional>
#include <utility>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wall"
#include <gflags/gflags.h>
#include <pqxx/pqxx>
#pragma GCC diagnostic pop

#include "db_populator.hpp"
#include "sce_constants.hpp"

template <typename T>
class OverrideInScope {
  public:
    explicit OverrideInScope(T& orig, T override_value)
        : m_orig(orig)
        , m_orig_value(orig) {
        orig = override_value;
    }
    ~OverrideInScope() {
        m_orig = m_orig_value;
    }

  protected:
    T& m_orig;
    T m_orig_value;
};

DEFINE_string(abilities_for_class,      "",    "Show abilities for class in form \"style,discipline\"");
DEFINE_bool(all_abilities,              false, "All abilities");
DEFINE_bool(all_action_events,          false, "Show all nouns/details with the 'Event' verb");
DEFINE_bool(all_actions,                false, "Show all actions");
DEFINE_bool(all_action_verbs,           false, "Show just the unique verbs in actions");
DEFINE_bool(all_class_abilities,        false, "Show all abilities per class");
// TODO
DEFINE_bool(all_class_unique_abilities, false, "Show unique abilities for each class");
DEFINE_bool(all_classes,                false, "Show all classes");
DEFINE_bool(all_combats,                false, "Show all combats");
DEFINE_bool(all_effects,                false, "Show all effects");
// TODO
DEFINE_string(class_unique_abilities,   "",    "Show unique abilities for a specific class in the form \"style,discipline\"");
DEFINE_string(combat_abilities_for_class,  "", "Show abilities used in combat by class in form \"style,discipline\"");
DEFINE_int32(dump_event_by_id,          -1,    "Pretty-print the event with the supplied integer ID");
DEFINE_bool(duplicate_name_counts,      false, "Show how many names have the same string but different id");
DEFINE_string(find_name,                "",    "Given an integer, searches as a row ID or Name ID, otherwise searches as a 'LIKE' pattern;"
                                               " print row ID, name ID, and name as \"name {row_id:name_id}\"");
DEFINE_string(find_class_uses_ability,  "",    "Given an ability ID (Name ID), find classes that use that ability");
DEFINE_bool(num_events,                 false, "Show number of Events");
DEFINE_bool(num_logfiles,               false, "Show number of Log_Files");
DEFINE_string(name_details,             "t",   "What to show for Names; a combination of 't' for the name text, 'r' for the Name row ID, and 'i' for the Name ID");
DEFINE_bool(pcs_in_combats,             false, "Show all PCs in all combats");

auto pretty_print_name_id(int row_id, uint64_t name_id, const std::string_view name) {
    auto t = FLAGS_name_details.find('t') != std::string::npos;
    auto r = FLAGS_name_details.find('r') != std::string::npos;
    auto i = FLAGS_name_details.find('i') != std::string::npos;
    auto bits = (t << 2) + (r << 1) + i;
    // tri
    switch (bits) {
      case 0b111:
        return std::format("{} {{{}:{}}}", name, row_id, name_id);
      case 0b110:
        return std::format("{} {{{}}}", name, row_id);
      case 0b101:
        return std::format("{} {{{}}}", name, name_id);
      case 0b100:
        return std::format("{}", name);
      case 0b011:
        return std::format("{}:{}", row_id, name_id);
      case 0b010:
        return std::format("{}", row_id);
      case 0b001:
        return std::format("{}", name_id);
      case 0b000:
        return std::format("{}", name);
      default:
        return std::format("{}", name);
    }
}

auto pretty_print_name(pqxx::nontransaction& tx, int row_id) -> std::string {
    auto [name_id, name] = tx.query1<uint64_t, std::string_view>("SELECT Name.name_id, Name.name FROM Name WHERE Name.id = $1", row_id);
    return pretty_print_name_id(row_id, name_id, name);
}

auto pretty_print_action(pqxx::nontransaction& tx, int row_id) -> std::string {
    auto row = tx.exec("SELECT verb, noun, detail FROM Action WHERE id = $1", row_id)[0];
    auto verb_name = pretty_print_name(tx, row[0].as<int>());
    auto noun_name = pretty_print_name(tx, row[1].as<int>());
    auto detail_name = !row[2].is_null() ? pretty_print_name(tx, row[2].as<int>()) : "n/a";
    return std::format("{}: {}/{}", verb_name, noun_name, detail_name);
}

auto pretty_print_class(pqxx::nontransaction& tx, int row_id) -> std::string {
    auto [style, discipline] = tx.query1<int,int>(" SELECT style.id, disc.id FROM Advanced_Class as ac"
                                                  "     JOIN Name AS style ON ac.style = style.id"
                                                  "     JOIN Name AS disc  ON ac.class = class.id"
                                                  " WHERE ac.id = $1", row_id);
    return std::format("{} {}", pretty_print_name(tx, style), pretty_print_name(tx, discipline));
}

auto pretty_print_event(pqxx::nontransaction& tx, int row_id) -> std::string {
    // We use LEFT JOIN here because it will handle NULLs in the FKs, since we have a bunch of nullable FKs. A standard
    // (INNER) JOIN would fail because if the FK is NULL then an (INNER) JOIN would fail none of the referred row would
    // who up in the joined row. With the LEFT (OUTER) JOIN, a NULL FK will fail to match but the referred rows columns
    // will be joined but filled with NULLs.
    auto res = tx.exec(" SELECT ts, src_name.id, tgt_name.id, ability_name.id, action FROM Event"
                       "     LEFT JOIN Actor AS src_act ON Event.source = src_act.id"
                       "       LEFT JOIN Name AS src_name ON src_act.name = src_name.id"
                       "     LEFT JOIN Actor AS tgt_act ON Event.target = tgt_act.id"
                       "       LEFT JOIN Name AS tgt_name ON tgt_act.name = tgt_name.id"
                       "     LEFT JOIN Name AS ability_name ON Event.ability = ability_name.id"
                       " WHERE Event.id = $1", row_id);
    auto row = res.one_row();
    auto ts = row[0].as<uint64_t>();
    auto src_name = !row[1].is_null() ? row[1].as<int>() : DbPopulator::NOT_APPLICABLE_ROW_ID;
    auto tgt_name = !row[2].is_null() ? row[2].as<int>() : DbPopulator::NOT_APPLICABLE_ROW_ID;
    auto ability_name = !row[3].is_null() ? row[3].as<int>() : DbPopulator::NOT_APPLICABLE_ROW_ID;
    auto action_desc = pretty_print_action(tx, row[4].as<int>());
    return std::format("{},{},{},{},{},{}\n",
                        row_id,
                           DbPopulator::event_ts_to_str(ts),
                              pretty_print_name(tx, src_name),
                                 pretty_print_name(tx, tgt_name),
                                    pretty_print_name(tx, ability_name),
                                       action_desc);
}

auto style_disc_from_csv_string(const std::string& pair) -> std::optional<std::pair<std::string_view,std::string_view>> {
    auto comma_pos = std::find(pair.begin(), pair.end(), ',');
    if (   comma_pos == pair.end()
        || pair.starts_with(',') 
        || pair.ends_with(',')) {
        std::cerr << "Supplied class style/discipline, " << std::quoted(FLAGS_abilities_for_class)
                  << " is incorrectly formatted\n";
        return {};
    }
    return std::pair<std::string_view,std::string_view>{std::string_view(pair.begin(), comma_pos),
                                                        std::string_view(comma_pos + 1, pair.end())};
}

auto main(int argc, char* argv[]) -> int {
    gflags::SetUsageMessage("Extract information from the SW:ToR combat database");
    gflags::ParseCommandLineFlags(&argc, &argv, true);

    auto cx = pqxx::connection{"dbname = swtor_combat_explorer   user = jason   password = jason"};
    auto tx = pqxx::nontransaction{cx};

    if (!FLAGS_abilities_for_class.empty()) {
        auto style_disc = style_disc_from_csv_string(FLAGS_abilities_for_class);
        if (!style_disc) {
            return 1;
        }
        auto [style, discipline] = *style_disc;
        auto res = tx.exec(" SELECT DISTINCT ab.name_id,ab.name FROM Event"
                           "     JOIN Actor as act ON Event.source = act.id"
                           "     JOIN Advanced_Class AS ac ON act.class = ac.id"
                           "     JOIN Name as sn ON ac.style = sn.id"
                           "     JOIN Name as dn ON ac.class = dn.id"
                           "     JOIN Name AS ab ON Event.ability = ab.id"
                           " WHERE Event.source IS NOT NULL"
                           "   AND Event.target IS NOT NULL"
                           "   AND Event.source = Event.target"
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
    if (!FLAGS_class_unique_abilities.empty()) {
        // TODO - fix this - this is the specific check but the code is for the universal search.
        auto style_disc = style_disc_from_csv_string(FLAGS_class_unique_abilities);
        if (!style_disc) {
            return 1;
        }
        // auto [style, discipline] = *style_disc;
        // auto res = tx.exec(" SELECT a.class, style_name.name, discipline_name.name, a.ability, ability_name.name FROM"
        //                    "     (SELECT DISTINCT ability, act.class as class FROM Event"
        //                    "         JOIN Actor as act on Event.source = act.id"
        //                    "     WHERE Event.source IS NOT NULL"
        //                    "       AND Event.target IS NOT NULL"
        //                    "       AND Event.source = Event.target"
        //                    "       AND Event.ability IS NOT NULL"
        //                    "       AND act.type = $1"
        //                    "     ORDER BY ability) AS a"
        //                    "   JOIN"
        //                    "     (SELECT ability, COUNT(DISTINCT adv.id) AS num FROM Event"
        //                    "         JOIN Actor as act on Event.source = act.id"
        //                    "         JOIN Advanced_class as adv ON act.class = adv.id"
        //                    "     WHERE Event.source IS NOT NULL"
        //                    "       AND Event.target IS NOT NULL"
        //                    "       AND Event.source = Event.target"
        //                    "       AND Event.ability IS NOT NULL"
        //                    "       AND act.type = $1"
        //                    "     GROUP BY ability"
        //                    "     ORDER BY ability) AS b"
        //                    "   ON a.ability = b.ability"
        //                    "   JOIN Name AS ability_name ON a.ability = ability_name.id"
        //                    "   JOIN Advanced_class as adv ON a.class = adv.id"
        //                    "     JOIN Name AS style_name ON adv.style = style_name.id"
        //                    "     JOIN Name AS discipline_name ON adv.class = discipline_name.id"
        //                    " WHERE b.num = 1"
        //                    " ORDER BY a.class, ability_name.name", pqxx::params(DbPopulator::ACTOR_PC_CLASS_TYPE_NAME));
        // std::cout << "class_id,style,discipline,ability_id,ability_name\n";
        // for (auto row : res) {
        //     auto [class_id,style,discipline,ability_id,ability_name] =
        //         row.as<int, std::string_view, std::string_view, int, std::string_view>();
        //     std::cout << class_id << "," << style << "," << discipline << "," << ability_id << "," << ability_name << "\n";
        // }
        // TODO: This doesn't quite work, but I feel like it's getting closer. Here's one output:
        // 
        // 31,Gunslinger,Dirty Fighting,13742,D-R0M Probe
        // 31,Gunslinger,Dirty Fighting,5589,Dirty Blast
        // 31,Gunslinger,Dirty Fighting,5600,Dirty Shot
        // 31,Gunslinger,Dirty Fighting,5611,Dodge Stance
        // 31,Gunslinger,Dirty Fighting,5596,Hemorrhaging Blast
        // 31,Gunslinger,Dirty Fighting,7511,Ikas XK-13
        // 31,Gunslinger,Dirty Fighting,5767,Illegal Mods
        // 31,Gunslinger,Dirty Fighting,1287,Major Fleet Requisition Grant
        // 31,Gunslinger,Dirty Fighting,5598,Smuggler's Chems
        // 31,Gunslinger,Dirty Fighting,13729,Vectron TM-22 Volo
        // 31,Gunslinger,Dirty Fighting,5599,Wounding Shots
        // This isn't yet working. I added the "only show ability activate" events, which should help... but it doesn't.
        auto res = tx.exec(" SELECT a.class, style_name.name, discipline_name.name, a.ability, ability_name.name FROM"
                           "     (SELECT DISTINCT ability, Actor.class as class FROM Event"
                           "         JOIN Actor ON Event.source = Actor.id"
                           "         JOIN Action ON Event.action = Action.id"
                           "           JOIN Name AS action_noun_name ON Action.noun = action_noun_name.id"
                           "     WHERE Event.source IS NOT NULL"
                           "       AND Event.target IS NOT NULL"
                           "       AND Event.source = Event.target"
                           "       AND Event.ability IS NOT NULL"
                           "       AND action_noun_name.name_id = $2"
                           "       AND Actor.type = $1"
                           "     ORDER BY ability) AS a"
                           "   JOIN"
                           "     (SELECT ability, COUNT(DISTINCT adv.id) AS num FROM Event"
                           "         JOIN Actor ON Event.source = Actor.id"
                           "         JOIN Advanced_class AS adv ON Actor.class = adv.id"
                           "         JOIN Action ON Event.action = Action.id"
                           "           JOIN Name AS action_noun_name ON Action.noun = action_noun_name.id"
                           "     WHERE Event.source IS NOT NULL"
                           "       AND Event.target IS NOT NULL"
                           "       AND Event.source = Event.target"
                           "       AND Event.ability IS NOT NULL"
                           "       AND action_noun_name.name_id = $2"
                           "       AND Actor.type = $1"
                           "     GROUP BY ability"
                           "     ORDER BY ability) AS b"
                           "   ON a.ability = b.ability"
                           "   JOIN Name AS ability_name ON a.ability = ability_name.id"
                           "   JOIN Advanced_class as adv ON a.class = adv.id"
                           "     JOIN Name AS style_name ON adv.style = style_name.id"
                           "     JOIN Name AS discipline_name ON adv.class = discipline_name.id"
                           " WHERE b.num = 1"
                           " ORDER BY a.class, ability_name.name",
                           pqxx::params(DbPopulator::ACTOR_PC_CLASS_TYPE_NAME, SCE::ABILITY_ACTIVATE_ID));
        std::cout << "class_id,style,discipline,ability_id,ability_name\n";
        for (auto row : res) {
            auto [class_id,style,discipline,ability_id,ability_name] =
                row.as<int, std::string_view, std::string_view, int, std::string_view>();
            std::cout << class_id << "," << style << "," << discipline << "," << ability_id << "," << ability_name << "\n";
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
        std:: cout << "rows=" << res.size() << "\n";
    }
    if (FLAGS_all_effects) {
        std::cout << "You requested all of the unique effects in the Action table (no effect has details).\n";
        auto res = tx.exec(" SELECT DISTINCT en.name FROM Action as act"
                           "     JOIN Name AS en ON act.noun = en.id"
                           "     JOIN Name AS aen ON aen.name_id = $1"
                           "     JOIN Name AS ren ON ren.name_id = $2"
                           " WHERE act.verb = aen.id OR act.verb = ren.id",
                           pqxx::params(SCE::APPLY_EFFECT_ID, SCE::REMOVE_EFFECT_ID));
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
        std::cout << tx.query_value<int>("SELECT COUNT(*) FROM Log_File") << " rows\n";
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

    if (!FLAGS_combat_abilities_for_class.empty()) {
        auto style_disc = style_disc_from_csv_string(FLAGS_combat_abilities_for_class);
        if (!style_disc) {
            return 1;
        }
        auto [style, discipline] = *style_disc;
        auto res = tx.exec(" SELECT DISTINCT ab.name_id,ab.name FROM Event"
                           "     JOIN Actor ON Event.source = Actor.id"
                           "     JOIN Action ON Event.action = Action.id"
                           "       JOIN Name AS action_noun_name ON Action.noun = action_noun_name.id"
                           "     JOIN Advanced_Class AS ac ON Actor.class = ac.id"
                           "     JOIN Name as sn ON ac.style = sn.id"
                           "     JOIN Name as dn ON ac.class = dn.id"
                           "     JOIN Name AS ab ON Event.ability = ab.id"
                           " WHERE Event.source IS NOT NULL"
                           "   AND Event.target IS NOT NULL"
                           "   AND Event.source = Event.target"
                           "   AND Event.combat IS NOT NULL"
                           "   AND Actor.type = $1"
                           "   AND action_noun_name.name_id = $2"
                           "   AND Event.ability IS NOT NULL"
                           "   AND sn.name = $3"
                           "   AND dn.name = $4"
                           " ORDER BY ab.name",
                           pqxx::params(DbPopulator::ACTOR_PC_CLASS_TYPE_NAME, SCE::ABILITY_ACTIVATE_ID, style, discipline));
        std::cout << "ability_id,ability_name\n";
        for (auto row : res) {
            auto [ability_id, ability_name] = row.as<uint64_t,std::string_view>();
            std::cout << ability_id << "," << ability_name << "\n";
        }
        std::cout << res.size() << " rows\n";
    }
    if (!FLAGS_find_name.empty()) {
        // Override the user's choice of details.
        auto _ = OverrideInScope(FLAGS_name_details, std::string("rit"));
        std::cout << "Finding Name that have as id or name " << std::quoted(FLAGS_find_name) << "\n";
        auto all_digits = std::find_if(FLAGS_find_name.begin(), FLAGS_find_name.end(),
                                       [] (auto c) { return !std::isdigit(c); }) == FLAGS_find_name.end();
        if (all_digits) {
            auto row_id = tx.exec("SELECT id FROM Name WHERE name_id = $1", pqxx::params(FLAGS_find_name));
            if (row_id.empty()) {
                 row_id = tx.exec("SELECT id FROM Name WHERE id = $1", pqxx::params(FLAGS_find_name));
                if (row_id.empty()) {
                    std::cout << "No Name row found with id or name_id = " << FLAGS_find_name << "\n";
                    return 1;
                } 
            } 
            std::cout << "id,name_id,name\n";
            std::cout << pretty_print_name(tx, row_id[0][0].as<int>()) << "\n";
            std::cout << "1 rows\n";
            return 0;
        }
        auto query = std::format(" SELECT id FROM Name WHERE name LIKE '{}' ORDER BY name", FLAGS_find_name);
        auto res = tx.exec(query);
        std::cout << "name {row_id:name_id}\n";
        for (auto row : res) {
            std::cout << pretty_print_name(tx, row[0].as<int>()) << "\n";
        }
        std::cout << res.size() << " rows\n";
    }
    if (!FLAGS_find_class_uses_ability.empty()) {
        std::cout << "Finding class(es) that use the ability " << std::quoted(FLAGS_find_class_uses_ability) << "\n";
        auto res = tx.exec(" SELECT style.id, disc.id FROM Event"
                           "     JOIN Name AS ability_name ON Event.ability = ability_name.id"
                           "     JOIN Actor AS act ON Event.source = act.id"
                           "     JOIN Advanced_class AS ac ON act.class = ac.id"
                           "       JOIN Name AS style ON ac.style = style.id"
                           "       JOIN Name AS disc  ON ac.class = disc.id"
                           " WHERE Event.source IS NOT NULL"
                           "   AND Event.ability IS NOT NULL"
                           "   AND act.type = 'pc'");
        std::cout << "style,discipline\n";
        for (auto row : res) {
            auto [style, discipline] = row.as<int,int>();
            std::cout << pretty_print_name(tx, style) << "," << pretty_print_name(tx, discipline) << "\n";
        }
        std::cout << res.size() << " rows\n";
    }

    if (FLAGS_dump_event_by_id != -1) {
        std::cout << "Pretty-printing event with ID=" << FLAGS_dump_event_by_id << "\n";
        std::cout << "row_id,ts,src_name,tgt_name,ability_name,action_desc\n";
        std::cout << pretty_print_event(tx, FLAGS_dump_event_by_id) << "\n";
    }

    return 0;
}
