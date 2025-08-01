// -*- fil-column: 120; indent-tabs-mode: nil -*-
#include <cmath>
#include <string_view>
#include <variant>

#include "gtest.h"

#include "timestamps.hpp"
#include "log_parser_types.hpp"
#include "log_parser.hpp"
#include "log_parser_helpers.hpp"

using namespace std::literals::string_view_literals;

namespace {
    const LogParserHelpers lph;
} // namespace

TEST(StrToUint64, Empty) {
    uint64_t dist_to_first_non_int_char {};

    auto val = lph.str_to_uint64("", &dist_to_first_non_int_char);
    EXPECT_FALSE(val);

    val = lph.str_to_uint64(" ", &dist_to_first_non_int_char);
    EXPECT_FALSE(val);
}

TEST(StrToUint64, SuroundingWhitespace) {
    uint64_t dist_to_first_non_int_char {};

    auto val = lph.str_to_uint64(" 10", &dist_to_first_non_int_char);
    EXPECT_TRUE(val);
    EXPECT_EQ(*val, 10);
    EXPECT_EQ(dist_to_first_non_int_char, 3);

    val = lph.str_to_uint64("20 ", &dist_to_first_non_int_char);
    EXPECT_TRUE(val);
    EXPECT_EQ(*val, 20);
    EXPECT_EQ(dist_to_first_non_int_char, 2);

    val = lph.str_to_uint64(" 30 ", &dist_to_first_non_int_char);
    EXPECT_TRUE(val);
    EXPECT_EQ(*val, 30);
    EXPECT_EQ(dist_to_first_non_int_char, 3);
}

TEST(StrToUint64, NonIntChars) {
    uint64_t dist_to_first_non_int_char {};

    auto val = lph.str_to_uint64("A10", &dist_to_first_non_int_char);
    EXPECT_FALSE(val);

    val = lph.str_to_uint64("20B ", &dist_to_first_non_int_char);
    EXPECT_TRUE(val);
    EXPECT_EQ(*val, 20);
    EXPECT_EQ(dist_to_first_non_int_char, 2);

    val = lph.str_to_uint64(" 3C0 ", &dist_to_first_non_int_char);
    EXPECT_TRUE(val);
    EXPECT_EQ(*val, 3);
    EXPECT_EQ(dist_to_first_non_int_char, 2);
}

TEST(StrToDouble, Empty) {
    uint64_t dist_to_first_non_double_char {};

    auto val = lph.str_to_double("", &dist_to_first_non_double_char);
    EXPECT_FALSE(val);

    val = lph.str_to_double(" ", &dist_to_first_non_double_char);
    EXPECT_FALSE(val);
}

TEST(StrToDouble, SuroundingWhitespace) {
    uint64_t dist_to_first_non_double_char {};

    auto val = lph.str_to_double(" 10.0", &dist_to_first_non_double_char);
    EXPECT_TRUE(val);
    EXPECT_DOUBLE_EQ(*val, 10.0);
    EXPECT_EQ(dist_to_first_non_double_char, 5);

    val = lph.str_to_double("20.0 ", &dist_to_first_non_double_char);
    EXPECT_TRUE(val);
    EXPECT_DOUBLE_EQ(*val, 20.0);
    EXPECT_EQ(dist_to_first_non_double_char, 4);

    val = lph.str_to_double(" -30.0 ", &dist_to_first_non_double_char);
    EXPECT_TRUE(val);
    EXPECT_DOUBLE_EQ(*val, -30.0);
    EXPECT_EQ(dist_to_first_non_double_char, 6);
}

TEST(StrToDouble, NonIntChars) {
    uint64_t dist_to_first_non_double_char {};

    auto val = lph.str_to_double("A", &dist_to_first_non_double_char);
    EXPECT_FALSE(val);

    val = lph.str_to_double("A10", &dist_to_first_non_double_char);
    EXPECT_FALSE(val);

    val = lph.str_to_double("20.1B ", &dist_to_first_non_double_char);
    EXPECT_TRUE(val);
    EXPECT_DOUBLE_EQ(*val, 20.1);
    EXPECT_EQ(dist_to_first_non_double_char, 4);

    val = lph.str_to_double(" 30.9C0 ", &dist_to_first_non_double_char);
    EXPECT_TRUE(val);
    EXPECT_DOUBLE_EQ(*val, 30.9);
    EXPECT_EQ(dist_to_first_non_double_char, 5);

    val = lph.str_to_double(" 30.", &dist_to_first_non_double_char);
    EXPECT_FALSE(val);

    val = lph.str_to_double(" 30. ", &dist_to_first_non_double_char);
    EXPECT_FALSE(val);

    val = lph.str_to_double(" 30.C0 ", &dist_to_first_non_double_char);
    EXPECT_FALSE(val);
}

