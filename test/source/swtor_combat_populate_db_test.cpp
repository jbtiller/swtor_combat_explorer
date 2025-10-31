// -*- fil-column: 120; indent-tabs-mode: nil -*-

#include <chrono>
#include <memory>
#include <string>
#include <log_parser_types.hpp>
#include "timestamps.hpp"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wall"
#include "gtest.h"
#include <pqxx/pqxx>
#pragma GCC diagnostic pop

#include "db_populator.hpp"
#include "db_custom_types.hpp"

class TestDbPopulator : public DbPopulator {
  public:
    TestDbPopulator(const DbPopulator::ConnStr& conn_str,
                    const DbPopulator::LogfileFilename& logfile_filename,
                    Timestamps::timestamp logfile_ts,
                    DbPopulator::ExistingLogfileBehavior existing_logfile_behavior)
        : DbPopulator(conn_str, logfile_filename, logfile_ts, existing_logfile_behavior) {
    }

    auto mark_fully_parsed(void) -> void {
        DbPopulator::mark_fully_parsed();
    }

    auto add_name_id(const LogParserTypes::NameId& name_id) -> int {
        return DbPopulator::add_name_id(name_id);
    }

    auto add_action(VerbId verb, NounId noun, DetailId detail) -> int {
        return DbPopulator::add_action(verb, noun, detail);
    }

    auto add_actor(LogParserTypes::Actor& actor) -> int {
        return DbPopulator::add_actor(actor);
    }

    auto add_npc_actor(LogParserTypes::NpcActor npc_actor) -> int {
        return DbPopulator::add_npc_actor(npc_actor);
    }

    auto add_pc_actor(LogParserTypes::PcActor pc_actor) -> int {
        return DbPopulator::add_pc_actor(pc_actor);
    }

    auto add_companion_actor(LogParserTypes::CompanionActor comp_actor) -> int {
        return DbPopulator::add_companion_actor(comp_actor);
    }

    auto add_pc_class(DbPopulator::PcClass pc_class) -> int {
        return DbPopulator::add_pc_class(pc_class);
    }

    auto add_class_to_pc_actor(LogParserTypes::PcActor pc_actor, PcClass pc_class) -> int {
        return DbPopulator::add_class_to_pc_actor(pc_actor, pc_class);
    }

    auto record_area_entered(DbPopulator::AreaName(area),
                             std::optional<DbPopulator::DifficultyName> difficulty =
                                 std::optional<DbPopulator::DifficultyName>()) -> int {
        return DbPopulator::record_area_entered(area, difficulty);
    }

    auto record_enter_combat(const Timestamps::timestamp& combat_begin) -> int {
        return DbPopulator::record_enter_combat(combat_begin);
    }

    auto record_exit_combat(const Timestamps::timestamp& combat_end) -> int {
        return DbPopulator::record_exit_combat(combat_end);
    }

    using DbPopulator::m_cx;
    using DbPopulator::m_tx;
    using DbPopulator::m_logfile_id;
    using DbPopulator::m_combat_id;
    using DbPopulator::m_area_id;
    using DbPopulator::m_pcs;
    
    auto get_logfile_id() -> decltype(m_logfile_id) {
        return m_logfile_id;
    }
    auto get_combat_id() -> decltype(m_combat_id) {
        return m_combat_id;
    }
    auto get_area_id() -> decltype(m_area_id) {
        return m_area_id;
    }

    inline static constexpr uint64_t NOT_APPLICABLE_NAME_ID {0};
    inline static constexpr std::string NOT_APPLICABLE_NAME_NAME {"n/a"};

    // These are pre-populated in the Name table.
    inline static constexpr int NOT_APPLICABLE_ROW_ID {1};
    inline static constexpr int UNKNOWN_COMBAT_STYLE_ROW_ID {2};
    inline static constexpr int UNKNOWN_ADVANCED_CLASS_ROW_ID {3};
    inline static constexpr int DIFFICULTY_NONE_ROW_ID {4};
    inline static constexpr int UNKNOWN_AREA_ROW_ID {5};

    // This is pre-populated in the AdvancedClass table.
    static constexpr int UNKNOWN_CLASS_ROW_ID {1};
};

class DbPopTestFix
    : public ::testing::Test {
  public:
    void SetUp() override {
        m_cx = std::make_unique<pqxx::connection>(m_conn_str);
        m_tx = std::make_unique<pqxx::nontransaction>(*m_cx, "test");

        clear_names();
        init_pop();
    }

    void TearDown() override {
        m_tx.release();
        m_cx.release();
    }

    std::unique_ptr<pqxx::connection> m_cx;
    std::unique_ptr<pqxx::nontransaction> m_tx;
    std::unique_ptr<TestDbPopulator> m_dbp;

    void clear_names() {
        // Ordering must be most dependent to least dependent to satisfy FK constraints.
        m_tx->exec("DELETE FROM Event");
        m_tx->exec("DELETE FROM Combat");
        m_tx->exec("DELETE FROM Area WHERE id > 1");
        m_tx->exec("DELETE FROM Action");
        m_tx->exec("DELETE FROM Actor ");
        m_tx->exec("DELETE FROM Advanced_Class WHERE id > 10");
        m_tx->exec("DELETE FROM Name           WHERE id > 10");
        m_tx->exec("DELETE FROM Log_File");
    }

    void init_pop() {
        ASSERT_NO_THROW(m_dbp = std::make_unique<TestDbPopulator>(
                                    DbPopulator::ConnStr(m_conn_str),
                                    DbPopulator::LogfileFilename(m_lfn),
                                    std::chrono::system_clock::now(),
                                    DbPopulator::ExistingLogfileBehavior::DELETE_ON_EXISTING));
    }


    static std::string m_conn_str;
    static std::string m_lfn;
};

std::string DbPopTestFix::m_conn_str {"dbname = sce_test   user = jason   password = jason"};
std::string DbPopTestFix::m_lfn {"logfile.txt"};

// Empty DB with existing behavior as DELETE_ON_EXISTING.
TEST(DbPopulator, Construction_EmptyDB_1) {
    pqxx::connection conn {DbPopTestFix::m_conn_str};
    pqxx::nontransaction tx {conn};
    tx.exec("DELETE FROM Event");
    tx.exec("DELETE FROM Combat");
    tx.exec("DELETE FROM Log_File");
    conn.close();

    auto now = std::chrono::system_clock::now();
    auto now_ms = static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count());
    std::unique_ptr<TestDbPopulator> dbp;
    // This will insert a new row in the Log_File table.
    ASSERT_NO_THROW(dbp = std::make_unique<TestDbPopulator>(
                    DbPopulator::ConnStr(DbPopTestFix::m_conn_str),
                    DbPopulator::LogfileFilename(DbPopTestFix::m_lfn),
                    now,
                    DbPopulator::ExistingLogfileBehavior::DELETE_ON_EXISTING));
    EXPECT_EQ(dbp->db_version(), "1");
    auto logfile_id = dbp->m_tx->query_value<int>("SELECT id FROM Log_File WHERE filename = $1",
                                                  pqxx::params(DbPopTestFix::m_lfn));
    EXPECT_EQ(logfile_id, dbp->get_logfile_id());

    pqxx::result res;
    ASSERT_NO_THROW(res = dbp->m_tx->exec("SELECT id, filename, creation_ts, fully_parsed FROM Log_File \
                                               WHERE filename = $1", pqxx::params(DbPopTestFix::m_lfn)));

    auto row = res.one_row();
    EXPECT_EQ(row.at("filename").as<std::string>(), DbPopTestFix::m_lfn);
    EXPECT_EQ(row.at("creation_ts").as<uint64_t>(), now_ms);
    EXPECT_EQ(row.at("fully_parsed").as<bool>(), false);

    ASSERT_NO_THROW(dbp.release());
}

// Empty DB with existing behavior as DELETE_ON_EXISTING_UNFINISHED.
TEST(DbPopulator, Construction_EmptyDB_2) {
    pqxx::connection conn {DbPopTestFix::m_conn_str};
    pqxx::nontransaction tx {conn};
    tx.exec("DELETE FROM Event");
    tx.exec("DELETE FROM Combat");
    tx.exec("DELETE FROM Log_File");
    conn.close();

    auto now = std::chrono::system_clock::now();
    auto now_ms = static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count());
    std::unique_ptr<TestDbPopulator> dbp;
    // This will insert a new row in the Log_File table.
    ASSERT_NO_THROW(dbp = std::make_unique<TestDbPopulator>(
                    DbPopulator::ConnStr(DbPopTestFix::m_conn_str),
                    DbPopulator::LogfileFilename(DbPopTestFix::m_lfn),
                    now,
                    DbPopulator::ExistingLogfileBehavior::DELETE_ON_EXISTING_UNFINISHED));
    EXPECT_EQ(dbp->db_version(), "1");
    auto logfile_id = dbp->m_tx->query_value<int>("SELECT id FROM Log_File WHERE filename = $1",
                                                  pqxx::params(DbPopTestFix::m_lfn));
    EXPECT_EQ(logfile_id, dbp->get_logfile_id());

    pqxx::result res;
    ASSERT_NO_THROW(res = dbp->m_tx->exec("SELECT id, filename, creation_ts, fully_parsed FROM Log_File \
                                               WHERE filename = $1", pqxx::params(DbPopTestFix::m_lfn)));

    auto row = res.one_row();
    EXPECT_EQ(row.at("filename").as<std::string>(), DbPopTestFix::m_lfn);
    EXPECT_EQ(row.at("creation_ts").as<uint64_t>(), now_ms);
    EXPECT_EQ(row.at("fully_parsed").as<bool>(), false);

    ASSERT_NO_THROW(dbp.release());
}

// Populate a Log_File row with unfinished and DELETE_ON_EXISTING.
TEST(DbPopulator, Construction_LogfileExists1) {
    auto now = std::chrono::system_clock::now();
    auto now_ms = Timestamps::timestamp_to_ms_past_epoch(now);
    int logf_row_id {};

    // Preconditions: A row exists in Log_File with the same filename and didn't finish parsing.
    {
        pqxx::connection conn {DbPopTestFix::m_conn_str};
        pqxx::nontransaction tx {conn};
        tx.exec("DELETE FROM Event");
        tx.exec("DELETE FROM Combat");
        tx.exec("DELETE FROM Log_File");

        // Populate with a log_file row.
        auto const fully_parsed = false;
        logf_row_id = tx.query_value<int>("INSERT INTO Log_File (filename, creation_ts, fully_parsed) VALUES ($1, $2, $3)"
                                          " RETURNING id", pqxx::params{DbPopTestFix::m_lfn, now_ms, fully_parsed});
        EXPECT_EQ(tx.query_value<int>("SELECT COUNT(*) FROM Log_File"), 1);
    }

    auto new_now = std::chrono::system_clock::now();
    auto new_now_ms = Timestamps::timestamp_to_ms_past_epoch(new_now);

    // Behavior: Delete previous Log_File entry and add a new one.
    std::unique_ptr<TestDbPopulator> dbp;
    ASSERT_NO_THROW(dbp = std::make_unique<TestDbPopulator>(
                        DbPopulator::ConnStr(DbPopTestFix::m_conn_str),
                        DbPopulator::LogfileFilename(DbPopTestFix::m_lfn),
                        new_now,
                        DbPopulator::ExistingLogfileBehavior::DELETE_ON_EXISTING));

    // Postconditions: Original row is deleted and new row created with different timestamp.
    {
        pqxx::connection conn {DbPopTestFix::m_conn_str};
        pqxx::nontransaction tx {conn};

        EXPECT_EQ(tx.query_value<int>("SELECT COUNT(*) FROM Log_File"), 1);
        auto res = tx.exec( "SELECT id, filename, creation_ts, fully_parsed FROM Log_File");
        ASSERT_EQ(res.size(), 1);
        auto row = res.one_row();
        EXPECT_NE(row[0].as<int>(), logf_row_id);
        EXPECT_EQ(row[1].as<std::string>(), DbPopTestFix::m_lfn);
        EXPECT_EQ(row[2].as<decltype(new_now_ms)>(), new_now_ms);
        EXPECT_EQ(row[3].as<bool>(), false);
    }
}

