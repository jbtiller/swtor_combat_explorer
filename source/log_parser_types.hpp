// -*- fil-column: 120; indent-tabs-mode: nil -*-
#pragma once

#include <string>
#include <optional>
#include <variant>
#include <cstdint>

#include "wrapper.hpp"
#include "timestamps.hpp"

namespace LogParserTypes {
    struct Health {
	WRAPPER(Current, uint64_t);
	WRAPPER(Total, uint64_t);
	Current current;
	Total total;
    };
    struct Location {
	WRAPPER(X, double);
	WRAPPER(Y, double);
	WRAPPER(Z, double);
	WRAPPER(Rot, double);
        X x;
        Y y;
        Z z;
	Rot rot;
    };
    struct NameId {
	// Note that the name field is not unique; only the id field is.
	std::string name;
	uint64_t id;
    };
    struct NameIdInstance {
	NameId name_id;
	uint64_t instance;
    };
    using PcSubject = NameId;
    using NpcSubject = NameIdInstance;
    struct CompanionSubject {
	NameId pc;
	NameIdInstance companion;
    };
    using Subject = std::variant<PcSubject, NpcSubject, CompanionSubject>;
    struct SourceOrTarget {
	Subject subject;
	Location loc;
	Health health;
    };
    using Ability = NameId;
    struct Action {
	WRAPPER(Verb, NameId);
	WRAPPER(Noun, NameId);
	WRAPPER(Detail, std::optional<NameId>);
	Verb verb;
	Noun noun;
	Detail detail;
    };
    struct LogInfoValue {
	std::string info;
    };
    // Note it seems a bit odd to me that an "unmitigated value" should an effective value, which implies some sort of
    // mitigation. However, for this case there's no mention of why base != effective in the log entry.
    struct UnmitigatedValue {
	uint64_t base_value;
	bool crit = false;
	std::optional<NameId> detail;
	std::optional<uint64_t> effective;
    };
    // absorbed and deflected have (almost) the exact same structure. Probably need to change this to "MitigatedValue"
    // and store the mitigation type. Thank goodness increasing the level of abstraction solves all problems.  First
    // step: add "reflected" boolean
    struct AbsorbedValue {
	uint64_t base_value;
	bool crit = false;
        std::optional<uint64_t> effective;
	uint64_t absorbed;
	std::optional<NameId> detail;
	std::optional<NameId> absorbed_reason;
        bool reflected = false;
    };
    struct FullyMitigatedValue {
        std::optional<NameId> damage_avoided_reason;
    };
    using Value = std::variant<LogInfoValue, UnmitigatedValue, AbsorbedValue, FullyMitigatedValue>;
    using Threat = std::variant<double, std::string_view>;
    struct ParsedLogLine {
	Timestamps::timestamp ts;
        std::optional<SourceOrTarget> source;
	std::optional<SourceOrTarget> target;
	NameId ability;
	Action action;
	std::optional<Value> value;
	std::optional<Threat> threat;
    };
} // namespace LogParserTypes
