#include "check.hpp"
#include <algorithm>
#include <cstdint>
#include <optional>
#include <utility>

namespace cuecheck {
bool valid_utf8(std::string_view s) {
    for (std::size_t i = 0; i < s.size();) {
        unsigned c = static_cast<unsigned char>(s[i++]);
        if (c < 0x80) continue;
        if (c < 0xc2 || c > 0xf4) return false;
        int extra = c < 0xe0 ? 1 : c < 0xf0 ? 2 : 3;
        if (s.size() - i < static_cast<unsigned>(extra)) return false;
        unsigned first = static_cast<unsigned char>(s[i]);
        if ((c == 0xe0 && first < 0xa0) || (c == 0xed && first >= 0xa0) ||
            (c == 0xf0 && first < 0x90) || (c == 0xf4 && first >= 0x90)) return false;
        while (extra--) if ((static_cast<unsigned char>(s[i++]) & 0xc0) != 0x80) return false;
    }
    return true;
}

namespace {
using View = std::string_view;
View trim(View s) {
    const auto start = s.find_first_not_of(" \t");
    return start == View::npos ? View{} : s.substr(start, s.find_last_not_of(" \t") - start + 1);
}
bool digits(View s) {
    return !s.empty() && std::all_of(s.begin(), s.end(), [](char c) { return c >= '0' && c <= '9'; });
}
int number(View s) {
    int value = 0;
    for (char c : s) value = value * 10 + (c - '0');
    return value;
}
std::optional<int> timestamp(View s) {
    if (s.size() != 12 || s[2] != ':' || s[5] != ':' || s[8] != ',') return {};
    auto h = s.substr(0, 2), m = s.substr(3, 2), sec = s.substr(6, 2), ms = s.substr(9);
    if (!digits(h) || !digits(m) || !digits(sec) || !digits(ms) || number(m) > 59 || number(sec) > 59) return {};
    return ((number(h) * 60 + number(m)) * 60 + number(sec)) * 1000 + number(ms);
}
struct Interval { int start, end; std::size_t line; };
bool clock_token(View s) {
    return !s.empty() && s.find(':') != View::npos &&
           s.find_first_not_of("0123456789:,.") == View::npos;
}
bool looks_like_range(View s) {
    const auto arrow = s.find('>');
    if (arrow != View::npos) {
        const auto left = trim(s.substr(0, s.find_first_of("-=>")));
        return clock_token(left) || clock_token(trim(s.substr(arrow + 1)));
    }
    const auto gap = s.find_first_of(" \t");
    return gap != View::npos && clock_token(s.substr(0, gap)) &&
           clock_token(s.substr(s.find_last_of(" \t") + 1));
}
std::size_t characters(View s) {
    std::size_t count = 0, spaces = 0;
    while (!s.empty()) {
        if (s.front() == '<') {
            auto end = s.substr(0, 4).find('>');
            if (end == 2 || end == 3) {
                auto name = s.substr(1, end - 1);
                if (name.front() == '/') name.remove_prefix(1);
                if (name == "b" || name == "i" || name == "u") { s.remove_prefix(end + 1); continue; }
            }
        }
        if (s.front() == ' ' || s.front() == '\t') {
            if (count) ++spaces;
        } else if ((static_cast<unsigned char>(s.front()) & 0xc0) != 0x80) {
            count += spaces + 1;
            spaces = 0;
        }
        s.remove_prefix(1);
    }
    return count;
}
std::optional<Interval> interval(View s, std::size_t line) {
    auto arrow = s.find("-->");
    if (arrow == View::npos) return {};
    auto a = timestamp(trim(s.substr(0, arrow))), b = timestamp(trim(s.substr(arrow + 3)));
    if (!a || !b) return {};
    return Interval{*a, *b, line};
}
}

Report check(View file, Limits limits) {
    Report r;
    std::vector<Interval> times;
    auto error = [&](std::size_t line, const char* code, std::string message) {
        r.issues.push_back({line, Severity::error, code, std::move(message)});
    };
    auto warn = [&](std::size_t line, const char* code, std::string message) {
        r.issues.push_back({line, Severity::warning, code, std::move(message)});
    };
    if (file.substr(0, 3) == "\xef\xbb\xbf") file.remove_prefix(3);
    enum class State { index, timing, text } state = State::index;
    std::size_t line = 0, timing_line = 0;
    std::size_t chars = 0;
    int duration = 0;
    View pending_number;
    auto add_text = [&](View s, std::size_t n) {
        const auto length = characters(s);
        if (limits.max_line && length > static_cast<std::size_t>(limits.max_line))
            warn(n, "line-length", "Line has " + std::to_string(length) + " characters; limit is " + std::to_string(limits.max_line) + ".");
        if (length) chars += length + (chars ? 1 : 0);
    };
    auto finish = [&] {
        if (state == State::timing) error(timing_line, "missing-timing", "Add a timestamp line after the cue number.");
        if (state == State::text && !chars) error(timing_line, "empty-text", "Add subtitle text after the timestamps.");
        if (limits.max_cps && duration && std::uint64_t(chars) * 1000 > std::uint64_t(duration) * limits.max_cps)
            warn(timing_line, "reading-speed", "Shorten or extend this cue to stay within " + std::to_string(limits.max_cps) + " characters per second.");
        state = State::index;
        chars = duration = 0;
    };
    auto check_number = [&](View s, std::size_t n) {
        while (s.size() > 1 && s.front() == '0') s.remove_prefix(1);
        if (s != std::to_string(r.cues)) warn(n, "numbering", "Expected cue number " + std::to_string(r.cues) + ".");
    };
    while (!file.empty()) {
        auto eol = file.find_first_of("\r\n");
        View s = file.substr(0, eol);
        if (eol == View::npos) file = {};
        else file.remove_prefix(eol + (file.substr(eol, 2) == "\r\n" ? 2 : 1));
        ++line;
        if (!valid_utf8(s) || s.find('\0') != View::npos) {
            error(line, "encoding", "Save as UTF-8 without NUL bytes.");
            return r;
        }
        s = trim(s);
        if (!pending_number.empty()) {
            if (looks_like_range(s)) {
                finish();
                ++r.cues;
                check_number(pending_number, line - 1);
                error(line - 1, "missing-separator", "Insert a blank line before this cue number.");
                state = State::timing;
            } else add_text(pending_number, line - 1);
            pending_number = {};
        }
        if (s.empty()) { finish(); continue; }
        if (state == State::index) {
            ++r.cues;
            timing_line = line;
            state = State::timing;
            if (digits(s)) {
                check_number(s, line);
                continue;
            }
            error(line, "missing-number", "Start each cue with a numeric index.");
            if (!looks_like_range(s)) continue;
        } else if (state == State::text) {
            if (digits(s)) pending_number = s;
            else add_text(s, line);
            continue;
        }
        state = State::text;
        if (!looks_like_range(s) && !clock_token(s)) {
            error(timing_line, "missing-timing", "Add a timestamp line after the cue number.");
            add_text(s, line);
            continue;
        }
        timing_line = line;
        auto t = interval(s, line);
        if (!t) error(line, "timestamp", "Use HH:MM:SS,mmm --> HH:MM:SS,mmm.");
        else if (t->end <= t->start) error(line, "duration", "Set the end time after the start time.");
        else {
            duration = t->end - t->start;
            if (!times.empty() && t->start < times.back().start)
                warn(line, "out-of-order", "Start time precedes the previous cue's start time.");
            times.push_back(*t);
        }
    }
    if (!pending_number.empty()) add_text(pending_number, line);
    finish();
    if (!r.cues) error(1, "empty-file", "No subtitle cues found.");
    std::stable_sort(times.begin(), times.end(), [](auto a, auto b) { return a.start < b.start; });
    std::optional<Interval> furthest;
    for (auto t : times) {
        if (furthest && t.start < furthest->end)
            warn(t.line, "overlap", "Overlaps the cue timed on line " + std::to_string(furthest->line) + ".");
        if (!furthest || t.end > furthest->end) furthest = t;
    }
    std::stable_sort(r.issues.begin(), r.issues.end(), [](const auto& a, const auto& b) { return a.line < b.line; });
    return r;
}
}
