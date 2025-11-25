#include <iostream>
#include <optional>
#include <span>
#include <utility>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wall"
#include <gflags/gflags.h>
#include <pqxx/pqxx>
#pragma GCC diagnostic pop

#include "db_populator.hpp"
#include "sce_constants.hpp"
#include "wrapper.hpp"

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
DEFINE_int32(all_events_in_combat,      -1,    "Pretty-print all events in the specified combat by the row id");
DEFINE_string(all_events_in_logfile,    "",    "Pretty-print all events in specified logfile filename");
// TODO
DEFINE_string(class_unique_abilities,   "",    "Show unique abilities for a specific class in the form \"style,discipline\"");
DEFINE_string(combat_abilities_for_class,  "", "Show abilities used in combat by class in form \"style,discipline\"");
DEFINE_int32(dump_event_by_id,          -1,    "Pretty-print the event with the supplied integer ID");
DEFINE_string(dump_events_by_id,        "",    "Pretty-print the range of events provided as 'beg-end'");
DEFINE_bool(duplicate_name_counts,      false, "Show how many names have the same string but different id");
DEFINE_string(find_name,                "",    "Given an integer, searches as a row ID or Name ID, otherwise searches as a 'LIKE' pattern;"
                                               " print row ID, name ID, and name as \"name {row_id:name_id}\"");
DEFINE_string(find_class_uses_ability,  "",    "Given an ability ID (Name ID), find classes that use that ability");
DEFINE_bool(human_readable_timestamps,  false, "Show human-readable timestamps instead of database's ms-past-epoch");
DEFINE_bool(num_events,                 false, "Show number of Events");
DEFINE_bool(num_logfiles,               false, "Show number of Log_Files");
DEFINE_string(name_details,             "t",   "What to show for Names; a combination of 't' for the name text, 'r' for the Name row ID, and 'i' for the Name ID");
DEFINE_bool(pcs_in_combats,             false, "Show all PCs in all combats");

template<typename C, typename T>
auto my_contains(C& c, T& t) -> bool {
    return std::find(c.begin(), c.end(), t) != c.end();
}

class Col {
  public:
    WRAPPER(NAME_ROW_ID , std::string_view);
    WRAPPER(NAME_NAME_ID, std::string_view);
    WRAPPER(NAME_NAME   , std::string_view);
    enum class col_type_t {
        NAME_ROW_ID
        , NAME_NAME_ID
        , NAME_NAME
        , NON_NAME
    };

    static constexpr std::array<std::tuple<char,col_type_t>,3> name_col_opt_char_to_col_type {
        {{'r', col_type_t::NAME_ROW_ID}
        , {'i', col_type_t::NAME_NAME_ID}
        , {'t', col_type_t::NAME_NAME}}
    };

    static auto pretty_name_str(NAME_ROW_ID row_id, NAME_NAME_ID name_id, NAME_NAME name) -> std::string {
        return std::format("{},{},{}",
                           pretty_col_str(col_type_t::NAME_ROW_ID, row_id.val()),
                           pretty_col_str(col_type_t::NAME_NAME_ID, name_id.val()),
                           pretty_col_str(col_type_t::NAME_NAME, name.val()));
    }

    static auto pretty_name_row(const pqxx::row& row) -> std::string {
        return std::format("{},{},{}",
                           pretty_col_str(col_type_t::NAME_ROW_ID, row[0]),
                           pretty_col_str(col_type_t::NAME_NAME_ID, row[1]),
                           pretty_col_str(col_type_t::NAME_NAME, row[2]));
    }

    static auto pretty_col_str(col_type_t col_type, std::string_view col_val) -> std::string_view {
        if (col_type == col_type_t::NON_NAME) {
            return col_val;
        }
        const auto& f = std::string_view(FLAGS_name_details);
        for (auto opt : name_col_opt_char_to_col_type) {
            if (my_contains(f, std::get<0>(opt)) && col_type == std::get<1>(opt)) {
                return col_val;
            }
        }
        return "";
    }

    static auto pretty_col_str(col_type_t col_type, const pqxx::field& col) -> std::string_view {
        return pretty_col_str(col_type, col.view());
    }
};

static auto name_row_id_to_pretty_str(pqxx::nontransaction& tx, int row_id) -> std::string {
    auto res = tx.exec("SELECT id, name_id, name FROM Name WHERE id = $1", row_id);
    return Col::pretty_name_row(res.one_row());
}