TEST(LStrip, Test) {
    EXPECT_EQ("abcd"sv, lph.lstrip("abcd"sv, " "));

    EXPECT_EQ("abcd"sv, lph.lstrip(" abcd"sv, " "));

    EXPECT_EQ("abcd"sv, lph.lstrip(" abcd"sv));

    EXPECT_EQ("abcd "sv, lph.lstrip("abcd "sv, " "));

    EXPECT_EQ("ab cd "sv, lph.lstrip(" ab cd "sv, " "));

    EXPECT_EQ(""sv, lph.lstrip(" "sv, " "));

    EXPECT_EQ(" abcd"sv, lph.lstrip(" abcd"sv, ""));

    //              space then tab -v             v- space then tab
    EXPECT_EQ("abcd"sv, lph.lstrip(" 	abcd"sv, " 	"));

    //              space then tab -v
    EXPECT_EQ("abcd"sv, lph.lstrip(" 	abcd"sv));

    //    tab -v          space+tab -v            v- space only
    EXPECT_EQ("	abcd"sv, lph.lstrip("	abcd"sv, " "));

    //  space -v              space -v          v- tab only
    EXPECT_EQ(" abcd"sv, lph.lstrip(" abcd"sv, "	"));
}

TEST(RStrip, Test) {
    EXPECT_EQ("abcd"sv, lph.rstrip("abcd"sv, " "));

    EXPECT_EQ("abcd"sv, lph.rstrip("abcd "sv, " "));

    EXPECT_EQ("abcd"sv, lph.rstrip("abcd "sv));

    EXPECT_EQ(" abcd"sv, lph.rstrip(" abcd"sv, " "));

    EXPECT_EQ(" ab cd"sv, lph.rstrip(" ab cd "sv, " "));

    EXPECT_EQ(""sv, lph.rstrip(" "sv, " "));

    EXPECT_EQ("abcd "sv, lph.rstrip("abcd "sv, ""));

    //                  space then tab -v             v- space then tab
    EXPECT_EQ("abcd"sv, lph.rstrip("abcd 	"sv, " 	"));

    //                  space then tab -v
    EXPECT_EQ("abcd"sv, lph.rstrip("abcd 	"sv));

    //        tab -v              space+tab -v        v- space only
    EXPECT_EQ("abcd	"sv, lph.rstrip("abcd	"sv, " "));

    //      space -v              space -v      v- tab only
    EXPECT_EQ("abcd "sv, lph.rstrip("abcd "sv, "	"));
}

TEST(Strip, Test) {
    EXPECT_EQ("abcd"sv, lph.strip("abcd"sv, " "));

    EXPECT_EQ("abcd"sv, lph.strip("abcd "sv, " "));

    EXPECT_EQ("abcd"sv, lph.strip(" abcd"sv, " "));

    EXPECT_EQ("ab cd"sv, lph.strip(" ab cd "sv, " "));

    EXPECT_EQ("ab cd"sv, lph.strip(" ab cd "sv));

    EXPECT_EQ(""sv, lph.strip(" "sv, " "));

    EXPECT_EQ(" abcd "sv, lph.strip(" abcd "sv, ""));

    //             space then tab -v--------v         v- space then tab
    EXPECT_EQ("abcd"sv, lph.strip(" 	abcd 	"sv, " 	"));

    //             space then tab -v--------v
    EXPECT_EQ("abcd"sv, lph.strip(" 	abcd 	"sv));

    //                      space -v    v- tab        v- space then tab
    EXPECT_EQ("abcd"sv, lph.strip(" abcd	"sv, " 	"));

    //             space then tab -v        v- tab then space   v- space then tab
    EXPECT_EQ("abcd"sv, lph.strip(" 	abcd	 "sv,          " 	"));

    //  space -v----v         space -v----v      v- tab only
    EXPECT_EQ(" abcd "sv, lph.strip(" abcd "sv, "	"));
}

