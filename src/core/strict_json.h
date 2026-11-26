#pragma once
#include <nlohmann/json.hpp>
#include <set>
#include <string>
#include <string_view>
#include <vector>

namespace locus::core::detail {
using Json = nlohmann::json;
// The pinned JSON 3.12 callback DOM parser scans its parent array at
// every end_object (including when the callback always returns true).
// For the turns array that makes strict duplicate checking quadratic.
// Use the public streaming interface to validate unique decoded keys,
// then the ordinary linear DOM parser to feed the existing schema checks.
// No private library internals, schema duplication or permissive fallback.
class UniqueKeysSax final : public nlohmann::json_sax<Json> {
public:
    explicit UniqueKeysSax(std::string& error) : error_(error) {}
    bool null() override { return true; }
    bool boolean(bool) override { return true; }
    bool number_integer(number_integer_t) override { return true; }
    bool number_unsigned(number_unsigned_t) override { return true; }
    bool number_float(number_float_t,const string_t&) override { return true; }
    bool string(string_t&) override { return true; }
    bool binary(binary_t&) override { return true; }
    bool start_array(std::size_t) override { return Enter(); }
    bool end_array() override { --depth_; return true; }
    bool start_object(std::size_t) override {
        if (!Enter()) return false;
        members_.emplace_back();
        return true;
    }
    bool key(string_t& value) override {
        if (!members_.back().insert(value).second) {
            error_="JSON: duplicate JSON key: "+value;
            return false;
        }
        return true;
    }
    bool end_object() override {
        members_.pop_back();
        --depth_;
        return true;
    }
    bool parse_error(std::size_t,const std::string&,
                     const Json::exception& exception) override {
        error_=std::string("JSON: ")+exception.what();
        return false;
    }
private:
    bool Enter() {
        if (++depth_ <= 64) return true;
        error_="JSON: nesting exceeds 64 levels"; return false;
    }
    std::size_t depth_=0;
    std::string& error_;
    std::vector<std::set<std::string>> members_;
};

inline bool ValidateJsonStructure(std::string_view bytes,std::string& error) {
    UniqueKeysSax parser(error);
    return Json::sax_parse(bytes.begin(),bytes.end(),&parser);
}
} // namespace locus::core::detail
