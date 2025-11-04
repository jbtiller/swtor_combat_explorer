// -*- fil-column: 120; indent-tabs-mode: nil -*-
#pragma once

#include <memory>
#include <stdexcept>
#include <string>
#include <map>
#include <stdexcept>

#include "log_parser_types.hpp"
#include "timestamps.hpp"
#include "wrapper.hpp"

class ScopeRuns {
  public:
    ScopeRuns(const std::string& function_name);
    auto enter() -> void;
    auto exit() -> void;

    std::string m_func_name;
    uint32_t m_num_calls {};
    timespec m_enter_time;
    int64_t m_total_time_in_func {};
};

// Forward references
namespace pqxx {
    class connection;
    class nontransaction;
} // namespace pqxx

extern ScopeRuns measure_add_name_id;

class DbPopulator {
  public:
    WRAPPER(AreaName, LogParserTypes::NameId);
    WRAPPER(ConnStr, std::string);
    WRAPPER(DifficultyName, LogParserTypes::NameId);
    WRAPPER(LogfileFilename, std::string);

    enum class ExistingLogfileBehavior {
        DELETE_ON_EXISTING,
        DELETE_ON_EXISTING_UNFINISHED
    };

    struct duplicate_logfile : std::runtime_error {
        duplicate_logfile(const std::string& description, bool fully_parsed)
            : std::runtime_error(description)
            , m_fully_parsed(fully_parsed) {}

        bool m_fully_parsed {false};
    };
    
  public:
    /**
     * Create a DbPopulator object
     *
     * Opens a database connection using the supplied connection string.
     *
     * Store the logfile information in the database. If the supplied logfile is already represented in the database,
     * then behave as per the `existing_logfile_behavior` argument.
     *
     * 1. DELETE_ON_EXISTING
     *    Remove existing Log_File entry and delete all referents to retain integrity.
     * 2. DELETE_ON_EXISTING_UNFINISHED
     *    If existing Log_File entry is not completely parsed (`fully_parsed` attribute is false), delete as per
     *    DELETE_ON_EXISTING. If Log_File entry is fully parsed, throw.
     *
     * Retrieve the schema version from the database.
     *
     * The database connection is held open for the lifecycle of this object.
     *
     * @param[in] conn_str Connection string to pass to database handler
     * @param[in] logfile_filename Log entries we're populating come from this file
     * @param[in] logfile_ts Logfile creation timestamp
     * @param[in] existing_logfile_behavior Specify behavior if logfile corresponding to `logfile_filename` is already
     *            in database.
     *
     * @throws duplicate_logflie Thrown if `logfile_filename` already exists in the database, the existing Log_File
     *         entry is fully parsed, and `existing_logfile_behavior` is set to DELETE_ON_EXISTING_UNFINISHED.

     * @throws pqxx* Throw pqxx exceptions on database operation errors
     */
    DbPopulator(const ConnStr& conn_str,
                const LogfileFilename& logfile_filename,
                Timestamps::timestamp logfile_ts,
                ExistingLogfileBehavior existing_logfile_behavior);

    /**
     * Destroy a DbPopulator object
     *
     * Free local state and close the database connection.
     * 
     * @note This is required because the destructor must have access to the full definition of pqxx::connection, which
     * is forward-declared to avoid letting pqxx leak into the caller.
     */
     ~DbPopulator();

    auto populate_from_entry(const LogParserTypes::ParsedLogLine& entry) -> int;

    auto mark_fully_parsed(void) -> void;

    auto db_version() const -> std::string {
        return m_db_version;
    }

    auto in_combat() const -> bool {
        return m_combat_id.has_value();
    }
    
    // Added for type-safety and least-surprise. Needs to be public so tests can access.
    WRAPPER(VerbId, int);
    WRAPPER(NounId, int);
    WRAPPER(DetailId, std::optional<int>);

