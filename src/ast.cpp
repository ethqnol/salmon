#include "ast.h"
#include "ast_visitor.h"
#include <sstream>

static std::string indent_str(int indent) {
    return std::string(indent * 4, ' ');
}

std::string_view primitive_kind_str(PrimitiveKind kind) {
    switch (kind) {
    case PrimitiveKind::Int:
        return "Int";
    case PrimitiveKind::I8:
        return "I8";
    case PrimitiveKind::I16:
        return "I16";
    case PrimitiveKind::I32:
        return "I32";
    case PrimitiveKind::I64:
        return "I64";
    case PrimitiveKind::UInt:
        return "UInt";
    case PrimitiveKind::U8:
        return "U8";
    case PrimitiveKind::U16:
        return "U16";
    case PrimitiveKind::U32:
        return "U32";
    case PrimitiveKind::U64:
        return "U64";
    case PrimitiveKind::Float:
        return "Float";
    case PrimitiveKind::F32:
        return "F32";
    case PrimitiveKind::F64:
        return "F64";
    case PrimitiveKind::Bool:
        return "Bool";
    case PrimitiveKind::Char:
        return "Char";
    case PrimitiveKind::String:
        return "String";
    case PrimitiveKind::Void:
        return "Void";
    }
    return "Unknown";
}

std::string_view assign_op_str(AssignOp op) {
    switch (op) {
    case AssignOp::Assign:
        return "Assign";
    case AssignOp::AddAssign:
        return "AddAssign";
    case AssignOp::SubAssign:
        return "SubAssign";
    case AssignOp::MulAssign:
        return "MulAssign";
    case AssignOp::DivAssign:
        return "DivAssign";
    }
    return "Unknown";
}

std::string_view binary_op_str(BinaryOp op) {
    switch (op) {
    case BinaryOp::LogicalOr:
        return "LogicalOr";
    case BinaryOp::LogicalAnd:
        return "LogicalAnd";
    case BinaryOp::Equal:
        return "Equal";
    case BinaryOp::NotEqual:
        return "NotEqual";
    case BinaryOp::Less:
        return "Less";
    case BinaryOp::LessEqual:
        return "LessEqual";
    case BinaryOp::Greater:
        return "Greater";
    case BinaryOp::GreaterEqual:
        return "GreaterEqual";
    case BinaryOp::Add:
        return "Add";
    case BinaryOp::Sub:
        return "Sub";
    case BinaryOp::Mul:
        return "Mul";
    case BinaryOp::Div:
        return "Div";
    case BinaryOp::Mod:
        return "Mod";
    }
    return "Unknown";
}

std::string_view unary_op_str(UnaryOp op) {
    switch (op) {
    case UnaryOp::AddressOf:
        return "AddressOf";
    case UnaryOp::Dereference:
        return "Dereference";
    case UnaryOp::LogicalNot:
        return "LogicalNot";
    case UnaryOp::Negate:
        return "Negate";
    }
    return "Unknown";
}

std::string_view postfix_op_str(PostfixOp op) {
    switch (op) {
    case PostfixOp::PostIncrement:
        return "PostIncrement";
    case PostfixOp::PostDecrement:
        return "PostDecrement";
    }
    return "Unknown";
}

std::string ASTNode::to_asdl(int indent) const {
    std::ostringstream out;
    emit_asdl(out, indent);
    return out.str();
}

std::string ASTNode::to_pretty(int indent) const {
    std::ostringstream out;
    pretty_print(out, indent);
    return out.str();
}

std::string ASTNode::to_tree() const {
    std::ostringstream out;
    print_tree(out, "", true);
    return out.str();
}

void ASTNode::print_tree(std::ostream &out, const std::string &prefix, bool is_last) const {
    out << prefix << (is_last ? "└── " : "├── ") << to_pretty(0) << "\n";
}

template <typename T>
static void emit_node_list(std::ostream &out, const std::vector<std::unique_ptr<T>> &list, int indent) {
    if (list.empty()) {
        out << "[]";
        return;
    }
    out << "[\n";
    for (size_t i = 0; i < list.size(); ++i) {
        out << indent_str(indent + 1);
        list[i]->emit_asdl(out, indent + 1);
        if (i + 1 < list.size()) {
            out << ",";
        }
        out << "\n";
    }
    out << indent_str(indent) << "]";
}

void PrimitiveType::emit_asdl(std::ostream &out, int /*indent*/) const {
    out << "PrimitiveType(kind=" << primitive_kind_str(kind_) << ")";
}

void PrimitiveType::pretty_print(std::ostream &out, int /*indent*/) const {
    switch (kind_) {
    case PrimitiveKind::Int:
        out << "int";
        break;
    case PrimitiveKind::I8:
        out << "i8";
        break;
    case PrimitiveKind::I16:
        out << "i16";
        break;
    case PrimitiveKind::I32:
        out << "i32";
        break;
    case PrimitiveKind::I64:
        out << "i64";
        break;
    case PrimitiveKind::UInt:
        out << "uint";
        break;
    case PrimitiveKind::U8:
        out << "u8";
        break;
    case PrimitiveKind::U16:
        out << "u16";
        break;
    case PrimitiveKind::U32:
        out << "u32";
        break;
    case PrimitiveKind::U64:
        out << "u64";
        break;
    case PrimitiveKind::Float:
        out << "float";
        break;
    case PrimitiveKind::F32:
        out << "f32";
        break;
    case PrimitiveKind::F64:
        out << "f64";
        break;
    case PrimitiveKind::Bool:
        out << "bool";
        break;
    case PrimitiveKind::Char:
        out << "char";
        break;
    case PrimitiveKind::String:
        out << "string";
        break;
    case PrimitiveKind::Void:
        out << "void";
        break;
    }
}

