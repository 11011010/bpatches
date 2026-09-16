#pragma once

#include <string>
#include <vector>
#include <map>
#include <cstdint>

namespace json {

enum class Type {
    Null,
    Boolean,
    Number,
    String,
    Array,
    Object
};

class Value {
public:
    Type type = Type::Null;
    bool bool_val = false;
    double num_val = 0.0;
    std::string str_val;
    std::vector<Value> arr_val;
    std::map<std::string, Value> obj_val;

    Value() : type(Type::Null) {}
    explicit Value(bool b) : type(Type::Boolean), bool_val(b) {}
    explicit Value(double n) : type(Type::Number), num_val(n) {}
    explicit Value(int64_t n) : type(Type::Number), num_val(static_cast<double>(n)) {}
    explicit Value(const std::string& s) : type(Type::String), str_val(s) {}
    explicit Value(const char* s) : type(Type::String), str_val(s ? s : "") {}
    explicit Value(Type t) : type(t) {}

    bool is_null() const { return type == Type::Null; }
    bool is_bool() const { return type == Type::Boolean; }
    bool is_number() const { return type == Type::Number; }
    bool is_string() const { return type == Type::String; }
    bool is_array() const { return type == Type::Array; }
    bool is_object() const { return type == Type::Object; }

    const std::string& as_string(const std::string& default_val = "") const {
        return is_string() ? str_val : default_val;
    }

    int64_t as_int64(int64_t default_val = 0) const {
        return is_number() ? static_cast<int64_t>(num_val) : default_val;
    }

    bool as_bool(bool default_val = false) const {
        return is_bool() ? bool_val : default_val;
    }

    const Value& operator[](const std::string& key) const;
    Value& operator[](const std::string& key);
    const Value& operator[](const char* key) const;
    Value& operator[](const char* key);

    const Value& operator[](size_t index) const;
    Value& operator[](size_t index);
    const Value& operator[](int index) const { return (*this)[static_cast<size_t>(index)]; }
    Value& operator[](int index) { return (*this)[static_cast<size_t>(index)]; }

    bool has(const std::string& key) const {
        if (!is_object()) return false;
        return obj_val.find(key) != obj_val.end();
    }

    size_t size() const {
        if (is_array()) return arr_val.size();
        if (is_object()) return obj_val.size();
        return 0;
    }
};

Value parse(const std::string& text, std::string* error_out = nullptr);
std::string serialize(const Value& val, int indent = 0);

} // namespace json