    WRAPPER(CombatStyle, LogParserTypes::NameId);
    WRAPPER(AdvancedClass, LogParserTypes::NameId);
    struct PcClass {
        CombatStyle style;
        AdvancedClass advanced_class;
    };

    inline static constexpr uint64_t NOT_APPLICABLE_NAME_ID {0};
    // This is legal because the initializer string is short and can use the short-string initialization trick to embed
    // the string in the class without requiring dynamic allocation.
    inline static constexpr std::string NOT_APPLICABLE_NAME_NAME {"n/a"};
    inline static constexpr uint64_t UNKNOWN_COMBAT_STYLE_NAME_ID {1};
    // This cannot be a constexpr because it requires dynamic allocation due to the initializer string being longer than
    // supported by short-string initialization.
    inline static const std::string UNKNOWN_COMBAT_STYLE_NAME_NAME {"unknown combat style"};
    inline static constexpr uint64_t UNKNOWN_ADVANCED_CLASS_NAME_ID {2};
    inline static const std::string UNKNOWN_ADVANCED_CLASS_NAME_NAME {"unknown advanced class"};

    // I chose to not use NULLs so for columns with nullable values I have a "pseudo-null" row in the Name table with
    // the name 'n/a' (not applicable. These are pre-populated in the Name table.
    inline static constexpr int NOT_APPLICABLE_ROW_ID {1};
    inline static constexpr int UNKNOWN_COMBAT_STYLE_ROW_ID {2};
    inline static constexpr int UNKNOWN_ADVANCED_CLASS_ROW_ID {3};
    inline static constexpr int DIFFICULTY_NONE_ROW_ID {4};
    inline static constexpr int UNKNOWN_AREA_ROW_ID {5};

    // This is pre-populated in the AdvancedClass table.
    static constexpr int UNKNOWN_CLASS_ROW_ID {1};
    
    // TODO: These are extracted from a log. They should probably move into a configuration file.
    inline static constexpr uint64_t DISCIPLINE_CHANGED_ID {836045448953665};
    inline static constexpr uint64_t AREA_ENTERED_ID       {836045448953664};
    inline static constexpr uint64_t ENTER_COMBAT_ID       {836045448945489};
    inline static constexpr uint64_t EXIT_COMBAT_ID        {836045448945490};

    inline static constexpr std::string ACTOR_PC_CLASS_TYPE_NAME {"pc"};
    inline static constexpr std::string ACTOR_NPC_CLASS_TYPE_NAME {"npc"};
    inline static constexpr std::string ACTOR_COMPANION_CLASS_TYPE_NAME {"companion"};

    // Held by m_pcs.
    struct ActorRowInfo {
        int row_id;
        int class_id;
    };

  protected:
    /**
     * Ensure name and ID are in database
     *
     * Looks up name/id in the Name table and inserts if not present.
     *
     * @param[in] name_id Identifier for name from log
     * @param[in] name String name from log
     * @returns The row identifier for the name/id in the Name table
     */
    // TODO: Check stuff in and upload to github.
    // TODO: Add cache for Name and Action and check to see how performance changes.
    auto add_name_id(const LogParserTypes::NameId& name_id) -> int;

    /**
     * Add action to the database
     *
     * An action has three components, verb & noun, which are required, and detail, which may not be present. These
     * all reference names/ids stored in the database.
     *
     * @param[in] vid Identifier of verb name/id
     * @param[in] nid Identifier of noun name/id
     * @param[in] did Optional identifier of detail name/id
     * @returns The identifier of the row in the action table that holds the noun, verb, detail values
     */
    auto add_action(VerbId vid, NounId nid, DetailId did) -> int;

    /**
     * Add an actor to the database
     *
     * A subect is the source or target of an action. There are three types of actors: PC, NPC, and PC's Companion.
     * This function adds the Name/IDs of the actor based on the type. It then adds a row to the specific Actor table.
     *
     * @param[in] Actor info to be added to the database
     * @returns The row ID in the database with the new data
     */
    auto add_actor(const LogParserTypes::Actor& actor) -> int;
    
