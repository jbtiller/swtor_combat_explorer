// -*- fil-column: 120; indent-tabs-mode: nil -*-
#pragma once

#include <cmath>
#include <string>
#include <optional>
#include <variant>
#include <cstdint>

#include "wrapper.hpp"
#include "timestamps.hpp"

namespace LogParserTypes {
    struct Health {
	WRAPPER(Current, unsigned);
	WRAPPER(Total, unsigned);
        Health() : current(Current(0)), total(Total(0)) {}
        Health(Current c, Total t) : current(c), total(t) {}
        auto operator==(const Health& other) const -> bool {
            return current.val() == other.current.val()
                && total.val() == other.total.val();
        }
	Current current;
	Total total;
    };
    struct Location {
	WRAPPER(X, double);
	WRAPPER(Y, double);
	WRAPPER(Z, double);
	WRAPPER(Rot, double);
        Location() : x(X(0)), y(Y(0)), z(Z(0)), rot(Rot(0)) {}
        Location(X _x, Y _y, Z _z, Rot _rot) : x(_x), y(_y), z(_z), rot(_rot) {}
        auto operator==(const Location& other) const -> bool {
            return (fabs(x.val() - other.x.val()) < 0.01)
                && (fabs(y.val() - other.y.val()) < 0.01)
                && (fabs(z.val() - other.z.val()) < 0.01)
                && (fabs(rot.val() - other.rot.val()) < 0.01);
        }
        X x;
        Y y;
        Z z;
	Rot rot;
    };
    struct NameId {
	// Note that the name field is not unique; only the id field is.
	std::string name;
	uint64_t id {};
    };
    struct NameIdInstance {
	NameId name_id;
	uint64_t instance;
    };
    using PcActor = NameId;
    using NpcActor = NameIdInstance;
    struct CompanionActor {
	NameId pc;
	NameIdInstance companion;
    };
    using Actor = std::variant<PcActor, NpcActor, CompanionActor>;
    struct SourceOrTarget {
	Actor actor;
	Location loc;
	Health health;
    };
    using Ability = NameId;
    struct Action {
	WRAPPER(Verb, NameId);
	WRAPPER(Noun, NameId);
	WRAPPER(Detail, std::optional<NameId>);
        Action() : verb(NameId()), noun(NameId()), detail(NameId()) {}
        Action(Verb v, Noun n, Detail d) : verb(v), noun(n), detail(d) {}
	Verb verb;
	Noun noun;
	Detail detail;
    };
    struct LogInfoValue {
	std::string info;
    };
    struct MitigationEffect {
        std::optional<uint64_t> value;
        std::optional<NameId> effect;
    };
    struct RealValue {
        uint64_t base_value {0};
        bool crit {false};
        std::optional<uint64_t> effective;
        std::optional<NameId> type;
        std::optional<NameId> mitigation_reason;
        std::optional<MitigationEffect> mitigation_effect;
    };
    using Value = std::variant<LogInfoValue, RealValue>;
    using Threat = std::variant<double, std::string>;
    struct ParsedLogLine {
	Timestamps::timestamp ts;
        std::optional<SourceOrTarget> source;
	std::optional<SourceOrTarget> target;
        std::optional<NameId> ability;
	Action action;
	std::optional<Value> value;
	std::optional<Threat> threat;
    };
} // namespace LogParserTypes
