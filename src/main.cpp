#include "lexer.h"
#include "parser.h"
#include "codegen/llvm_codegen.h"
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

enum class OutputMode {
    Asdl,
    Pretty,
    Tree,
    LLVM
};

static void run_compiler(const std::string &src_name,
                         const std::string &src_code,
                         bool show_tokens = false,
                         OutputMode mode = OutputMode::Asdl) {
    if (mode == OutputMode::LLVM) {
        std::cout << "; Compiling: " << src_name << "\n";
    } else {
        std::cout << "========================================\n";
        std::cout << " Compiling: " << src_name << "\n";
        std::cout << "========================================\n";
    }

    Lexer lexer(src_code);
    std::vector<Token> tokens = lexer.tokenize();

    if (show_tokens) {
        for (const auto &token : tokens) {
            std::cout << token.to_string() << "\n";
        }
        std::cout << "\n";
    }

    Parser parser(std::move(tokens));
    std::unique_ptr<Program> program = parser.parse_program();

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
        LLVMCodegen codegen(std::cout);
        codegen.generate(*program);
        break;
    }
    }
}

int main(int argc, char *argv[]) {
    try {
        bool show_tokens = false;
        OutputMode mode = OutputMode::Asdl;
        std::string filename;

        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];
            if (arg == "-h" || arg == "--help") {
                std::cout << "Usage: salmon [options] [source_file]\n";
                std::cout << "Options:\n";
                std::cout << "  -S, --emit-llvm  Emit textual LLVM IR (or --ir)\n";
                std::cout << "  -p, --pretty     Pretty-print formatted source code from AST\n";
                std::cout << "  --tree           Print visual AST hierarchy tree\n";
                std::cout << "  --asdl           Print Zephyr ASDL AST (default)\n";
                std::cout << "  -t, --tokens     Print lexer tokens before AST\n";
                std::cout << "  -h, --help       Show this help message\n";
                return 0;
            }
            if (arg == "-S" || arg == "--emit-llvm" || arg == "-emit-llvm" || arg == "--ir") {
                mode = OutputMode::LLVM;
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

        if (!filename.empty()) {
            std::ifstream file(filename);
            if (!file.is_open()) {
                std::cerr << "Error: Could not open file '" << filename << "'\n";
                return 1;
            }
            std::stringstream buffer;
            buffer << file.rdbuf();
            run_compiler(filename, buffer.str(), show_tokens, mode);
            return 0;
        }

        const std::string example1 = R"(
include "std/io"
include "std/list"

def print_integers(int[] view) -> void {
    for (int i = 0; i < view.len; i++) {
        printf("%d ", view[i]);
    }
    printf("\n");
}

def main() -> int {
    // Fixed-size stack array
    int[4] fixed = [10, 20, 30, 40];
    print_integers(fixed);

    // Dynamic growable list
    list<int> numbers = list{1, 2, 3};
    numbers.push(4);
    numbers.push(5);

    printf("Dynamic list length: %d\n", numbers.len);
    for (int i = 0; i < numbers.len; i++) {
        printf("numbers[%d] = %d\n", i, numbers[i]);
    }

    // Explicit cleanup for dynamic list backing store
    numbers.free();
    return 0;
}
)";

        const std::string example2 = R"(
include "std/io"
include "std/mem"

struct Node {
    int value;
    Node* next;
}

def create_node(int val) -> Node* {
    Node* node = alloc<Node>();
    if (node == null) {
        return null;
    }
    node->value = val;
    node->next = null;
    return node;
}

def free_list(Node* head) -> void {
    Node* curr = head;
    while (curr != null) {
        Node* temp = curr->next;
        free(curr);
        curr = temp;
    }
}

def main() -> int {
    Node* head = create_node(100);
    head->next = create_node(200);
    head->next->next = create_node(300);

    // Traversing raw heap nodes
    Node* curr = head;
    while (curr != null) {
        printf("Node value: %d\n", curr->value);
        curr = curr->next;
    }

    free_list(head);
    return 0;
}
)";

        run_compiler("Example 1: Dynamic Lists & Fixed Arrays", example1,
                     show_tokens, mode);
        run_compiler("Example 2: Structs, Pointers, and Memory Allocation",
                     example2, show_tokens, mode);

        return 0;
    } catch (const ParseError &e) {
        std::cerr << e.what() << "\n";
        return 1;
    } catch (const std::exception &e) {
        std::cerr << "Fatal error: " << e.what() << "\n";
        return 1;
    }
}
