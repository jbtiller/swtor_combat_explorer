#pragma once

#include <map>

#include "log_parser_types.hpp"

class PcActorTracker {
  public:
    struct PcActorClass {
        LogParserTypes::NameId combat_style;
        LogParserTypes::NameId combat_discipline;
    };

    struct PcActorInfo {
        LogParserTypes::NameId name_id;
        std::optional<PcActorClass> pc_class;
    };

    struct AreaInfo {
        LogParserTypes::NameId name;
        std::optional<LogParserTypes::NameId> difficulty;
    };

    inline static constexpr uint64_t DISCIPLINE_CHANGED_ID {836045448953665};
    inline static constexpr uint64_t AREA_ENTERED_ID       {836045448953664};
    inline static constexpr uint64_t ENTER_COMBAT_ID       {836045448945489};
    inline static constexpr uint64_t EXIT_COMBAT_ID        {836045448945490};

  public:
    PcActorTracker() = default;

    auto add_actor(const LogParserTypes::Actor& actor) -> void;
    auto track(const LogParserTypes::ParsedLogLine& entry) -> void;
    
  protected:
    std::map<uint64_t,PcActorInfo> m_pcs;
};