    /**
     * Add the combat style/advanced class combination to the database
     *
     * PC's can have multiple styles/classes but only one combination at a time.
     *
     * @note Relies on add_name_id() for the style/class names.
     *
     * @param[in] Combat style and advanced class to add to database
     * @returns Row ID in the Advanced_Class table containing the new information
     *
     */
    auto add_pc_class(const PcClass& pc_class) -> int;

    /**
     * Add an Actor that's a Non-player Character (NPC) to the database
     *
     * NPC's are represented as a name/ID and an instance ID for the specific NPC.
     *
     * @note Uses add_name_id() for the NPC name
     * 
     * @param[in] NPC's information
     * @returns Row in Actor table corresponding to the NPC information
     */
    auto add_npc_actor(const LogParserTypes::NpcActor& npc_actor) -> int;

    /**
     * Add a PC actor to the database
     *
     * An Actor row for a PC contains the PC's name and the PC's advanced class. This function is called when a PC is
     * encountered as the source or target of an event and we don't yet know which PC Actor row refers to the input
     * `pc_actor`'s name ID.
     *
     * This function maintains a local cache, m_pcs, that relates the PC name's ID (LogParserTypes::NameId.id or the
     * Name table's name_id column, which are equivalent) to a PC row in the Actor table. There can be multiple Actor
     * rows that have the same PC name ID because the same PC can have multiple advanced classes over time; the m_pcs
     * cache is used to remember *which* Actor row is active for the current PC, meaning which advanced class this
     * PC has.
     *
     * In general, the only time a PC name's ID won't be present in the local m_pcs cache is when it's encountered for
     * the first time in the first logfile that's been parsed and pushed into the db. However, because the database can
     * contain the events for multiple logfiles, the local m_pcs cache may be empty even though the Actor table may have
     * multiple rows related with the PC's name.
     *
     * If the local m_pcs cache contains the input PC name's ID, then we simply return the related Actor row ID. This
     * means we "know" the PC's advanced class. Elsewise, we try to add a new row to the Actor table for the PC and give
     * it the "uknown" advanced class.
     *
     * After this function completes, m_pcs will always relate the input PC name's ID to an Actor row ID.
     *
     * @note This method uses add_name_id() for the PC's name.
     *
     * @param pc_actor PC Actor information to be added/updated
     * @returns The Actor row ID associated with the pc_actor
     */
    auto add_pc_actor(const LogParserTypes::PcActor& pc_actor) -> int;

    /**
     * Add an Actor who is a companion to the database
     *
     * A companion is a kind of NPC that is related to a specific PC. Thus, a companion has a name, an instance, and a
     * PC that "owns" it.
     *
     * @note This function relies on add_pc_actor() to add the companion's owning PC. Companions don't have classes
     * (that are visible via the log). It also relies on add_name_id() for the companion's name.
     */
    auto add_companion_actor(const LogParserTypes::CompanionActor& comp_actor) -> int;
    