void NamedType::emit_asdl(std::ostream &out, int /*indent*/) const {
    out << "NamedType(name=\"" << name_ << "\")";
}

void NamedType::pretty_print(std::ostream &out, int /*indent*/) const {
    out << name_;
}

void ListType::emit_asdl(std::ostream &out, int indent) const {
    out << "ListType(elementType=";
    elem_type_->emit_asdl(out, indent);
    out << ")";
}

void ListType::pretty_print(std::ostream &out, int indent) const {
    out << "list<";
    elem_type_->pretty_print(out, indent);
    out << ">";
}

void PointerType::emit_asdl(std::ostream &out, int indent) const {
    out << "PointerType(baseType=";
    base_type_->emit_asdl(out, indent);
    out << ")";
}

void PointerType::pretty_print(std::ostream &out, int indent) const {
    base_type_->pretty_print(out, indent);
    out << "*";
}

void SliceType::emit_asdl(std::ostream &out, int indent) const {
    out << "SliceType(elementType=";
    elem_type_->emit_asdl(out, indent);
    out << ")";
}

void SliceType::pretty_print(std::ostream &out, int indent) const {
    elem_type_->pretty_print(out, indent);
    out << "[]";
}

void ArrayType::emit_asdl(std::ostream &out, int indent) const {
    out << "ArrayType(elementType=";
    elem_type_->emit_asdl(out, indent);
    out << ", size=" << size_ << ")";
}

void ArrayType::pretty_print(std::ostream &out, int indent) const {
    elem_type_->pretty_print(out, indent);
    out << "[" << size_ << "]";
}

void IncludeDirective::emit_asdl(std::ostream &out, int /*indent*/) const {
    out << "Include(path=\"" << path_ << "\")";
}

void IncludeDirective::pretty_print(std::ostream &out, int indent) const {
    out << indent_str(indent) << "include \"" << path_ << "\"";
}

void IncludeDirective::print_tree(std::ostream &out, const std::string &prefix, bool is_last) const {
    out << prefix << (is_last ? "└── " : "├── ") << "Include \"" << path_ << "\"\n";
}

void StructField::emit_asdl(std::ostream &out, int indent) const {
    out << "Field(type=";
    type_->emit_asdl(out, indent);
    out << ", name=\"" << name_ << "\")";
}

void StructField::pretty_print(std::ostream &out, int indent) const {
    out << indent_str(indent);
    type_->pretty_print(out);
    out << " " << name_ << ";";
}

void StructField::print_tree(std::ostream &out, const std::string &prefix, bool is_last) const {
    out << prefix << (is_last ? "└── " : "├── ") << "Field " << name_ << ": " << type_->to_pretty() << "\n";
}

void StructDecl::emit_asdl(std::ostream &out, int indent) const {
    out << "StructDecl(\n";
    out << indent_str(indent + 1) << "name=\"" << name_ << "\",\n";
    out << indent_str(indent + 1) << "fields=";
    emit_node_list(out, fields_, indent + 1);
    out << "\n"
        << indent_str(indent) << ")";
}

void StructDecl::pretty_print(std::ostream &out, int indent) const {
    out << indent_str(indent) << "struct " << name_ << " {\n";
    for (const auto &field : fields_) {
        field->pretty_print(out, indent + 1);
        out << "\n";
    }
    out << indent_str(indent) << "}";
}

void StructDecl::print_tree(std::ostream &out, const std::string &prefix, bool is_last) const {
    out << prefix << (is_last ? "└── " : "├── ") << "StructDecl " << name_ << "\n";
    std::string next_prefix = prefix + (is_last ? "    " : "│   ");
    for (size_t i = 0; i < fields_.size(); ++i) {
        fields_[i]->print_tree(out, next_prefix, i + 1 == fields_.size());
    }
}

void Param::emit_asdl(std::ostream &out, int indent) const {
    out << "Param(type=";
    type_->emit_asdl(out, indent);
    out << ", name=\"" << name_ << "\")";
}

void Param::pretty_print(std::ostream &out, int /*indent*/) const {
    type_->pretty_print(out);
    out << " " << name_;
}

void Param::print_tree(std::ostream &out, const std::string &prefix, bool is_last) const {
    out << prefix << (is_last ? "└── " : "├── ") << "Param " << name_ << ": " << type_->to_pretty() << "\n";
}

void FunctionDecl::emit_asdl(std::ostream &out, int indent) const {
    out << "FunctionDecl(\n";
    out << indent_str(indent + 1) << "name=\"" << name_ << "\",\n";
    out << indent_str(indent + 1) << "params=";
    emit_node_list(out, params_, indent + 1);
    out << ",\n";
    out << indent_str(indent + 1) << "returnType=";
    if (ret_type_) {
        ret_type_->emit_asdl(out, indent + 1);
    } else {
        out << "None";
    }
    out << ",\n";
    out << indent_str(indent + 1) << "body=";
    if (body_) {
        body_->emit_asdl(out, indent + 1);
    } else {
        out << "None";
    }
    out << "\n"
        << indent_str(indent) << ")";
}

