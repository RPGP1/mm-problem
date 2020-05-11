#pragma once

#include <cstddef>


namespace MM
{

namespace Problem
{

template <class Element>
struct Result final {
    // how many digits the standards are relaxed
    static int loose_standard_digits, strict_standard_digits;

    Element loose_standard, strict_standard;
    size_t loose_violations{0}, strict_violations{0};

    Element max_difference{0};
};

}  // namespace Problem

}  // namespace MM
