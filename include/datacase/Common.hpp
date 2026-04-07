#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace datacase {

inline constexpr std::size_t PAGE_SIZE = 4096;
inline constexpr std::size_t MAX_PAGES = 100;
using PageId = std::uint32_t;
using Byte = std::byte;
using PageBuffer = std::array<Byte, PAGE_SIZE>;
using MutablePageView = std::span<Byte, PAGE_SIZE>;
using ConstPageView = std::span<const Byte, PAGE_SIZE>;

}  // namespace datacase