void FunctionDecl::pretty_print(std::ostream &out, int indent) const {
    out << indent_str(indent);
    if (is_extern_) {
        out << "extern ";
    }
    out << "def " << name_ << "(";
    for (size_t i = 0; i < params_.size(); ++i) {
        params_[i]->pretty_print(out);
        if (i + 1 < params_.size() || is_vararg_) {
            out << ", ";
        }
    }
    if (is_vararg_) {
        out << "...";
    }
    out << ")";
    if (ret_type_) {
        out << " -> ";
        ret_type_->pretty_print(out);
    }
    if (body_) {
        out << " ";
        body_->pretty_print(out, indent);
    } else {
        out << ";";
    }
}

void FunctionDecl::print_tree(std::ostream &out, const std::string &prefix, bool is_last) const {
    out << prefix << (is_last ? "└── " : "├── ");
    if (is_extern_) {
        out << "Extern";
    }
    out << "FunctionDecl " << name_;
    if (ret_type_) {
        out << " -> " << ret_type_->to_pretty();
    }
    out << "\n";
    std::string next_prefix = prefix + (is_last ? "    " : "│   ");
    for (size_t i = 0; i < params_.size(); ++i) {
        bool last_param = (i + 1 == params_.size()) && !body_;
        params_[i]->print_tree(out, next_prefix, last_param);
    }
    if (body_) {
        body_->print_tree(out, next_prefix, true);
    }
}

void BlockStmt::emit_asdl(std::ostream &out, int indent) const {
    out << "Block(\n";
    out << indent_str(indent + 1) << "statements=";
    emit_node_list(out, stmts_, indent + 1);
    out << "\n"
        << indent_str(indent) << ")";
}

void BlockStmt::pretty_print(std::ostream &out, int indent) const {
    out << "{\n";
    for (const auto &stmt : stmts_) {
        stmt->pretty_print(out, indent + 1);
        out << "\n";
    }
    out << indent_str(indent) << "}";
}

void BlockStmt::print_tree(std::ostream &out, const std::string &prefix, bool is_last) const {
    out << prefix << (is_last ? "└── " : "├── ") << "Block\n";
    std::string next_prefix = prefix + (is_last ? "    " : "│   ");
    for (size_t i = 0; i < stmts_.size(); ++i) {
        stmts_[i]->print_tree(out, next_prefix, i + 1 == stmts_.size());
    }
}

void VarDeclStmt::emit_asdl(std::ostream &out, int indent) const {
    out << "VarDecl(\n";
    out << indent_str(indent + 1) << "type=";
    type_->emit_asdl(out, indent + 1);
    out << ",\n";
    out << indent_str(indent + 1) << "name=\"" << name_ << "\",\n";
    out << indent_str(indent + 1) << "init=";
    if (init_) {
        init_->emit_asdl(out, indent + 1);
    } else {
        out << "None";
    }
    out << "\n"
        << indent_str(indent) << ")";
}

void VarDeclStmt::pretty_print(std::ostream &out, int indent) const {
    out << indent_str(indent);
    type_->pretty_print(out);
    out << " " << name_;
    if (init_) {
        out << " = ";
        init_->pretty_print(out);
    }
    out << ";";
}

void VarDeclStmt::print_tree(std::ostream &out, const std::string &prefix, bool is_last) const {
    out << prefix << (is_last ? "└── " : "├── ") << "VarDecl " << type_->to_pretty() << " " << name_ << "\n";
    if (init_) {
        std::string next_prefix = prefix + (is_last ? "    " : "│   ");
        init_->print_tree(out, next_prefix, true);
    }
}

void AssignStmt::emit_asdl(std::ostream &out, int indent) const {
    out << "Assignment(\n";
    out << indent_str(indent + 1) << "target=";
    target_->emit_asdl(out, indent + 1);
    out << ",\n";
    out << indent_str(indent + 1) << "op=" << assign_op_str(op_) << ",\n";
    out << indent_str(indent + 1) << "value=";
    value_->emit_asdl(out, indent + 1);
    out << "\n"
        << indent_str(indent) << ")";
}

void AssignStmt::pretty_print(std::ostream &out, int indent) const {
    out << indent_str(indent);
    target_->pretty_print(out);
    switch (op_) {
    case AssignOp::Assign:
        out << " = ";
        break;
    case AssignOp::AddAssign:
        out << " += ";
        break;
    case AssignOp::SubAssign:
        out << " -= ";
        break;
    case AssignOp::MulAssign:
        out << " *= ";
        break;
    case AssignOp::DivAssign:
        out << " /= ";
        break;
    }
    value_->pretty_print(out);
    out << ";";
}

void AssignStmt::print_tree(std::ostream &out, const std::string &prefix, bool is_last) const {
    out << prefix << (is_last ? "└── " : "├── ") << "Assignment (" << assign_op_str(op_) << ")\n";
    std::string next_prefix = prefix + (is_last ? "    " : "│   ");
    target_->print_tree(out, next_prefix, false);
    value_->print_tree(out, next_prefix, true);
}

