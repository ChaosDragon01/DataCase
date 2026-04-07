#include "datacase/query/QueryParser.hpp"

#include <algorithm>
#include <cctype>
#include <sstream>

namespace datacase::query {

namespace {

std::string Upper(std::string token) {
  std::transform(token.begin(), token.end(), token.begin(),
                 [](unsigned char ch) { return static_cast<char>(std::toupper(ch)); });
  return token;
}

}  // namespace

std::optional<Statement> QueryParser::Parse(std::string_view sql) const {
  auto tokens = Tokenize(sql);
  if (tokens.empty()) {
    return std::nullopt;
  }

  if (Upper(tokens[0]) == "CREATE" && tokens.size() >= 3 && Upper(tokens[1]) == "TABLE") {
    return CreateTableStatement{.table_name = tokens[2]};
  }

  if (Upper(tokens[0]) == "INSERT" && tokens.size() >= 5 && Upper(tokens[1]) == "INTO") {
    const std::string& table_name = tokens[2];
    try {
      const std::int64_t key = std::stoll(tokens[3]);
      return InsertStatement{.table_name = table_name, .key = key, .value = tokens[4]};
    } catch (...) {
      return std::nullopt;
    }
  }

  if (Upper(tokens[0]) == "SELECT" && tokens.size() >= 4 && tokens[1] == "*" && Upper(tokens[2]) == "FROM") {
    SelectStatement statement{.table_name = tokens[3]};
    if (tokens.size() == 6 && Upper(tokens[4]) == "WHERE") {
      try {
        statement.key = std::stoll(tokens[5]);
      } catch (...) {
        return std::nullopt;
      }
    }
    return statement;
  }

  return std::nullopt;
}

std::vector<std::string> QueryParser::Tokenize(std::string_view sql) {
  std::string normalized(sql);
  std::replace(normalized.begin(), normalized.end(), ';', ' ');

  std::istringstream stream(normalized);
  std::vector<std::string> tokens;
  std::string token;
  while (stream >> token) {
    tokens.push_back(token);
  }
  return tokens;
}

}  // namespace datacase::query
