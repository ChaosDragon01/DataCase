#include <filesystem>
#include <memory>

#include <gtest/gtest.h>

#include "datacase/storage/BufferPool.hpp"

namespace {

std::filesystem::path TempDbPath(const std::string& suffix) {
  return std::filesystem::temp_directory_path() / ("datacase_" + suffix + ".db");
}

TEST(StorageBufferTests, PagerPersistsPageBytes) {
  const auto path = TempDbPath("pager");
  std::filesystem::remove(path);

  auto pager = std::make_shared<datacase::storage::Pager>(path);
  const auto page_id = pager->AllocatePage();

  datacase::PageBuffer page{};
  page.fill(datacase::Byte{0x3A});
  pager->WritePage(page_id, datacase::ConstPageView(page));

  datacase::PageBuffer read_back{};
  pager->ReadPage(page_id, datacase::MutablePageView(read_back));
  EXPECT_EQ(read_back[10], datacase::Byte{0x3A});

  std::filesystem::remove(path);
}

TEST(StorageBufferTests, BufferPoolEvictsUsingLru) {
  const auto path = TempDbPath("buffer");
  std::filesystem::remove(path);

  auto pager = std::make_shared<datacase::storage::Pager>(path);
  datacase::storage::BufferPoolManager buffer_pool(2, pager);

  auto first = buffer_pool.NewPage();
  first.data->at(0) = datacase::Byte{0x0A};
  buffer_pool.UnpinPage(first.id, true);

  auto second = buffer_pool.NewPage();
  second.data->at(0) = datacase::Byte{0x0B};
  buffer_pool.UnpinPage(second.id, true);

  // Touch second so first becomes least recently used.
  auto second_reload = buffer_pool.FetchPage(second.id);
  buffer_pool.UnpinPage(second_reload.id, false);

  auto third = buffer_pool.NewPage();
  third.data->at(0) = datacase::Byte{0x0C};
  buffer_pool.UnpinPage(third.id, true);

  auto first_reload = buffer_pool.FetchPage(first.id);
  EXPECT_EQ(first_reload.data->at(0), datacase::Byte{0x0A});
  buffer_pool.UnpinPage(first_reload.id, false);

  std::filesystem::remove(path);
}

}  // namespace