void ExprStmt::emit_asdl(std::ostream &out, int indent) const {
    out << "ExprStmt(expr=";
    expr_->emit_asdl(out, indent);
    out << ")";
}

void ExprStmt::pretty_print(std::ostream &out, int indent) const {
    out << indent_str(indent);
    expr_->pretty_print(out);
    out << ";";
}

void ExprStmt::print_tree(std::ostream &out, const std::string &prefix, bool is_last) const {
    out << prefix << (is_last ? "└── " : "├── ") << "ExprStmt\n";
    std::string next_prefix = prefix + (is_last ? "    " : "│   ");
    expr_->print_tree(out, next_prefix, true);
}

void IfStmt::emit_asdl(std::ostream &out, int indent) const {
    out << "IfStmt(\n";
    out << indent_str(indent + 1) << "condition=";
    cond_->emit_asdl(out, indent + 1);
    out << ",\n";
    out << indent_str(indent + 1) << "thenBranch=";
    then_branch_->emit_asdl(out, indent + 1);
    out << ",\n";
    out << indent_str(indent + 1) << "elseBranch=";
    if (else_branch_) {
        else_branch_->emit_asdl(out, indent + 1);
    } else {
        out << "None";
    }
    out << "\n"
        << indent_str(indent) << ")";
}

void IfStmt::pretty_print(std::ostream &out, int indent) const {
    out << indent_str(indent) << "if (";
    cond_->pretty_print(out);
    out << ") ";
    then_branch_->pretty_print(out, indent);
    if (else_branch_) {
        out << " else ";
        if (auto *else_if = dynamic_cast<const IfStmt *>(else_branch_.get())) {
            std::string s = else_if->to_pretty(0);
            out << s;
        } else {
            else_branch_->pretty_print(out, indent);
        }
    }
}

void IfStmt::print_tree(std::ostream &out, const std::string &prefix, bool is_last) const {
    out << prefix << (is_last ? "└── " : "├── ") << "IfStmt\n";
    std::string next_prefix = prefix + (is_last ? "    " : "│   ");
    cond_->print_tree(out, next_prefix, false);
    then_branch_->print_tree(out, next_prefix, else_branch_ == nullptr);
    if (else_branch_) {
        else_branch_->print_tree(out, next_prefix, true);
    }
}

void WhileStmt::emit_asdl(std::ostream &out, int indent) const {
    out << "WhileStmt(\n";
    out << indent_str(indent + 1) << "condition=";
    cond_->emit_asdl(out, indent + 1);
    out << ",\n";
    out << indent_str(indent + 1) << "body=";
    body_->emit_asdl(out, indent + 1);
    out << "\n"
        << indent_str(indent) << ")";
}

void WhileStmt::pretty_print(std::ostream &out, int indent) const {
    out << indent_str(indent) << "while (";
    cond_->pretty_print(out);
    out << ") ";
    body_->pretty_print(out, indent);
}

void WhileStmt::print_tree(std::ostream &out, const std::string &prefix, bool is_last) const {
    out << prefix << (is_last ? "└── " : "├── ") << "WhileStmt\n";
    std::string next_prefix = prefix + (is_last ? "    " : "│   ");
    cond_->print_tree(out, next_prefix, false);
    body_->print_tree(out, next_prefix, true);
}

void ForStmt::emit_asdl(std::ostream &out, int indent) const {
    out << "ForStmt(\n";
    out << indent_str(indent + 1) << "init=";
    if (init_) {
        init_->emit_asdl(out, indent + 1);
    } else {
        out << "None";
    }
    out << ",\n";
    out << indent_str(indent + 1) << "condition=";
    if (cond_) {
        cond_->emit_asdl(out, indent + 1);
    } else {
        out << "None";
    }
    out << ",\n";
    out << indent_str(indent + 1) << "update=";
    if (update_) {
        update_->emit_asdl(out, indent + 1);
    } else {
        out << "None";
    }
    out << ",\n";
    out << indent_str(indent + 1) << "body=";
    body_->emit_asdl(out, indent + 1);
    out << "\n"
        << indent_str(indent) << ")";
}

void ForStmt::pretty_print(std::ostream &out, int indent) const {
    out << indent_str(indent) << "for (";
    if (init_) {
        std::string s = init_->to_pretty(0);
        out << s << " ";
    } else {
        out << "; ";
    }
    if (cond_) {
        cond_->pretty_print(out);
    }
    out << "; ";
    if (update_) {
        std::string s = update_->to_pretty(0);
        if (!s.empty() && s.back() == ';') {
            s.pop_back();
        }
        out << s;
    }
    out << ") ";
    body_->pretty_print(out, indent);
}

void ForStmt::print_tree(std::ostream &out, const std::string &prefix, bool is_last) const {
    out << prefix << (is_last ? "└── " : "├── ") << "ForStmt\n";
    std::string next_prefix = prefix + (is_last ? "    " : "│   ");
    if (init_) {
        init_->print_tree(out, next_prefix, false);
    }
    if (cond_) {
        cond_->print_tree(out, next_prefix, false);
    }
    if (update_) {
        update_->print_tree(out, next_prefix, false);
    }
    body_->print_tree(out, next_prefix, true);
}

void ReturnStmt::emit_asdl(std::ostream &out, int indent) const {
    out << "ReturnStmt(value=";
    if (value_) {
        value_->emit_asdl(out, indent);
    } else {
        out << "None";
    }
    out << ")";
}

