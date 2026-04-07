#include <memory>

#include "datacase/execution/ExecutionEngine.hpp"
#include "datacase/index/BTreeIndex.hpp"
#include "datacase/query/QueryParser.hpp"
#include "datacase/storage/BufferPool.hpp"
#include "datacase/storage/StorageManager.hpp"

int main() {
  auto pager = std::make_shared<datacase::storage::Pager>("datacase.db");
  datacase::storage::BufferPoolManager buffer_pool(datacase::MAX_PAGES, pager);
  datacase::index::BPlusTree tree(buffer_pool);
  datacase::query::QueryParser parser;
  datacase::execution::ExecutionEngine engine(tree);
  datacase::execution::RunCli(parser, engine, [] { return true; });
  return 0;
}
