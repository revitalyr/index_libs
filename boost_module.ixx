module;

#include <cstdlib>
#include <boost/process.hpp>
#include <boost/asio.hpp>

export module boost_module;

import std;

namespace bp = boost::process;

export const std::string
get_process_output(std::string_view exe_name, std::vector<std::string_view> args) {
    boost::asio::io_context ios;
    boost::asio::streambuf buffer;
    std::future<int> exit_code;
    auto const  exe_path{ bp::search_path(exe_name) };

    if (exe_path.empty()) {
        throw std::runtime_error((std::ostringstream() << "Could not find '" << exe_name << "' executable in path.").str());
    }

    bp::child c(
        bp::exe = exe_path,
        bp::args = args,
        bp::std_out > buffer,
        bp::on_exit = exit_code,
        ios
    );

    ios.run();

    if (auto ec = exit_code.get(); ec != EXIT_SUCCESS) {
        auto joined_args{ args | std::views::join_with(' ') | std::ranges::to<std::string>() };
        LPSTR buffer = nullptr;

        auto size = FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER |
                                   FORMAT_MESSAGE_FROM_SYSTEM |
                                   FORMAT_MESSAGE_IGNORE_INSERTS,
                                   nullptr,
                                   ec,
                                   MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                                   reinterpret_cast<LPSTR>(&buffer),
                                   0,
                                   nullptr);
        std::ostringstream  msg;
        msg << "'" << exe_name << " " << joined_args << "' returns '" << std::string_view(buffer, size) << "' (" << ec << ")";
        LocalFree(buffer);

        throw std::runtime_error(msg.str());
    }

    boost::asio::streambuf::const_buffers_type bufs = buffer.data();
    std::string str(boost::asio::buffers_begin(bufs),
                    boost::asio::buffers_begin(bufs) + buffer.size());

    return str;
}
