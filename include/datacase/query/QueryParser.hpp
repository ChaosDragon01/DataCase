#pragma once

#include <optional>
#include <cstdint>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace datacase::query {

struct CreateTableStatement {
  std::string table_name;
};

struct InsertStatement {
  std::string table_name;
  std::int64_t key{};
  std::string value;
};

struct SelectStatement {
  std::string table_name;
  std::optional<std::int64_t> key;
};

using Statement = std::variant<CreateTableStatement, InsertStatement, SelectStatement>;

class IQueryParser {
 public:
  virtual ~IQueryParser() = default;
  virtual std::optional<Statement> Parse(std::string_view sql) const = 0;
};

class QueryParser final : public IQueryParser {
 public:
  std::optional<Statement> Parse(std::string_view sql) const override;

 private:
  static std::vector<std::string> Tokenize(std::string_view sql);
};

}  // namespace datacase::query