// Populate a Log_File row with finished and DELETE_ON_EXISTING.
TEST(DbPopulator, Construction_LogfileExists2) {
    auto now = std::chrono::system_clock::now();
    auto now_ms = Timestamps::timestamp_to_ms_past_epoch(now);
    int logf_row_id {};

    // Preconditions: A row exists in Log_File with the same filename that finished parsing.
    {
        pqxx::connection conn {DbPopTestFix::m_conn_str};
        pqxx::nontransaction tx {conn};
        tx.exec("DELETE FROM Event");
        tx.exec("DELETE FROM Combat");
        tx.exec("DELETE FROM Log_File");

        // Populate with a log_file row.
        auto const fully_parsed = true;
        logf_row_id = tx.query_value<int>("INSERT INTO Log_File (filename, creation_ts, fully_parsed) VALUES ($1, $2, $3)"
                                          " RETURNING id", pqxx::params{DbPopTestFix::m_lfn, now_ms, fully_parsed});
        EXPECT_EQ(tx.query_value<int>("SELECT COUNT(*) FROM Log_File"), 1);
    }

    auto new_now = std::chrono::system_clock::now();
    auto new_now_ms = Timestamps::timestamp_to_ms_past_epoch(new_now);

    // Behavior: Delete previous Log_File entry and add a new one.
    std::unique_ptr<TestDbPopulator> dbp;
    ASSERT_NO_THROW(dbp = std::make_unique<TestDbPopulator>(
                        DbPopulator::ConnStr(DbPopTestFix::m_conn_str),
                        DbPopulator::LogfileFilename(DbPopTestFix::m_lfn),
                        new_now,
                        DbPopulator::ExistingLogfileBehavior::DELETE_ON_EXISTING));

    // Postconditions: Original row is deleted and new row created with different timestamp.
    {
        pqxx::connection conn {DbPopTestFix::m_conn_str};
        pqxx::nontransaction tx {conn};

        EXPECT_EQ(tx.query_value<int>("SELECT COUNT(*) FROM Log_File"), 1);
        auto res = tx.exec( "SELECT id, filename, creation_ts, fully_parsed FROM Log_File");
        ASSERT_EQ(res.size(), 1);
        auto row = res.one_row();
        EXPECT_NE(row[0].as<int>(), logf_row_id);
        EXPECT_EQ(row[1].as<std::string>(), DbPopTestFix::m_lfn);
        EXPECT_EQ(row[2].as<decltype(new_now_ms)>(), new_now_ms);
        EXPECT_EQ(row[3].as<bool>(), false);
    }
}

// Populate a Log_File row with unfinished and DELETE_ON_EXISTING_UNFINISHED.
TEST(DbPopulator, Construction_LogfileExists3) {
    auto now = std::chrono::system_clock::now();
    auto now_ms = Timestamps::timestamp_to_ms_past_epoch(now);
    int logf_row_id {};

    // Preconditions: A row exists in Log_File with the same filename that didn't finish parsing.
    {
        pqxx::connection conn {DbPopTestFix::m_conn_str};
        pqxx::nontransaction tx {conn};
        tx.exec("DELETE FROM Event");
        tx.exec("DELETE FROM Combat");
        tx.exec("DELETE FROM Log_File");

        // Populate with a log_file row.
        auto const fully_parsed = false;
        logf_row_id = tx.query_value<int>("INSERT INTO Log_File (filename, creation_ts, fully_parsed) VALUES ($1, $2, $3)"
                                          " RETURNING id", pqxx::params{DbPopTestFix::m_lfn, now_ms, fully_parsed});
        EXPECT_EQ(tx.query_value<int>("SELECT COUNT(*) FROM Log_File"), 1);
    }

    auto new_now = std::chrono::system_clock::now();
    auto new_now_ms = Timestamps::timestamp_to_ms_past_epoch(new_now);

    // Behavior: Delete previous Log_File entry and add a new one.
    std::unique_ptr<TestDbPopulator> dbp;
    ASSERT_NO_THROW(dbp = std::make_unique<TestDbPopulator>(
                        DbPopulator::ConnStr(DbPopTestFix::m_conn_str),
                        DbPopulator::LogfileFilename(DbPopTestFix::m_lfn),
                        new_now,
                        DbPopulator::ExistingLogfileBehavior::DELETE_ON_EXISTING_UNFINISHED));

    // Postconditions: Original row is deleted and new row created with different timestamp.
    {
        pqxx::connection conn {DbPopTestFix::m_conn_str};
        pqxx::nontransaction tx {conn};

        EXPECT_EQ(tx.query_value<int>("SELECT COUNT(*) FROM Log_File"), 1);
        auto res = tx.exec( "SELECT id, filename, creation_ts, fully_parsed FROM Log_File");
        ASSERT_EQ(res.size(), 1);
        auto row = res.one_row();
        EXPECT_NE(row[0].as<int>(), logf_row_id);
        EXPECT_EQ(row[1].as<std::string>(), DbPopTestFix::m_lfn);
        EXPECT_EQ(row[2].as<decltype(new_now_ms)>(), new_now_ms);
        EXPECT_EQ(row[3].as<bool>(), false);
    }
}

// Populate a Log_File row with finished and DELETE_ON_EXISTING_UNFINISHED.
TEST(DbPopulator, Construction_LogfileExists4) {
    auto now = std::chrono::system_clock::now();
    auto now_ms = Timestamps::timestamp_to_ms_past_epoch(now);
    int logf_row_id {};

    // Preconditions: A row exists in Log_File with the same filename that finished parsing.
    {
        pqxx::connection conn {DbPopTestFix::m_conn_str};
        pqxx::nontransaction tx {conn};
        tx.exec("DELETE FROM Event");
        tx.exec("DELETE FROM Combat");
        tx.exec("DELETE FROM Log_File");

        // Populate with a log_file row.
        auto const fully_parsed = true;
        logf_row_id = tx.query_value<int>("INSERT INTO Log_File (filename, creation_ts, fully_parsed) VALUES ($1, $2, $3)"
                                          " RETURNING id", pqxx::params{DbPopTestFix::m_lfn, now_ms, fully_parsed});
        EXPECT_EQ(tx.query_value<int>("SELECT COUNT(*) FROM Log_File"), 1);
    }

    auto new_now = std::chrono::system_clock::now();
    auto new_now_ms = Timestamps::timestamp_to_ms_past_epoch(new_now);

    // Behavior: Exception thrown.
    std::unique_ptr<TestDbPopulator> dbp;
    ASSERT_THROW(dbp = std::make_unique<TestDbPopulator>(
                     DbPopulator::ConnStr(DbPopTestFix::m_conn_str),
                     DbPopulator::LogfileFilename(DbPopTestFix::m_lfn),
                     new_now,
                     DbPopulator::ExistingLogfileBehavior::DELETE_ON_EXISTING_UNFINISHED),
                 DbPopulator::duplicate_logfile);

    // Postconditions: Original row is unchanged and no new row added.
    {
        pqxx::connection conn {DbPopTestFix::m_conn_str};
        pqxx::nontransaction tx {conn};

        EXPECT_EQ(tx.query_value<int>("SELECT COUNT(*) FROM Log_File"), 1);
        auto res = tx.exec( "SELECT id, filename, creation_ts, fully_parsed FROM Log_File");
        ASSERT_EQ(res.size(), 1);
        auto row = res.one_row();
        EXPECT_EQ(row[0].as<int>(), logf_row_id);
        EXPECT_EQ(row[1].as<std::string>(), DbPopTestFix::m_lfn);
        EXPECT_EQ(row[2].as<decltype(now_ms)>(), now_ms);
        EXPECT_EQ(row[3].as<bool>(), true);
    }
}

// Populate a Log_File row with unfinished and DELETE_ON_EXISTING_UNFINISHED. Add additional Event, Combat, and Action
// entries and confirm that Event & Combat entries are deleted.
TEST(DbPopulator, Construction_LogfileExists5) {
    auto now = std::chrono::system_clock::now();
    auto now_ms = Timestamps::timestamp_to_ms_past_epoch(now);
    int logf_row_id {};

    // Preconditions: A row exists in Log_File that didn't finish parsing with the same filename. The Log_File entry is
    // related to Combat and Event entries.
    {
        pqxx::connection conn {DbPopTestFix::m_conn_str};
        pqxx::nontransaction tx {conn};
        tx.exec("DELETE FROM Event");
        tx.exec("DELETE FROM Combat");
        tx.exec("DELETE FROM Action");
        tx.exec("DELETE FROM Log_File");
        tx.exec("DELETE FROM Name WHERE id > 10");

        // Everything required to populate a minimal event in a combat.
        auto fully_parsed = false;
        logf_row_id = tx.query_value<int>("INSERT INTO Log_File (filename, creation_ts, fully_parsed) VALUES ($1, $2, $3)"
                                          " RETURNING id", pqxx::params{DbPopTestFix::m_lfn, now_ms, fully_parsed});
        auto action_verb_id = tx.query_value<int>("INSERT INTO Name (name_id, name) VALUES ($1, $2) RETURNING id",
                                                  pqxx::params(1000, "DoSomething"));
        auto action_noun_id = tx.query_value<int>("INSERT INTO Name (name_id, name) VALUES ($1, $2) RETURNING id",
                                                  pqxx::params(1001, "SomethingCool"));
        auto action_id = tx.query_value<int>("INSERT INTO Action (verb, noun, detail) VALUES ($1, $2, $3) RETURNING id",
                                             pqxx::params(action_verb_id, action_noun_id, DbPopulator::NOT_APPLICABLE_ROW_ID));
        auto combat_id = tx.query_value<int>("INSERT INTO Combat (ts_begin, ts_end, area, logfile) VALUES ($1, $2, $3, $4)"
                                        " RETURNING id", pqxx::params(1000000, 1100000, 1, logf_row_id));
        tx.exec("INSERT INTO Event (ts, combat, action, logfile) VALUES ($1, $2, $3, $4)",
                pqxx::params(now_ms, combat_id, action_id, logf_row_id));
    }

    auto new_now = std::chrono::system_clock::now();
    auto new_now_ms = Timestamps::timestamp_to_ms_past_epoch(new_now);

    // Behavior: Existing rows in Log_File, Combat, and Event are deleted. New row is created.
    std::unique_ptr<TestDbPopulator> dbp;
    ASSERT_NO_THROW(dbp = std::make_unique<TestDbPopulator>(
                        DbPopulator::ConnStr(DbPopTestFix::m_conn_str),
                        DbPopulator::LogfileFilename(DbPopTestFix::m_lfn),
                        new_now,
                        DbPopulator::ExistingLogfileBehavior::DELETE_ON_EXISTING_UNFINISHED));

    // Postconditions: Only new row in Log_File remains.
    {
        pqxx::connection conn {DbPopTestFix::m_conn_str};
        pqxx::nontransaction tx {conn};

        EXPECT_EQ(tx.query_value<int>("SELECT COUNT(*) FROM Event"), 0);
        EXPECT_EQ(tx.query_value<int>("SELECT COUNT(*) FROM Combat"), 0);

        EXPECT_EQ(tx.query_value<int>("SELECT COUNT(*) FROM Log_File"), 1);
        auto res = tx.exec( "SELECT id, filename, creation_ts, fully_parsed FROM Log_File");
        ASSERT_EQ(res.size(), 1);
        auto row = res.one_row();
        EXPECT_NE(row[0].as<int>(), logf_row_id);
        EXPECT_EQ(row[1].as<std::string>(), DbPopTestFix::m_lfn);
        EXPECT_EQ(row[2].as<decltype(new_now_ms)>(), new_now_ms);
        EXPECT_EQ(row[3].as<bool>(), false);
    }
}