static const size_t num_event_columns {22};
struct EventColInfo {
    const char* header_text;
    const char* select_text;
    Col::col_type_t type;
};

static const std::array<EventColInfo,num_event_columns> event_cols_info = {{
    /* 0*/   {"event_id"             , "event.id",              Col::col_type_t::NON_NAME}
    /* 1*/ , {"logfile"              , "logfile.filename",      Col::col_type_t::NON_NAME}
    /* 2*/ , {"combat"               , "event.combat",          Col::col_type_t::NON_NAME}
    /* 3*/ , {"event_timestamp"      , "event.ts",              Col::col_type_t::NON_NAME}
    /* 4*/ , {"src_id"               , "src_name.id",           Col::col_type_t::NAME_ROW_ID}
    /* 5*/ , {"src_name_id"          , "src_name.name_id",      Col::col_type_t::NAME_NAME_ID}
    /* 6*/ , {"src_name"             , "src_name.name",         Col::col_type_t::NAME_NAME}
    /* 7*/ , {"tgt_id"               , "tgt_name.id",           Col::col_type_t::NAME_ROW_ID}
    /* 8*/ , {"tgt_name_id"          , "tgt_name.name_id",      Col::col_type_t::NAME_NAME_ID}
    /* 9*/ , {"tgt_name"             , "tgt_name.name",         Col::col_type_t::NAME_NAME}
    /*10*/ , {"ability_id"           , "ability.id",            Col::col_type_t::NAME_ROW_ID}
    /*11*/ , {"ability_name_id"      , "ability.name_id",       Col::col_type_t::NAME_NAME_ID}
    /*12*/ , {"ability_name"         , "ability.name",          Col::col_type_t::NAME_NAME}
    /*13*/ , {"action_verb_id"       , "action_verb.id",        Col::col_type_t::NAME_ROW_ID}
    /*14*/ , {"action_verb_name_id"  , "action_verb.name_id",   Col::col_type_t::NAME_NAME_ID}
    /*15*/ , {"action_verb_name"     , "action_verb.name",      Col::col_type_t::NAME_NAME}
    /*16*/ , {"action_noun_id"       , "action_noun.id",        Col::col_type_t::NAME_ROW_ID}
    /*17*/ , {"action_noun_name_id"  , "action_noun.name_id",   Col::col_type_t::NAME_NAME_ID}
    /*18*/ , {"action_noun_name"     , "action_noun.name",      Col::col_type_t::NAME_NAME}
    /*19*/ , {"action_detail_id"     , "action_detail.id",      Col::col_type_t::NAME_ROW_ID}
    /*20*/ , {"action_detail_name_id", "action_detail.name_id", Col::col_type_t::NAME_NAME_ID}
    /*21*/ , {"action_detail_name"   , "action_detail.name",    Col::col_type_t::NAME_NAME}
}};

static const char* event_joins{
    // We use LEFT JOIN here because it will handle NULLs in the FKs, since we have a bunch of nullable FKs. A standard
    // (INNER) JOIN would fail because if the FK is NULL then an (INNER) JOIN would fail none of the referred row would
    // who up in the joined row. With the LEFT (OUTER) JOIN, a NULL FK will fail to match but the referred rows columns
    // will be joined but filled with NULLs.
    "     JOIN Log_File AS logfile ON Event.logfile = logfile.id"
    "     LEFT JOIN Actor AS src ON Event.source = src.id"
    "       LEFT JOIN Name AS src_name ON src.name = src_name.id"
    "     LEFT JOIN Actor AS tgt ON Event.target = tgt.id"
    "       LEFT JOIN Name AS tgt_name ON tgt.name = tgt_name.id"
    "     LEFT JOIN Name AS ability ON Event.ability = ability.id"
    "     JOIN Action AS action ON Event.action = action.id"
    "       JOIN Name AS action_verb ON action.verb = action_verb.id"
    "       JOIN Name AS action_noun ON action.noun = action_noun.id"
    "       LEFT JOIN Name AS action_detail ON action.detail = action_detail.id"
};

template <typename T>
auto skip_first(T& c) {
    return std::span(std::next(c.begin()), c.end());
}

auto construct_event_query(const std::string where_clause) -> std::string {
    std::stringstream cols_ss;
    cols_ss << event_cols_info[0].select_text;
    for (auto col_info : skip_first(event_cols_info)) {
        cols_ss << "," << col_info.select_text;
    }
    return std::format("SELECT {} FROM Event {} WHERE {}",
                       cols_ss.str(),
                       event_joins,
                       where_clause);
}