void ReturnStmt::pretty_print(std::ostream &out, int indent) const {
    out << indent_str(indent) << "return";
    if (value_) {
        out << " ";
        value_->pretty_print(out);
    }
    out << ";";
}

void ReturnStmt::print_tree(std::ostream &out, const std::string &prefix, bool is_last) const {
    out << prefix << (is_last ? "└── " : "├── ") << "ReturnStmt\n";
    if (value_) {
        std::string next_prefix = prefix + (is_last ? "    " : "│   ");
        value_->print_tree(out, next_prefix, true);
    }
}

void DeferStmt::emit_asdl(std::ostream &out, int indent) const {
    out << "DeferStmt(stmt=";
    stmt_->emit_asdl(out, indent);
    out << ")";
}

void DeferStmt::pretty_print(std::ostream &out, int indent) const {
    out << indent_str(indent) << "defer ";
    stmt_->pretty_print(out, 0);
}

void DeferStmt::print_tree(std::ostream &out, const std::string &prefix, bool is_last) const {
    out << prefix << (is_last ? "└── " : "├── ") << "DeferStmt\n";
    std::string next_prefix = prefix + (is_last ? "    " : "│   ");
    stmt_->print_tree(out, next_prefix, true);
}

void BinaryExpr::emit_asdl(std::ostream &out, int indent) const {
    out << "BinaryExpr(\n";
    out << indent_str(indent + 1) << "op=" << binary_op_str(op_) << ",\n";
    out << indent_str(indent + 1) << "left=";
    left_->emit_asdl(out, indent + 1);
    out << ",\n";
    out << indent_str(indent + 1) << "right=";
    right_->emit_asdl(out, indent + 1);
    out << "\n"
        << indent_str(indent) << ")";
}

void BinaryExpr::pretty_print(std::ostream &out, int /*indent*/) const {
    left_->pretty_print(out);
    switch (op_) {
    case BinaryOp::LogicalOr:
        out << " || ";
        break;
    case BinaryOp::LogicalAnd:
        out << " && ";
        break;
    case BinaryOp::Equal:
        out << " == ";
        break;
    case BinaryOp::NotEqual:
        out << " != ";
        break;
    case BinaryOp::Less:
        out << " < ";
        break;
    case BinaryOp::LessEqual:
        out << " <= ";
        break;
    case BinaryOp::Greater:
        out << " > ";
        break;
    case BinaryOp::GreaterEqual:
        out << " >= ";
        break;
    case BinaryOp::Add:
        out << " + ";
        break;
    case BinaryOp::Sub:
        out << " - ";
        break;
    case BinaryOp::Mul:
        out << " * ";
        break;
    case BinaryOp::Div:
        out << " / ";
        break;
    case BinaryOp::Mod:
        out << " % ";
        break;
    }
    right_->pretty_print(out);
}

void BinaryExpr::print_tree(std::ostream &out, const std::string &prefix, bool is_last) const {
    out << prefix << (is_last ? "└── " : "├── ") << "BinaryExpr (" << binary_op_str(op_) << ")\n";
    std::string next_prefix = prefix + (is_last ? "    " : "│   ");
    left_->print_tree(out, next_prefix, false);
    right_->print_tree(out, next_prefix, true);
}

void UnaryExpr::emit_asdl(std::ostream &out, int indent) const {
    out << "UnaryExpr(op=" << unary_op_str(op_) << ", operand=";
    operand_->emit_asdl(out, indent);
    out << ")";
}

void UnaryExpr::pretty_print(std::ostream &out, int /*indent*/) const {
    switch (op_) {
    case UnaryOp::AddressOf:
        out << "&";
        break;
    case UnaryOp::Dereference:
        out << "*";
        break;
    case UnaryOp::LogicalNot:
        out << "!";
        break;
    case UnaryOp::Negate:
        out << "-";
        break;
    }
    operand_->pretty_print(out);
}

void UnaryExpr::print_tree(std::ostream &out, const std::string &prefix, bool is_last) const {
    out << prefix << (is_last ? "└── " : "├── ") << "UnaryExpr (" << unary_op_str(op_) << ")\n";
    std::string next_prefix = prefix + (is_last ? "    " : "│   ");
    operand_->print_tree(out, next_prefix, true);
}

void SizeofExpr::emit_asdl(std::ostream &out, int indent) const {
    out << "SizeofExpr(operand=";
    operand_->emit_asdl(out, indent);
    out << ")";
}

void SizeofExpr::pretty_print(std::ostream &out, int /*indent*/) const {
    out << "sizeof(";
    operand_->pretty_print(out);
    out << ")";
}

void SizeofExpr::print_tree(std::ostream &out, const std::string &prefix, bool is_last) const {
    out << prefix << (is_last ? "└── " : "├── ") << "SizeofExpr\n";
    std::string next_prefix = prefix + (is_last ? "    " : "│   ");
    operand_->print_tree(out, next_prefix, true);
}

void PostfixUpdateExpr::emit_asdl(std::ostream &out, int indent) const {
    out << "PostfixUpdateExpr(op=" << postfix_op_str(op_) << ", operand=";
    operand_->emit_asdl(out, indent);
    out << ")";
}

