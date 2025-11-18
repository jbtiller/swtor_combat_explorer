#include "world_state_tracker.hpp"
#include "sce_constants.hpp"
#include "log_parser_types.hpp"

namespace LPT = LogParserTypes;

auto WorldStateTracker::add_actor(const LPT::Actor& actor) -> void {
    if (std::holds_alternative<LPT::PcActor>(actor)) {
        auto& pc = std::get<LPT::PcActor>(actor);
        if (!m_pcs.contains(pc.id)) {
            m_pcs[pc.id] = {};
        }
    } else if (std::holds_alternative<LPT::NpcActor>(actor)) {
        update_names(std::get<LPT::NpcActor>(actor).name_id);
    } else {
        update_names(std::get<LPT::CompanionActor>(actor).companion.name_id);
    }
}

auto WorldStateTracker::track(const LPT::ParsedLogLine& entry) -> void {
    if (entry.source) {
        add_actor(entry.source->actor);
    }
    if (entry.target) {
        add_actor(entry.source->actor);
    }
    if (entry.ability) {
        update_names(*entry.ability);
    }
    
    auto& av = entry.action.verb.cref();
    auto& an = entry.action.noun.cref();
    auto& ad = entry.action.detail.cref();
    update_names(av);
    update_names(an);
    if (ad) {
        update_names(*ad);
    }

    if (av.id == SCE::DISCIPLINE_CHANGED_ID) {
        auto& pc = std::get<LPT::PcActor>(entry.source->actor);
        m_pcs[pc.id] = {.combat_style = an.id, .combat_discipline = ad->id};
    }

    if (av.id == SCE::AREA_ENTERED_ID) {
        if (ad) {
            *m_current_area = {.name = an.id, .difficulty = ad->id};
        } else {
            *m_current_area = {.name = an.id, .difficulty = std::nullopt};
        }
    }

    if (av.id == SCE::ENTER_COMBAT_ID) {
        m_begin_combat = entry.ts;
    }
}
