#include "datacase/storage/BufferPool.hpp"

#include <algorithm>
#include <limits>
#include <list>
#include <stdexcept>
#include <unordered_map>

namespace datacase::storage {

struct BufferPoolManager::Frame {
  PageBuffer buffer{};
  PageId page_id = 0;
  bool occupied = false;
  bool dirty = false;
  std::size_t pin_count = 0;
  std::uint64_t last_used = 0;
};

namespace {
struct RuntimeState {
  std::unordered_map<PageId, std::size_t> page_table;
  std::uint64_t tick = 1;
};

std::unordered_map<BufferPoolManager*, RuntimeState>& StateMap() {
  static std::unordered_map<BufferPoolManager*, RuntimeState> states;
  return states;
}

RuntimeState& State(BufferPoolManager* manager) {
  return StateMap()[manager];
}

void EraseState(BufferPoolManager* manager) {
  StateMap().erase(manager);
}
}  // namespace

BufferPoolManager::BufferPoolManager(std::size_t capacity, std::shared_ptr<IStorageManager> storage_manager)
    : capacity_(capacity), storage_manager_(std::move(storage_manager)) {
  if (capacity_ == 0) {
    throw std::invalid_argument("buffer pool capacity must be greater than zero");
  }
  if (storage_manager_ == nullptr) {
    throw std::invalid_argument("storage manager must not be null");
  }

  frames_.reserve(capacity_);
  for (std::size_t i = 0; i < capacity_; ++i) {
    frames_.push_back(std::make_unique<Frame>());
  }
}

BufferPoolManager::~BufferPoolManager() {
  FlushAllPages();
  EraseState(this);
}

PageHandle BufferPoolManager::FetchPage(PageId page_id) {
  auto& state = State(this);
  if (const auto iter = state.page_table.find(page_id); iter != state.page_table.end()) {
    auto& frame = *frames_[iter->second];
    ++frame.pin_count;
    frame.last_used = state.tick++;
    return PageHandle{.id = page_id, .data = &frame.buffer};
  }

  const std::size_t frame_index = SelectVictimFrame();
  EvictFrame(frame_index);

  auto& frame = *frames_[frame_index];
  storage_manager_->ReadPage(page_id, MutablePageView(frame.buffer));
  frame.page_id = page_id;
  frame.occupied = true;
  frame.dirty = false;
  frame.pin_count = 1;
  frame.last_used = state.tick++;
  state.page_table[page_id] = frame_index;

  return PageHandle{.id = page_id, .data = &frame.buffer};
}

PageHandle BufferPoolManager::NewPage() {
  const PageId page_id = storage_manager_->AllocatePage();
  auto handle = FetchPage(page_id);
  handle.data->fill(Byte{0});
  UnpinPage(page_id, true);
  return FetchPage(page_id);
}

void BufferPoolManager::UnpinPage(PageId page_id, bool dirty) {
  auto& state = State(this);
  const auto iter = state.page_table.find(page_id);
  if (iter == state.page_table.end()) {
    throw std::runtime_error("cannot unpin unknown page");
  }

  auto& frame = *frames_[iter->second];
  if (frame.pin_count == 0) {
    throw std::runtime_error("cannot unpin page with zero pin count");
  }

  --frame.pin_count;
  frame.dirty = frame.dirty || dirty;
  frame.last_used = state.tick++;
}

void BufferPoolManager::FlushPage(PageId page_id) {
  auto& state = State(this);
  const auto iter = state.page_table.find(page_id);
  if (iter == state.page_table.end()) {
    return;
  }

  auto& frame = *frames_[iter->second];
  if (frame.occupied && frame.dirty) {
    storage_manager_->WritePage(page_id, ConstPageView(frame.buffer));
    frame.dirty = false;
  }
}

void BufferPoolManager::FlushAllPages() {
  auto& state = State(this);
  for (const auto& [page_id, _] : state.page_table) {
    (void)_;
    FlushPage(page_id);
  }
}

std::size_t BufferPoolManager::SelectVictimFrame() {
  std::size_t free_index = capacity_;
  std::size_t victim_index = capacity_;
  std::uint64_t oldest_tick = std::numeric_limits<std::uint64_t>::max();

  for (std::size_t i = 0; i < frames_.size(); ++i) {
    const auto& frame = *frames_[i];
    if (!frame.occupied) {
      free_index = i;
      break;
    }

    if (frame.pin_count == 0 && frame.last_used < oldest_tick) {
      oldest_tick = frame.last_used;
      victim_index = i;
    }
  }

  if (free_index != capacity_) {
    return free_index;
  }
  if (victim_index != capacity_) {
    return victim_index;
  }

  throw std::runtime_error("buffer pool has no evictable frame");
}

void BufferPoolManager::EvictFrame(std::size_t frame_index) {
  auto& state = State(this);
  auto& frame = *frames_[frame_index];

  if (!frame.occupied) {
    return;
  }

  if (frame.dirty) {
    storage_manager_->WritePage(frame.page_id, ConstPageView(frame.buffer));
  }

  state.page_table.erase(frame.page_id);
  frame = Frame{};
}

}  // namespace datacase::storage