TEST(GetNextField, Invalid) {
    EXPECT_FALSE(lph.get_next_field("", '{', '}'));

    EXPECT_FALSE(lph.get_next_field(" {", '{', '}'));

    EXPECT_FALSE(lph.get_next_field("} ", '{', '}'));

    EXPECT_FALSE(lph.get_next_field("  }{", '{', '}'));

    EXPECT_FALSE(lph.get_next_field("{  )", '{', '}'));

    EXPECT_FALSE(lph.get_next_field("{a{c}", '{', '}'));

    EXPECT_FALSE(lph.get_next_field("()", '{', '}'));
}

TEST(GetNextField, Valid) {
    uint64_t dist_beyond_field {};
    auto f = lph.get_next_field("{}", '{', '}', &dist_beyond_field);
    EXPECT_TRUE(f);
    EXPECT_EQ(*f, ""sv);
    EXPECT_EQ(dist_beyond_field, 2);

    f = lph.get_next_field(" {} ", '{', '}', &dist_beyond_field);
    EXPECT_TRUE(f);
    EXPECT_EQ(*f, ""sv);
    EXPECT_EQ(dist_beyond_field, 3);

    f = lph.get_next_field("{a}", '{', '}', &dist_beyond_field);
    EXPECT_TRUE(f);
    EXPECT_EQ(*f, "a"sv);
    EXPECT_EQ(dist_beyond_field, 3);

    //                      012345678901
    f = lph.get_next_field("{a (bc>cd.}", '{', '}', &dist_beyond_field);
    EXPECT_TRUE(f);
    EXPECT_EQ(*f, "a (bc>cd."sv);
    EXPECT_EQ(dist_beyond_field, 11);

    //                      0123456
    f = lph.get_next_field("(abcd}", '(', '}', &dist_beyond_field);
    EXPECT_TRUE(f);
    EXPECT_EQ(*f, "abcd"sv);
    EXPECT_EQ(dist_beyond_field, 6);

    //                      01234567890
    f = lph.get_next_field("(a{{a}bcd)", '(', ')', &dist_beyond_field);
    EXPECT_TRUE(f);
    EXPECT_EQ(*f, "a{{a}bcd"sv);
    EXPECT_EQ(dist_beyond_field, 10);

    //                      012345678901234567
    f = lph.get_next_field("< ab <a><d<c>>cd>", '<', '>', &dist_beyond_field);
    EXPECT_TRUE(f);
    EXPECT_EQ(*f, " ab <a><d<c>>cd"sv);
    EXPECT_EQ(dist_beyond_field, 17);
}

TEST(ParseStLocation, Invalid) {
    EXPECT_FALSE(lph.parse_st_location(""));
    EXPECT_FALSE(lph.parse_st_location("1"));
    EXPECT_FALSE(lph.parse_st_location("1,3"));
    EXPECT_FALSE(lph.parse_st_location("1,3,2"));
    EXPECT_FALSE(lph.parse_st_location("1,3,2 4"));
    EXPECT_FALSE(lph.parse_st_location("1,2,3,A"));
    EXPECT_FALSE(lph.parse_st_location("B,2,3,1"));
    EXPECT_FALSE(lph.parse_st_location("1.0,2.0,3.0,1.A"));
}

TEST(ParseStLocation, Valid) {
    auto loc = lph.parse_st_location("1.1, 2.2, 3.3, 4.4");
    ASSERT_TRUE(loc);
    EXPECT_DOUBLE_EQ(loc->x.val(), 1.1);
    EXPECT_DOUBLE_EQ(loc->y.val(), 2.2);
    EXPECT_DOUBLE_EQ(loc->z.val(), 3.3);
    EXPECT_DOUBLE_EQ(loc->rot.val(), 4.4);

    loc = lph.parse_st_location("1.1,2.2,    3.3,         4.4");
    ASSERT_TRUE(loc);
    EXPECT_DOUBLE_EQ(loc->x.val(), 1.1);
    EXPECT_DOUBLE_EQ(loc->y.val(), 2.2);
    EXPECT_DOUBLE_EQ(loc->z.val(), 3.3);
    EXPECT_DOUBLE_EQ(loc->rot.val(), 4.4);

    loc = lph.parse_st_location("1.1,2.2,3.3,4.4as- 7fdj1");
    ASSERT_TRUE(loc);
    EXPECT_DOUBLE_EQ(loc->x.val(), 1.1);
    EXPECT_DOUBLE_EQ(loc->y.val(), 2.2);
    EXPECT_DOUBLE_EQ(loc->z.val(), 3.3);
    EXPECT_DOUBLE_EQ(loc->rot.val(), 4.4);
}