    /**
     * Add a combat style/advanced class to a PC in the database


     *
     *
     * There can be any number of PC Actor rows with the same name already in the database because PC's can change their
     * combat style/discipline freely. As of this writing, a PC may select between two styles and three disciplines
     * associated with those styles. Therefore, there might 6 existing PCs in the Actor table that all share the same
     * name.
     *
     * These are the possible states of the database prior to this function:
     *
     * 1. There is no PC Actor with the same Name as in the input.
     * 2. There is one PC Actor with the same Name as the input but with a different class.
     * 3. There is one PC Actor with the same Name as the input with the same class.
     * 4. There is more than one PC Actor with the same name as the input.
     *
     * There is one bit of additional state that is maintained locally: m_pcs, which maps the most recently-seen PC
     * names to the corresponding Actor row for that PC. This produces two more possible states:
     *
     * 5. There is a cache entry for the PC pointing to a specific Actor row.
     * 6. There isn't a cache entry for the PC.
     *
     * This is important because of prior state #4 above - if we see a PC Actor's name in an event, how do we know which
     * one of the potential multiple PC Actors is actually being referenced?
     *
     * Because the database contains events from multiple logfiles, it's possible that we see a PC name for the first
     * time that already has multiple PC Actor rows associated with it, meaning we don't have an entry in m_pcs to tell
     * us which of the rows to use.
     *
     * Finally, there's one more state related to this function's input parameter `pc_class`:
     *
     * 7. The `pc_class` argument is populated.
     * 8. The `pc_class` argument is empty.
     *
     * If #7 holds, then we have all the information we need to either find a unique existing Actor row in the database,
     * matching on Name and Advanced_Class, or add a new row. However...
     *
     * So, the most interesting case is the state when #4, #6, and #8 are all true. When this occurs, meaning:
     *
     * A. There are multiple Actor rows with the same PC Name as the input.
     * B. The local cache does not have an entry for the input PC Name.
     * C. The `pc_class` argument is empty.
     *
     * In this case, we attempt to add a new PC Actor row with an "unknown" advanced_class.
     *
     * In all cases, if the local cache is lacking a row for the PC we populate it with an Actor row.
     *
     * A new PC actor will be added to the database in the following conditions:
     *
     * 1. No existing PC actor matching input in database
     * 2. Existing PC actor matching input found in database has advanced class that's different from input
     *
     * An existing PC actor matching input in the database will be updated with the input advanced class in the
     * following conditions:
     *
     * 1. Existing PC actor has no advanced class
     * 2. Input pc_class is not empty
     *
     * Otherwise, return the existing actor matching the input in the database.
     */
    
    /**
     * Add advanced class information to an existing PC Actor
     *
     * A PC will be added to the database before we encounter the log entry that specifies their current combat
     * style/advanced class. When the log finally tells us the PC's class, call this to add that information to the PC
     * in the database.
     *
     * Preconditions:
     *
     * 1. Actor row with PC type and the name of `pc_actor` exists.
     * 2. The local m_pcs cache variable is populated for `pc_actor.id`.
     * 3. `pc_class` is not the "unknown" advanced class.
     *
     * These are the possible states of the Actor table before this function is called:
     *
     * A1. There is one row for the `pc_actor`.
     * A2. There are muiltiple rows for the `pc_actor` and none have the "unknown" advanced class.
     * A3. There are muiltiple rows for the `pc_actor` and one  has  the "unknown" advanced class.
     *
     * These are the possible states of the row pointed to by local m_pcs cache variable:
     *
     * C1. The row has the "uknown" advanced class.
     * C2. The row has a known advanced class.
     *
     * Behavior by Actor row and m_pcs row state combinations:
     *
     * A1&C1: Update row with class id corresponding to `pc_class`.
     *
     * A1&C2: If the row's class is the same as `pc_class`, do nothing. If the row's class is different than `pc_class`,
     * add a new row for `pc_actor` and `pc_class`.
     *
     * A2&C1. Not possible.
     *
     * A2&C2. Search the matching Actor rows. If any has the same advanced class as `pc_class`, use that row and update
     * m_pcs. Otherwise, add a new for for the `pc_class`.
     *
     * A3&C1. Search the matching Actor rows. If any has the same advanced class as `pc_class`, use that row and update
     * m_pcs. Otherwise, use the row with the "unknown" class and set its class to be `pc_class`.
     *
     * A3&C2. If the m_pcs Actor row has the same advanced class as `pc_class`, use that row. Otherwise, add a new row
     * for the `pc_actor` and `pc_class`.
     *
     * @note add_pc_actor() will ensure the three preconditions are satisfied and must have been called for `pc_actor`
     * before it's safe to call this function.
     *
     * @note It is possible that we learn a PC's class *during* *combat*. If this is the case, then the Combat_Actor
     * table may need to be updated in some situations. This is *not* *currently* *handled*.
     *
     * @param[in] pc_actor PC Actor information to be updated
     * @param[in] Class information for the PC
     * @returns The Actor row ID associated with the pc_actor that has the new class information
     */
    auto add_class_to_pc_actor(const LogParserTypes::PcActor& pc_actor, const PcClass& pc_class) -> int;

