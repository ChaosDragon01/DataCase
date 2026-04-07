#include "datacase/execution/ExecutionEngine.hpp"

#include <iostream>
#include <sstream>
#include <type_traits>

namespace datacase::execution {

ExecutionEngine::ExecutionEngine(index::IBTreeIndex& index) : index_(index) {}

std::string ExecutionEngine::Execute(const query::Statement& statement) {
  return std::visit(
      [this](const auto& value) -> std::string {
        using T = std::decay_t<decltype(value)>;
        if constexpr (std::is_same_v<T, query::CreateTableStatement>) {
          tables_.try_emplace(value.table_name);
          return "OK";
        } else if constexpr (std::is_same_v<T, query::InsertStatement>) {
          auto table_it = tables_.find(value.table_name);
          if (table_it == tables_.end()) {
            return "ERROR: table does not exist";
          }
          table_it->second.values[value.key] = value.value;
          index_.Insert(value.key, static_cast<index::RecordId>(value.key));
          return "OK";
        } else if constexpr (std::is_same_v<T, query::SelectStatement>) {
          auto table_it = tables_.find(value.table_name);
          if (table_it == tables_.end()) {
            return "ERROR: table does not exist";
          }

          std::ostringstream output;
          output << "+-----+-------+\n";
          output << "| key | value |\n";
          output << "+-----+-------+\n";

          if (value.key.has_value()) {
            const auto found = index_.Find(*value.key);
            if (found.has_value()) {
              const auto data_it = table_it->second.values.find(static_cast<index::Key>(*found));
              if (data_it != table_it->second.values.end()) {
                output << "| " << *value.key << " | " << data_it->second << " |\n";
              }
            }
          } else {
            for (const auto& [key, text] : table_it->second.values) {
              output << "| " << key << " | " << text << " |\n";
            }
          }

          output << "+-----+-------+";
          return output.str();
        }
      },
      statement);
}

void RunCli(const query::IQueryParser& parser, ExecutionEngine& engine, const std::function<bool()>& should_continue) {
  std::string line;
  while (should_continue()) {
    std::cout << "db > ";
    if (!std::getline(std::cin, line)) {
      break;
    }

    if (line == "exit" || line == "quit") {
      break;
    }

    auto statement = parser.Parse(line);
    if (!statement.has_value()) {
      std::cout << "ERROR: parse failure" << std::endl;
      continue;
    }

    std::cout << engine.Execute(*statement) << std::endl;
  }
}

}  // namespace datacase::execution
