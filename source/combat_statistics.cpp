#include "combat_statistics.hpp"
#include "log_parser_types.hpp"
#include "sce_constants.hpp"

uint64_t CombatStatistics::dmg_from(const LogParserTypes::RealValue& val) {
    return val.effective.value_or(val.base_value);
}

auto CombatStatistics::combat_actor_from(const std::optional<LogParserTypes::SourceOrTarget>& actor) -> std::optional<CombatActor*> {
    if (!actor) {
        return {};
    }
    auto act_id = LogParserTypes::actor_id_from(actor->actor);
    if (m_combat_actors.contains(act_id)) {
        return &m_combat_actors.at(act_id);
    }
    m_combat_actors[act_id] = CombatActor(actor->actor);
    return &m_combat_actors.at(act_id);
}

auto CombatStatistics::update(const LogParserTypes::ParsedLogLine& event) -> void {
    m_ws.track(event);
    auto maybe_src = combat_actor_from(event.source);
    if (maybe_src) {
        (*maybe_src)->health = event.source->health;
        (*maybe_src)->location = event.source->loc;
    }
    auto maybe_tgt = combat_actor_from(event.target);
    if (maybe_tgt) {
        (*maybe_tgt)->health = event.target->health;
        (*maybe_tgt)->location = event.target->loc;
    }
        
    if (event.action.verb.cref().id == SCE::APPLY_EFFECT_ID) {
        if (event.action.noun.cref().id == SCE::DAMAGE_EFFECT_ID) {
            const auto& rv = std::get<LogParserTypes::RealValue>(*event.value);
            auto dmg = dmg_from(rv);
            auto src = *maybe_src;
            src->damage_done += dmg;
            src->num_hits += 1;
            src->num_crits += rv.crit;
            auto tgt = *maybe_tgt;
            tgt->damage_received += dmg;
        }
    } else if (event.action.verb.cref().id == SCE::EVENT_ID) {
        if (event.action.noun.cref().id == SCE::ABILITY_ACTIVATE_ID) {
        }
    }
}
