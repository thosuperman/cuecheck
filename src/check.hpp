#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace cuecheck {
enum class Severity { error, warning };

struct Issue {
    std::size_t line;
    Severity severity;
    const char* code;
    std::string message;
};

struct Report {
    std::size_t cues = 0;
    std::vector<Issue> issues;
};

bool valid_utf8(std::string_view bytes);
struct Limits { int max_cps = 0, max_line = 0; };
Report check(std::string_view file, Limits limits = {});
}
