//utils.h
#pragma once

import std;
import termcolor_module;

//https://stackoverflow.com/questions/37227300/why-doesnt-c-use-stdnested-exception-to-allow-throwing-from-destructor
// this function will re-throw the current exception, nested inside a
// new one. If the std::current_exception is derived from logic_error, 
// this function will throw a logic_error. Otherwise it will throw a
// runtime_error
// The message of the exception will be composed of the arguments
// context and the variadic arguments args... which may be empty.
// The current exception will be nested inside the new one
// @pre context and args... must support ostream operator <<
template<class Context, class...Args>
inline void rethrow(Context &&context, Args&&... args) {
    // build an error message
    std::ostringstream ss;
    ss << termcolor::colorize << termcolor::red << context << termcolor::reset;
    auto sep = ": ";
    using expand = int[];
    void(expand{ 0, ((ss << sep << args), sep = " ", 0)... });
    // figure out what kind of exception is active
    try {
        std::rethrow_exception(std::current_exception());
    }
    catch (const std::invalid_argument &) {
        std::throw_with_nested(std::invalid_argument(ss.str()));
    }
    catch (...) {
        std::throw_with_nested(std::runtime_error(ss.str()));
    }
}

// unwrap nested exceptions, printing each nested exception to 
// std::cerr
inline void print_exception(const std::exception &e, std::size_t depth = 0) {
    std::cerr << "exception: " << std::string(depth, ' ') << e.what() << '\n';
    try {
        std::rethrow_if_nested(e);
    }
    catch (const std::exception &nested) {
        print_exception(nested, depth + 1);
    }
}