TEST(ParseStHealth, Invalid) {
    EXPECT_FALSE(lph.parse_st_health(""));
    EXPECT_FALSE(lph.parse_st_health("1"));
    EXPECT_FALSE(lph.parse_st_health("1/"));
    EXPECT_FALSE(lph.parse_st_health("/2"));
    EXPECT_FALSE(lph.parse_st_health("A/B"));
}

TEST(ParseStHealth, Valid) {
    auto health = lph.parse_st_health("1/2");
    ASSERT_TRUE(health);
    EXPECT_EQ(health->current.val(), 1);
    EXPECT_EQ(health->total.val(), 2);
}

TEST(ParseNameAndId, Invalid) {
    auto naid = lph.parse_name_and_id("");
    EXPECT_FALSE(naid);

    naid = lph.parse_name_and_id("a");
    EXPECT_FALSE(naid);

    naid = lph.parse_name_and_id("a {");
    EXPECT_FALSE(naid);

    naid = lph.parse_name_and_id("}{");
    EXPECT_FALSE(naid);

    naid = lph.parse_name_and_id("a {abc}");
    EXPECT_FALSE(naid);

    naid = lph.parse_name_and_id("a {}");
    EXPECT_FALSE(naid);
}

TEST(ParseNameAndId, Valid) {
    auto naid = lph.parse_name_and_id("{100}");
    ASSERT_TRUE(naid);
    EXPECT_EQ(naid->name, "");
    EXPECT_EQ(naid->id, 100);

    naid = lph.parse_name_and_id("    {100}");
    ASSERT_TRUE(naid);
    EXPECT_EQ(naid->name, "");
    EXPECT_EQ(naid->id, 100);

    naid = lph.parse_name_and_id("ab cd {200}");
    ASSERT_TRUE(naid);
    EXPECT_EQ(naid->name, "ab cd");
    EXPECT_EQ(naid->id, 200);

    naid = lph.parse_name_and_id("  ab cd   {300}");
    ASSERT_TRUE(naid);
    EXPECT_EQ(naid->name, "ab cd");
    EXPECT_EQ(naid->id, 300);

    naid = lph.parse_name_and_id("  ab cd{400}");
    ASSERT_TRUE(naid);
    EXPECT_EQ(naid->name, "ab cd");
    EXPECT_EQ(naid->id, 400);
}

TEST(ParseNameIdInstance, Invalid) {
    auto niid = lph.parse_name_id_instance("");
    EXPECT_FALSE(niid);

    niid = lph.parse_name_id_instance("{1}");
    EXPECT_FALSE(niid);

    niid = lph.parse_name_id_instance("{}:");
    EXPECT_FALSE(niid);

    niid = lph.parse_name_id_instance("{1}:");
    EXPECT_FALSE(niid);

    niid = lph.parse_name_id_instance("{}:1");
    EXPECT_FALSE(niid);

    niid = lph.parse_name_id_instance("a {} 1");
    EXPECT_FALSE(niid);

    niid = lph.parse_name_id_instance("a {} 1:");
    EXPECT_FALSE(niid);
}

TEST(ParseNameIdInstance, Valid) {
    auto niid = lph.parse_name_id_instance("abc {1}:2");
    ASSERT_TRUE(niid);
    EXPECT_EQ(niid->name_id.name, "abc");
    EXPECT_EQ(niid->name_id.id, 1);
    EXPECT_EQ(niid->instance, 2);

    niid = lph.parse_name_id_instance(" {3}:4");
    ASSERT_TRUE(niid);
    EXPECT_EQ(niid->name_id.name, "");
    EXPECT_EQ(niid->name_id.id, 3);
    EXPECT_EQ(niid->instance, 4);

    niid = lph.parse_name_id_instance("a { 5 }: 6 ");
    ASSERT_TRUE(niid);
    EXPECT_EQ(niid->name_id.name, "a");
    EXPECT_EQ(niid->name_id.id, 5);
    EXPECT_EQ(niid->instance, 6);
}

TEST(ParseSourceTargetSubject, Invalid) {
    auto sts = lph.parse_source_target_subject("");
    EXPECT_FALSE(sts);
}

TEST(ParseSourceTargetSubject, PcInvalid) {
    auto sts = lph.parse_source_target_subject("@#");
    EXPECT_FALSE(sts);

    sts = lph.parse_source_target_subject("@abc#");
    EXPECT_FALSE(sts);

    sts = lph.parse_source_target_subject("@abc123");
    EXPECT_FALSE(sts);
}

