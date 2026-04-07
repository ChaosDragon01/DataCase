#pragma once

#include <filesystem>

#include "datacase/Common.hpp"

namespace datacase::storage {

class IStorageManager {
 public:
  virtual ~IStorageManager() = default;

  virtual void ReadPage(PageId page_id, MutablePageView destination) = 0;
  virtual void WritePage(PageId page_id, ConstPageView source) = 0;
  virtual PageId AllocatePage() = 0;
  virtual PageId PageCount() const = 0;
};

class Pager final : public IStorageManager {
 public:
  explicit Pager(std::filesystem::path path);
  ~Pager() override;

  void ReadPage(PageId page_id, MutablePageView destination) override;
  void WritePage(PageId page_id, ConstPageView source) override;
  PageId AllocatePage() override;
  PageId PageCount() const override;

 private:
  std::filesystem::path file_path_;
  int fd_ = -1;
  PageId page_count_ = 0;
};

}  // namespace datacase::storage