TEST_F(DbPopTestFix, add_name_id) {
    LogParserTypes::NameId name_id {.name = "a name", .id = 123};

    // This will insert a new row in the Name table.
    int row_id {};
    ASSERT_NO_THROW(row_id = m_dbp->add_name_id(name_id));

    auto [nid, n] = m_tx->query1<uint64_t, std::string>("SELECT name_id, name FROM Name WHERE id = $1", pqxx::params(row_id));
    EXPECT_EQ(nid, name_id.id);
    EXPECT_EQ(n, name_id.name);

    // This should not insert a new row into the table.
    int new_row_id {0};
    ASSERT_NO_THROW(new_row_id = m_dbp->add_name_id(name_id));
    EXPECT_EQ(row_id, new_row_id);

    // And the row data should not have changed.
    auto res = m_tx->exec("SELECT name_id, name FROM Name WHERE id = $1", pqxx::params(row_id));
    EXPECT_NO_THROW(res.one_row());
    EXPECT_EQ(res[0][0].as<uint64_t>(), name_id.id);
    EXPECT_EQ(res[0][1].as<std::string>(), name_id.name);
}

TEST_F(DbPopTestFix, add_action) {
    struct name_row {
        int id;
        LogParserTypes::NameId name_id;
    };

    // 0s are placeholders.
    name_row verb   {0, {.name = "Strike", .id = 1234}};
    name_row noun   {0, {.name = "Slash", .id = 2345}};
    name_row detail {0, {.name = "Parry", .id = 3456}};

    verb.id = m_dbp->add_name_id(verb.name_id);
    noun.id = m_dbp->add_name_id(noun.name_id);
    detail.id = m_dbp->add_name_id(detail.name_id);

    int id = m_dbp->add_action(TestDbPopulator::VerbId(verb.id),
                               TestDbPopulator::NounId(noun.id),
                               TestDbPopulator::DetailId(detail.id));

    // Ensure the right values were stored.
    auto [rvid, rnid, rdid] = m_tx->query1<int, int, int>("SELECT verb, noun, detail FROM Action \
                                                               WHERE id = $1", pqxx::params(id));
    EXPECT_EQ(verb.id, rvid);
    EXPECT_EQ(noun.id, rnid);
    EXPECT_EQ(detail.id, rdid);

    // Call again and ensure the row ID didn't change.
    int id2 = m_dbp->add_action(TestDbPopulator::VerbId(verb.id),
                                TestDbPopulator::NounId(noun.id),
                                TestDbPopulator::DetailId(detail.id));

    EXPECT_EQ(id2, id);

    auto [rvid2, rnid2, rdid2] = m_tx->query1<int, int, int>("SELECT verb, noun, detail FROM Action \
                                                                  WHERE id = $1", pqxx::params(id));
    EXPECT_EQ(rvid, rvid2);
    EXPECT_EQ(rnid, rnid2);
    EXPECT_EQ(rdid, rdid2);
    
    // Now with an empty ("not applicable") detail.
    int id3 = m_dbp->add_action(TestDbPopulator::VerbId(verb.id),
                                TestDbPopulator::NounId(noun.id),
                                TestDbPopulator::DetailId(std::optional<int>()));
    EXPECT_NE(id3, id);

    auto na_did = TestDbPopulator::DetailId(TestDbPopulator::NOT_APPLICABLE_ROW_ID);
    auto nres= m_tx->exec("SELECT verb, noun, detail FROM Action WHERE id = $1", pqxx::params(id3));
    auto nrow = nres.one_row();
    EXPECT_EQ(verb.id, nrow.at(0).as<int>());
    EXPECT_EQ(noun.id, nrow.at(1).as<int>());
    EXPECT_EQ(na_did.val(), nrow.at(2).as<int>());
}

TEST_F(DbPopTestFix, add_npc_actor) {
    LogParserTypes::NpcActor npc {.name_id = {.name = "Bleah", .id = 1234}, .instance = 100};

    auto npc_id1 = m_dbp->add_npc_actor(npc);
    auto res = m_tx->exec("SELECT Actor.type, Name.name_id, Actor.instance FROM Actor \
                               JOIN Name ON Actor.name = Name.id \
                               WHERE Actor.id = $1", pqxx::params(npc_id1));
    ASSERT_EQ(res.size(), 1);
    auto res_row = res[0];
    EXPECT_EQ(res_row[0].as<std::string>(), "npc");
    EXPECT_EQ(res_row[1].as<int>(), 1234);
    EXPECT_EQ(res_row[2].as<uint64_t>(), 100UL);

    auto npc_id2 = m_dbp->add_npc_actor(npc);
    EXPECT_EQ(npc_id1, npc_id2);
    res = m_tx->exec("SELECT Actor.type, Name.name_id, Actor.instance FROM Actor \
                               JOIN Name ON Actor.name = Name.id \
                               WHERE Actor.id = $1", pqxx::params(npc_id2));
    res_row = res[0];
    EXPECT_EQ(res_row[0].as<std::string>(), "npc");
    EXPECT_EQ(res_row[1].as<int>(), 1234);
    EXPECT_EQ(res_row[2].as<uint64_t>(), 100UL);
}

TEST_F(DbPopTestFix, add_pc_class) {
    DbPopulator::PcClass pcc {
        .style = DbPopulator::CombatStyle{LogParserTypes::NameId{.name = "Guardian", .id = 100}},
        .advanced_class = DbPopulator::AdvancedClass{LogParserTypes::NameId{.name = "Defense", .id = 101}}
    };

    auto acid = m_dbp->add_pc_class(pcc);

    auto res = m_tx->exec("SELECT n1.name_id, n1.name, n2.name_id, n2.name \
                               FROM Advanced_Class as AC \
                                   JOIN Name as n1 ON AC.style = n1.id \
                                   JOIN Name as n2 ON AC.class = n2.id \
                               WHERE AC.id = $1", acid);
                                                    
    ASSERT_EQ(res.size(), 1);
    auto row = res[0];
    EXPECT_EQ(row[0].as<int>(), 100);
    EXPECT_EQ(row[1].as<std::string>(), "Guardian");
    EXPECT_EQ(row[2].as<int>(), 101);
    EXPECT_EQ(row[3].as<std::string>(), "Defense");

    auto acid2 = m_dbp->add_pc_class(pcc);
    EXPECT_EQ(acid, acid2);

}

// add_pc_actor tests:
//
// Possible preconditions:
//
// 1. There IS/IS NOT A PC Actor row with the specific actor's name_id with the "unknown" class.
// 2. There IS/IS NOT an entry in m_pcs for the specific pc_actor's name_id.

namespace {
    // Values to use when we want to specify them.
    LogParserTypes::NameId actor_name {.name = "Teek", .id = 101};
    LogParserTypes::NameId other_actor_name {.name = "Golo", .id = 102};
    DbPopulator::CombatStyle style_name {{.name = "Guardian", .id = 103}};
    DbPopulator::CombatStyle other_style_name {{.name = "Commando", .id = 104}};
    DbPopulator::AdvancedClass class_name {{.name = "Defense", .id = 105}};
    DbPopulator::AdvancedClass other_class_name {{.name = "Gunnery", .id = 106}};
    DbPopulator::PcClass pc_class {.style = style_name, .advanced_class = class_name};
    DbPopulator::PcClass other_pc_class {.style = other_style_name, .advanced_class = other_class_name};

    LogParserTypes::NameId comp_name {.name = "Shae Vizla", .id = 110};
    LogParserTypes::CompanionActor comp_actor {.pc = actor_name, .companion = {.name_id = comp_name, .instance = 42}};
} // namespace

// Returns results with one row with columns:
//
// 0. Actor.type
// 1. Actor.name
// 2. Actor.class
// 3/4. Actor name_id/name from Name
// 5/6. Actor combat style name_id/name via Advanced_Class
// 7/8. Actor advanced class name_id/name via Advanced_Class

auto get_actor_info(pqxx::nontransaction& tx, int actor_id) -> pqxx::result {
    //                                                          /- actor name ----\  /- style name ----\  /- class name ----\
    // cols in return row: 0           1           2            3           4        5           6        7           8
    return tx.exec("SELECT Actor.type, Actor.name, Actor.class, n1.name_id, n1.name, n2.name_id, n2.name, n3.name_id, n3.name, Actor.instance \
                         FROM Actor \
                           JOIN Name as n1 ON Actor.name = n1.id \
                           JOIN Advanced_Class as ac ON Actor.class = ac.id \
                           JOIN Name as n2 ON ac.style = n2.id \
                           JOIN Name as n3 ON ac.class = n3.id \
                         WHERE Actor.id = $1", pqxx::params(actor_id));
}

// Add PC Actor test cases. Randomly switching actor name does/doesn't exist in database.

// Case 1:
// 1. actor name NOT in table
// 2. actor with name NOT in table
// 3. m_pcs not populated
TEST_F(DbPopTestFix, add_pc_actor_1) {
    // Initial state.
    
    auto actor_id = m_dbp->add_pc_actor(actor_name);
    EXPECT_EQ(actor_id, m_dbp->m_pcs[actor_name.id].row_id);
    EXPECT_EQ(m_dbp->m_pcs[actor_name.id].class_id, DbPopulator::UNKNOWN_CLASS_ROW_ID);

    auto res = get_actor_info(*m_tx, actor_id);
    ASSERT_EQ(res.size(), 1);
    auto row = res[0];
    EXPECT_EQ(row[0].as<std::string>(), DbPopulator::ACTOR_PC_CLASS_TYPE_NAME);
    // EXPECT_EQ(row[1].as<int>(), actor_id);
    EXPECT_EQ(row[2].as<int>(), DbPopulator::UNKNOWN_CLASS_ROW_ID);
    EXPECT_EQ(row[3].as<uint64_t>(), actor_name.id);
    EXPECT_EQ(row[4].as<std::string>(), actor_name.name);
}

