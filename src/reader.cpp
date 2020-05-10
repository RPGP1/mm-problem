#include "reader.hpp"

namespace MM
{

namespace Problem
{

EmptyDirectory::EmptyDirectory(std::filesystem::path const& path)
    : std::runtime_error{std::string{"directory \""} + path.string() + "\" contains no file"}
{
}

}  // namespace Problem

}  // namespace MM