void PostfixUpdateExpr::pretty_print(std::ostream &out, int /*indent*/) const {
    operand_->pretty_print(out);
    out << (op_ == PostfixOp::PostIncrement ? "++" : "--");
}

void PostfixUpdateExpr::print_tree(std::ostream &out, const std::string &prefix, bool is_last) const {
    out << prefix << (is_last ? "└── " : "├── ") << "PostfixUpdateExpr (" << postfix_op_str(op_) << ")\n";
    std::string next_prefix = prefix + (is_last ? "    " : "│   ");
    operand_->print_tree(out, next_prefix, true);
}

void CallExpr::emit_asdl(std::ostream &out, int indent) const {
    out << "CallExpr(\n";
    out << indent_str(indent + 1) << "callee=";
    callee_->emit_asdl(out, indent + 1);
    out << ",\n";
    out << indent_str(indent + 1) << "args=";
    emit_node_list(out, args_, indent + 1);
    out << "\n"
        << indent_str(indent) << ")";
}

void CallExpr::pretty_print(std::ostream &out, int /*indent*/) const {
    callee_->pretty_print(out);
    out << "(";
    for (size_t i = 0; i < args_.size(); ++i) {
        args_[i]->pretty_print(out);
        if (i + 1 < args_.size()) {
            out << ", ";
        }
    }
    out << ")";
}

void CallExpr::print_tree(std::ostream &out, const std::string &prefix, bool is_last) const {
    out << prefix << (is_last ? "└── " : "├── ") << "CallExpr\n";
    std::string next_prefix = prefix + (is_last ? "    " : "│   ");
    callee_->print_tree(out, next_prefix, args_.empty());
    for (size_t i = 0; i < args_.size(); ++i) {
        args_[i]->print_tree(out, next_prefix, i + 1 == args_.size());
    }
}

void IndexExpr::emit_asdl(std::ostream &out, int indent) const {
    out << "IndexExpr(\n";
    out << indent_str(indent + 1) << "object=";
    object_->emit_asdl(out, indent + 1);
    out << ",\n";
    out << indent_str(indent + 1) << "index=";
    index_->emit_asdl(out, indent + 1);
    out << "\n"
        << indent_str(indent) << ")";
}

void IndexExpr::pretty_print(std::ostream &out, int /*indent*/) const {
    object_->pretty_print(out);
    out << "[";
    index_->pretty_print(out);
    out << "]";
}

void IndexExpr::print_tree(std::ostream &out, const std::string &prefix, bool is_last) const {
    out << prefix << (is_last ? "└── " : "├── ") << "IndexExpr\n";
    std::string next_prefix = prefix + (is_last ? "    " : "│   ");
    object_->print_tree(out, next_prefix, false);
    index_->print_tree(out, next_prefix, true);
}

void MemberAccessExpr::emit_asdl(std::ostream &out, int indent) const {
    out << "MemberAccessExpr(\n";
    out << indent_str(indent + 1) << "object=";
    object_->emit_asdl(out, indent + 1);
    out << ",\n";
    out << indent_str(indent + 1) << "member=\"" << member_ << "\"\n";
    out << indent_str(indent) << ")";
}

void MemberAccessExpr::pretty_print(std::ostream &out, int /*indent*/) const {
    object_->pretty_print(out);
    out << "." << member_;
}

void MemberAccessExpr::print_tree(std::ostream &out, const std::string &prefix, bool is_last) const {
    out << prefix << (is_last ? "└── " : "├── ") << "MemberAccessExpr ." << member_ << "\n";
    std::string next_prefix = prefix + (is_last ? "    " : "│   ");
    object_->print_tree(out, next_prefix, true);
}

void IntLiteralExpr::emit_asdl(std::ostream &out, int /*indent*/) const {
    out << "IntLiteral(value=" << value_ << ")";
}

void IntLiteralExpr::pretty_print(std::ostream &out, int /*indent*/) const {
    out << value_;
}

void IntLiteralExpr::print_tree(std::ostream &out, const std::string &prefix, bool is_last) const {
    out << prefix << (is_last ? "└── " : "├── ") << "IntLiteral " << value_ << "\n";
}

void FloatLiteralExpr::emit_asdl(std::ostream &out, int /*indent*/) const {
    out << "FloatLiteral(value=" << value_ << ")";
}

void FloatLiteralExpr::pretty_print(std::ostream &out, int /*indent*/) const {
    out << value_;
}

void FloatLiteralExpr::print_tree(std::ostream &out, const std::string &prefix, bool is_last) const {
    out << prefix << (is_last ? "└── " : "├── ") << "FloatLiteral " << value_ << "\n";
}

void StringLiteralExpr::emit_asdl(std::ostream &out, int /*indent*/) const {
    out << "StringLiteral(value=\"";
    for (char c : value_) {
        switch (c) {
        case '\n':
            out << "\\n";
            break;
        case '\t':
            out << "\\t";
            break;
        case '\r':
            out << "\\r";
            break;
        case '\\':
            out << "\\\\";
            break;
        case '\"':
            out << "\\\"";
            break;
        default:
            out << c;
            break;
        }
    }
    out << "\")";
}

