#include <filesystem>
#include <memory>

#include <gtest/gtest.h>

#include "datacase/index/BTreeIndex.hpp"

namespace {

TEST(BTreeIndexTests, InsertFindDeleteAndRangeScan) {
  const auto path = std::filesystem::temp_directory_path() / "datacase_index.db";
  std::filesystem::remove(path);

  auto pager = std::make_shared<datacase::storage::Pager>(path);
  datacase::storage::BufferPoolManager buffer_pool(8, pager);
  datacase::index::BPlusTree tree(buffer_pool);

  for (int key = 1; key <= 200; ++key) {
    tree.Insert(key, static_cast<datacase::index::RecordId>(key * 10));
  }

  ASSERT_TRUE(tree.Find(42).has_value());
  EXPECT_EQ(*tree.Find(42), 420);

  const auto range = tree.RangeScan(50, 55);
  ASSERT_EQ(range.size(), 6);
  EXPECT_EQ(range.front(), 500);
  EXPECT_EQ(range.back(), 550);

  EXPECT_TRUE(tree.Delete(42));
  EXPECT_FALSE(tree.Find(42).has_value());

  std::filesystem::remove(path);
}

}  // namespace
