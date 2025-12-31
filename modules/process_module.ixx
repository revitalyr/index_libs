module;

#include <cstdio>

export module process_module;

import std;

export const std::string
get_process_output(std::string_view exe_name, std::vector<std::string_view> args) {
    std::ostringstream cmd;
    cmd << '"' << exe_name << '"';
    for (auto const arg : args) {
        cmd << ' ' << arg;
    }

    auto pipe = _popen(cmd.str().c_str(), "r");
    if (!pipe) {
        throw std::runtime_error(std::format("Could not start '{}'", exe_name));
    }

    std::string output;
    char        buffer[4096];
    while (fgets(buffer, static_cast<int>(std::size(buffer)), pipe)) {
        output.append(buffer);
    }

    auto const rc = _pclose(pipe);
    if (rc != 0) {
        throw std::runtime_error(std::format("'{}' exited with code {}", exe_name, rc));
    }

    return output;
}