void StringLiteralExpr::pretty_print(std::ostream &out, int /*indent*/) const {
    out << "\"";
    for (char c : value_) {
        switch (c) {
        case '\n':
            out << "\\n";
            break;
        case '\t':
            out << "\\t";
            break;
        case '\r':
            out << "\\r";
            break;
        case '\\':
            out << "\\\\";
            break;
        case '\"':
            out << "\\\"";
            break;
        default:
            out << c;
            break;
        }
    }
    out << "\"";
}

void StringLiteralExpr::print_tree(std::ostream &out, const std::string &prefix, bool is_last) const {
    out << prefix << (is_last ? "└── " : "├── ") << "StringLiteral \"" << to_pretty() << "\"\n";
}

void CharLiteralExpr::emit_asdl(std::ostream &out, int /*indent*/) const {
    out << "CharLiteral(value='";
    if (value_ == '\n') {
        out << "\\n";
    } else if (value_ == '\t') {
        out << "\\t";
    } else if (value_ == '\r') {
        out << "\\r";
    } else if (value_ == '\\') {
        out << "\\\\";
    } else if (value_ == '\'') {
        out << "\\'";
    } else {
        out << value_;
    }
    out << "')";
}

void CharLiteralExpr::pretty_print(std::ostream &out, int /*indent*/) const {
    out << "'";
    if (value_ == '\n')
        out << "\\n";
    else if (value_ == '\t')
        out << "\\t";
    else if (value_ == '\r')
        out << "\\r";
    else if (value_ == '\\')
        out << "\\\\";
    else if (value_ == '\'')
        out << "\\'";
    else
        out << value_;
    out << "'";
}

void CharLiteralExpr::print_tree(std::ostream &out, const std::string &prefix, bool is_last) const {
    out << prefix << (is_last ? "└── " : "├── ") << "CharLiteral " << to_pretty() << "\n";
}

void BoolLiteralExpr::emit_asdl(std::ostream &out, int /*indent*/) const {
    out << "BoolLiteral(value=" << (value_ ? "true" : "false") << ")";
}

void BoolLiteralExpr::pretty_print(std::ostream &out, int /*indent*/) const {
    out << (value_ ? "true" : "false");
}

void BoolLiteralExpr::print_tree(std::ostream &out, const std::string &prefix, bool is_last) const {
    out << prefix << (is_last ? "└── " : "├── ") << "BoolLiteral " << (value_ ? "true" : "false") << "\n";
}

void NullLiteralExpr::emit_asdl(std::ostream &out, int /*indent*/) const {
    out << "NullLiteral";
}

void NullLiteralExpr::pretty_print(std::ostream &out, int /*indent*/) const {
    out << "null";
}

void NullLiteralExpr::print_tree(std::ostream &out, const std::string &prefix, bool is_last) const {
    out << prefix << (is_last ? "└── " : "├── ") << "NullLiteral\n";
}

void ArrayLiteralExpr::emit_asdl(std::ostream &out, int indent) const {
    out << "ArrayLiteral(elements=";
    emit_node_list(out, elements_, indent);
    out << ")";
}

void ArrayLiteralExpr::pretty_print(std::ostream &out, int /*indent*/) const {
    out << "[";
    for (size_t i = 0; i < elements_.size(); ++i) {
        elements_[i]->pretty_print(out);
        if (i + 1 < elements_.size()) {
            out << ", ";
        }
    }
    out << "]";
}

void ArrayLiteralExpr::print_tree(std::ostream &out, const std::string &prefix, bool is_last) const {
    out << prefix << (is_last ? "└── " : "├── ") << "ArrayLiteral\n";
    std::string next_prefix = prefix + (is_last ? "    " : "│   ");
    for (size_t i = 0; i < elements_.size(); ++i) {
        elements_[i]->print_tree(out, next_prefix, i + 1 == elements_.size());
    }
}

void ListLiteralExpr::emit_asdl(std::ostream &out, int indent) const {
    out << "ListLiteral(elements=";
    emit_node_list(out, elements_, indent);
    out << ")";
}

void ListLiteralExpr::pretty_print(std::ostream &out, int /*indent*/) const {
    out << "list{";
    for (size_t i = 0; i < elements_.size(); ++i) {
        elements_[i]->pretty_print(out);
        if (i + 1 < elements_.size()) {
            out << ", ";
        }
    }
    out << "}";
}

void ListLiteralExpr::print_tree(std::ostream &out, const std::string &prefix, bool is_last) const {
    out << prefix << (is_last ? "└── " : "├── ") << "ListLiteral\n";
    std::string next_prefix = prefix + (is_last ? "    " : "│   ");
    for (size_t i = 0; i < elements_.size(); ++i) {
        elements_[i]->print_tree(out, next_prefix, i + 1 == elements_.size());
    }
}

void AllocExpr::emit_asdl(std::ostream &out, int indent) const {
    out << "AllocExpr(\n";
    out << indent_str(indent + 1) << "allocType=";
    alloc_type_->emit_asdl(out, indent + 1);
    out << ",\n";
    out << indent_str(indent + 1) << "count=";
    if (count_) {
        count_->emit_asdl(out, indent + 1);
    } else {
        out << "None";
    }
    out << "\n"
        << indent_str(indent) << ")";
}

void AllocExpr::pretty_print(std::ostream &out, int /*indent*/) const {
    out << "alloc<";
    alloc_type_->pretty_print(out);
    out << ">(";
    if (count_) {
        count_->pretty_print(out);
    }
    out << ")";
}

