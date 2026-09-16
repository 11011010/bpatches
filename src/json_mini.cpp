#include "json_mini.h"
#include <sstream>
#include <cctype>
#include <iomanip>

namespace json {

static const Value kNullValue;
static Value kMutableNullValue;

const Value& Value::operator[](const std::string& key) const {
    if (!is_object()) return kNullValue;
    auto it = obj_val.find(key);
    if (it != obj_val.end()) return it->second;
    return kNullValue;
}

Value& Value::operator[](const std::string& key) {
    if (type != Type::Object) {
        type = Type::Object;
        obj_val.clear();
    }
    return obj_val[key];
}

const Value& Value::operator[](const char* key) const {
    return (*this)[std::string(key ? key : "")];
}

Value& Value::operator[](const char* key) {
    return (*this)[std::string(key ? key : "")];
}

const Value& Value::operator[](size_t index) const {
    if (!is_array() || index >= arr_val.size()) return kNullValue;
    return arr_val[index];
}

Value& Value::operator[](size_t index) {
    if (type != Type::Array) {
        type = Type::Array;
        arr_val.clear();
    }
    if (index >= arr_val.size()) {
        arr_val.resize(index + 1);
    }
    return arr_val[index];
}

class Parser {
    const std::string& src;
    size_t pos = 0;
    std::string* err = nullptr;

    void skip_whitespace() {
        while (pos < src.size() && (src[pos] == ' ' || src[pos] == '\t' || src[pos] == '\n' || src[pos] == '\r')) {
            pos++;
        }
    }

    char peek() {
        skip_whitespace();
        if (pos >= src.size()) return '\0';
        return src[pos];
    }

    char get() {
        skip_whitespace();
        if (pos >= src.size()) return '\0';
        return src[pos++];
    }

    void set_error(const std::string& msg) {
        if (err && err->empty()) {
            *err = msg + " at pos " + std::to_string(pos);
        }
    }

public:
    Parser(const std::string& s, std::string* e) : src(s), err(e) {}

    Value parse_value() {
        char c = peek();
        if (c == '\0') {
            set_error("Unexpected end of input");
            return Value();
        }
        if (c == '{') return parse_object();
        if (c == '[') return parse_array();
        if (c == '"') return parse_string();
        if (c == 't' || c == 'f') return parse_bool();
        if (c == 'n') return parse_null();
        if (c == '-' || std::isdigit(static_cast<unsigned char>(c))) return parse_number();

        set_error(std::string("Unexpected character: ") + c);
        return Value();
    }

    Value parse_object() {
        Value val(Type::Object);
        get(); // consume '{'
        skip_whitespace();
        if (peek() == '}') {
            get();
            return val;
        }

        while (pos < src.size()) {
            skip_whitespace();
            if (peek() != '"') {
                set_error("Expected string key in object");
                return Value();
            }
            Value key = parse_string();
            skip_whitespace();
            if (get() != ':') {
                set_error("Expected ':' after object key");
                return Value();
            }
            Value child = parse_value();
            val.obj_val[key.str_val] = child;

            skip_whitespace();
            char next = get();
            if (next == '}') return val;
            if (next != ',') {
                set_error("Expected ',' or '}' in object");
                return Value();
            }
        }
        set_error("Unterminated object");
        return Value();
    }

    Value parse_array() {
        Value val(Type::Array);
        get(); // consume '['
        skip_whitespace();
        if (peek() == ']') {
            get();
            return val;
        }

        while (pos < src.size()) {
            val.arr_val.push_back(parse_value());
            skip_whitespace();
            char next = get();
            if (next == ']') return val;
            if (next != ',') {
                set_error("Expected ',' or ']' in array");
                return Value();
            }
        }
        set_error("Unterminated array");
        return Value();
    }

    Value parse_string() {
        get(); // consume '"'
        std::string out;
        while (pos < src.size()) {
            char c = src[pos++];
            if (c == '"') {
                return Value(out);
            }
            if (c == '\\') {
                if (pos >= src.size()) break;
                char esc = src[pos++];
                switch (esc) {
                    case '"': out += '"'; break;
                    case '\\': out += '\\'; break;
                    case '/': out += '/'; break;
                    case 'b': out += '\b'; break;
                    case 'f': out += '\f'; break;
                    case 'n': out += '\n'; break;
                    case 'r': out += '\r'; break;
                    case 't': out += '\t'; break;
                    case 'u': {
                        if (pos + 4 <= src.size()) {
                            pos += 4;
                            out += '?';
                        }
                        break;
                    }
                    default: out += esc; break;
                }
            } else {
                out += c;
            }
        }
        set_error("Unterminated string");
        return Value();
    }

