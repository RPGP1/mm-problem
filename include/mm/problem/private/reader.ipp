#pragma once

#include "mm/problem/reader.hpp"

#include "mm/problem/definition.hpp"
#include "mm/problem/private/is_large.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <fstream>
#include <iterator>
#include <limits>
#include <random>
#include <vector>


namespace MM
{

namespace Problem
{

template <class Element>
class Reader<Element>::ImplBase
{
public:
    ImplBase() = default;
    virtual ~ImplBase() = default;

    virtual void get(
        Element* lhs, Element* rhs,
        uint32_t lhs_rows, uint32_t lhs_cols, uint32_t rhs_cols,
        size_t lhs_pitch, size_t rhs_pitch)
        = 0;
    virtual Result<Element> score(
        const Element* calced,
        uint32_t lhs_rows, uint32_t lhs_cols, uint32_t rhs_cols,
        size_t pitch,
        std::function<void(uint32_t row, uint32_t col, Element calced, Element answer)> violation_callback)
        = 0;

protected:
    static Result<Element> create_result(uint32_t lhs_cols);
    static bool score_element(Result<Element>&, Element calced, Element answer);
};

template <class Element>
class Reader<Element>::LargeImpl : public ImplBase
{
public:
    explicit LargeImpl(std::ifstream&&);
    ~LargeImpl() = default;

    void get(
        Element* lhs, Element* rhs,
        uint32_t lhs_rows, uint32_t lhs_cols, uint32_t rhs_cols,
        size_t lhs_pitch, size_t rhs_pitch) override;
    Result<Element> score(
        const Element* calced,
        uint32_t lhs_rows, uint32_t lhs_cols, uint32_t rhs_cols,
        size_t pitch,
        std::function<void(uint32_t row, uint32_t col, Element calced, Element answer)> violation_callback) override;

private:
    std::ifstream m_stream;
};

template <class Element>
class Reader<Element>::RegularImpl : public ImplBase
{
public:
    explicit RegularImpl(std::ifstream&&);
    ~RegularImpl() = default;