// Uses same test inputs as ParseNameIdInstance but with a valid PC name/id in the front.
TEST(ParseSourceTargetSubject, CompInvalid) {
    auto sts = lph.parse_source_target_subject("@abc#123/");
    EXPECT_FALSE(sts);

    sts = lph.parse_source_target_subject("@abc#123/abc");
    EXPECT_FALSE(sts);

    sts = lph.parse_source_target_subject("@abc#123/abc {}");
    EXPECT_FALSE(sts);

    sts = lph.parse_source_target_subject("@abc#123/abc {1}");
    EXPECT_FALSE(sts);

    sts = lph.parse_source_target_subject("@abc#123/abc {1}:");
    EXPECT_FALSE(sts);

    sts = lph.parse_source_target_subject("@abc#123/abc {1}:");
    EXPECT_FALSE(sts);

    sts = lph.parse_source_target_subject("@abc#123/abc {}:1");
    EXPECT_FALSE(sts);

    sts = lph.parse_source_target_subject("@abc#123/abc {} 1");
    EXPECT_FALSE(sts);
}

// Uses same test inputs as ParseNameIdInstance.
TEST(ParseSourceTargetSubject, NpcInvalid) {
    auto sts = lph.parse_source_target_subject("");
    EXPECT_FALSE(sts);

    sts = lph.parse_source_target_subject("{1}");
    EXPECT_FALSE(sts);

    sts = lph.parse_source_target_subject("{}:");
    EXPECT_FALSE(sts);

    sts = lph.parse_source_target_subject("{1}:");
    EXPECT_FALSE(sts);

    sts = lph.parse_source_target_subject("{}:1");
    EXPECT_FALSE(sts);

    sts = lph.parse_source_target_subject("a {} 1");
    EXPECT_FALSE(sts);

    sts = lph.parse_source_target_subject("a {} 1:");
    EXPECT_FALSE(sts);
}

TEST(ParseSourceTargetSubject, Valid) {
    auto stso = lph.parse_source_target_subject("@bubba#1234");
    ASSERT_TRUE(stso);
    EXPECT_TRUE(std::holds_alternative<LogParserTypes::PcSubject>(*stso));
    auto pc = std::get<LogParserTypes::PcSubject>(*stso);
    EXPECT_EQ(pc.name, "bubba");
    EXPECT_EQ(pc.id, 1234);

    stso = lph.parse_source_target_subject("@#1234");
    ASSERT_TRUE(stso);
    EXPECT_TRUE(std::holds_alternative<LogParserTypes::PcSubject>(*stso));
    pc = std::get<LogParserTypes::PcSubject>(*stso);
    EXPECT_EQ(pc.name, "");
    EXPECT_EQ(pc.id, 1234);

    stso = lph.parse_source_target_subject("foo {42}:13");
    ASSERT_TRUE(stso);
    EXPECT_TRUE(std::holds_alternative<LogParserTypes::NpcSubject>(*stso));
    auto npc = std::get<LogParserTypes::NpcSubject>(*stso);
    EXPECT_EQ(npc.name_id.name, "foo");
    EXPECT_EQ(npc.name_id.id, 42);
    EXPECT_EQ(npc.instance, 13);

    stso = lph.parse_source_target_subject("@elric of melnibone#19/moonglum {23}:42");
    ASSERT_TRUE(stso);
    EXPECT_TRUE(std::holds_alternative<LogParserTypes::CompanionSubject>(*stso));
    auto comp = std::get<LogParserTypes::CompanionSubject>(*stso);
    EXPECT_EQ(comp.pc.name, "elric of melnibone");
    EXPECT_EQ(comp.pc.id, 19);
    EXPECT_EQ(comp.companion.name_id.name, "moonglum");
    EXPECT_EQ(comp.companion.name_id.id, 23);
    EXPECT_EQ(comp.companion.instance, 42);
}

