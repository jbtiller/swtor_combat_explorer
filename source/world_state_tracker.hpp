#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <string>

#include "log_parser_types.hpp"
#include "timestamps.hpp"

class WorldStateTracker {
  public:
    struct PcActorClass {
        uint64_t combat_style;
        uint64_t combat_discipline;
    };

    struct AreaInfo {
        uint64_t name;
        std::optional<uint64_t> difficulty;
    };

  public:
    WorldStateTracker() = default;

    auto track(const LogParserTypes::ParsedLogLine& entry) -> void;
    
  protected:
    auto add_actor(const LogParserTypes::Actor& actor) -> void;

    inline auto update_names(const LogParserTypes::NameId& name_id) -> void {
        if (!m_names.contains(name_id.id)) {
            m_names[name_id.id] = name_id.name;
        }
    }

    std::map<uint64_t,PcActorClass> m_pcs;
    std::optional<AreaInfo> m_current_area;
    std::optional<Timestamps::timestamp> m_begin_combat;
    std::map<uint64_t,std::string> m_names;
};