// Case 2:
// 1. actor name IS in table
// 2. actor with name NOT in table
// 3. m_pcs not populated
TEST_F(DbPopTestFix, add_pc_actor_2) {
    // Initial state.
    
    auto name_row_id = m_tx->query_value<int>("INSERT INTO Name (name_id, name) VALUES ($1, $2) RETURNING id",
                                              pqxx::params(actor_name.id, actor_name.name));
    auto actor_id = m_dbp->add_pc_actor(actor_name);
    EXPECT_EQ(actor_id, m_dbp->m_pcs[actor_name.id].row_id);
    EXPECT_EQ(m_dbp->m_pcs[actor_name.id].class_id, DbPopulator::UNKNOWN_CLASS_ROW_ID);

    auto res = get_actor_info(*m_tx, actor_id);
    ASSERT_EQ(res.size(), 1);
    auto row = res[0];
    EXPECT_EQ(row[0].as<std::string>(), DbPopulator::ACTOR_PC_CLASS_TYPE_NAME);
    EXPECT_EQ(row[1].as<int>(), name_row_id);
    EXPECT_EQ(row[2].as<int>(), DbPopulator::UNKNOWN_CLASS_ROW_ID);
}

// Case 3:
// 1. actor with name IS in table with "unknown" class.
// 2. m_pcs not populated
TEST_F(DbPopTestFix, add_pc_actor_3) {
    // Initial state.
    
    auto name_row_id = m_tx->query_value<int>("INSERT INTO Name (name_id, name) VALUES ($1, $2) RETURNING id",
                                              pqxx::params(actor_name.id, actor_name.name));
    auto orig_actor_id = m_tx->query_value<int>("INSERT INTO Actor (type, name, class) VALUES ($1, $2, $3) RETURNING id",
                                                pqxx::params(DbPopulator::ACTOR_PC_CLASS_TYPE_NAME, name_row_id,
                                                             DbPopulator::UNKNOWN_CLASS_ROW_ID));
    auto new_actor_id = m_dbp->add_pc_actor(actor_name);
    EXPECT_EQ(orig_actor_id, new_actor_id);
    EXPECT_EQ(new_actor_id, m_dbp->m_pcs[actor_name.id].row_id);
    EXPECT_EQ(m_dbp->m_pcs[actor_name.id].class_id, DbPopulator::UNKNOWN_CLASS_ROW_ID);

    auto res = get_actor_info(*m_tx, new_actor_id);
    ASSERT_EQ(res.size(), 1);
    auto row = res[0];
    EXPECT_EQ(row[0].as<std::string>(), DbPopulator::ACTOR_PC_CLASS_TYPE_NAME);
    EXPECT_EQ(row[1].as<int>(), name_row_id);
    EXPECT_EQ(row[2].as<int>(), DbPopulator::UNKNOWN_CLASS_ROW_ID);
}

// Case 4:
// 1. actor with name IS in table with known class.
// 2. m_pcs not populated
TEST_F(DbPopTestFix, add_pc_actor_4) {
    // Initial state.
    
    auto name_row_id = m_tx->query_value<int>("INSERT INTO Name (name_id, name) VALUES ($1, $2) RETURNING id",
                                              pqxx::params(actor_name.id, actor_name.name));
    auto class_row_id = m_tx->query_value<int>("INSERT INTO Name (name_id, name) VALUES ($1, $2) RETURNING id",
                                               pqxx::params(class_name.ref().id, class_name.ref().name));
    auto orig_actor_id = m_tx->query_value<int>("INSERT INTO Actor (type, name, class) VALUES ($1, $2, $3) RETURNING id",
                                                pqxx::params(DbPopulator::ACTOR_PC_CLASS_TYPE_NAME, name_row_id, class_row_id));

    auto new_actor_id = m_dbp->add_pc_actor(actor_name);
    EXPECT_NE(orig_actor_id, new_actor_id);
    EXPECT_EQ(new_actor_id, m_dbp->m_pcs[actor_name.id].row_id);
    EXPECT_EQ(m_dbp->m_pcs[actor_name.id].class_id, DbPopulator::UNKNOWN_CLASS_ROW_ID);

    auto res = get_actor_info(*m_tx, new_actor_id);
    ASSERT_EQ(res.size(), 1);
    auto row = res[0];
    EXPECT_EQ(row[0].as<std::string>(), DbPopulator::ACTOR_PC_CLASS_TYPE_NAME);
    EXPECT_EQ(row[1].as<int>(), name_row_id);
    EXPECT_EQ(row[2].as<int>(), DbPopulator::UNKNOWN_CLASS_ROW_ID);
}

// Case 5:
// 1. actor with name IS in table with "unknown" class.
// 2. m_pcs IS populated
TEST_F(DbPopTestFix, add_pc_actor_5) {
    // Initial state.
    
    auto name_row_id = m_tx->query_value<int>("INSERT INTO Name (name_id, name) VALUES ($1, $2) RETURNING id",
                                              pqxx::params(actor_name.id, actor_name.name));
    auto orig_actor_id = m_tx->query_value<int>("INSERT INTO Actor (type, name, class) VALUES ($1, $2, $3) RETURNING id",
                                                pqxx::params(DbPopulator::ACTOR_PC_CLASS_TYPE_NAME, name_row_id,
                                                             DbPopulator::UNKNOWN_CLASS_ROW_ID));
    m_dbp->m_pcs[actor_name.id] = DbPopulator::ActorRowInfo {.row_id = orig_actor_id, .class_id = DbPopulator::UNKNOWN_CLASS_ROW_ID};

    auto new_actor_id = m_dbp->add_pc_actor(actor_name);
    EXPECT_EQ(orig_actor_id, new_actor_id);
    EXPECT_EQ(new_actor_id, m_dbp->m_pcs[actor_name.id].row_id);
    EXPECT_EQ(m_dbp->m_pcs[actor_name.id].class_id, DbPopulator::UNKNOWN_CLASS_ROW_ID);

    auto res = get_actor_info(*m_tx, new_actor_id);
    ASSERT_EQ(res.size(), 1);
    auto row = res[0];
    EXPECT_EQ(row[0].as<std::string>(), DbPopulator::ACTOR_PC_CLASS_TYPE_NAME);
    EXPECT_EQ(row[1].as<int>(), name_row_id);
    EXPECT_EQ(row[2].as<int>(), DbPopulator::UNKNOWN_CLASS_ROW_ID);
}

// Case 6:
// 1. actor with name IS in table with known class.
// 2. m_pcs IS populated
TEST_F(DbPopTestFix, add_pc_actor_6) {
    // Initial state.
    
    auto name_row_id = m_tx->query_value<int>("INSERT INTO Name (name_id, name) VALUES ($1, $2) RETURNING id",
                                              pqxx::params(actor_name.id, actor_name.name));
    auto style_row_id = m_tx->query_value<int>("INSERT INTO Name (name_id, name) VALUES ($1, $2) RETURNING id",
                                               pqxx::params(style_name.ref().id, style_name.ref().name));
    auto discipline_row_id = m_tx->query_value<int>("INSERT INTO Name (name_id, name) VALUES ($1, $2) RETURNING id",
                                                    pqxx::params(class_name.ref().id, class_name.ref().name));
    auto class_row_id = m_tx->query_value<int>("INSERT INTO Advanced_Class (style, class) VALUES ($1, $2) RETURNING id",
                                               pqxx::params(style_row_id, discipline_row_id));
    auto orig_actor_id = m_tx->query_value<int>("INSERT INTO Actor (type, name, class) VALUES ($1, $2, $3) RETURNING id",
                                                pqxx::params(DbPopulator::ACTOR_PC_CLASS_TYPE_NAME, name_row_id, class_row_id));
    m_dbp->m_pcs[actor_name.id] = DbPopulator::ActorRowInfo {.row_id = orig_actor_id, .class_id = class_row_id};

    auto new_actor_id = m_dbp->add_pc_actor(actor_name);
    EXPECT_EQ(orig_actor_id, new_actor_id);
    EXPECT_EQ(new_actor_id, m_dbp->m_pcs[actor_name.id].row_id);
    EXPECT_EQ(m_dbp->m_pcs[actor_name.id].class_id, class_row_id);

    auto res = get_actor_info(*m_tx, new_actor_id);
    ASSERT_EQ(res.size(), 1);
    auto row = res[0];
    EXPECT_EQ(row[0].as<std::string>(), DbPopulator::ACTOR_PC_CLASS_TYPE_NAME);
    EXPECT_EQ(row[1].as<int>(), name_row_id);
    EXPECT_EQ(row[2].as<int>(), class_row_id);
}

// Companion actor tests
// 
auto get_comp_info(pqxx::nontransaction& tx, int actor_id) -> pqxx::result {
    // cols in return row: 0           1               2            3                4             5                6
    return tx.exec("SELECT Actor.type, Actor.instance, pc_actor.id, my_name.name_id, my_name.name, pc_name.name_id, pc_name.name \
                         FROM Actor \
                           JOIN Name as my_name ON Actor.name = my_name.id \
                           JOIN Actor as pc_actor ON Actor.pc = pc_actor.id \
                           JOIN Name as pc_name ON pc_actor.name = pc_name.id \
                         WHERE Actor.id = $1", pqxx::params(actor_id));
}

// Add companion test 1: Nothing yet in database
TEST_F(DbPopTestFix, add_comp_actor_1) {
    auto comp_row_id = m_dbp->add_companion_actor(comp_actor);

    auto comp_res = get_comp_info(*m_tx, comp_row_id);
    ASSERT_EQ(comp_res.size(), 1);
    auto row = comp_res[0];
    EXPECT_EQ(row[0].as<std::string>(), DbPopulator::ACTOR_COMPANION_CLASS_TYPE_NAME);
    EXPECT_EQ(row[1].as<uint64_t>(), comp_actor.companion.instance);
    // EXPECT_EQ(row[2].as<int>(), pc_row_id);
    EXPECT_EQ(row[3].as<uint64_t>(), comp_actor.companion.name_id.id);
    EXPECT_EQ(row[4].as<std::string>(), comp_actor.companion.name_id.name);
    EXPECT_EQ(row[5].as<uint64_t>(), comp_actor.pc.id);
    EXPECT_EQ(row[6].as<std::string>(), comp_actor.pc.name);
}

// Add companion test 2: PC already in database
TEST_F(DbPopTestFix, add_comp_actor_2) {
    auto pc_row_id = m_dbp->add_pc_actor(comp_actor.pc);

    auto comp_row_id = m_dbp->add_companion_actor(comp_actor);

    auto comp_res = get_comp_info(*m_tx, comp_row_id);
    ASSERT_EQ(comp_res.size(), 1);
    auto row = comp_res[0];
    EXPECT_EQ(row[0].as<std::string>(), DbPopulator::ACTOR_COMPANION_CLASS_TYPE_NAME);
    EXPECT_EQ(row[1].as<uint64_t>(), comp_actor.companion.instance);
    EXPECT_EQ(row[2].as<int>(), pc_row_id);
    EXPECT_EQ(row[3].as<uint64_t>(), comp_actor.companion.name_id.id);
    EXPECT_EQ(row[4].as<std::string>(), comp_actor.companion.name_id.name);
    EXPECT_EQ(row[5].as<uint64_t>(), comp_actor.pc.id);
    EXPECT_EQ(row[6].as<std::string>(), comp_actor.pc.name);

}

