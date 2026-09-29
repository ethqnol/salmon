#include "codegen/llvm_codegen.h"
#include "diagnostic.h"
#include "lexer.h"
#include "parser.h"
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <unistd.h>

enum class OutputMode {
    Binary,
    Assembly,
    LLVM,
    Run,
    Pretty,
    Tree,
    Asdl
};

static std::string get_stem(const std::string &path) {
    size_t last_slash = path.find_last_of("/\\");
    std::string filename = (last_slash == std::string::npos) ? path : path.substr(last_slash + 1);
    size_t last_dot = filename.find_last_of('.');
    if (last_dot != std::string::npos) {
        return filename.substr(0, last_dot);
    }
    return filename;
}

static bool run_compiler(const std::string &src_name,
                         const std::string &src_code,
                         bool show_tokens,
                         OutputMode mode,
                         const std::string &output_file) {
    SourceManager source_mgr;
    source_mgr.add_source(src_name, src_code);
    DiagnosticEngine diag(source_mgr, std::cerr);

    Lexer lexer(src_code);
    std::vector<Token> tokens = lexer.tokenize();

    for (const auto &token : tokens) {
        if (token.type == TokenType::Invalid) {
            SourceLoc loc{src_name, token.line, token.col, token.lexeme.size()};
            diag.error(loc, "Lexer error: " + token.lexeme);
        }
    }
    if (diag.has_errors()) {
        return false;
    }

    if (show_tokens) {
        for (const auto &token : tokens) {
            std::cout << token.to_string() << "\n";
        }
        std::cout << "\n";
    }

    std::unique_ptr<Program> program;
    try {
        Parser parser(std::move(tokens), src_name);
        program = parser.parse_program();
    } catch (const ParseError &e) {
        diag.error(e.loc(), e.what());
        return false;
    }

    try {
        switch (mode) {
        case OutputMode::Pretty:
            program->pretty_print(std::cout);
            std::cout << "\n";
            break;
        case OutputMode::Tree:
            program->print_tree(std::cout);
            std::cout << "\n";
            break;
        case OutputMode::Asdl:
            program->emit_asdl(std::cout);
            std::cout << "\n";
            break;
        case OutputMode::LLVM: {
            std::string stem = get_stem(src_name);
            std::string target_ll = output_file.empty() ? (stem + ".ll") : output_file;
            if (target_ll == "-") {
                LLVMCodegen codegen(std::cout, &diag);
                codegen.generate(*program);
            } else {
                std::ofstream ll_out(target_ll);
                if (!ll_out.is_open()) {
                    std::cerr << "Error: Could not open output file '" << target_ll << "'\n";
                    return false;
                }
                LLVMCodegen codegen(ll_out, &diag);
                codegen.generate(*program);
            }
            break;
        }
        case OutputMode::Assembly: {
            std::stringstream ir_stream;
            LLVMCodegen codegen(ir_stream, &diag);
            codegen.generate(*program);
            if (diag.has_errors()) {
                return false;
            }

            std::string stem = get_stem(src_name);
            std::string target_asm = output_file.empty() ? (stem + ".s") : output_file;

            std::string tmp_ll = "/tmp/salmon_tmp_" + std::to_string(getpid()) + ".ll";
            std::ofstream ll_file(tmp_ll);
            ll_file << ir_stream.str();
            ll_file.close();

            std::string cmd = "clang -S " + tmp_ll + " -o " + target_asm;
            int ret = std::system(cmd.c_str());
            std::remove(tmp_ll.c_str());

            if (ret != 0) {
                std::cerr << "Assembly generation failed: clang exited with status " << ret << "\n";
                return false;
            }
            break;
        }
        case OutputMode::Binary:
        case OutputMode::Run: {
            std::stringstream ir_stream;
            LLVMCodegen codegen(ir_stream, &diag);
            codegen.generate(*program);
            if (diag.has_errors()) {
                return false;
            }

            std::string stem = get_stem(src_name);
            std::string target_bin = output_file.empty() ? stem : output_file;
            if (mode == OutputMode::Run && output_file.empty()) {
                target_bin = "/tmp/salmon_run_" + std::to_string(getpid());
            }

            std::string tmp_ll = "/tmp/salmon_tmp_" + std::to_string(getpid()) + ".ll";
            std::ofstream ll_file(tmp_ll);
            ll_file << ir_stream.str();
            ll_file.close();

            std::string cmd = "clang " + tmp_ll + " -o " + target_bin + " -lm";
            int ret = std::system(cmd.c_str());
            std::remove(tmp_ll.c_str());

            if (ret != 0) {
                std::cerr << "Compilation failed: clang exited with status " << ret << "\n";
                return false;
            }

            if (mode == OutputMode::Run) {
                int run_ret = std::system(target_bin.c_str());
                if (output_file.empty()) {
                    std::remove(target_bin.c_str());
                }
                return (run_ret == 0);
            }
            break;
        }
        }
    } catch (const CompileError &e) {
        return false;
    } catch (const std::exception &e) {
        std::cerr << "Fatal error: " << e.what() << "\n";
        return false;
    }

    return !diag.has_errors();
}

