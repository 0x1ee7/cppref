#include "formula.hpp"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstddef>
#include <expected>
#include <format>
#include <functional>
#include <map>
#include <numeric>
#include <optional>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace cppref::formula {

namespace {

struct Node {
    enum class Kind { number, cell, call, negate, binary };

    Kind kind = Kind::number;
    double number = 0.0;
    std::string name;  // cell or function name
    char op = '\0';    // binary operator: + - * /
    std::vector<Node> children;
};

using ParseResult = std::expected<Node, Error>;
using Value = std::expected<double, Error>;

std::unexpected<Error> syntax_error(std::string message) {
    return std::unexpected(Error{ErrorKind::syntax, std::move(message)});
}

std::unexpected<Error> eval_error(ErrorKind kind, std::string message) {
    return std::unexpected(Error{kind, std::move(message)});
}

bool is_space(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

bool is_digit(char c) {
    return c >= '0' && c <= '9';
}

bool is_ident_start(char c) {
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '_';
}

bool is_ident_char(char c) {
    return is_ident_start(c) || is_digit(c);
}

Node make_binary(char op, Node lhs, Node rhs) {
    Node n;
    n.kind = Node::Kind::binary;
    n.op = op;
    n.children.push_back(std::move(lhs));
    n.children.push_back(std::move(rhs));
    return n;
}

// Recursive-descent parser. Produces an AST; each rule returns a syntax error on failure.
class Parser {
public:
    explicit Parser(std::string_view source) : src_(source) {}

    ParseResult parse() {
        ParseResult root = parse_expr();
        if (!root) return root;
        skip_space();
        if (pos_ != src_.size()) return syntax_error("unexpected input after expression");
        return root;
    }

private:
    char peek() const { return pos_ < src_.size() ? src_[pos_] : '\0'; }

    void skip_space() {
        while (pos_ < src_.size() && is_space(src_[pos_])) ++pos_;
    }

    ParseResult parse_expr() {
        ParseResult lhs = parse_term();
        if (!lhs) return lhs;
        for (;;) {
            skip_space();
            const char op = peek();
            if (op != '+' && op != '-') return lhs;
            ++pos_;
            ParseResult rhs = parse_term();
            if (!rhs) return rhs;
            lhs = make_binary(op, std::move(*lhs), std::move(*rhs));
        }
    }

    ParseResult parse_term() {
        ParseResult lhs = parse_unary();
        if (!lhs) return lhs;
        for (;;) {
            skip_space();
            const char op = peek();
            if (op != '*' && op != '/') return lhs;
            ++pos_;
            ParseResult rhs = parse_unary();
            if (!rhs) return rhs;
            lhs = make_binary(op, std::move(*lhs), std::move(*rhs));
        }
    }

    ParseResult parse_unary() {
        skip_space();
        if (peek() == '-') {
            ++pos_;
            ParseResult operand = parse_unary();
            if (!operand) return operand;
            Node n;
            n.kind = Node::Kind::negate;
            n.children.push_back(std::move(*operand));
            return n;
        }
        return parse_primary();
    }

    ParseResult parse_primary() {
        skip_space();
        const char c = peek();
        if (c == '(') {
            ++pos_;
            ParseResult inner = parse_expr();
            if (!inner) return inner;
            skip_space();
            if (peek() != ')') return syntax_error("expected ')'");
            ++pos_;
            return inner;
        }
        if (is_digit(c) || c == '.') return parse_number();
        if (is_ident_start(c)) return parse_name();
        return syntax_error("expected a number, a name or '('");
    }

    ParseResult parse_number() {
        const std::size_t start = pos_;
        while (is_digit(peek())) ++pos_;
        if (peek() == '.') {
            ++pos_;
            while (is_digit(peek())) ++pos_;
        }
        const char* first = src_.data() + start;
        const char* last = src_.data() + pos_;
        double value = 0.0;
        const auto [ptr, ec] = std::from_chars(first, last, value);
        if (ec != std::errc{} || ptr != last) return syntax_error("malformed number");
        Node n;
        n.kind = Node::Kind::number;
        n.number = value;
        return n;
    }

    ParseResult parse_name() {
        const std::size_t start = pos_;
        while (pos_ < src_.size() && is_ident_char(src_[pos_])) ++pos_;
        std::string name(src_.substr(start, pos_ - start));

        skip_space();
        if (peek() != '(') {
            Node n;
            n.kind = Node::Kind::cell;
            n.name = std::move(name);
            return n;
        }

        ++pos_;  // '('
        Node call;
        call.kind = Node::Kind::call;
        call.name = std::move(name);
        skip_space();
        if (peek() == ')') {
            ++pos_;
            return call;
        }
        for (;;) {
            ParseResult arg = parse_expr();
            if (!arg) return arg;
            call.children.push_back(std::move(*arg));
            skip_space();
            if (peek() == ',') {
                ++pos_;
                continue;
            }
            if (peek() == ')') {
                ++pos_;
                return call;
            }
            return syntax_error("expected ',' or ')' in call");
        }
    }

    std::string_view src_;
    std::size_t pos_ = 0;
};

void collect_cells(const Node& n, std::set<std::string>& out) {
    if (n.kind == Node::Kind::cell) out.insert(n.name);
    for (const Node& child : n.children) collect_cells(child, out);
}

Value eval(const Node& n, const Environment& env);

Value eval_call(const Node& n, const Environment& env) {
    const auto ref = env.find_function(n.name);
    if (!ref) return eval_error(ErrorKind::unknown_function, "unknown function " + n.name);
    if (ref->arity && *ref->arity != n.children.size()) {
        return eval_error(ErrorKind::arity, n.name + " called with the wrong number of arguments");
    }
    std::vector<double> args;
    args.reserve(n.children.size());
    for (const Node& child : n.children) {
        Value v = eval(child, env);
        if (!v) return v;
        args.push_back(*v);
    }
    return ref->impl(args);
}

Value eval(const Node& n, const Environment& env) {
    switch (n.kind) {
        case Node::Kind::number:
            return n.number;
        case Node::Kind::cell: {
            const auto value = env.cell(n.name);
            if (!value) return eval_error(ErrorKind::unknown_cell, "unknown cell " + n.name);
            return *value;
        }
        case Node::Kind::negate: {
            Value v = eval(n.children[0], env);
            if (!v) return v;
            return -*v;
        }
        case Node::Kind::binary: {
            Value lhs = eval(n.children[0], env);
            if (!lhs) return lhs;
            Value rhs = eval(n.children[1], env);
            if (!rhs) return rhs;
            switch (n.op) {
                case '+':
                    return *lhs + *rhs;
                case '-':
                    return *lhs - *rhs;
                case '*':
                    return *lhs * *rhs;
                default:
                    if (*rhs == 0.0) return eval_error(ErrorKind::division_by_zero, "division by zero");
                    return *lhs / *rhs;
            }
        }
        case Node::Kind::call:
            return eval_call(n, env);
    }
    return eval_error(ErrorKind::syntax, "invalid expression tree");
}

}  // namespace

void Environment::set_cell(std::string name, double value) {
    cells_.insert_or_assign(std::move(name), value);
}

void Environment::define_function(std::string name, std::optional<std::size_t> arity, Function impl) {
    functions_.insert_or_assign(std::move(name), Entry{arity, std::move(impl)});
}

std::optional<double> Environment::cell(std::string_view name) const {
    const auto it = cells_.find(name);
    if (it == cells_.end()) return std::nullopt;
    return it->second;
}

std::optional<Environment::FunctionRef> Environment::find_function(std::string_view name) const {
    const auto it = functions_.find(name);
    if (it == functions_.end()) return std::nullopt;
    return FunctionRef{it->second.arity, it->second.impl};
}

Environment standard_environment() {
    Environment env;
    env.define_function("SUM", std::nullopt, [](std::span<const double> args) {
        return std::accumulate(args.begin(), args.end(), 0.0);
    });
    env.define_function("ABS", std::size_t{1}, [](std::span<const double> args) {
        return std::fabs(args[0]);
    });
    env.define_function("MIN", std::size_t{2}, [](std::span<const double> args) {
        return std::min(args[0], args[1]);
    });
    env.define_function("MAX", std::size_t{2}, [](std::span<const double> args) {
        return std::max(args[0], args[1]);
    });
    return env;
}

std::expected<double, Error> evaluate(std::string_view expression, const Environment& env) {
    ParseResult ast = Parser(expression).parse();
    if (!ast) return std::unexpected(ast.error());
    return eval(*ast, env);
}

std::expected<std::vector<std::string>, Error> referenced_cells(std::string_view expression) {
    ParseResult ast = Parser(expression).parse();
    if (!ast) return std::unexpected(ast.error());
    std::set<std::string> cells;
    collect_cells(*ast, cells);
    return std::vector<std::string>(cells.begin(), cells.end());
}

}  // namespace cppref::formula