    void get(Element* lhs, Element* rhs,
        uint32_t lhs_rows, uint32_t lhs_cols, uint32_t rhs_cols,
        size_t lhs_pitch, size_t rhs_pitch) override;
    Result<Element> score(
        const Element* calced,
        uint32_t lhs_rows, uint32_t lhs_cols, uint32_t rhs_cols,
        size_t pitch,
        std::function<void(uint32_t row, uint32_t col, Element calced, Element answer)> violation_callback) override;

private:
    std::ifstream m_stream;
};


template <class Element>
Reader<Element>::Reader(std::filesystem::path const& path)
{
    namespace fs = std::filesystem;

    auto construct_from_file = [this](fs::path const& path) {
        std::ifstream ifs{path, std::ios_base::binary};

        ifs.read(reinterpret_cast<char*>(&m_lhs_rows), sizeof(m_lhs_rows));
        ifs.read(reinterpret_cast<char*>(&m_lhs_cols), sizeof(m_lhs_cols));
        ifs.read(reinterpret_cast<char*>(&m_rhs_cols), sizeof(m_rhs_cols));

        if (isLarge(m_lhs_rows, m_lhs_cols, m_rhs_cols)) {
            pimpl.reset(new LargeImpl{std::move(ifs)});
        } else {
            pimpl.reset(new RegularImpl{std::move(ifs)});
        }
    };

    if (fs::is_directory(path)) {
        std::vector<fs::directory_entry> entries;
        std::copy_if(fs::directory_iterator{path}, fs::directory_iterator{},
            std::back_inserter(entries),
            [](auto entry) { return entry.is_regular_file(); });

        if (entries.empty()) {
            throw EmptyDirectory{path};
        }

        // randomly choose one
        std::random_device device;
        construct_from_file(entries.at(std::uniform_int_distribution<size_t>{0, entries.size() - 1}(device)));

    } else {
        construct_from_file(path);
    }
}

template <class Element>
void Reader<Element>::get(
    Element* const lhs, Element* const rhs,
    const size_t lhs_pitch, const size_t rhs_pitch) const
{
    pimpl->get(lhs, rhs, m_lhs_rows, m_lhs_cols, m_rhs_cols, lhs_pitch, rhs_pitch);
}

template <class Element>
Result<Element> Reader<Element>::score(const Element* const calced, const size_t pitch, std::function<void(uint32_t row, uint32_t col, Element calced, Element answer)> violation_callback) const
{
    return pimpl->score(calced, m_lhs_rows, m_lhs_cols, m_rhs_cols, pitch, std::move(violation_callback));
}


template <class Element>
Result<Element> Reader<Element>::ImplBase::create_result(const uint32_t lhs_cols)
{
    Result<Element> result;

    using std::exp2;
    result.strict_standard = static_cast<Element>(lhs_cols)
                             * static_cast<Element>(exp2(ELEMENT_DIGIT_2 * 2 - std::numeric_limits<Element>::digits - result.strict_standard_digits));
    result.loose_standard = static_cast<Element>(lhs_cols)
                            * static_cast<Element>(exp2(ELEMENT_DIGIT_2 * 2 - std::numeric_limits<Element>::digits - result.loose_standard_digits));

    return result;
}
template <class Element>
bool Reader<Element>::ImplBase::score_element(Result<Element>& result, const Element calced, const Element answer)
{
    using std::abs, std::isfinite;

    const auto diff = abs(calced - answer);
    auto is_correct = true;

    if (!isfinite(diff) || diff > result.strict_standard) {
        result.strict_violations++;
        result.loose_violations++;
        is_correct = false;
    } else if (diff > result.loose_standard) {
        result.loose_violations++;
    }

    result.max_difference = std::max(diff, result.max_difference);

    return is_correct;
}


template <class Element>
Reader<Element>::LargeImpl::LargeImpl(std::ifstream&& stream)
    : m_stream{std::move(stream)}
{
}

template <class Element>
void Reader<Element>::LargeImpl::get(
    Element* const lhs, Element* const rhs,
    const uint32_t lhs_rows, const uint32_t lhs_cols, const uint32_t rhs_cols,
    size_t lhs_pitch, size_t rhs_pitch)
{
    auto const& rhs_rows = lhs_cols;

    m_stream.seekg(sizeof(uint32_t) * 3);

    uint32_t lhs_repeat_rows, lhs_repeat_cols, rhs_repeat_cols;
    uint32_t const& rhs_repeat_rows{lhs_repeat_cols};

    uint32_t lhs_pad_rows, lhs_pad_cols, rhs_pad_cols;
    uint32_t const& rhs_pad_rows{lhs_pad_cols};

    m_stream.read(reinterpret_cast<char*>(&lhs_repeat_rows), sizeof(lhs_repeat_rows));
    m_stream.read(reinterpret_cast<char*>(&rhs_repeat_cols), sizeof(rhs_repeat_cols));
    m_stream.read(reinterpret_cast<char*>(&lhs_repeat_cols), sizeof(lhs_repeat_cols));
    m_stream.ignore(sizeof(uint32_t));
    m_stream.read(reinterpret_cast<char*>(&lhs_pad_rows), sizeof(lhs_pad_rows));
    m_stream.read(reinterpret_cast<char*>(&lhs_pad_cols), sizeof(lhs_pad_cols));
    m_stream.read(reinterpret_cast<char*>(&rhs_pad_cols), sizeof(rhs_pad_cols));


    auto reader = [this](
                      Element* const dist, const uint32_t rows, const uint32_t cols, const size_t pitch,
                      const uint32_t repeat_rows, const uint32_t repeat_cols, const uint32_t pad_rows, const uint32_t pad_cols) {
        assert(rows % repeat_rows == pad_rows);
        assert(cols % repeat_cols == pad_cols);

        const auto repeat = std::make_unique<Element[]>(size_t{repeat_rows} * repeat_cols);
        const auto pad_bottom = std::make_unique<Element[]>(size_t{pad_rows} * repeat_cols);
        const auto pad_right = std::make_unique<Element[]>(size_t{repeat_rows} * pad_cols);
        const auto pad_bottom_right = std::make_unique<Element[]>(size_t{pad_rows} * pad_cols);

        m_stream.read(reinterpret_cast<char*>(repeat.get()), sizeof(Element) * size_t{repeat_rows} * repeat_cols);
        m_stream.read(reinterpret_cast<char*>(pad_bottom.get()), size_t{pad_rows} * repeat_cols);
        m_stream.read(reinterpret_cast<char*>(pad_right.get()), size_t{repeat_rows} * pad_cols);
        m_stream.read(reinterpret_cast<char*>(pad_bottom_right.get()), size_t{pad_rows} * pad_cols);

        uint32_t row{0};
        for (; row + repeat_rows <= rows; row += repeat_rows) {
            for (uint32_t row_in_repetition; row_in_repetition < repeat_rows; row_in_repetition++) {

                uint32_t col{0};
                for (; col + repeat_cols <= cols; col += repeat_cols) {

                    std::copy_n(&repeat[size_t{row_in_repetition} * repeat_cols], repeat_cols,
                        &dist[size_t{row + row_in_repetition} * pitch + col]);
                }

                std::copy_n(&pad_right[0], pad_cols,
                    &dist[size_t{row + row_in_repetition} + col]);
            }
        }
        for (uint32_t row_in_padding; row_in_padding < pad_rows; row_in_padding++) {
            uint32_t col{0};
            for (; col + repeat_cols <= cols; col += repeat_cols) {

                std::copy_n(&pad_bottom[size_t{row_in_padding} * repeat_cols], repeat_cols,
                    &dist[size_t{row + row_in_padding} * pitch + col]);
            }

            std::copy_n(&pad_right[0], pad_cols,
                &dist[size_t{row + row_in_padding} + col]);
        }
    };

    reader(lhs, lhs_rows, lhs_cols, lhs_pitch,
        lhs_repeat_rows, lhs_repeat_cols, lhs_pad_rows, lhs_pad_cols);
    reader(rhs, rhs_rows, rhs_cols, rhs_pitch,
        rhs_repeat_rows, rhs_repeat_cols, rhs_pad_rows, rhs_pad_cols);
}

template <class Element>
Result<Element> Reader<Element>::LargeImpl::score(
    const Element* const calced,
    const uint32_t lhs_rows, const uint32_t lhs_cols, const uint32_t rhs_cols,
    const size_t pitch,
    std::function<void(uint32_t row, uint32_t col, Element calced, Element answer)> violation_callback)
{
    auto const& rows = lhs_rows;
    auto const& cols = rhs_cols;

    m_stream.seekg(sizeof(uint32_t) * 3);

    uint32_t repeat_rows, repeat_cols;
    uint32_t pad_rows, pad_cols;

    m_stream.read(reinterpret_cast<char*>(&repeat_rows), sizeof(repeat_rows));
    m_stream.read(reinterpret_cast<char*>(&repeat_cols), sizeof(repeat_cols));
    m_stream.ignore(sizeof(uint32_t) * 2);
    m_stream.read(reinterpret_cast<char*>(&pad_rows), sizeof(pad_rows));
    m_stream.ignore(sizeof(uint32_t));
    m_stream.read(reinterpret_cast<char*>(&pad_cols), sizeof(pad_cols));

    assert(rows % repeat_rows == pad_rows);
    assert(cols % repeat_cols == pad_cols);


    m_stream.seekg(sizeof(Element) * lhs_rows * lhs_cols
                       + sizeof(Element) * lhs_cols * rhs_cols,
        std::ios_base::cur);


    const auto repeat = std::make_unique<Element[]>(size_t{repeat_rows} * repeat_cols);
    const auto pad_bottom = std::make_unique<Element[]>(size_t{pad_rows} * repeat_cols);
    const auto pad_right = std::make_unique<Element[]>(size_t{repeat_rows} * pad_cols);
    const auto pad_bottom_right = std::make_unique<Element[]>(size_t{pad_rows} * pad_cols);

    m_stream.read(reinterpret_cast<char*>(repeat.get()), sizeof(Element) * size_t{repeat_rows} * repeat_cols);
    m_stream.read(reinterpret_cast<char*>(pad_bottom.get()), size_t{pad_rows} * repeat_cols);
    m_stream.read(reinterpret_cast<char*>(pad_right.get()), size_t{repeat_rows} * pad_cols);
    m_stream.read(reinterpret_cast<char*>(pad_bottom_right.get()), size_t{pad_rows} * pad_cols);


    auto result = this->create_result(lhs_cols);

    uint32_t row{0};
    for (; row + repeat_rows <= rows; row += repeat_rows) {
        for (uint32_t row_in_repetition; row_in_repetition < repeat_rows; row_in_repetition++) {

            uint32_t col{0};
            for (; col + repeat_cols <= cols; col += repeat_cols) {
                for (uint32_t col_in_repetition; col_in_repetition < repeat_cols; col_in_repetition++) {

                    auto const& calced_elem = calced[(row + row_in_repetition) * pitch + (col + col_in_repetition)];
                    auto const& answer_elem = repeat[size_t{row_in_repetition} * rows + col_in_repetition];

                    if (!this->score_element(result, calced_elem, answer_elem) && violation_callback) {
                        violation_callback(row + row_in_repetition, col + col_in_repetition, calced_elem, answer_elem);
                    }
                }
            }

            for (uint32_t col_in_padding; col_in_padding < repeat_cols; col_in_padding++) {

                auto const& calced_elem = calced[(row + row_in_repetition) * pitch + (col + col_in_padding)];
                auto const& answer_elem = pad_right[size_t{row_in_repetition} * rows + col_in_padding];

                if (!this->score_element(result, calced_elem, answer_elem) && violation_callback) {
                    violation_callback(row + row_in_repetition, col + col_in_padding, calced_elem, answer_elem);
                }
            }
        }
    }
    for (uint32_t row_in_padding; row_in_padding < pad_rows; row_in_padding++) {

        uint32_t col{0};
        for (; col + repeat_cols <= cols; col += repeat_cols) {
            for (uint32_t col_in_repetition; col_in_repetition < repeat_cols; col_in_repetition++) {

                auto const& calced_elem = calced[(row + row_in_padding) * pitch + (col + col_in_repetition)];
                auto const& answer_elem = pad_bottom[size_t{row_in_padding} * rows + col_in_repetition];

                if (!this->score_element(result, calced_elem, answer_elem) && violation_callback) {
                    violation_callback(row + row_in_padding, col + col_in_repetition, calced_elem, answer_elem);
                }
            }
        }

        for (uint32_t col_in_padding; col_in_padding < repeat_cols; col_in_padding++) {

            auto const& calced_elem = calced[(row + row_in_padding) * pitch + (col + col_in_padding)];
            auto const& answer_elem = pad_right[size_t{row_in_padding} * rows + col_in_padding];

            if (!this->score_element(result, calced_elem, answer_elem) && violation_callback) {
                violation_callback(row + row_in_padding, col + col_in_padding, calced_elem, answer_elem);
            }
        }
    }

    return result;
}


template <class Element>
Reader<Element>::RegularImpl::RegularImpl(std::ifstream&& stream)
    : m_stream{std::move(stream)}
{
}

template <class Element>
void Reader<Element>::RegularImpl::get(
    Element* const lhs, Element* const rhs,
    const uint32_t lhs_rows, const uint32_t lhs_cols, const uint32_t rhs_cols,
    const size_t lhs_pitch, const size_t rhs_pitch)
{
    m_stream.seekg(sizeof(uint32_t) * 3);

    for (uint32_t row = 0; row < lhs_rows; row++) {
        m_stream.read(reinterpret_cast<char*>(&lhs[row * lhs_pitch]), sizeof(Element) * lhs_cols);
    }
    auto const& rhs_rows = lhs_cols;
    for (uint32_t row = 0; row < rhs_rows; row++) {
        m_stream.read(reinterpret_cast<char*>(&rhs[row * rhs_pitch]), sizeof(Element) * rhs_cols);
    }
}

template <class Element>
Result<Element> Reader<Element>::RegularImpl::score(
    const Element* const calced,
    const uint32_t lhs_rows, const uint32_t lhs_cols, const uint32_t rhs_cols,
    const size_t pitch,
    std::function<void(uint32_t row, uint32_t col, Element calced, Element answer)> violation_callback)
{
    auto const& rows = lhs_rows;

    m_stream.seekg(sizeof(uint32_t) * 3
                   + sizeof(Element) * lhs_rows * lhs_cols
                   + sizeof(Element) * lhs_cols * rhs_cols);

    const auto answer = std::make_unique<Element[]>(size_t{lhs_rows} * rhs_cols);
    m_stream.read(reinterpret_cast<char*>(answer.get()), sizeof(Element) * lhs_rows * rhs_cols);

    auto result = this->create_result(lhs_cols);

    for (uint32_t row = 0; row < lhs_rows; row++) {
        for (uint32_t col = 0; col < rhs_cols; col++) {
            if (!this->score_element(result, calced[row * pitch + col], answer[size_t{row} * rows + col])) {
                violation_callback(row, col, calced[row * pitch + col], answer[size_t{row} * rows + col]);
            }
        }
    }

    return result;
}

}  // namespace Problem

}  // namespace MM