TEST(ParseSourceTargetField, Invalid) {
    // The documentation explicitly states that the 3 subfields (subject, location, and health) are parsed using other
    // functions; therefore, this test will not delve into the format of the individual subfields - if they're present
    // they'll be valid. This just tests if they're present or not, although there are a few tests at the end for the
    // individual formats - just one each.
    //
    // NOTE: I know the documentation isn't lying as I wrote it BUT this might change over time so be vigilant or else
    // the Vigilant might come for you.
    auto stfo = lph.parse_source_target_field("@bubba#1234");
    EXPECT_FALSE(stfo);

    stfo = lph.parse_source_target_field("@bubba#1234|");
    EXPECT_FALSE(stfo);

    stfo = lph.parse_source_target_field("@bubba#1234||");
    EXPECT_FALSE(stfo);

    stfo = lph.parse_source_target_field("|(1.0,2.0,3.0,4.0)");
    EXPECT_FALSE(stfo);

    stfo = lph.parse_source_target_field("|(1.0,2.0,3.0,4.0)|");
    EXPECT_FALSE(stfo);

    stfo = lph.parse_source_target_field("||");
    EXPECT_FALSE(stfo);

    stfo = lph.parse_source_target_field("||(430000/435000");
    EXPECT_FALSE(stfo);

    stfo = lph.parse_source_target_field("@bubba#1234|(1.0,2.0,3.0,4.0)");
    EXPECT_FALSE(stfo);

    stfo = lph.parse_source_target_field("@bubba#1234|(1.0,2.0,3.0,4.0)|");
    EXPECT_FALSE(stfo);

    stfo = lph.parse_source_target_field("@bubba#1234||(1/2)");
    EXPECT_FALSE(stfo);

    stfo = lph.parse_source_target_field("|(1.0,2.0,3.0,4.0)|(1/2)");
    EXPECT_FALSE(stfo);

    // Missing '#' for PC.
    stfo = lph.parse_source_target_field("@bubba1234|(1.0,2.0,3.0,4.0)|(1/2)");
    EXPECT_FALSE(stfo);

    // Missing 'w' for location.
    stfo = lph.parse_source_target_field("@bubba#1234|(1.0,2.0,3.0,)|(1/2)");
    EXPECT_FALSE(stfo);

    // Total health isn't a double.
    stfo = lph.parse_source_target_field("@bubba#1234|(1.0,2.0,3.0,4.0)|(1/A)");
    EXPECT_FALSE(stfo);
}

TEST(ParseSourceTargetField, Valid) {
    auto stfo = lph.parse_source_target_field("@sckyzm#1234/Gus {234}:567|(1.0,2.0,3.0,4.0)|(200/300)");
    ASSERT_TRUE(stfo);
    ASSERT_TRUE(std::holds_alternative<LogParserTypes::CompanionSubject>(stfo->subject));
    auto comp = std::get<LogParserTypes::CompanionSubject>(stfo->subject);
    EXPECT_EQ(comp.pc.name, "sckyzm");
    EXPECT_EQ(comp.pc.id, 1234);
    EXPECT_EQ(comp.companion.name_id.name, "Gus");
    EXPECT_EQ(comp.companion.name_id.id, 234);
    EXPECT_EQ(comp.companion.instance, 567);
    auto& loc = stfo->loc;
    EXPECT_DOUBLE_EQ(loc.x.val(), 1.0);
    EXPECT_DOUBLE_EQ(loc.y.val(), 2.0);
    EXPECT_DOUBLE_EQ(loc.z.val(), 3.0);
    EXPECT_DOUBLE_EQ(loc.rot.val(), 4.0);
    auto& health = stfo->health;
    EXPECT_EQ(health.current.val(), 200);
    EXPECT_EQ(health.total.val(), 300);
}

TEST(ParseAbilityField, Test) {
    auto ability = lph.parse_ability_field("Rifle Shot {1234}");
    ASSERT_TRUE(ability);
    EXPECT_EQ(ability->name, "Rifle Shot");
    EXPECT_EQ(ability->id, 1234);

    ability = lph.parse_ability_field("{2345}");
    ASSERT_TRUE(ability);
    EXPECT_TRUE(ability->name.empty());
    EXPECT_EQ(ability->id, 2345);

    ability = lph.parse_ability_field("Rifle Shot {A}");
    EXPECT_FALSE(ability);
}

TEST(ParseActionField, Invalid) {
    EXPECT_FALSE(lph.parse_action_field(""));

    EXPECT_FALSE(lph.parse_action_field(":/"));

    EXPECT_FALSE(lph.parse_action_field(": "));

    EXPECT_FALSE(lph.parse_action_field("foo {123}"));

    EXPECT_FALSE(lph.parse_action_field("foo {123}:"));

    EXPECT_FALSE(lph.parse_action_field("foo {123}: bar {234}/"));

    EXPECT_FALSE(lph.parse_action_field("foo {123}: bar {234} "));

    EXPECT_FALSE(lph.parse_action_field("foo {123}: /"));
}

