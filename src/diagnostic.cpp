#include "diagnostic.h"
#include <iomanip>
#include <sstream>
#include <unistd.h>

std::string SourceLoc::to_string() const {
    if (file.empty() || file == "<unknown>") {
        return std::to_string(line) + ":" + std::to_string(col);
    }
    return file + ":" + std::to_string(line) + ":" + std::to_string(col);
}

void SourceManager::add_source(std::string name, std::string content) {
    sources_[name] = std::move(content);
    lines_cache_.erase(name);
}

const std::string *SourceManager::get_source(const std::string &name) const {
    auto it = sources_.find(name);
    if (it != sources_.end()) {
        return &it->second;
    }
    return nullptr;
}

std::string_view SourceManager::get_line(const std::string &name, size_t line_num) const {
    if (line_num == 0) {
        return "";
    }
    auto it = sources_.find(name);
    if (it == sources_.end()) {
        return "";
    }

    auto cache_it = lines_cache_.find(name);
    if (cache_it == lines_cache_.end()) {
        std::vector<std::string_view> lines;
        std::string_view src = it->second;
        size_t start = 0;
        while (start < src.size()) {
            size_t end = src.find('\n', start);
            if (end == std::string_view::npos) {
                end = src.size();
            }
            std::string_view line_str = src.substr(start, end - start);
            if (!line_str.empty() && line_str.back() == '\r') {
                line_str.remove_suffix(1);
            }
            lines.push_back(line_str);
            start = end + 1;
        }
        cache_it = lines_cache_.emplace(name, std::move(lines)).first;
    }

    const auto &lines = cache_it->second;
    if (line_num <= lines.size()) {
        return lines[line_num - 1];
    }
    return "";
}

DiagnosticEngine::DiagnosticEngine(SourceManager &source_mgr, std::ostream &out)
    : source_mgr_(source_mgr), out_(out) {
    bool is_terminal = isatty(STDERR_FILENO);
    const char *no_color = getenv("NO_COLOR");
    const char *term = getenv("TERM");
    bool term_dumb = (term && std::string_view(term) == "dumb");
    color_enabled_ = is_terminal && (!no_color || no_color[0] == '\0') && !term_dumb;
}

std::string DiagnosticEngine::color_code(const char *code) const {
    return color_enabled_ ? code : "";
}

std::string DiagnosticEngine::format_diagnostic(const Diagnostic &diag) const {
    std::ostringstream ss;
    const std::string bold = color_code("\033[1m");
    const std::string reset = color_code("\033[0m");
    const std::string cyan = color_code("\033[1;36m");
    const std::string red = color_code("\033[1;31m");
    const std::string yellow = color_code("\033[1;33m");
    const std::string green = color_code("\033[1;32m");

    std::string level_str;
    std::string level_color;
    switch (diag.level) {
    case DiagnosticLevel::Error:
        level_str = "error";
        level_color = red;
        break;
    case DiagnosticLevel::Warning:
        level_str = "warning";
        level_color = yellow;
        break;
    case DiagnosticLevel::Note:
        level_str = "note";
        level_color = cyan;
        break;
    case DiagnosticLevel::Help:
        level_str = "help";
        level_color = green;
        break;
    }

    ss << level_color << level_str << reset << ": " << bold << diag.message << reset << "\n";

    if (diag.loc.is_valid()) {
        std::string line_num_str = std::to_string(diag.loc.line);
        size_t gutter_width = std::max<size_t>(2, line_num_str.size());
        std::string gutter_pad(gutter_width, ' ');

        ss << cyan << gutter_pad << "--> " << reset << diag.loc.to_string() << "\n";

        std::string_view source_line = source_mgr_.get_line(diag.loc.file, diag.loc.line);
        if (!source_line.empty() || diag.loc.line > 0) {
            ss << cyan << gutter_pad << " |" << reset << "\n";
            ss << cyan << std::setw(static_cast<int>(gutter_width)) << line_num_str << " | " << reset;

            size_t visual_col = 1;
            std::string rendered_line;
            for (size_t i = 0; i < source_line.size(); ++i) {
                char ch = source_line[i];
                if (ch == '\t') {
                    size_t spaces = 4 - ((visual_col - 1) % 4);
                    rendered_line.append(spaces, ' ');
                    visual_col += spaces;
                } else {
                    rendered_line.push_back(ch);
                    visual_col += 1;
                }
            }
            ss << rendered_line << "\n";

            ss << cyan << gutter_pad << " | " << reset;

            size_t underline_offset = (diag.loc.col > 0) ? (diag.loc.col - 1) : 0;
            size_t adjusted_offset = 0;
            for (size_t i = 0; i < source_line.size() && i < underline_offset; ++i) {
                if (source_line[i] == '\t') {
                    adjusted_offset += 4 - (adjusted_offset % 4);
                } else {
                    adjusted_offset += 1;
                }
            }

            ss << std::string(adjusted_offset, ' ');
            ss << level_color << "^";
            size_t underline_len = (diag.loc.length > 1) ? (diag.loc.length - 1) : 0;
            ss << std::string(underline_len, '~') << reset << "\n";
        }
    }

    for (const auto &note_msg : diag.notes) {
        ss << cyan << "note: " << reset << note_msg << "\n";
    }

    for (const auto &suggestion : diag.suggestions) {
        ss << green << "help: " << reset << suggestion << "\n";
    }

    return ss.str();
}

