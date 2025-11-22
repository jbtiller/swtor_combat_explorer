#pragma once

#include <cstdint>
#include <map>
#include <optional>

#include "log_parser_types.hpp"
#include "timestamps.hpp"
#include "world_state_tracker.hpp"

class CombatStatistics {
  public:
    struct CombatActor;
    struct Effect {
        // TODO: Flesh this out.
        uint64_t id {};
        Timestamps::timestamp applied_ts {};
        Timestamps::timestamp removed_ts {};
        CombatActor* applied_by;
        // std::vector
    };
    struct CombatActor
    {
        CombatActor() = default;
        CombatActor(const LogParserTypes::Actor& act) : actor(act) {}
        LogParserTypes::Actor actor;
        std::optional<Timestamps::timestamp> first_ability_ts {};
        std::optional<Timestamps::timestamp> last_ability_ts {};
        uint64_t damage_done {};
        uint64_t damage_received {};
        unsigned num_hits {};
        unsigned num_crits {};
        float dps {};
        float dtps {};
        LogParserTypes::Health health {};
        LogParserTypes::Location location {};
        unsigned num_abilities {};
    };

  public:
    CombatStatistics(WorldStateTracker& ws) : m_ws(ws) {}
    auto update(const LogParserTypes::ParsedLogLine& event) -> void;

  protected:
    static uint64_t dmg_from(const LogParserTypes::RealValue& val);
    auto combat_actor_from(const std::optional<LogParserTypes::SourceOrTarget>& actor) -> std::optional<CombatActor*>;
    WorldStateTracker& m_ws;
    std::map<uint64_t,CombatActor> m_combat_actors;
};
