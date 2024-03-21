// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT license.

#pragma once

// STL
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include "sm4.h"

// GSL
#include "gsl/span"

// SEAL
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4804)
#endif
#include "seal/util/blake2.h"
#ifdef _MSC_VER
#pragma warning(pop)
#endif

namespace apsi {
    namespace sender {
        namespace util {
            class HashFunc {
            public:
                HashFunc() = default;
                SM4 sm4;
                HashFunc(std::uint64_t seed)
                {   uint64_t key;
                    
                    key =seed;
                    sm4.set_key(0,key);
                }

                inline std::uint64_t operator()(uint64_t data) noexcept
                {
                    auto ciphertext = sm4.encrypt(0, data);
	                return (ciphertext.second ^ data) & ((1ull << 43) - 1);
                }

                inline std::uint64_t operator()(gsl::span<const std::uint64_t> data) noexcept
                {
                    std::uint64_t result = 0;
                    for (std::size_t i = 0; i < data.size(); i++) {
                        result ^= operator()(data[i]);
                    }
                    return result;
                }

            private:

                static constexpr std::size_t block_size_ = 1;

                static constexpr std::size_t block_count_ = sizeof(std::uint64_t);

                static constexpr std::size_t block_value_count_ =
                    (std::size_t(1) << (8 * block_size_));

                static constexpr std::size_t random_array_size_ = block_value_count_ * block_count_;

                static constexpr std::uint32_t block_mask_ =
                    static_cast<std::uint32_t>(block_value_count_ - 1);

                std::array<std::uint64_t, random_array_size_> random_array_;
            };
        } // namespace util
    }     // namespace sender
} // namespace apsi