void DiagnosticEngine::report(const Diagnostic &diag) {
    if (diag.level == DiagnosticLevel::Error) {
        error_count_++;
    } else if (diag.level == DiagnosticLevel::Warning) {
        warning_count_++;
    }

    out_ << format_diagnostic(diag);
}

void DiagnosticEngine::error(const SourceLoc &loc, const std::string &msg,
                             const std::vector<std::string> &notes,
                             const std::vector<std::string> &suggestions) {
    report(Diagnostic{DiagnosticLevel::Error, loc, msg, {}, notes, suggestions});
}

void DiagnosticEngine::warning(const SourceLoc &loc, const std::string &msg,
                               const std::vector<std::string> &notes,
                               const std::vector<std::string> &suggestions) {
    report(Diagnostic{DiagnosticLevel::Warning, loc, msg, {}, notes, suggestions});
}

void DiagnosticEngine::note(const SourceLoc &loc, const std::string &msg) {
    report(Diagnostic{DiagnosticLevel::Note, loc, msg, {}, {}, {}});
}

void DiagnosticEngine::help(const SourceLoc &loc, const std::string &msg) {
    report(Diagnostic{DiagnosticLevel::Help, loc, msg, {}, {}, {}});
}

static size_t levenshtein(std::string_view a, std::string_view b) {
    const size_t m = a.size();
    const size_t n = b.size();
    std::vector<size_t> dp(n + 1);

    for (size_t j = 0; j <= n; ++j) {
        dp[j] = j;
    }

    for (size_t i = 1; i <= m; ++i) {
        size_t prev = dp[0];
        dp[0] = i;
        for (size_t j = 1; j <= n; ++j) {
            size_t temp = dp[j];
            if (a[i - 1] == b[j - 1]) {
                dp[j] = prev;
            } else {
                dp[j] = 1 + std::min({dp[j], dp[j - 1], prev});
            }
            prev = temp;
        }
    }
    return dp[n];
}

std::string DiagnosticEngine::find_similar(std::string_view target,
                                           const std::vector<std::string> &candidates,
                                           size_t max_dist) {
    std::string best_match;
    size_t min_dist = max_dist + 1;

    for (const auto &candidate : candidates) {
        size_t dist = levenshtein(target, candidate);
        if (dist <= max_dist && dist < min_dist) {
            min_dist = dist;
            best_match = candidate;
        }
    }
    return best_match;
}
