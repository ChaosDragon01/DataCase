#pragma once

#include <functional>
#include <string>
#include <unordered_map>

#include "datacase/index/BTreeIndex.hpp"
#include "datacase/query/QueryParser.hpp"

namespace datacase::execution {

class ExecutionEngine {
 public:
  explicit ExecutionEngine(index::IBTreeIndex& index);

  std::string Execute(const query::Statement& statement);

 private:
  struct TableState {
    std::unordered_map<index::Key, std::string> values;
  };

  std::unordered_map<std::string, TableState> tables_;
  index::IBTreeIndex& index_;
};

void RunCli(const query::IQueryParser& parser, ExecutionEngine& engine,
            const std::function<bool()>& should_continue);

}  // namespace datacase::execution
