module;

#include <boost/process.hpp>
#include <boost/asio.hpp>

export module boost_module;

import std;

namespace bp = boost::process;

export const std::string 
get_process_output(std::string_view exe_name, std::vector<std::string_view> args) {
    boost::asio::io_context ios;
    boost::asio::streambuf buffer;
    auto const  exe_path{ bp::search_path(exe_name) };

    if (exe_path.empty()) {
        throw std::runtime_error((std::ostringstream() << "Could not find '" << exe_name << "' executable in path.").str());
    }

    bp::child c(
        bp::exe = exe_path,
        bp::args = args,
        bp::std_out > buffer,
        ios
    );

    ios.run();

    boost::asio::streambuf::const_buffers_type bufs = buffer.data();
    std::string str(boost::asio::buffers_begin(bufs),
                    boost::asio::buffers_begin(bufs) + buffer.size());

    return str;
}
