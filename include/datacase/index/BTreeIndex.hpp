#pragma once

#include <cstdint>
#include <optional>
#include <vector>

#include "datacase/storage/BufferPool.hpp"

namespace datacase::index {

using Key = std::int64_t;
using RecordId = std::uint64_t;

class IBTreeIndex {
 public:
  virtual ~IBTreeIndex() = default;

  virtual void Insert(Key key, RecordId record_id) = 0;
  virtual std::optional<RecordId> Find(Key key) = 0;
  virtual bool Delete(Key key) = 0;
  virtual std::vector<RecordId> RangeScan(Key start, Key end) = 0;
};

class BPlusTree final : public IBTreeIndex {
 public:
  explicit BPlusTree(storage::IBufferPool& buffer_pool);

  void Insert(Key key, RecordId record_id) override;
  std::optional<RecordId> Find(Key key) override;
  bool Delete(Key key) override;
  std::vector<RecordId> RangeScan(Key start, Key end) override;

 private:
  struct Node;

  std::optional<std::pair<Key, datacase::PageId>> InsertRecursive(datacase::PageId node_id, Key key, RecordId record_id);
  datacase::PageId CreateNode(bool is_leaf);
  Node LoadNode(datacase::PageId node_id);
  void StoreNode(const Node& node);
  std::optional<RecordId> FindInNode(datacase::PageId node_id, Key key);
  bool DeleteInNode(datacase::PageId node_id, Key key);

  storage::IBufferPool& buffer_pool_;
  datacase::PageId root_id_{};
};

}  // namespace datacase::index
