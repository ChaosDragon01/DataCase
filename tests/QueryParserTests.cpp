#include <gtest/gtest.h>

#include "datacase/execution/ExecutionEngine.hpp"
#include "datacase/query/QueryParser.hpp"

namespace {

TEST(QueryParserTests, ParsesSupportedStatements) {
  datacase::query::QueryParser parser;

  auto create = parser.Parse("CREATE TABLE users;");
  ASSERT_TRUE(create.has_value());
  EXPECT_TRUE(std::holds_alternative<datacase::query::CreateTableStatement>(*create));

  auto insert = parser.Parse("INSERT INTO users 7 Alice;");
  ASSERT_TRUE(insert.has_value());
  EXPECT_TRUE(std::holds_alternative<datacase::query::InsertStatement>(*insert));

  auto select = parser.Parse("SELECT * FROM users WHERE 7;");
  ASSERT_TRUE(select.has_value());
  EXPECT_TRUE(std::holds_alternative<datacase::query::SelectStatement>(*select));
}

class StubIndex final : public datacase::index::IBTreeIndex {
 public:
  void Insert(datacase::index::Key key, datacase::index::RecordId record_id) override { data_[key] = record_id; }
  std::optional<datacase::index::RecordId> Find(datacase::index::Key key) override {
    const auto it = data_.find(key);
    if (it == data_.end()) {
      return std::nullopt;
    }
    return it->second;
  }
  bool Delete(datacase::index::Key key) override { return data_.erase(key) > 0; }
  std::vector<datacase::index::RecordId> RangeScan(datacase::index::Key, datacase::index::Key) override { return {}; }

 private:
  std::unordered_map<datacase::index::Key, datacase::index::RecordId> data_;
};

TEST(QueryParserTests, ExecutionEngineHandlesRoundTrip) {
  StubIndex index;
  datacase::execution::ExecutionEngine engine(index);

  EXPECT_EQ(engine.Execute(datacase::query::CreateTableStatement{"users"}), "OK");
  EXPECT_EQ(engine.Execute(datacase::query::InsertStatement{"users", 1, "Alice"}), "OK");

  const auto output = engine.Execute(datacase::query::SelectStatement{"users", 1});
  EXPECT_NE(output.find("Alice"), std::string::npos);
}

}  // namespace