// Add class to PC Actor test 1:
// 1. m_pcs points to an Actor with the same class as the input.
TEST_F(DbPopTestFix, add_class_to_pc_actor_1) {
    // Initial condition.
    auto name_row_id = m_tx->query_value<int>("INSERT INTO Name (name_id, name) VALUES ($1, $2) RETURNING id",
                                              pqxx::params(actor_name.id, actor_name.name));
    auto style_row_id = m_tx->query_value<int>("INSERT INTO Name (name_id, name) VALUES ($1, $2) RETURNING id",
                                               pqxx::params(style_name.ref().id, style_name.ref().name));
    auto discipline_row_id = m_tx->query_value<int>("INSERT INTO Name (name_id, name) VALUES ($1, $2) RETURNING id",
                                                    pqxx::params(class_name.ref().id, class_name.ref().name));
    auto class_row_id = m_tx->query_value<int>("INSERT INTO Advanced_Class (style, class) VALUES ($1, $2) RETURNING id",
                                               pqxx::params(style_row_id, discipline_row_id));
    auto orig_actor_id = m_tx->query_value<int>("INSERT INTO Actor (type, name, class) VALUES ($1, $2, $3) RETURNING id",
                                                pqxx::params(DbPopulator::ACTOR_PC_CLASS_TYPE_NAME, name_row_id, class_row_id));
    m_dbp->m_pcs[actor_name.id] = DbPopulator::ActorRowInfo {.row_id = orig_actor_id, .class_id = class_row_id};

    auto new_actor_id = m_dbp->add_class_to_pc_actor(actor_name, pc_class);
    EXPECT_EQ(orig_actor_id, new_actor_id);
    EXPECT_EQ(new_actor_id, m_dbp->m_pcs[actor_name.id].row_id);
    EXPECT_EQ(m_dbp->m_pcs[actor_name.id].class_id, class_row_id);

    auto res = get_actor_info(*m_tx, new_actor_id);
    ASSERT_EQ(res.size(), 1);
    auto row = res[0];
    EXPECT_EQ(row[0].as<std::string>(), DbPopulator::ACTOR_PC_CLASS_TYPE_NAME);
    EXPECT_EQ(row[1].as<int>(), name_row_id);
    EXPECT_EQ(row[2].as<int>(), class_row_id);
}

// Add class to PC Actor test 2:
// 1. m_pcs points to an Actor with the "unknown" class.
TEST_F(DbPopTestFix, add_class_to_pc_actor_2) {
    // Initial condition.
    auto name_row_id = m_tx->query_value<int>("INSERT INTO Name (name_id, name) VALUES ($1, $2) RETURNING id",
                                              pqxx::params(actor_name.id, actor_name.name));
    auto orig_actor_id = m_tx->query_value<int>("INSERT INTO Actor (type, name, class) VALUES ($1, $2, $3) RETURNING id",
                                                pqxx::params(DbPopulator::ACTOR_PC_CLASS_TYPE_NAME, name_row_id,
                                                             DbPopulator::UNKNOWN_CLASS_ROW_ID));
    m_dbp->m_pcs[actor_name.id] = DbPopulator::ActorRowInfo {.row_id = orig_actor_id,
                                                             .class_id = DbPopulator::UNKNOWN_CLASS_ROW_ID};

    auto new_actor_id = m_dbp->add_class_to_pc_actor(actor_name, pc_class);
    EXPECT_EQ(orig_actor_id, new_actor_id);
    EXPECT_EQ(new_actor_id, m_dbp->m_pcs[actor_name.id].row_id);
    auto class_row_id = m_tx->query_value<int>("SELECT ac.id FROM Advanced_Class AS ac"
                                               "    JOIN Name as sn ON ac.style = sn.id"
                                               "    JOIN Name as dn ON ac.class = dn.id"
                                               "  WHERE (sn.name_id, dn.name_id) = ($1, $2)",
                                               pqxx::params(pc_class.style.ref().id, pc_class.advanced_class.ref().id));
    EXPECT_EQ(m_dbp->m_pcs[actor_name.id].class_id, class_row_id);

    auto res = get_actor_info(*m_tx, new_actor_id);
    ASSERT_EQ(res.size(), 1);
    auto row = res[0];
    EXPECT_EQ(row[0].as<std::string>(), DbPopulator::ACTOR_PC_CLASS_TYPE_NAME);
    EXPECT_EQ(row[1].as<int>(), name_row_id);
    EXPECT_EQ(row[2].as<int>(), class_row_id);
}

// Add class to PC Actor test 3:
// 1. m_pcs points to an Actor with a known class that's different from the input class.
// 2. There are multiple Actor rows and none of them have the input class.
// 3. There are multiple Actor rows and none of them have the "unknown" class.
TEST_F(DbPopTestFix, add_class_to_pc_actor_3) {
    // Initial condition.
    auto name_row_id = m_tx->query_value<int>("INSERT INTO Name (name_id, name) VALUES ($1, $2) RETURNING id",
                                              pqxx::params(actor_name.id, actor_name.name));
    auto style1_row_id = m_tx->query_value<int>("INSERT INTO Name (name_id, name) VALUES ($1, $2) RETURNING id",
                                                pqxx::params(style_name.ref().id, style_name.ref().name));
    auto style2_row_id = m_tx->query_value<int>("INSERT INTO Name (name_id, name) VALUES ($1, $2) RETURNING id",
                                                pqxx::params(other_style_name.ref().id, other_style_name.ref().name));
    auto disc1_row_id = m_tx->query_value<int>("INSERT INTO Name (name_id, name) VALUES ($1, $2) RETURNING id",
                                               pqxx::params(class_name.ref().id, class_name.ref().name));
    auto disc2_row_id = m_tx->query_value<int>("INSERT INTO Name (name_id, name) VALUES ($1, $2) RETURNING id",
                                               pqxx::params(other_class_name.ref().id, other_class_name.ref().name));
    auto class1_row_id = m_tx->query_value<int>("INSERT INTO Advanced_Class (style, class) VALUES ($1, $2) RETURNING id",
                                                pqxx::params(style1_row_id, disc1_row_id));
    auto class2_row_id = m_tx->query_value<int>("INSERT INTO Advanced_Class (style, class) VALUES ($1, $2) RETURNING id",
                                                pqxx::params(style2_row_id, disc2_row_id));
    // Both Actor rows have the same name.
    auto actor1_id = m_tx->query_value<int>("INSERT INTO Actor (type, name, class) VALUES ($1, $2, $3) RETURNING id",
                                            pqxx::params(DbPopulator::ACTOR_PC_CLASS_TYPE_NAME, name_row_id, class1_row_id));
    auto actor2_id = m_tx->query_value<int>("INSERT INTO Actor (type, name, class) VALUES ($1, $2, $3) RETURNING id",
                                            pqxx::params(DbPopulator::ACTOR_PC_CLASS_TYPE_NAME, name_row_id, class2_row_id));
    m_dbp->m_pcs[actor_name.id] = DbPopulator::ActorRowInfo {.row_id = actor1_id, .class_id = class1_row_id};

    auto style = DbPopulator::CombatStyle(LogParserTypes::NameId{.name = "CoolStyle", .id = 200});
    auto discipline = DbPopulator::AdvancedClass(LogParserTypes::NameId{.name = "CoolDiscipline", .id = 201});
    auto diff_class = DbPopulator::PcClass {.style = style, .advanced_class = discipline};

    auto new_actor_id = m_dbp->add_class_to_pc_actor(actor_name, diff_class);

    auto class_row_id = m_tx->query_value<int>("SELECT ac.id FROM Advanced_Class AS ac"
                                               "    JOIN Name as sn ON ac.style = sn.id"
                                               "    JOIN Name as dn ON ac.class = dn.id"
                                               "  WHERE (sn.name_id, dn.name_id) = ($1, $2)",
                                               pqxx::params(diff_class.style.ref().id, diff_class.advanced_class.ref().id));
    EXPECT_NE(new_actor_id, actor1_id);
    EXPECT_NE(new_actor_id, actor2_id);
    EXPECT_EQ(new_actor_id, m_dbp->m_pcs[actor_name.id].row_id);
    EXPECT_EQ(m_dbp->m_pcs[actor_name.id].class_id, class_row_id);

    auto res = get_actor_info(*m_tx, new_actor_id);
    ASSERT_EQ(res.size(), 1);
    auto row = res[0];
    EXPECT_EQ(row[0].as<std::string>(), DbPopulator::ACTOR_PC_CLASS_TYPE_NAME);
    EXPECT_EQ(row[1].as<int>(), name_row_id);
    EXPECT_EQ(row[2].as<int>(), class_row_id);
}

// Add class to PC Actor test 4:
// 1. m_pcs points to an Actor with a known class that's different from the input class.
// 2. There are multiple Actor rows and one of them has the input class.
// 3. There are multiple Actor rows and none of them have the "unknown" class.
TEST_F(DbPopTestFix, add_class_to_pc_actor_4) {
    // Initial condition.
    auto name_row_id = m_tx->query_value<int>("INSERT INTO Name (name_id, name) VALUES ($1, $2) RETURNING id",
                                              pqxx::params(actor_name.id, actor_name.name));
    auto style1_row_id = m_tx->query_value<int>("INSERT INTO Name (name_id, name) VALUES ($1, $2) RETURNING id",
                                                pqxx::params(style_name.ref().id, style_name.ref().name));
    auto style2_row_id = m_tx->query_value<int>("INSERT INTO Name (name_id, name) VALUES ($1, $2) RETURNING id",
                                                pqxx::params(other_style_name.ref().id, other_style_name.ref().name));
    auto disc1_row_id = m_tx->query_value<int>("INSERT INTO Name (name_id, name) VALUES ($1, $2) RETURNING id",
                                               pqxx::params(class_name.ref().id, class_name.ref().name));
    auto disc2_row_id = m_tx->query_value<int>("INSERT INTO Name (name_id, name) VALUES ($1, $2) RETURNING id",
                                               pqxx::params(other_class_name.ref().id, other_class_name.ref().name));
    auto class1_row_id = m_tx->query_value<int>("INSERT INTO Advanced_Class (style, class) VALUES ($1, $2) RETURNING id",
                                                pqxx::params(style1_row_id, disc1_row_id));
    auto class2_row_id = m_tx->query_value<int>("INSERT INTO Advanced_Class (style, class) VALUES ($1, $2) RETURNING id",
                                                pqxx::params(style2_row_id, disc2_row_id));
    // Both Actor rows have the same name.
    auto actor1_id = m_tx->query_value<int>("INSERT INTO Actor (type, name, class) VALUES ($1, $2, $3) RETURNING id",
                                            pqxx::params(DbPopulator::ACTOR_PC_CLASS_TYPE_NAME, name_row_id, class1_row_id));
    auto actor2_id = m_tx->query_value<int>("INSERT INTO Actor (type, name, class) VALUES ($1, $2, $3) RETURNING id",
                                            pqxx::params(DbPopulator::ACTOR_PC_CLASS_TYPE_NAME, name_row_id, class2_row_id));
    m_dbp->m_pcs[actor_name.id] = DbPopulator::ActorRowInfo {.row_id = actor1_id, .class_id = class1_row_id};

    // Note: m_pcs points to the actor with pc_class but we're adding other_pc_class.
    auto new_actor_id = m_dbp->add_class_to_pc_actor(actor_name, other_pc_class);

    auto class_row_id = m_tx->query_value<int>("SELECT ac.id FROM Advanced_Class AS ac"
                                               "    JOIN Name as sn ON ac.style = sn.id"
                                               "    JOIN Name as dn ON ac.class = dn.id"
                                               "  WHERE (sn.name_id, dn.name_id) = ($1, $2)",
                                               pqxx::params(other_pc_class.style.ref().id,
                                                            other_pc_class.advanced_class.ref().id));
    EXPECT_EQ(new_actor_id, actor2_id);
    EXPECT_EQ(new_actor_id, m_dbp->m_pcs[actor_name.id].row_id);
    EXPECT_EQ(m_dbp->m_pcs[actor_name.id].class_id, class_row_id);

    auto res = get_actor_info(*m_tx, new_actor_id);
    ASSERT_EQ(res.size(), 1);
    auto row = res[0];
    EXPECT_EQ(row[0].as<std::string>(), DbPopulator::ACTOR_PC_CLASS_TYPE_NAME);
    EXPECT_EQ(row[1].as<int>(), name_row_id);
    EXPECT_EQ(row[2].as<int>(), class_row_id);
}

