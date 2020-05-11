#include "mm/problem/private/is_large.hpp"

#include <cstddef>

namespace MM
{

namespace Problem
{

bool isLarge(uint32_t lhs_rows, uint32_t lhs_cols, uint32_t rhs_cols)
{
    return static_cast<size_t>(lhs_rows) * static_cast<size_t>(lhs_cols)
               + static_cast<size_t>(lhs_cols) * static_cast<size_t>(rhs_cols)
               + static_cast<size_t>(lhs_rows) * static_cast<size_t>(rhs_cols)
           >= 3 * (size_t{1} << 22);
}

}  // namespace Problem

}  // namespace MM
