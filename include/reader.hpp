#pragma once

#include "result.hpp"

#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <stdexcept>


namespace MM
{

namespace Problem
{

struct EmptyDirectory : public std::runtime_error
{
    explicit EmptyDirectory(std::string const&);
};

template <class Element>
class Reader final
{
    uint32_t m_lhs_rows, m_lhs_cols, m_rhs_cols;
    uint32_t& m_rhs_rows = m_lhs_cols;

public:
    using ElementType = Element;

    explicit Reader(std::filesystem::path const&);
    ~Reader() = default;

    void get(
        Element* lhs, Element* rhs,
        size_t lhs_pitch, size_t rhs_pitch);
    Result<Element> score(
        const Element* calced, size_t pitch,
        std::function<void(uint32_t row, uint32_t col, Element calced, Element answer)> violation_callback);

    uint32_t lhs_rows() { return m_lhs_rows; }
    uint32_t lhs_cols() { return m_lhs_cols; }
    uint32_t rhs_rows() { return m_rhs_rows; }
    uint32_t rhs_cols() { return m_rhs_cols; }

private:
    class ImplBase;
    class RegularImpl;
    class LargeImpl;

    std::unique_ptr<ImplBase> pimpl;
};

}  // namespace Problem

}  // namespace MM

#include "reader.ipp"