// Add class to PC Actor test 5:
// 1. m_pcs points to an Actor with a known class that's different from the input class.
// 2. There are multiple Actor rows and none of them has the input class.
// 3. There are multiple Actor rows and one of them have the "unknown" class.
TEST_F(DbPopTestFix, add_class_to_pc_actor_5) {
    // Initial condition.
    auto name_row_id = m_tx->query_value<int>("INSERT INTO Name (name_id, name) VALUES ($1, $2) RETURNING id",
                                              pqxx::params(actor_name.id, actor_name.name));
    auto style1_row_id = m_tx->query_value<int>("INSERT INTO Name (name_id, name) VALUES ($1, $2) RETURNING id",
                                                pqxx::params(style_name.ref().id, style_name.ref().name));
    auto disc1_row_id = m_tx->query_value<int>("INSERT INTO Name (name_id, name) VALUES ($1, $2) RETURNING id",
                                               pqxx::params(class_name.ref().id, class_name.ref().name));
    auto class1_row_id = m_tx->query_value<int>("INSERT INTO Advanced_Class (style, class) VALUES ($1, $2) RETURNING id",
                                                pqxx::params(style1_row_id, disc1_row_id));
    // Both Actor rows have the same name.
    auto actor1_id = m_tx->query_value<int>("INSERT INTO Actor (type, name, class) VALUES ($1, $2, $3) RETURNING id",
                                            pqxx::params(DbPopulator::ACTOR_PC_CLASS_TYPE_NAME, name_row_id, class1_row_id));
    auto actor2_id = m_tx->query_value<int>("INSERT INTO Actor (type, name, class) VALUES ($1, $2, $3) RETURNING id",
                                            pqxx::params(DbPopulator::ACTOR_PC_CLASS_TYPE_NAME,
                                                         name_row_id,
                                                         DbPopulator::UNKNOWN_CLASS_ROW_ID));
    m_dbp->m_pcs[actor_name.id] = DbPopulator::ActorRowInfo {.row_id = actor1_id, .class_id = class1_row_id};

    // m_pcs has pc_class, we're adding other_class, one Actor is "unknown".
    auto new_actor_id = m_dbp->add_class_to_pc_actor(actor_name, other_pc_class);

    auto class_row_id = m_tx->query_value<int>("SELECT ac.id FROM Advanced_Class AS ac"
                                               "    JOIN Name as sn ON ac.style = sn.id"
                                               "    JOIN Name as dn ON ac.class = dn.id"
                                               "  WHERE (sn.name_id, dn.name_id) = ($1, $2)",
                                               pqxx::params(other_pc_class.style.ref().id,
                                                            other_pc_class.advanced_class.ref().id));
    EXPECT_EQ(new_actor_id, actor2_id);
    EXPECT_EQ(new_actor_id, m_dbp->m_pcs[actor_name.id].row_id);
    EXPECT_EQ(m_dbp->m_pcs[actor_name.id].class_id, class_row_id);

    auto res = get_actor_info(*m_tx, new_actor_id);
    ASSERT_EQ(res.size(), 1);
    auto row = res[0];
    EXPECT_EQ(row[0].as<std::string>(), DbPopulator::ACTOR_PC_CLASS_TYPE_NAME);
    EXPECT_EQ(row[1].as<int>(), name_row_id);
    EXPECT_EQ(row[2].as<int>(), class_row_id);
}

TEST_F(DbPopTestFix, add_actor_pc) {
    // No preconditions
    LogParserTypes::Actor act = LogParserTypes::PcActor(actor_name);
    auto actor_id = m_dbp->add_actor(act);
    EXPECT_EQ(actor_id, m_dbp->m_pcs[actor_name.id].row_id);
    EXPECT_EQ(m_dbp->m_pcs[actor_name.id].class_id, DbPopulator::UNKNOWN_CLASS_ROW_ID);

    auto res = get_actor_info(*m_tx, actor_id);
    ASSERT_EQ(res.size(), 1);
    auto row = res[0];
    EXPECT_EQ(row[0].as<std::string>(), DbPopulator::ACTOR_PC_CLASS_TYPE_NAME);
    // EXPECT_EQ(row[1].as<int>(), name_row_id);
    EXPECT_EQ(row[2].as<int>(), DbPopulator::UNKNOWN_CLASS_ROW_ID);
    EXPECT_EQ(row[3].as<uint64_t>(), actor_name.id);
    EXPECT_EQ(row[4].as<std::string>(), actor_name.name);
    // EXPECT_EQ(row[5].as<uint64_t>(), style_name.ref().id);
    // EXPECT_EQ(row[6].as<std::string>(), style_name.ref().name);
    // EXPECT_EQ(row[7].as<uint64_t>(), class_name.ref().id);
    // EXPECT_EQ(row[8].as<std::string>(), class_name.ref().name);
}

TEST_F(DbPopTestFix, add_actor_npc) {
    // No preconditions
    LogParserTypes::Actor act = LogParserTypes::NpcActor({.name_id = {.name = "Droid", .id = 100UL}, .instance = 1UL});
    auto actor_id = m_dbp->add_actor(act);

    auto res = m_tx->exec("SELECT type, name, class, pc, instance FROM Actor WHERE id = $1", pqxx::params(actor_id));
    ASSERT_EQ(res.size(), 1);
    auto row = res[0];
    auto actor_type = row[0].as<std::string>();
    auto name_row_id = row[1].as<int>();
    auto [nid, nn] = m_tx->query1<uint64_t, std::string>("SELECT name_id, name FROM Name WHERE id = $1", pqxx::params(name_row_id));
    auto instance = row[4].as<uint64_t>();
    EXPECT_EQ(actor_type, DbPopulator::ACTOR_NPC_CLASS_TYPE_NAME);
    EXPECT_EQ(nid, 100UL);
    EXPECT_EQ(nn, "Droid");
    EXPECT_TRUE(row[2].is_null());
    EXPECT_TRUE(row[3].is_null());
    EXPECT_EQ(instance, 1UL);
}

TEST_F(DbPopTestFix, add_actor_companion) {
    LogParserTypes::Actor act(comp_actor);
    auto comp_row_id = m_dbp->add_actor(act);

    auto comp_res = get_comp_info(*m_tx, comp_row_id);
    ASSERT_EQ(comp_res.size(), 1);
    auto row = comp_res[0];
    EXPECT_EQ(row[0].as<std::string>(), DbPopulator::ACTOR_COMPANION_CLASS_TYPE_NAME);
    EXPECT_EQ(row[1].as<uint64_t>(), comp_actor.companion.instance);
    // EXPECT_EQ(row[2].as<int>(), pc_row_id);
    EXPECT_EQ(row[3].as<uint64_t>(), comp_actor.companion.name_id.id);
    EXPECT_EQ(row[4].as<std::string>(), comp_actor.companion.name_id.name);
    EXPECT_EQ(row[5].as<uint64_t>(), comp_actor.pc.id);
    EXPECT_EQ(row[6].as<std::string>(), comp_actor.pc.name);
}

namespace {
    auto area_name = LogParserTypes::NameId {.name = "Coruscant", .id = 101UL};
    auto difficulty_name = LogParserTypes::NameId {.name = "Veteran", .id = 102UL};
} // namespace

TEST_F(DbPopTestFix, record_area_entered_1) {
    auto area_row_id = m_dbp->record_area_entered(DbPopulator::AreaName(area_name),
                                                  DbPopulator::DifficultyName(difficulty_name));
    EXPECT_EQ(*m_dbp->m_area_id, area_row_id);

    auto res = m_tx->exec("SELECT an.name_id, dn.name_id FROM Area \
                               JOIN Name AS an ON Area.area = an.id \
                               JOIN Name AS dn ON Area.difficulty = dn.id \
                           WHERE Area.id = $1", pqxx::params(area_row_id));
    ASSERT_EQ(res.size(), 1);
    auto row = res.one_row();
    EXPECT_EQ(row[0].as<uint64_t>(), area_name.id);
    EXPECT_EQ(row[1].as<uint64_t>(), difficulty_name.id);
}

TEST_F(DbPopTestFix, record_area_entered_2) {
    auto area_name_row_id = m_tx->query_value<int>("INSERT INTO Name (name_id, name) VALUES ($1, $2) RETURNING id",
                                                   pqxx::params(area_name.id, area_name.name));
    auto area_row_id = m_dbp->record_area_entered(DbPopulator::AreaName(area_name),
                                                  DbPopulator::DifficultyName(difficulty_name));
    EXPECT_EQ(*m_dbp->m_area_id, area_row_id);

    auto res = m_tx->exec("SELECT Area.area, dn.name_id FROM Area \
                               JOIN Name AS dn ON Area.difficulty = dn.id \
                           WHERE Area.id = $1", pqxx::params(area_row_id));
    ASSERT_EQ(res.size(), 1);
    auto row = res.one_row();
    EXPECT_EQ(row[0].as<int>(), area_name_row_id);
    EXPECT_EQ(row[1].as<uint64_t>(), difficulty_name.id);
}

TEST_F(DbPopTestFix, record_area_entered_3) {
    auto difficulty_name_row_id = m_tx->query_value<int>("INSERT INTO Name (name_id, name) VALUES ($1, $2) RETURNING id",
                                                   pqxx::params(difficulty_name.id, difficulty_name.name));
    auto area_row_id = m_dbp->record_area_entered(DbPopulator::AreaName(area_name),
                                                  DbPopulator::DifficultyName(difficulty_name));
    EXPECT_EQ(*m_dbp->m_area_id, area_row_id);

    auto res = m_tx->exec("SELECT an.name_id, Area.difficulty FROM Area \
                               JOIN Name AS an ON Area.area = an.id \
                           WHERE Area.id = $1", pqxx::params(area_row_id));
    ASSERT_EQ(res.size(), 1);
    auto row = res.one_row();
    EXPECT_EQ(row[0].as<uint64_t>(), area_name.id);
    EXPECT_EQ(row[1].as<int>(), difficulty_name_row_id);
}

