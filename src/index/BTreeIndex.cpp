#include "datacase/index/BTreeIndex.hpp"

#include <algorithm>
#include <cstring>
#include <stdexcept>

namespace datacase::index {

namespace {
constexpr std::uint16_t kMaxKeys = 64;

struct RawNodeHeader {
  std::uint8_t is_leaf;
  std::uint16_t key_count;
  datacase::PageId self_id;
  datacase::PageId next_leaf;
};

}  // namespace

struct BPlusTree::Node {
  bool is_leaf = true;
  datacase::PageId self_id{};
  datacase::PageId next_leaf{};
  std::vector<Key> keys;
  std::vector<RecordId> values;
  std::vector<datacase::PageId> children;
};

BPlusTree::BPlusTree(storage::IBufferPool& buffer_pool) : buffer_pool_(buffer_pool) {
  root_id_ = CreateNode(true);
}

void BPlusTree::Insert(Key key, RecordId record_id) {
  auto promoted = InsertRecursive(root_id_, key, record_id);
  if (!promoted.has_value()) {
    return;
  }

  Node new_root;
  new_root.is_leaf = false;
  new_root.self_id = CreateNode(false);
  new_root.keys.push_back(promoted->first);
  new_root.children = {root_id_, promoted->second};
  root_id_ = new_root.self_id;
  StoreNode(new_root);
}

std::optional<RecordId> BPlusTree::Find(Key key) { return FindInNode(root_id_, key); }

bool BPlusTree::Delete(Key key) { return DeleteInNode(root_id_, key); }

std::vector<RecordId> BPlusTree::RangeScan(Key start, Key end) {
  std::vector<RecordId> results;
  Node node = LoadNode(root_id_);

  while (!node.is_leaf) {
    const auto child_iter = std::upper_bound(node.keys.begin(), node.keys.end(), start);
    const auto child_index = static_cast<std::size_t>(std::distance(node.keys.begin(), child_iter));
    node = LoadNode(node.children[child_index]);
  }

  while (true) {
    for (std::size_t i = 0; i < node.keys.size(); ++i) {
      if (node.keys[i] >= start && node.keys[i] <= end) {
        results.push_back(node.values[i]);
      }
      if (node.keys[i] > end) {
        return results;
      }
    }

    if (node.next_leaf == 0) {
      break;
    }
    node = LoadNode(node.next_leaf);
  }

  return results;
}

std::optional<std::pair<Key, datacase::PageId>> BPlusTree::InsertRecursive(datacase::PageId node_id, Key key,
                                                                            RecordId record_id) {
  Node node = LoadNode(node_id);

  if (node.is_leaf) {
    const auto insert_pos = std::lower_bound(node.keys.begin(), node.keys.end(), key);
    const auto insert_idx = static_cast<std::size_t>(std::distance(node.keys.begin(), insert_pos));

    if (insert_pos != node.keys.end() && *insert_pos == key) {
      node.values[insert_idx] = record_id;
      StoreNode(node);
      return std::nullopt;
    }

    node.keys.insert(insert_pos, key);
    node.values.insert(node.values.begin() + static_cast<std::ptrdiff_t>(insert_idx), record_id);

    if (node.keys.size() <= kMaxKeys) {
      StoreNode(node);
      return std::nullopt;
    }

    Node right;
    right.is_leaf = true;
    right.self_id = CreateNode(true);
    const std::size_t split_index = node.keys.size() / 2;
    right.keys.assign(node.keys.begin() + static_cast<std::ptrdiff_t>(split_index), node.keys.end());
    right.values.assign(node.values.begin() + static_cast<std::ptrdiff_t>(split_index), node.values.end());
    node.keys.resize(split_index);
    node.values.resize(split_index);

    right.next_leaf = node.next_leaf;
    node.next_leaf = right.self_id;

    StoreNode(node);
    StoreNode(right);
    return std::make_pair(right.keys.front(), right.self_id);
  }

  const auto child_iter = std::upper_bound(node.keys.begin(), node.keys.end(), key);
  const auto child_index = static_cast<std::size_t>(std::distance(node.keys.begin(), child_iter));
  auto promoted = InsertRecursive(node.children[child_index], key, record_id);

  if (!promoted.has_value()) {
    return std::nullopt;
  }

  node.keys.insert(node.keys.begin() + static_cast<std::ptrdiff_t>(child_index), promoted->first);
  node.children.insert(node.children.begin() + static_cast<std::ptrdiff_t>(child_index + 1), promoted->second);

  if (node.keys.size() <= kMaxKeys) {
    StoreNode(node);
    return std::nullopt;
  }

  Node right;
  right.is_leaf = false;
  right.self_id = CreateNode(false);
  const std::size_t split_index = node.keys.size() / 2;
  const Key middle_key = node.keys[split_index];

  right.keys.assign(node.keys.begin() + static_cast<std::ptrdiff_t>(split_index + 1), node.keys.end());
  right.children.assign(node.children.begin() + static_cast<std::ptrdiff_t>(split_index + 1), node.children.end());

  node.keys.resize(split_index);
  node.children.resize(split_index + 1);

  StoreNode(node);
  StoreNode(right);
  return std::make_pair(middle_key, right.self_id);
}

datacase::PageId BPlusTree::CreateNode(bool is_leaf) {
  auto handle = buffer_pool_.NewPage();
  Node node;
  node.is_leaf = is_leaf;
  node.self_id = handle.id;
  StoreNode(node);
  buffer_pool_.UnpinPage(handle.id, false);
  return handle.id;
}

BPlusTree::Node BPlusTree::LoadNode(datacase::PageId node_id) {
  auto handle = buffer_pool_.FetchPage(node_id);
  Node node;

  RawNodeHeader header{};
  std::memcpy(&header, handle.data->data(), sizeof(RawNodeHeader));

  node.is_leaf = header.is_leaf != 0;
  node.self_id = header.self_id;
  node.next_leaf = header.next_leaf;

  std::size_t offset = sizeof(RawNodeHeader);
  node.keys.resize(header.key_count);
  std::memcpy(node.keys.data(), handle.data->data() + offset, header.key_count * sizeof(Key));
  offset += header.key_count * sizeof(Key);

  if (node.is_leaf) {
    node.values.resize(header.key_count);
    std::memcpy(node.values.data(), handle.data->data() + offset, header.key_count * sizeof(RecordId));
  } else {
    node.children.resize(static_cast<std::size_t>(header.key_count) + 1);
    std::memcpy(node.children.data(), handle.data->data() + offset,
                (static_cast<std::size_t>(header.key_count) + 1) * sizeof(datacase::PageId));
  }

  buffer_pool_.UnpinPage(node_id, false);
  return node;
}

void BPlusTree::StoreNode(const Node& node) {
  if (node.keys.size() > kMaxKeys) {
    throw std::runtime_error("node overflow during serialization");
  }

  auto handle = buffer_pool_.FetchPage(node.self_id);
  handle.data->fill(Byte{0});

  RawNodeHeader header{};
  header.is_leaf = node.is_leaf ? 1 : 0;
  header.key_count = static_cast<std::uint16_t>(node.keys.size());
  header.self_id = node.self_id;
  header.next_leaf = node.next_leaf;
  std::memcpy(handle.data->data(), &header, sizeof(RawNodeHeader));

  std::size_t offset = sizeof(RawNodeHeader);
  std::memcpy(handle.data->data() + offset, node.keys.data(), node.keys.size() * sizeof(Key));
  offset += node.keys.size() * sizeof(Key);

  if (node.is_leaf) {
    std::memcpy(handle.data->data() + offset, node.values.data(), node.values.size() * sizeof(RecordId));
  } else {
    std::memcpy(handle.data->data() + offset, node.children.data(), node.children.size() * sizeof(datacase::PageId));
  }

  buffer_pool_.UnpinPage(node.self_id, true);
}

std::optional<RecordId> BPlusTree::FindInNode(datacase::PageId node_id, Key key) {
  Node node = LoadNode(node_id);
  if (node.is_leaf) {
    const auto it = std::lower_bound(node.keys.begin(), node.keys.end(), key);
    if (it == node.keys.end() || *it != key) {
      return std::nullopt;
    }
    const auto idx = static_cast<std::size_t>(std::distance(node.keys.begin(), it));
    return node.values[idx];
  }

  const auto child_iter = std::upper_bound(node.keys.begin(), node.keys.end(), key);
  const auto child_index = static_cast<std::size_t>(std::distance(node.keys.begin(), child_iter));
  return FindInNode(node.children[child_index], key);
}

bool BPlusTree::DeleteInNode(datacase::PageId node_id, Key key) {
  Node node = LoadNode(node_id);
  if (node.is_leaf) {
    const auto it = std::lower_bound(node.keys.begin(), node.keys.end(), key);
    if (it == node.keys.end() || *it != key) {
      return false;
    }

    const auto idx = static_cast<std::size_t>(std::distance(node.keys.begin(), it));
    node.keys.erase(it);
    node.values.erase(node.values.begin() + static_cast<std::ptrdiff_t>(idx));
    StoreNode(node);
    return true;
  }

  const auto child_iter = std::upper_bound(node.keys.begin(), node.keys.end(), key);
  const auto child_index = static_cast<std::size_t>(std::distance(node.keys.begin(), child_iter));
  return DeleteInNode(node.children[child_index], key);
}

}  // namespace datacase::index