auto pretty_str_event(pqxx::row& row) -> std::string {
    assert(row.size() == event_cols_info.size() && "Event row to print has incorrect number of columns");
    std::stringstream event_ss;
    event_ss << Col::pretty_col_str(event_cols_info[0].type, row[0]);
    for (int i = 1; i < row.size(); ++i) {
        event_ss << "," << Col::pretty_col_str(event_cols_info[i].type, row[i]);
    }
    return event_ss.str();
}

auto pretty_str_event_row_by_id(pqxx::nontransaction& tx, int event_row_id) {
    auto query = construct_event_query(std::format("Event.id = {}", event_row_id));
    auto row = tx.exec(query).one_row();
    return pretty_str_event(row);
}

auto event_header_str() -> std::string {
    std::stringstream header_ss;
    header_ss << event_cols_info[0].header_text;
    for (auto hdr : skip_first(event_cols_info)) {
        header_ss << "," << hdr.header_text;
    }
    return header_ss.str();
}

// 
// auto pretty_print_action(pqxx::nontransaction& tx, int row_id) -> std::string {
//     auto row = tx.exec("SELECT verb, noun, detail FROM Action WHERE id = $1", row_id)[0];
//     auto verb_name = pretty_print_name(tx, row[0].as<int>());
//     auto noun_name = pretty_print_name(tx, row[1].as<int>());
//     auto detail_name = !row[2].is_null() ? pretty_print_name(tx, row[2].as<int>()) : "n/a";
//     return std::format("{}: {}/{}", verb_name, noun_name, detail_name);
// }
// 
// auto pretty_print_class(pqxx::nontransaction& tx, int row_id) -> std::string {
//     auto [style, discipline] = tx.query1<int,int>(" SELECT style.id, disc.id FROM Advanced_Class as ac"
//                                                   "     JOIN Name AS style ON ac.style = style.id"
//                                                   "     JOIN Name AS disc  ON ac.class = class.id"
//                                                   " WHERE ac.id = $1", row_id);
//     return std::format("{} {}", pretty_print_name(tx, style), pretty_print_name(tx, discipline));
// }
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
        auto res = tx.exec(" SELECT DISTINCT ab.id, ab.name_id,ab.name FROM Event"
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
        std::cout << "ability_row_id,ability_name_id,ability_name\n";
        for (auto row : res) {
            std::cout << Col::pretty_name_row(row) << "\n";
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
        auto res = tx.exec(" SELECT Combat.id, ts_begin, combat_info.combat_size, an.name, lf.filename FROM Combat"
                           "     JOIN Area as ar  ON Combat.area = ar.id" 
                           "       JOIN Name as an  ON ar.area = an.id"
                           "     JOIN Log_File as lf ON combat.logfile = lf.id"
                           "     JOIN (SELECT e.combat AS combat_id, COUNT(*) AS combat_size FROM Event AS e "
                           "               WHERE e.combat IS NOT NULL"
                           "           GROUP BY e.combat) AS combat_info ON Combat.id = combat_info.combat_id"
                           " ORDER BY an.name");
        std::cout << "combat_id,begin_ts,combat_length,area,logfile\n";
        for (auto row : res) {
            auto combat_id = row[0].as<int>();
            auto begin_ts = row[1].as<uint64_t>();
            auto combat_length = row[2].as<int>();
            auto area = row[3].as<std::string_view>();
            auto logfile = row[4].as<std::string_view>();
            std::cout << std::format("{},{},{},{},{}\n", combat_id, begin_ts, combat_length, area, logfile);
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
            auto res = tx.exec("SELECT id, name_id, name FROM Name WHERE name_id = $1", pqxx::params(FLAGS_find_name));
            if (res.empty()) {
                 res = tx.exec("SELECT id, name_id, name FROM Name WHERE id = $1", pqxx::params(FLAGS_find_name));
                if (res.empty()) {
                    std::cout << "No Name row found with id or name_id = " << FLAGS_find_name << "\n";
                    return 1;
                } 
            }
            auto row = res.one_row();
            std::cout << "id,name_id,name\n";
            std::cout << Col::pretty_name_str(Col::NAME_ROW_ID(row[0].view()),
                                              Col::NAME_NAME_ID(row[1].view()),
                                              Col::NAME_NAME(row[2].view())) << "\n";
            std::cout << "1 rows\n";
            return 0;
        }
        auto query = std::format(" SELECT id, name_id, name FROM Name WHERE name LIKE '{}' ORDER BY name", FLAGS_find_name);
        auto res = tx.exec(query);
        std::cout << "id,name_id,name\n";
        for (auto row : res) {
            std::cout << Col::pretty_name_row(row) << "\n";
        }
        std::cout << res.size() << " rows\n";
    }
if (!FLAGS_find_class_uses_ability.empty()) {
    std::cout << "Finding class(es) that use the ability " << std::quoted(FLAGS_find_class_uses_ability) << "\n";
    auto res = tx.exec(" SELECT DISTINCT style.id, disc.id FROM Event"
                       "     JOIN Name AS ability_name ON Event.ability = ability_name.id"
                       "     JOIN Actor AS act ON Event.source = act.id"
                       "     JOIN Advanced_class AS ac ON act.class = ac.id"
                       "       JOIN Name AS style ON ac.style = style.id"
                       "       JOIN Name AS disc  ON ac.class = disc.id"
                       " WHERE Event.source IS NOT NULL"
                       "   AND Event.ability IS NOT NULL"
                       "   AND act.type = 'pc'"
                       "   AND ability_name.name = $1", pqxx::params(FLAGS_find_class_uses_ability));
    std::cout << "style_row_id,style_name_id,style_name,discipline_row_id,discipline_name_id,discipline_name\n";
    for (auto row : res) {
        auto [style, discipline] = row.as<int,int>();
        std::cout << name_row_id_to_pretty_str(tx, style) << "," << name_row_id_to_pretty_str(tx, discipline) << "\n";
    }
    std::cout << res.size() << " rows\n";
}

if (FLAGS_dump_event_by_id != -1) {
    std::cout << "Pretty-printing event with ID=" << FLAGS_dump_event_by_id << "\n";
    std::cout << event_header_str() << "\n";
    std::cout << pretty_str_event_row_by_id(tx, FLAGS_dump_event_by_id) << "\n";
}

if (!FLAGS_dump_events_by_id.empty()) {
    std::cout << "Pretty-printing events from IDs " << std::quoted(FLAGS_dump_events_by_id) << "\n";
    auto dash_pos = std::find(FLAGS_dump_events_by_id.begin(), FLAGS_dump_events_by_id.end(), '-');
    if (dash_pos == FLAGS_dump_events_by_id.end()) {
        std::cout << "Event ID range is incorrectly formatted - missing the '-' between begin and end IDs\n";
        return -1;
    }
    auto beg_str = std::string(FLAGS_dump_events_by_id.begin(), dash_pos);
    auto end_str = std::string(std::next(dash_pos), FLAGS_dump_events_by_id.end());
    auto where = std::format("Event.id BETWEEN {} AND {}", tx.esc(beg_str), tx.esc(end_str));
    auto query = construct_event_query(where);
    auto res = tx.exec(query);
    if (res.empty()) {
        std::cout << "0 rows\n";
        return 0;
    }
    std::cout << event_header_str() << "\n";
    std::for_each(res.begin(), res.end(), [] (pqxx::row row) {std::cout << pretty_str_event(row) << "\n";});
    std::cout << res.size() << " rows\n";
}
if (FLAGS_all_events_in_combat != -1) {
    std::cout << "Pretty-printing all events in combat with ID=" << FLAGS_all_events_in_combat << "\n";
    std::cout << event_header_str() << "\n";
    auto res = tx.exec(construct_event_query("event.combat = $1"), pqxx::params(FLAGS_all_events_in_combat));
    for (auto row : res) {
        std::cout << pretty_str_event(row) << "\n";
    }
    std::cout << res.size() << " rows\n";
}
if (!FLAGS_all_events_in_logfile.empty()) {
    std::cout << "Pretty-printing all events in logfile with name=" << FLAGS_all_events_in_logfile << "\n";
    std::cout << event_header_str() << "\n";
    auto res = tx.exec(construct_event_query("logfile.filename = $1"), pqxx::params(FLAGS_all_events_in_logfile));
    for (auto row : res) {
        std::cout << pretty_str_event(row) << "\n";
    }
    std::cout << res.size() << " rows\n";
}
// DEFINE_bool(human_readable_timestamps,  false, "Show human-readable timestamps instead of database's ms-past-epoch");
// if ( DO THIS. Add another column type and check flag, just like name details.

    return 0;
}