TEST_F(DbPopTestFix, record_area_entered_4) {
    auto difficulty_name_row_id = m_tx->query_value<int>("INSERT INTO Name (name_id, name) VALUES ($1, $2) RETURNING id",
                                                   pqxx::params(difficulty_name.id, difficulty_name.name));
    auto area_name_row_id = m_tx->query_value<int>("INSERT INTO Name (name_id, name) VALUES ($1, $2) RETURNING id",
                                                   pqxx::params(area_name.id, area_name.name));
    auto area_row_id = m_dbp->record_area_entered(DbPopulator::AreaName(area_name),
                                                  DbPopulator::DifficultyName(difficulty_name));
    EXPECT_EQ(*m_dbp->m_area_id, area_row_id);

    auto res = m_tx->exec("SELECT area, difficulty FROM Area \
                           WHERE Area.id = $1", pqxx::params(area_row_id));
    ASSERT_EQ(res.size(), 1);
    auto row = res.one_row();
    EXPECT_EQ(row[0].as<int>(), area_name_row_id);
    EXPECT_EQ(row[1].as<int>(), difficulty_name_row_id);
}

TEST_F(DbPopTestFix, record_area_entered_5) {
    auto area_row_id = m_dbp->record_area_entered(DbPopulator::AreaName(area_name));
                                                  
    EXPECT_EQ(*m_dbp->m_area_id, area_row_id);

    auto res = m_tx->exec("SELECT an.name_id, Area.difficulty FROM Area \
                               JOIN Name AS an ON Area.area = an.id \
                           WHERE Area.id = $1", pqxx::params(area_row_id));
    ASSERT_EQ(res.size(), 1);
    auto row = res.one_row();
    EXPECT_EQ(row[0].as<uint64_t>(), area_name.id);
    EXPECT_EQ(row[1].as<int>(), DbPopulator::DIFFICULTY_NONE_ROW_ID);
}

TEST_F(DbPopTestFix, record_area_entered_6) {
    auto area_row_id_1 = m_dbp->record_area_entered(DbPopulator::AreaName(area_name), DbPopulator::DifficultyName(difficulty_name));
    EXPECT_EQ(*m_dbp->m_area_id, area_row_id_1);

    auto area_row_id_2 = m_dbp->record_area_entered(DbPopulator::AreaName(area_name), DbPopulator::DifficultyName(difficulty_name));
    EXPECT_EQ(area_row_id_1, area_row_id_2);
}

TEST_F(DbPopTestFix, record_area_entered_7) {
    auto area_row_id_1 = m_dbp->record_area_entered(DbPopulator::AreaName(area_name));
    EXPECT_EQ(*m_dbp->m_area_id, area_row_id_1);

    auto area_row_id_2 = m_dbp->record_area_entered(DbPopulator::AreaName(area_name), DbPopulator::DifficultyName(difficulty_name));
    EXPECT_NE(area_row_id_1, area_row_id_2);
}

TEST_F(DbPopTestFix, record_area_entered_8) {
    auto area_row_id_1 = m_dbp->record_area_entered(DbPopulator::AreaName(area_name), DbPopulator::DifficultyName(difficulty_name));
    EXPECT_EQ(*m_dbp->m_area_id, area_row_id_1);

    auto area_row_id_2 = m_dbp->record_area_entered(DbPopulator::AreaName(area_name));
    EXPECT_NE(area_row_id_1, area_row_id_2);
}

TEST_F(DbPopTestFix, record_enter_combat) {
    m_dbp->m_area_id = m_dbp->record_area_entered(DbPopulator::AreaName({.name = "Coruscant", .id = 100}));
    m_dbp->m_logfile_id = m_tx->query_value<int>("SELECT id from Log_File LIMIT 1");
    auto now = std::chrono::system_clock::now();

    auto num_combat_rows_1 = m_tx->query_value<int>("SELECT COUNT(*) FROM Combat");
    auto combat_id = m_dbp->record_enter_combat(now);
    auto num_combat_rows_2 = m_tx->query_value<int>("SELECT COUNT(*) FROM Combat");

    ASSERT_EQ(num_combat_rows_2 - num_combat_rows_1, 1);

    auto res = m_tx->exec("SELECT area, logfile, ts_begin FROM Combat WHERE id = $1", pqxx::params(combat_id));
    ASSERT_EQ(res.size(), 1);
    auto row = res.one_row();
    EXPECT_EQ(row[0].as<int>(), m_dbp->m_area_id);
    EXPECT_EQ(row[1].as<int>(), m_dbp->m_logfile_id);
    EXPECT_EQ(row[2].as<uint64_t>(), static_cast<uint64_t>(Timestamps::timestamp_to_ms_past_epoch(now)));
}

TEST_F(DbPopTestFix, record_exit_combat_1) {
    m_dbp->record_area_entered(DbPopulator::AreaName({.name = "Coruscant", .id = 100}));
    m_tx->query_value<int>("SELECT id from Log_File LIMIT 1");
    auto begin = std::chrono::system_clock::now();
    auto end = std::chrono::system_clock::now();

    auto combat_id_1 = m_dbp->record_enter_combat(begin);
    auto combat_id_2 = m_dbp->record_exit_combat(end);
    ASSERT_EQ(combat_id_1, combat_id_2);

    auto res = m_tx->exec("SELECT area, logfile, ts_begin, ts_end FROM Combat WHERE id = $1", pqxx::params(combat_id_2));
    ASSERT_EQ(res.size(), 1);
    auto row = res.one_row();
    EXPECT_EQ(row[0].as<int>(), m_dbp->m_area_id);
    EXPECT_EQ(row[1].as<int>(), m_dbp->m_logfile_id);
    EXPECT_EQ(row[2].as<uint64_t>(), Timestamps::timestamp_to_ms_past_epoch(begin));
    EXPECT_EQ(row[3].as<uint64_t>(), Timestamps::timestamp_to_ms_past_epoch(end));
}

TEST_F(DbPopTestFix, record_exit_combat_2) {
    m_dbp->record_area_entered(DbPopulator::AreaName({.name = "Coruscant", .id = 100}));
    m_tx->query_value<int>("SELECT id from Log_File LIMIT 1");
    auto begin = std::chrono::system_clock::now();
    auto end = std::chrono::system_clock::now();

    auto combat_id_1 = m_dbp->record_enter_combat(begin);
    auto combat_id_2 = m_dbp->record_exit_combat(end);
    auto combat_id_3 = m_dbp->record_exit_combat(end);
    ASSERT_EQ(combat_id_1, combat_id_2);
    EXPECT_EQ(combat_id_3, 0);

    auto res = m_tx->exec("SELECT area, logfile, ts_begin, ts_end FROM Combat WHERE id = $1", pqxx::params(combat_id_2));
    ASSERT_EQ(res.size(), 1);
    auto row = res.one_row();
    EXPECT_EQ(row[0].as<int>(), m_dbp->m_area_id);
    EXPECT_EQ(row[1].as<int>(), m_dbp->m_logfile_id);
    EXPECT_EQ(row[2].as<uint64_t>(), Timestamps::timestamp_to_ms_past_epoch(begin));
    EXPECT_EQ(row[3].as<uint64_t>(), Timestamps::timestamp_to_ms_past_epoch(end));
}

TEST(DbCustomType, location1) {
    using loc_t = LogParserTypes::Location;
    loc_t loc;
    loc.x = loc_t::X(1.1);
    loc.y = loc_t::Y(2.2);
    loc.z = loc_t::Z(3.3);
    loc.rot = loc_t::Rot(4.4);

    using pstl = pqxx::string_traits<LogParserTypes::Location>;
    constexpr auto len = pstl::size_buffer(loc);
    char buf[len];
    char* end = pstl::into_buf(buf, buf + len, loc);
    const char* exp = "(1.1,2.2,3.3,4.4)";
    auto exp_len = strlen(exp);
    EXPECT_STREQ(buf, exp);
    EXPECT_EQ(end, buf + exp_len + 1);
}

TEST(DbCustomType, location2) {
    using loc_t = LogParserTypes::Location;
    loc_t loc;
    loc.x = loc_t::X(1.1);
    loc.y = loc_t::Y(2.2);
    loc.z = loc_t::Z(3.3);
    loc.rot = loc_t::Rot(4.4);

    using pstl = pqxx::string_traits<LogParserTypes::Location>;
    constexpr auto len = pstl::size_buffer(loc);
    char buf[len];
    pqxx::zview sv = pstl::to_buf(buf, buf + len, loc);
    const char* exp = "(1.1,2.2,3.3,4.4)";
    auto act_len = sv.length();
    auto exp_len = strlen(exp);
    EXPECT_STREQ(buf, exp);
    EXPECT_EQ(act_len, exp_len);
    EXPECT_EQ(&sv.front(), buf);
    EXPECT_EQ(&sv.back(), buf + exp_len - 1);
    EXPECT_EQ(*(&sv.back() + 1), '\0');
}

TEST(DbCustomType, location3) {
    using loc_t = LogParserTypes::Location;
    loc_t loc;
    loc.x = loc_t::X(1.1);
    loc.y = loc_t::Y(2.2);
    loc.z = loc_t::Z(3.3);
    loc.rot = loc_t::Rot(4.4);

    using pstl = pqxx::string_traits<LogParserTypes::Location>;
    loc_t loc_ret = pstl::from_string("(1.1,2.2,3.3,4.4)");
    EXPECT_DOUBLE_EQ(loc_ret.x.val(), loc.x.val());
    EXPECT_DOUBLE_EQ(loc_ret.y.val(), loc.y.val());
    EXPECT_DOUBLE_EQ(loc_ret.z.val(), loc.z.val());
    EXPECT_DOUBLE_EQ(loc_ret.rot.val(), loc.rot.val());
}

TEST(DbCustomType, health1) {
    using h_t = LogParserTypes::Health;
    h_t h;
    h.current = h_t::Current(100);
    h.total = h_t::Total(200);

    using pstl = pqxx::string_traits<LogParserTypes::Health>;
    constexpr auto len = pstl::size_buffer(h);
    char buf[len];
    char* end = pstl::into_buf(buf, buf + len, h);
    const char* exp = "(100,200)";
    auto exp_len = strlen(exp);
    EXPECT_STREQ(buf, exp);
    EXPECT_EQ(end, buf + exp_len + 1);
}

TEST(DbCustomType, health2) {
    using h_t = LogParserTypes::Health;
    h_t h;
    h.current = h_t::Current(100);
    h.total = h_t::Total(200);

    using pstl = pqxx::string_traits<LogParserTypes::Health>;
    constexpr auto len = pstl::size_buffer(h);
    char buf[len];
    pqxx::zview sv = pstl::to_buf(buf, buf + len, h);
    const char* exp = "(100,200)";
    auto act_len = sv.length();
    auto exp_len = strlen(exp);
    EXPECT_STREQ(buf, exp);
    EXPECT_EQ(act_len, exp_len);
    EXPECT_EQ(&sv.front(), buf);
    EXPECT_EQ(&sv.back(), buf + exp_len - 1);
    EXPECT_EQ(*(&sv.back() + 1), '\0');
}

TEST(DbCustomType, health3) {
    using h_t = LogParserTypes::Health;
    h_t h;
    h.current = h_t::Current(100);
    h.total = h_t::Total(200);

    using pstl = pqxx::string_traits<LogParserTypes::Health>;
    h_t h_ret = pstl::from_string("(100,200)");
    EXPECT_DOUBLE_EQ(h_ret.current.val(), h.current.val());
    EXPECT_DOUBLE_EQ(h_ret.total.val(), h.total.val());
}

namespace {
    LogParserTypes::Location sloc (LogParserTypes::Location::X(1),
                                   LogParserTypes::Location::Y(2),
                                   LogParserTypes::Location::Z(3),
                                   LogParserTypes::Location::Rot(4));
    LogParserTypes::Health shealth (LogParserTypes::Health::Current(10),
                                    LogParserTypes::Health::Total(20));
    LogParserTypes::PcActor spc (actor_name);