    Value parse_number() {
        size_t start = pos;
        if (pos < src.size() && src[pos] == '-') pos++;
        while (pos < src.size() && std::isdigit(static_cast<unsigned char>(src[pos]))) pos++;
        if (pos < src.size() && src[pos] == '.') {
            pos++;
            while (pos < src.size() && std::isdigit(static_cast<unsigned char>(src[pos]))) pos++;
        }
        if (pos < src.size() && (src[pos] == 'e' || src[pos] == 'E')) {
            pos++;
            if (pos < src.size() && (src[pos] == '+' || src[pos] == '-')) pos++;
            while (pos < src.size() && std::isdigit(static_cast<unsigned char>(src[pos]))) pos++;
        }
        std::string num_str = src.substr(start, pos - start);
        double d = 0.0;
        try {
            d = std::stod(num_str);
        } catch (...) {}
        return Value(d);
    }

    Value parse_bool() {
        if (src.compare(pos, 4, "true") == 0) {
            pos += 4;
            return Value(true);
        }
        if (src.compare(pos, 5, "false") == 0) {
            pos += 5;
            return Value(false);
        }
        set_error("Invalid boolean literal");
        return Value();
    }

    Value parse_null() {
        if (src.compare(pos, 4, "null") == 0) {
            pos += 4;
            return Value();
        }
        set_error("Invalid null literal");
        return Value();
    }
};

Value parse(const std::string& text, std::string* error_out) {
    Parser p(text, error_out);
    return p.parse_value();
}

static void escape_string(const std::string& s, std::string& out) {
    out += '"';
    for (char c : s) {
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\b': out += "\\b"; break;
            case '\f': out += "\\f"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (static_cast<unsigned char>(c) < 0x20) {
                    char buf[8];
                    snprintf(buf, sizeof(buf), "\\u%04x", c);
                    out += buf;
                } else {
                    out += c;
                }
                break;
        }
    }
    out += '"';
}

static void serialize_internal(const Value& val, std::string& out, int indent, int current_indent) {
    std::string indent_str = (indent > 0) ? std::string(current_indent * indent, ' ') : "";
    std::string next_indent_str = (indent > 0) ? std::string((current_indent + 1) * indent, ' ') : "";
    std::string newline = (indent > 0) ? "\n" : "";
    std::string space = (indent > 0) ? " " : "";

    switch (val.type) {
        case Type::Null: out += "null"; break;
        case Type::Boolean: out += val.bool_val ? "true" : "false"; break;
        case Type::Number: {
            if (val.num_val == static_cast<int64_t>(val.num_val)) {
                out += std::to_string(static_cast<int64_t>(val.num_val));
            } else {
                std::ostringstream ss;
                ss << val.num_val;
                out += ss.str();
            }
            break;
        }
        case Type::String: escape_string(val.str_val, out); break;
        case Type::Array: {
            if (val.arr_val.empty()) {
                out += "[]";
                break;
            }
            out += "[" + newline;
            for (size_t i = 0; i < val.arr_val.size(); i++) {
                out += next_indent_str;
                serialize_internal(val.arr_val[i], out, indent, current_indent + 1);
                if (i + 1 < val.arr_val.size()) out += ",";
                out += newline;
            }
            out += indent_str + "]";
            break;
        }
        case Type::Object: {
            if (val.obj_val.empty()) {
                out += "{}";
                break;
            }
            out += "{" + newline;
            size_t i = 0;
            for (const auto& pair : val.obj_val) {
                out += next_indent_str;
                escape_string(pair.first, out);
                out += ":" + space;
                serialize_internal(pair.second, out, indent, current_indent + 1);
                if (++i < val.obj_val.size()) out += ",";
                out += newline;
            }
            out += indent_str + "}";
            break;
        }
    }
}

std::string serialize(const Value& val, int indent) {
    std::string out;
    serialize_internal(val, out, indent, 0);
    return out;
}

} // namespace json
