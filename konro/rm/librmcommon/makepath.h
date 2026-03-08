#ifndef MAKEPATH_H
#define MAKEPATH_H

#include <string>

namespace rmcommon {

/*!
 * A very simple template function to create a path from
 * two or more subpaths.
 *
 * Note that a path separator character is always added
 * between the two parts.
 *
 * \returns the generated path
 */
template<typename T>
std::string make_path(T first, T second)
{
    const std::string &path1 = first;
    return path1 + '/' + second;
}

template<typename T1, typename T2>
std::string make_path(T1 first, T2 second)
{
    const std::string &path1 = first;
    return path1 + '/' + second;
}

template<typename T, typename... Args>
std::string make_path(T first, T second, Args... args)
{
    const std::string &path1 = first;
    return path1 + '/' + make_path(second, args...);
}

}   // namespace rmcommon

#endif // MAKEPATH_H
