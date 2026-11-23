#include "check.hpp"
#include <charconv>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace fs = std::filesystem;
using cuecheck::Severity;

namespace {
constexpr auto usage =
    "CueCheck - offline SubRip subtitle preflight\n"
    "Usage: cuecheck [--strict] [--json] [--max-cps N] [--max-line N] [--] FILE.srt ...\n"
    "       cuecheck --help | --version\n"
    "Exit 0: clean or warnings only; 1: errors (or warnings in strict mode);\n"
    "     2: usage or file access error. Files are never changed.\n";

std::string quoted(const std::string& value) {
    const char* hex = "0123456789abcdef";
    bool utf8 = cuecheck::valid_utf8(value);
    std::string out = "\"";
    for (unsigned char c : value) {
        if (c == '"' || c == '\\') { out += '\\'; out += static_cast<char>(c); }
        else if (c < 0x20 || c == 0x7f || (!utf8 && c >= 0x80)) {
            out += "\\u00"; out += hex[c >> 4]; out += hex[c & 15];
        } else out += static_cast<char>(c);
    }
    return out + '"';
}

std::string load(const fs::path& path, std::string& data) {
    constexpr std::size_t max_size = 4 * 1024 * 1024;
    std::error_code ec;
    if (!fs::is_regular_file(path, ec)) return "Not a readable regular file.";
    auto size = fs::file_size(path, ec);
    if (ec) return "Cannot determine the file size.";
    if (size > max_size) return "Input exceeds the 4 MiB limit.";
    std::ifstream in(path, std::ios::binary);
    if (!in) return "Cannot open the file.";
    data.resize(static_cast<std::size_t>(size));
    if (!in.read(data.data(), static_cast<std::streamsize>(size)) ||
        in.peek() != std::char_traits<char>::eof() || in.bad()) return "File changed or could not be read.";
    return {};
}

int run(const std::vector<fs::path>& args) {
    if (args.size() == 1 && args.front() == "--help") { std::cout << usage; return 0; }
    if (args.size() == 1 && args.front() == "--version") { std::cout << "CueCheck " CUECHECK_VERSION "\n"; return 0; }
    bool json = false, strict = false, options = true;
    cuecheck::Limits limits;
    std::vector<fs::path> files;
    for (std::size_t i = 0; i < args.size(); ++i) {
        const auto& arg = args[i];
        if (options && arg == "--") options = false;
        else if (options && arg == "--json") json = true;
        else if (options && arg == "--strict") strict = true;
        else if (options && (arg == "--max-cps" || arg == "--max-line")) {
            auto value = ++i < args.size() ? args[i].u8string() : "";
            int n = 0;
            const auto end = value.data() + value.size();
            auto parsed = std::from_chars(value.data(), end, n);
            if (parsed.ec != std::errc{} || parsed.ptr != end || n < 1) {
                std::cerr << "Supply a positive integer after " << arg.u8string() << ".\n"; return 2;
            }
            (arg == "--max-cps" ? limits.max_cps : limits.max_line) = n;
        }
        else if (options && !arg.empty() && arg.native()[0] == '-') {
            std::cerr << "Unknown option. Use -- before filenames beginning with '-'.\n" << usage; return 2;
        } else files.push_back(arg);
    }
    if (files.empty()) { std::cerr << usage; return 2; }
    int status = 0;
    if (json) std::cout << '[';
    for (std::size_t i = 0; i < files.size(); ++i) {
        const auto path = quoted(files[i].u8string());
        std::string data;
        const auto failure = load(files[i], data);
        auto report = failure.empty() ? cuecheck::check(data, limits) : cuecheck::Report{};
        if (!failure.empty()) {
            report.issues.push_back({0, Severity::error, "input", failure});
            status = 2;
        }
        if (json) std::cout << (i ? "," : "") << "{\"file\":" << path << ",\"cues\":" << report.cues << ",\"issues\":[";
        for (std::size_t j = 0; j < report.issues.size(); ++j) {
            const auto& issue = report.issues[j];
            bool error = issue.severity == Severity::error;
            const char* severity = error ? "error" : "warning";
            if (status == 0 && (error || strict)) status = 1;
            if (json) {
                std::cout << (j ? "," : "") << "{\"line\":" << issue.line << ",\"severity\":\"" << severity
                          << "\",\"code\":\"" << issue.code << "\",\"message\":" << quoted(issue.message) << '}';
            } else {
                std::cout << path;
                if (issue.line) std::cout << ':' << issue.line;
                std::cout << ": " << severity << " [" << issue.code << "] " << issue.message << '\n';
            }
        }
        if (json) std::cout << "]}";
        else std::cout << path << ": " << report.cues << " cue(s), " << report.issues.size() << " issue(s)\n";
    }
    if (json) std::cout << "]\n";
    return status;
}
}

#ifdef _WIN32
int wmain(int argc, wchar_t** argv) {
#else
int main(int argc, char** argv) {
#endif
    return run({argv + 1, argv + argc});
}