void AllocExpr::print_tree(std::ostream &out, const std::string &prefix, bool is_last) const {
    out << prefix << (is_last ? "└── " : "├── ") << "AllocExpr " << alloc_type_->to_pretty() << "\n";
    if (count_) {
        std::string next_prefix = prefix + (is_last ? "    " : "│   ");
        count_->print_tree(out, next_prefix, true);
    }
}

void IdentifierExpr::emit_asdl(std::ostream &out, int /*indent*/) const {
    out << "IdentifierExpr(name=\"" << name_ << "\")";
}

void IdentifierExpr::pretty_print(std::ostream &out, int /*indent*/) const {
    out << name_;
}

void IdentifierExpr::print_tree(std::ostream &out, const std::string &prefix, bool is_last) const {
    out << prefix << (is_last ? "└── " : "├── ") << "Identifier " << name_ << "\n";
}

void Program::emit_asdl(std::ostream &out, int indent) const {
    out << "Program(\n";
    out << indent_str(indent + 1) << "declarations=";
    emit_node_list(out, decls_, indent + 1);
    out << "\n"
        << indent_str(indent) << ")\n";
}

void Program::pretty_print(std::ostream &out, int indent) const {
    for (size_t i = 0; i < decls_.size(); ++i) {
        decls_[i]->pretty_print(out, indent);
        out << "\n";
        if (i + 1 < decls_.size()) {
            out << "\n";
        }
    }
}

void Program::print_tree(std::ostream &out, const std::string &prefix, bool /*is_last*/) const {
    out << prefix << "Program\n";
    for (size_t i = 0; i < decls_.size(); ++i) {
        decls_[i]->print_tree(out, prefix, i + 1 == decls_.size());
    }
}

void PrimitiveType::accept(ASTVisitor &visitor) const { visitor.visit(*this); }
void NamedType::accept(ASTVisitor &visitor) const { visitor.visit(*this); }
void ListType::accept(ASTVisitor &visitor) const { visitor.visit(*this); }
void PointerType::accept(ASTVisitor &visitor) const { visitor.visit(*this); }
void SliceType::accept(ASTVisitor &visitor) const { visitor.visit(*this); }
void ArrayType::accept(ASTVisitor &visitor) const { visitor.visit(*this); }

void IncludeDirective::accept(ASTVisitor &visitor) const { visitor.visit(*this); }
void StructField::accept(ASTVisitor &visitor) const { visitor.visit(*this); }
void StructDecl::accept(ASTVisitor &visitor) const { visitor.visit(*this); }
void Param::accept(ASTVisitor &visitor) const { visitor.visit(*this); }
void FunctionDecl::accept(ASTVisitor &visitor) const { visitor.visit(*this); }

void BlockStmt::accept(ASTVisitor &visitor) const { visitor.visit(*this); }
void VarDeclStmt::accept(ASTVisitor &visitor) const { visitor.visit(*this); }
void AssignStmt::accept(ASTVisitor &visitor) const { visitor.visit(*this); }
void ExprStmt::accept(ASTVisitor &visitor) const { visitor.visit(*this); }
void IfStmt::accept(ASTVisitor &visitor) const { visitor.visit(*this); }
void WhileStmt::accept(ASTVisitor &visitor) const { visitor.visit(*this); }
void ForStmt::accept(ASTVisitor &visitor) const { visitor.visit(*this); }
void ReturnStmt::accept(ASTVisitor &visitor) const { visitor.visit(*this); }
void DeferStmt::accept(ASTVisitor &visitor) const { visitor.visit(*this); }

void BinaryExpr::accept(ASTVisitor &visitor) const { visitor.visit(*this); }
void UnaryExpr::accept(ASTVisitor &visitor) const { visitor.visit(*this); }
void SizeofExpr::accept(ASTVisitor &visitor) const { visitor.visit(*this); }
void PostfixUpdateExpr::accept(ASTVisitor &visitor) const { visitor.visit(*this); }
void CallExpr::accept(ASTVisitor &visitor) const { visitor.visit(*this); }
void IndexExpr::accept(ASTVisitor &visitor) const { visitor.visit(*this); }
void MemberAccessExpr::accept(ASTVisitor &visitor) const { visitor.visit(*this); }
void IntLiteralExpr::accept(ASTVisitor &visitor) const { visitor.visit(*this); }
void FloatLiteralExpr::accept(ASTVisitor &visitor) const { visitor.visit(*this); }
void StringLiteralExpr::accept(ASTVisitor &visitor) const { visitor.visit(*this); }
void CharLiteralExpr::accept(ASTVisitor &visitor) const { visitor.visit(*this); }
void BoolLiteralExpr::accept(ASTVisitor &visitor) const { visitor.visit(*this); }
void NullLiteralExpr::accept(ASTVisitor &visitor) const { visitor.visit(*this); }
void ArrayLiteralExpr::accept(ASTVisitor &visitor) const { visitor.visit(*this); }
void ListLiteralExpr::accept(ASTVisitor &visitor) const { visitor.visit(*this); }
void AllocExpr::accept(ASTVisitor &visitor) const { visitor.visit(*this); }
void IdentifierExpr::accept(ASTVisitor &visitor) const { visitor.visit(*this); }

void Program::accept(ASTVisitor &visitor) const { visitor.visit(*this); }
