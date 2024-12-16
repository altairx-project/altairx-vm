#ifndef PANIC_HPP_INCLUDED
#define PANIC_HPP_INCLUDED

#include <string>
#include <stdexcept>
#include <sstream>

// throw a std::runtime_error after formatting args
template<typename... Args>
[[noreturn]] void ax_panic(Args&&... args)
{
    std::ostringstream error;
    error << "AltairXVM error:\n";
    (error << ... << args) << '\n';

    throw std::runtime_error{error.str()};
}

template<typename... Args>
void ax_check(bool b, Args&&... args)
{
    if(!b)
    {
        ax_panic(std::forward<Args>(args)...);
    }
}

#endif
