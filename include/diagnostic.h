#pragma once

#include <algorithm>
#include <iostream>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

struct SourceLoc {
    std::string file{"<stdin>"};
    size_t line{1};
    size_t col{1};
    size_t length{1};

    bool is_valid() const { return line > 0 && col > 0; }
    std::string to_string() const;
};

enum class DiagnosticLevel {
    Error,
    Warning,
    Note,
    Help
};

struct Diagnostic {
    DiagnosticLevel level{DiagnosticLevel::Error};
    SourceLoc loc;
    std::string message;
    std::vector<std::pair<SourceLoc, std::string>> labels;
    std::vector<std::string> notes;
    std::vector<std::string> suggestions;
};

class SourceManager {
public:
    void add_source(std::string name, std::string content);
    const std::string *get_source(const std::string &name) const;
    std::string_view get_line(const std::string &name, size_t line_num) const;

private:
    std::unordered_map<std::string, std::string> sources_;
    mutable std::unordered_map<std::string, std::vector<std::string_view>> lines_cache_;
};

class DiagnosticEngine {
public:
    explicit DiagnosticEngine(SourceManager &source_mgr, std::ostream &out = std::cerr);

    void set_color_enabled(bool enabled) { color_enabled_ = enabled; }
    bool is_color_enabled() const { return color_enabled_; }

    void report(const Diagnostic &diag);

    void error(const SourceLoc &loc, const std::string &msg,
               const std::vector<std::string> &notes = {},
               const std::vector<std::string> &suggestions = {});

    void warning(const SourceLoc &loc, const std::string &msg,
                 const std::vector<std::string> &notes = {},
                 const std::vector<std::string> &suggestions = {});

    void note(const SourceLoc &loc, const std::string &msg);
    void help(const SourceLoc &loc, const std::string &msg);

    size_t error_count() const { return error_count_; }
    size_t warning_count() const { return warning_count_; }
    bool has_errors() const { return error_count_ > 0; }
    void reset() {
        error_count_ = 0;
        warning_count_ = 0;
    }

    std::string format_diagnostic(const Diagnostic &diag) const;

    static std::string find_similar(std::string_view target, const std::vector<std::string> &candidates, size_t max_dist = 2);

private:
    std::string color_code(const char *code) const;

    SourceManager &source_mgr_;
    std::ostream &out_;
    bool color_enabled_{true};
    size_t error_count_{0};
    size_t warning_count_{0};
};

class CompileError : public std::exception {
public:
    explicit CompileError(std::string formatted_message)
        : msg_(std::move(formatted_message)) {}

    const char *what() const noexcept override { return msg_.c_str(); }

private:
    std::string msg_;
};