    /**
     * Add the action into the database
     *
     * @note This relies on add_name_id() to do most of the work.
     *
     * @param[in] action The action to be added
     * @return The row ID of the row in the database corresponding to the action
     */
    auto add_action(const LogParserTypes::Action& action) -> int;

    /**
     * Remember that we've entered a new area
     *
     * This adds the area information to the database as well as caching this as the current area, used during
     * combat. If the area is an instance that has an associated difficulty, that can also be associated with the area.
     *
     * @note m_area_id is updated to have a local store of the current area
     *
     * @param[in] area The name of the area being entered
     * @param[in] difficulty Optional difficulty level of the area
     * @return The row ID of the area in the database
     */
    auto record_area_entered(AreaName area, std::optional<DifficultyName> difficulty = std::optional<DifficultyName>()) -> int;
    
    /**
     * Remember that a combat is in progress
     *
     * This adds a Combat entry to the database. All subsequent log entries will be related to this Combat entry until
     * combat stops. This is triggered on an EnterCombat log entry.
     *
     * @note m_combat_id is updated to have a local store of the current combat
     *
     * @param[in] begin Timestamp in ms of the beginning of combat
     * @param[in] area The area row ID of the area where the combat takes place
     * @param[in] logfile The logfile row ID of the logfile that contains this combat
     * @return The row ID of the new combat in the database
     */
    auto record_enter_combat(const Timestamps::timestamp& combat_begin) -> int;

    /**
     * End the current combat
     *
     * This updates the current combat and sets the end timestamp. 
     *
     * @note m_combat_id is cleared indicating we're no longer in combat
     *
     * @note It's possible that the "exit combat" log entry may occur even though no combat is ongoing. I've seen this
     * once, where ExitCombat was triggered twice with about 1.5s in-between. Because of this, we ignore this event if
     * we're not currently in combat. In this case '0' is returned.
     *
     * @param[in] end Timestamp from event indicating combat ended
     * @return The row id of the combat that was ended; if not currently in combat, returns 0.
     */
    auto record_exit_combat(const Timestamps::timestamp& combat_end) -> int;

    // The connection. Made a pointer so we can delay construction until the body of our constructor and avoid throwing
    // exceptions in the initialization list.
    std::unique_ptr<pqxx::connection> m_cx;
    std::unique_ptr<pqxx::nontransaction> m_tx;

    std::string m_db_version;

    int m_logfile_id {};

    /*
     * Combat table row ID if currently in combat
     *
     * Value changes we see EnterCombat/ExitCombat events.
     */
    std::optional<int> m_combat_id;

    /*
     * Current Area row ID if we've entered an area
     *
     * Value is determined by AreaEnter events.
     */
    std::optional<int> m_area_id;

    /*
     * Most recent actor row ID associated with a PC's name.
     *
     * A PC may have multiple rows in the Actor table because PC's can change both their combat style and advanced
     * class. We care about the most recent one because this is the one we'll use to represent the PC when combat
     * starts.
     *
     * Value is updated each time a new Actor row is created for the PC's name_id.
     *
     * key: name ID of PC
     * value: most-recent row ID in Actor table for PC
     */
    std::map<uint64_t, ActorRowInfo> m_pcs;

    /**
     * Is the logfile parsing and population complete
     *
     * Initially false. Set to `true` when the user calls mark_fully_parsed() or when the constructor detects an
     * existing logfile with the same name.
     */
    bool m_parsing_finished {false};

    std::map<uint64_t, int> m_names;

    // TODO: Probably consider caches for NPC and Companion actors, too. Purely an optimization.
};