int main(int argc, char *argv[]) {
    try {
        bool show_tokens = false;
        OutputMode mode = OutputMode::Binary;
        std::string filename;
        std::string output_file;

        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];
            if (arg == "-h" || arg == "--help") {
                std::cout << "Usage: salmon [options] <source_file>\n\n";
                std::cout << "Options:\n";
                std::cout << "  -o <file>        Specify output binary or file (default: ./<stem>)\n";
                std::cout << "  -S               Compile to target assembly (.s)\n";
                std::cout << "  --emit-llvm      Emit LLVM IR (.ll)\n";
                std::cout << "  -run, --run      Compile and run binary immediately\n";
                std::cout << "  -p, --pretty     Pretty-print formatted source code from AST\n";
                std::cout << "  --tree           Print visual AST hierarchy tree\n";
                std::cout << "  --asdl           Print Zephyr ASDL AST\n";
                std::cout << "  -t, --tokens     Print lexer tokens\n";
                std::cout << "  -h, --help       Show this help message\n";
                return 0;
            }
            if (arg == "-o") {
                if (i + 1 < argc) {
                    output_file = argv[++i];
                }
            } else if (arg == "-S") {
                mode = OutputMode::Assembly;
            } else if (arg == "--emit-llvm" || arg == "-emit-llvm" || arg == "--ir") {
                mode = OutputMode::LLVM;
            } else if (arg == "-run" || arg == "--run" || arg == "-r") {
                mode = OutputMode::Run;
            } else if (arg == "-p" || arg == "--pretty") {
                mode = OutputMode::Pretty;
            } else if (arg == "--tree") {
                mode = OutputMode::Tree;
            } else if (arg == "--asdl") {
                mode = OutputMode::Asdl;
            } else if (arg == "-t" || arg == "--tokens") {
                show_tokens = true;
            } else if (filename.empty()) {
                filename = arg;
            }
        }

        if (filename.empty()) {
            std::cerr << "Error: No input file specified.\n";
            std::cerr << "Usage: salmon [options] <source_file>\n";
            std::cerr << "Run 'salmon --help' for available options.\n";
            return 1;
        }

        std::ifstream file(filename);
        if (!file.is_open()) {
            std::cerr << "Error: Could not open file '" << filename << "'\n";
            return 1;
        }
        std::stringstream buffer;
        buffer << file.rdbuf();
        bool ok = run_compiler(filename, buffer.str(), show_tokens, mode, output_file);
        return ok ? 0 : 1;
    } catch (const ParseError &e) {
        std::cerr << e.what() << "\n";
        return 1;
    } catch (const std::exception &e) {
        std::cerr << "Fatal error: " << e.what() << "\n";
        return 1;
    }
}