TEST(ParseActionField, Valid) {
    auto afo = lph.parse_action_field("foo {123}: bar {234}");
    EXPECT_EQ(afo->verb.val().name, "foo");
    EXPECT_EQ(afo->verb.val().id, 123);
    EXPECT_EQ(afo->noun.val().name, "bar");
    EXPECT_EQ(afo->noun.val().id, 234);
    EXPECT_FALSE(afo->detail.val());

    afo = lph.parse_action_field("baz {234}: quux {345}/bat {456}");
    EXPECT_EQ(afo->verb.val().name, "baz");
    EXPECT_EQ(afo->verb.val().id, 234);
    EXPECT_EQ(afo->noun.val().name, "quux");
    EXPECT_EQ(afo->noun.val().id, 345);
    ASSERT_TRUE(afo->detail.val());
    EXPECT_EQ(afo->detail.val()->name, "bat");
    EXPECT_EQ(afo->detail.val()->id, 456);

    afo = lph.parse_action_field("luz {345}: paz {456} yo {567}");
    EXPECT_EQ(afo->verb.val().name, "luz");
    EXPECT_EQ(afo->verb.val().id, 345);
    EXPECT_EQ(afo->noun.val().name, "paz");
    EXPECT_EQ(afo->noun.val().id, 456);
    ASSERT_TRUE(afo->detail.val());
    EXPECT_EQ(afo->detail.val()->name, "yo");
    EXPECT_EQ(afo->detail.val()->id, 567);
}

TEST(ParseValueField, ValidLogInfoValue) {
    auto vfo = lph.parse_value_field("he3001");
    ASSERT_TRUE(vfo);
}

TEST(ParseValueField, InvalidLogInfoValue) {
    auto vfo = lph.parse_value_field("he3000");
    ASSERT_FALSE(vfo);
}

TEST(ParseValueField, ValidUnmitigatedValue) {
    auto vfo = lph.parse_value_field("1");
    ASSERT_TRUE(vfo);
    ASSERT_TRUE(std::holds_alternative<LogParserTypes::UnmitigatedValue>(*vfo));
    auto umv = std::get<LogParserTypes::UnmitigatedValue>(*vfo);
    EXPECT_EQ(umv.base_value, 1);
    EXPECT_FALSE(umv.crit);
    EXPECT_FALSE(umv.detail);
    EXPECT_FALSE(umv.effective);

    vfo = lph.parse_value_field("2*");
    ASSERT_TRUE(vfo);
    ASSERT_TRUE(std::holds_alternative<LogParserTypes::UnmitigatedValue>(*vfo));
    umv = std::get<LogParserTypes::UnmitigatedValue>(*vfo);
    EXPECT_EQ(umv.base_value, 2);
    EXPECT_TRUE(umv.crit);
    EXPECT_FALSE(umv.detail);
    EXPECT_FALSE(umv.effective);

    vfo = lph.parse_value_field("3 ~1");
    ASSERT_TRUE(vfo);
    ASSERT_TRUE(std::holds_alternative<LogParserTypes::UnmitigatedValue>(*vfo));
    umv = std::get<LogParserTypes::UnmitigatedValue>(*vfo);
    EXPECT_EQ(umv.base_value, 3);
    EXPECT_FALSE(umv.crit);
    EXPECT_FALSE(umv.detail);
    ASSERT_TRUE(umv.effective);
    EXPECT_EQ(*umv.effective, 1);

    vfo = lph.parse_value_field("4* ~2");
    ASSERT_TRUE(vfo);
    ASSERT_TRUE(std::holds_alternative<LogParserTypes::UnmitigatedValue>(*vfo));
    umv = std::get<LogParserTypes::UnmitigatedValue>(*vfo);
    EXPECT_EQ(umv.base_value, 4);
    EXPECT_TRUE(umv.crit);
    EXPECT_FALSE(umv.detail);
    ASSERT_TRUE(umv.effective);
    EXPECT_EQ(*umv.effective, 2);

    vfo = lph.parse_value_field("5* ~3 foo {123}");
    ASSERT_TRUE(vfo);
    ASSERT_TRUE(std::holds_alternative<LogParserTypes::UnmitigatedValue>(*vfo));
    umv = std::get<LogParserTypes::UnmitigatedValue>(*vfo);
    EXPECT_EQ(umv.base_value, 5);
    EXPECT_TRUE(umv.crit);
    ASSERT_TRUE(umv.detail);
    EXPECT_EQ(umv.detail->name, "foo");
    EXPECT_EQ(umv.detail->id, 123);
    ASSERT_TRUE(umv.effective);
    EXPECT_EQ(*umv.effective, 3);

    vfo = lph.parse_value_field("5* ~3 foo {123}");
    ASSERT_TRUE(vfo);
    ASSERT_TRUE(std::holds_alternative<LogParserTypes::UnmitigatedValue>(*vfo));
    umv = std::get<LogParserTypes::UnmitigatedValue>(*vfo);
    EXPECT_EQ(umv.base_value, 5);
    EXPECT_TRUE(umv.crit);
    ASSERT_TRUE(umv.detail);
    EXPECT_EQ(umv.detail->name, "foo");
    EXPECT_EQ(umv.detail->id, 123);
    ASSERT_TRUE(umv.effective);
    EXPECT_EQ(*umv.effective, 3);
}

