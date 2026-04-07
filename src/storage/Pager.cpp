#include "datacase/storage/StorageManager.hpp"

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cstring>
#include <cerrno>
#include <stdexcept>
#include <algorithm>

namespace datacase::storage {

namespace {

[[noreturn]] void ThrowSystemError(const std::string& message) {
  throw std::runtime_error(message + ": " + std::strerror(errno));
}

}  // namespace

Pager::Pager(std::filesystem::path path) : file_path_(std::move(path)) {
  fd_ = ::open(file_path_.c_str(), O_RDWR | O_CREAT, 0644);
  if (fd_ < 0) {
    ThrowSystemError("failed to open database file");
  }

  struct stat file_stats {};
  if (::fstat(fd_, &file_stats) != 0) {
    ThrowSystemError("failed to stat database file");
  }

  page_count_ = static_cast<PageId>(file_stats.st_size / static_cast<off_t>(PAGE_SIZE));
}

Pager::~Pager() {
  if (fd_ >= 0) {
    ::close(fd_);
  }
}

void Pager::ReadPage(PageId page_id, MutablePageView destination) {
  if (page_id >= page_count_) {
    std::fill(destination.begin(), destination.end(), Byte{0});
    return;
  }

  const off_t offset = static_cast<off_t>(page_id) * static_cast<off_t>(PAGE_SIZE);
  const ssize_t bytes_read = ::pread(fd_, destination.data(), PAGE_SIZE, offset);
  if (bytes_read < 0) {
    ThrowSystemError("failed to read page");
  }

  if (bytes_read < static_cast<ssize_t>(PAGE_SIZE)) {
    std::memset(destination.data() + bytes_read, 0, PAGE_SIZE - bytes_read);
  }
}

void Pager::WritePage(PageId page_id, ConstPageView source) {
  const off_t offset = static_cast<off_t>(page_id) * static_cast<off_t>(PAGE_SIZE);
  const ssize_t bytes_written = ::pwrite(fd_, source.data(), PAGE_SIZE, offset);
  if (bytes_written != static_cast<ssize_t>(PAGE_SIZE)) {
    ThrowSystemError("failed to write page");
  }

  if (::fsync(fd_) != 0) {
    ThrowSystemError("failed to sync page write");
  }

  if (page_id >= page_count_) {
    page_count_ = page_id + 1;
  }
}

PageId Pager::AllocatePage() {
  PageBuffer empty_page{};
  empty_page.fill(Byte{0});
  WritePage(page_count_, ConstPageView(empty_page));
  return page_count_ - 1;
}

PageId Pager::PageCount() const { return page_count_; }

}  // namespace datacase::storage