    LogParserTypes::Location tloc (LogParserTypes::Location::X(5),
                                   LogParserTypes::Location::Y(6),
                                   LogParserTypes::Location::Z(7),
                                   LogParserTypes::Location::Rot(8));
    LogParserTypes::Health thealth (LogParserTypes::Health::Current(11),
                                    LogParserTypes::Health::Total(21));
    LogParserTypes::PcActor tpc (other_actor_name);
    LogParserTypes::Action action (LogParserTypes::Action::Verb({.name = "ApplyEffect", .id = 200}),
                                   LogParserTypes::Action::Noun({.name = "Damage", .id = 201}),
                                   LogParserTypes::Action::Detail(std::optional<LogParserTypes::NameId>()));
    auto rv = LogParserTypes::RealValue {.base_value = 1000, .crit = true, .effective = 999,
                                         .type = std::optional<LogParserTypes::NameId>({.name = "Weapon", .id = 600}),
                                         .mitigation_reason = std::optional<LogParserTypes::NameId>({.name = "Parry", .id = 300}),
                                         .mitigation_effect = std::optional<LogParserTypes::MitigationEffect>({
                                                 .value = 1,
                                                 .effect = std::optional<LogParserTypes::NameId>({.name = "Burn", .id = 400})})};
} // namespace

// Must be in the same namespace as the custom types for pretty-print functions to be used.
namespace LogParserTypes {
std::ostream& operator<<(std::ostream& os, const LogParserTypes::Health& h) {
    os << "Health: current=" << h.current.val() << ", total=" << h.total.val();
    return os;
}

std::ostream& operator<<(std::ostream& os, const LogParserTypes::Location& l) {
    os << "Location: x=" << l.x.val() << ", y=" << l.y.val() << ", z=" << l.z.val() << ", rot=" << l.rot.val();
    return os;
}
} // namespace LogParserTypes

TEST_F(DbPopTestFix, add_event_1) {
    auto now = std::chrono::system_clock::now();
    LogParserTypes::SourceOrTarget src = {.actor = spc, .loc = sloc, .health = shealth};
    LogParserTypes::SourceOrTarget tgt = {.actor = tpc, .loc = tloc, .health = thealth};
    LogParserTypes::Ability ability = {.name = "Strike", .id = 100};
    LogParserTypes::ParsedLogLine pll = {
        .ts = now,
        .source = src,
        .target = tgt,
        .ability = ability,
        .action = action,
        .value = rv,
        .threat = 50.0
    };

    auto event_row_id = m_dbp->populate_from_entry(pll);
    // auto res = m_tx->exec("SELECT "
    //                       /*0 */ "e.ts"
    //                       /*1 */ ",e.combat"
    //                       /*2 */ ",sa.type"
    //                       /*3 */ ",sa.class"
    //                       /*4 */ ",san.name_id"
    //                       /*5 */ ",e.source_location"
    //                       /*6 */ ",e.source_health"
    //                       /*7 */ ",tan.name_id"
    //                       /*8 */ ",ta.type"
    //                       /*9 */ ",ta.class"
    //                       /*10*/ ",e.target_location"
    //                       /*11*/ ",e.target_health"
    //                       /*12*/ ",ab.name_id"
    //                       /*13*/ ",actv.name_id"
    //                       /*14*/ ",actn.name_id"
    //                       /*15*/ ",act.detail"
    //                       /*16*/ ",e.value_version"
    //                       /*17*/ ",e.value_base"
    //                       /*18*/ ",e.value_crit"
    //                       /*19*/ ",e.value_effective"
    //                       /*20*/ ",type.id"
    //                       /*21*/ ",reason.id"
    //                       /*22*/ ",e.value_mitigation_effect_value"
    //                       /*23*/ ",eff.id"
    //                       /*24*/ ",e.threat_val"
    //                       /*25*/ ",e.threat_str"
    //                       " FROM Event as e"
    //                       "     JOIN Actor AS sa ON e.source = sa.id"
    //                       "       JOIN Name AS san ON san.id = sa.id"
    //                       "     JOIN Actor AS ta ON e.target = ta.id"
    //                       "       JOIN Name AS tan ON tan.id = ta.id"
    //                       "     JOIN Name AS ab ON e.ability = ab.id"
    //                       "     JOIN Action AS act ON e.action = act.id"
    //                       "       JOIN Name AS actv ON act.verb = actv.id"
    //                       "       JOIN Name AS actn ON act.noun = actn.id"
    //                       "     JOIN Name AS type ON e.value_type = type.id"
    //                       "     JOIN Name AS reason ON e.value_mitigation_reason = reason.id"
    //                       "     JOIN Name AS eff ON e.value_mitigation_effect_value_name = eff.id"
    //                       " WHERE e.id = $1", pqxx::params(event_row_id));
    // ASSERT_EQ(res.size(), 1);
    // auto row = res.one_row();
    // EXPECT_EQ(row[0].as<uint64_t>(), Timestamps::timestamp_to_ms_past_epoch(now));
    // EXPECT_TRUE(row[1].is_null());
    // EXPECT_EQ(row[2].as<std::string>(), DbPopulator::ACTOR_PC_CLASS_TYPE_NAME);
    // EXPECT_EQ(row[3].as<int>(), DbPopulator::UNKNOWN_CLASS_ROW_ID);
    // EXPECT_EQ(row[4].as<uint64_t>(), spc.id);
    // auto act_sloc = row[5].as<LogParserTypes::Location>();
    // EXPECT_DOUBLE_EQ(act_sloc.x.val(), sloc.x.val());
    // EXPECT_DOUBLE_EQ(act_sloc.y.val(), sloc.y.val());
    // EXPECT_DOUBLE_EQ(act_sloc.z.val(), sloc.z.val());
    // EXPECT_DOUBLE_EQ(act_sloc.rot.val(), sloc.rot.val());
    // auto act_shealth = row[6].as<LogParserTypes::Health>();
    // EXPECT_EQ(act_shealth.current.val(), shealth.current.val());
    // EXPECT_EQ(act_shealth.total.val(), shealth.total.val());

    auto res = m_tx->exec("SELECT "
                          /*0 */ "e.ts"
                          /*1 */ ",e.combat"
                          /*2 */ ",sa.type"
                          /*3 */ ",sa.class"
                          /*4 */ ",san.name_id"
                          /*5 */ ",e.source_location"
                          /*6 */ ",e.source_health"
                          /*7 */ ",ta.type"
                          /*8 */ ",ta.class"
                          /*9 */ ",tan.name_id"
                          /*10*/ ",e.target_location"
                          /*11*/ ",e.target_health"
                          /*12*/ ",abn.name_id"
                          /*13*/ ",vn.name_id"
                          /*14*/ ",nn.name_id"
                          /*15*/ ",act.detail"
                          /*16*/ ",e.value_version"
                          /*17*/ ",e.value_base"
                          /*18*/ ",e.value_crit"
                          /*19*/ ",e.value_effective"
                          /*20*/ ",vtn.name_id"
                          /*21*/ ",rn.name_id"
                          /*22*/ ",e.value_mitigation_effect_value"
                          /*23*/ ",evn.name_id"
                          /*24*/ ",e.threat_val"
                          /*25*/ ",e.threat_str"
                          /*26*/ ",e.logfile"
                          " FROM Event as e"
                          "     JOIN Actor AS sa ON e.source = sa.id"
                          "       JOIN Name AS san ON sa.name = san.id"
                          "     JOIN Actor AS ta ON e.target = ta.id"
                          "       JOIN Name AS tan ON ta.name = tan.id"
                          "     JOIN Name AS abn ON e.ability = abn.id"
                          "     JOIN Action AS act ON e.action = act.id"
                          "       JOIN Name AS vn ON act.verb = vn.id"
                          "       JOIN Name AS nn ON act.noun = nn.id"
                          "     JOIN Name AS vtn ON e.value_type = vtn.id"
                          "     JOIN Name AS rn ON e.value_mitigation_reason = rn.id"
                          "     JOIN Name AS evn ON e.value_mitigation_effect_value_name = evn.id"
                          " WHERE e.id = $1", pqxx::params(event_row_id));
    ASSERT_EQ(res.size(), 1);
    auto row = res.one_row();
    EXPECT_EQ(row[0].as<uint64_t>(), Timestamps::timestamp_to_ms_past_epoch(now));
    EXPECT_TRUE(row[1].is_null());

    EXPECT_EQ(row[2].as<std::string>(), DbPopulator::ACTOR_PC_CLASS_TYPE_NAME);
    EXPECT_EQ(row[3].as<int>(), DbPopulator::UNKNOWN_CLASS_ROW_ID);
    EXPECT_EQ(row[4].as<int>(), spc.id);
    auto act_sloc = row[5].as<LogParserTypes::Location>();
    auto act_shlth = row[6].as<LogParserTypes::Health>();
    EXPECT_EQ(act_sloc, src.loc);
    EXPECT_EQ(act_shlth, src.health);

    EXPECT_EQ(row[7].as<std::string>(), DbPopulator::ACTOR_PC_CLASS_TYPE_NAME);
    EXPECT_EQ(row[8].as<int>(), DbPopulator::UNKNOWN_CLASS_ROW_ID);
    EXPECT_EQ(row[9].as<int>(), tpc.id);
    auto act_tloc = row[10].as<LogParserTypes::Location>();
    auto act_thlth = row[11].as<LogParserTypes::Health>();
    EXPECT_EQ(act_tloc, tgt.loc);
    EXPECT_EQ(act_thlth, tgt.health);

    EXPECT_EQ(row[12].as<int>(), ability.id);

    EXPECT_EQ(row[13].as<int>(), action.verb.ref().id);
    EXPECT_EQ(row[14].as<int>(), action.noun.ref().id);
    EXPECT_EQ(row[15].as<int>(), DbPopulator::NOT_APPLICABLE_ROW_ID);

    EXPECT_TRUE(row[16].is_null());
    EXPECT_EQ(row[17].as<uint64_t>(), rv.base_value);
    EXPECT_TRUE(row[18].as<bool>());
    EXPECT_EQ(row[19].as<uint64_t>(), rv.effective);
    EXPECT_EQ(row[20].as<uint64_t>(), rv.type->id);
    EXPECT_EQ(row[21].as<uint64_t>(), rv.mitigation_reason->id);
    EXPECT_EQ(row[22].as<uint64_t>(), rv.mitigation_effect->value);
    EXPECT_EQ(row[23].as<uint64_t>(), rv.mitigation_effect->effect->id);

    EXPECT_DOUBLE_EQ(row[24].as<double>(), std::get<double>(*pll.threat));
    EXPECT_TRUE(row[25].is_null());

    EXPECT_EQ(row[26].as<int>(), m_dbp->m_logfile_id);
}

TEST_F(DbPopTestFix, mark_fully_parsed) {
    auto get_fp = [this] () {
        return m_tx->query_value<bool>("SELECT fully_parsed FROM Log_File WHERE id = $1",
                                       pqxx::params(m_dbp->m_logfile_id));
    };

    auto init_fully_parsed = get_fp();
    EXPECT_FALSE(init_fully_parsed);

    m_dbp->mark_fully_parsed();

    auto new_fully_parsed = get_fp();
    EXPECT_TRUE(new_fully_parsed);
}

