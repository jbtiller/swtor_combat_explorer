#pragma once

#include <cstdint>
#include <map>

#include "world_state_tracker.hpp"
#include "log_parser_types.hpp"
#include "timestamps.hpp"

class CombatStatistics {
  public:
    struct CombatActor {
        LogParserTypes::Actor& actor;
        Timestamps::timestamp first_ability_ts;
        Timestamps::timestamp last_ability_ts;
        uint64_t damage_done {};
        uint64_t damage_received {};
        float dps {};
        float dtps {};
        int curr_health {};
        int num_abilities {};
    };

  public:
    CombatStatistics(Timestamps::timestamp combat_start, WorldStateTracker& ws);
    auto update(const LogParserTypes::ParsedLogLine& event) -> void;

  protected:
    WorldStateTracker& m_ws;

    Timestamps::timestamp m_combat_start;
    std::map<uint64_t,CombatActor> bleah;
};
