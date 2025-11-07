#include "actor_tracker.hpp"
#include "log_parser_helpers.hpp"
#include "log_parser_types.hpp"

namespace LPT = LogParserTypes;

auto PcActorTracker::add_actor(const LogParserTypes::Actor& actor) -> void {
    if (std::holds_alternative<LPT::PcActor>(actor)) {
        auto& pc = std::get<LPT::PcActor>(actor);
        if (!m_pcs.contains(pc.id)) {
            m_pcs[pc.id] = PcActorInfo{.name_id = pc, .pc_class = {}};
        }
    }
}

auto PcActorTracker::track(const LPT::ParsedLogLine& entry) -> void {
    if (entry.source) {
        add_actor(entry.source->actor);
    }
    if (entry.target) {
        add_actor(entry.source->actor);
    }
    
    auto& av = entry.action.verb.cref();
    auto& an = entry.action.noun.cref();
    auto& ad = entry.action.detail.cref();

    if (av.id == DISCIPLINE_CHANGED_ID) {
        auto& pc = std::get<LPT::PcActor>(entry.source->actor);
        m_pcs[pc.id].pc_class.emplace(PcActorClass {.combat_style = an, .combat_discipline = *ad});
    }

    // if (av.id == 
}