TEST(ParseValueField, ValidAbsorbedValue) {
    auto vfo = lph.parse_value_field("6* ~4 bar {123} (2 absorbed {234})");
    ASSERT_TRUE(vfo);
    ASSERT_TRUE(std::holds_alternative<LogParserTypes::AbsorbedValue>(*vfo));
    auto av = std::get<LogParserTypes::AbsorbedValue>(*vfo);
    EXPECT_EQ(av.base_value, 6);
    EXPECT_TRUE(av.crit);
    ASSERT_TRUE(av.detail);
    EXPECT_EQ(av.detail->name, "bar");
    EXPECT_EQ(av.detail->id, 123);
    ASSERT_TRUE(av.effective);
    EXPECT_EQ(av.effective, 4);
    EXPECT_EQ(av.absorbed, 2);
    EXPECT_FALSE(av.absorbed_reason);

    vfo = lph.parse_value_field("7 ~5 baz {234} -shield {345} (1 absorbed {456})");
    ASSERT_TRUE(vfo);
    ASSERT_TRUE(std::holds_alternative<LogParserTypes::AbsorbedValue>(*vfo));
    av = std::get<LogParserTypes::AbsorbedValue>(*vfo);
    EXPECT_EQ(av.base_value, 7);
    EXPECT_FALSE(av.crit);
    ASSERT_TRUE(av.detail);
    EXPECT_EQ(av.detail->name, "baz");
    EXPECT_EQ(av.detail->id, 234);
    ASSERT_TRUE(av.effective);
    EXPECT_EQ(av.effective, 5);
    EXPECT_EQ(av.absorbed, 1);
    ASSERT_TRUE(av.absorbed_reason);
    EXPECT_EQ(av.absorbed_reason->name, "shield");
}

TEST(ParseValueField, ValidFullyMitigatedValue) {
    auto vfo = lph.parse_value_field("0");
    ASSERT_TRUE(vfo);
    ASSERT_TRUE(std::holds_alternative<LogParserTypes::FullyMitigatedValue>(*vfo));
    auto fmv = std::get<LogParserTypes::FullyMitigatedValue>(*vfo);
    ASSERT_FALSE(fmv.damage_avoided_reason);

    vfo = lph.parse_value_field("0 -");
    ASSERT_TRUE(vfo);
    ASSERT_TRUE(std::holds_alternative<LogParserTypes::FullyMitigatedValue>(*vfo));
    fmv = std::get<LogParserTypes::FullyMitigatedValue>(*vfo);
    ASSERT_FALSE(fmv.damage_avoided_reason);

    vfo = lph.parse_value_field("0 -deflect {123}");
    ASSERT_TRUE(vfo);
    ASSERT_TRUE(std::holds_alternative<LogParserTypes::FullyMitigatedValue>(*vfo));
    fmv = std::get<LogParserTypes::FullyMitigatedValue>(*vfo);
    ASSERT_TRUE(fmv.damage_avoided_reason);
    EXPECT_EQ(fmv.damage_avoided_reason->name, "deflect");
    EXPECT_EQ(fmv.damage_avoided_reason->id, 123);
}

TEST(ParseValueField, ValidPartiallyMitigatedValue) {
    auto vfo = lph.parse_value_field("1 2 ");
}

TEST(ParseThreatField, ValidThreat) {
    auto to = lph.parse_threat_field("1.0");
    ASSERT_TRUE(to);
    ASSERT_TRUE(std::holds_alternative<double>(*to));
    auto threat = std::get<double>(*to);
    EXPECT_DOUBLE_EQ(threat, 1.0);

    to = lph.parse_threat_field("v7.0.0b");
    ASSERT_TRUE(to);
    ASSERT_TRUE(std::holds_alternative<std::string_view>(*to));
    auto ver = std::get<std::string_view>(*to);
    EXPECT_EQ(ver, "v7.0.0b");
}
