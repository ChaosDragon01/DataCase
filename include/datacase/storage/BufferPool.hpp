#pragma once

#include <cstddef>
#include <memory>
#include <vector>

#include "datacase/Common.hpp"
#include "datacase/storage/StorageManager.hpp"

namespace datacase::storage {

struct PageHandle {
  PageId id{};
  PageBuffer* data{};
};

class IBufferPool {
 public:
  virtual ~IBufferPool() = default;

  virtual PageHandle FetchPage(PageId page_id) = 0;
  virtual PageHandle NewPage() = 0;
  virtual void UnpinPage(PageId page_id, bool dirty) = 0;
  virtual void FlushPage(PageId page_id) = 0;
  virtual void FlushAllPages() = 0;
};

class BufferPoolManager final : public IBufferPool {
 public:
  BufferPoolManager(std::size_t capacity, std::shared_ptr<IStorageManager> storage_manager);
  ~BufferPoolManager() override;

  PageHandle FetchPage(PageId page_id) override;
  PageHandle NewPage() override;
  void UnpinPage(PageId page_id, bool dirty) override;
  void FlushPage(PageId page_id) override;
  void FlushAllPages() override;

 private:
  struct Frame;

  std::size_t SelectVictimFrame();
  void EvictFrame(std::size_t frame_index);

  std::size_t capacity_;
  std::shared_ptr<IStorageManager> storage_manager_;
  std::vector<std::unique_ptr<Frame>> frames_;
};

}  // namespace datacase::storage
