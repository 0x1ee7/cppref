#include "layered_config.hpp"

#include <cstdint>
#include <initializer_list>
#include <map>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <variant>

#include <gtest/gtest.h>

namespace {

using cppref::stl::Config;
using cppref::stl::ConfigLayers;
using cppref::stl::Layer;
using cppref::stl::List;
using cppref::stl::Table;
using cppref::stl::Value;
using cppref::stl::deep_merge;
using cppref::stl::parse_env;

Value num(std::int64_t n) { return Value{n}; }
Value str(std::string s) { return Value{std::move(s)}; }
Value flag(bool b) { return Value{b}; }
Value null() { return Value{}; }
Value list(List items) { return Value{std::move(items)}; }
Value table(Table t) { return Value{std::move(t)}; }

const Value& at_path(const Value& root, std::initializer_list<std::string> keys) {
    const Value* cur = &root;
    for (const auto& k : keys) cur = &std::get<Table>(cur->data).at(k);
    return *cur;
}

Value& at_path_mut(Value& root, std::initializer_list<std::string> keys) {
    Value* cur = &root;
    for (const auto& k : keys) cur = &std::get<Table>(cur->data).at(k);
    return *cur;
}

}  // namespace

TEST(Part5Config, DeepMergeRecursesIntoNestedTables) {
    const Value base = table(Table{
        {"db", table(Table{{"host", str("a")}, {"pool", table(Table{{"size", num(5)}})}})},
        {"debug", flag(false)},
    });
    const Value override_v = table(Table{
        {"db", table(Table{{"pool", table(Table{{"size", num(10)}})}})},
        {"debug", flag(true)},
    });

    const Value merged = deep_merge(base, override_v);

    EXPECT_EQ(std::get<std::string>(at_path(merged, {"db", "host"}).data), "a");
    EXPECT_EQ(std::get<std::int64_t>(at_path(merged, {"db", "pool", "size"}).data), 10);
    EXPECT_EQ(std::get<bool>(at_path(merged, {"debug"}).data), true);
}

TEST(Part5Config, ListsAreReplacedNotConcatenated) {
    const Value base = table(Table{{"tags", list({str("a"), str("b")})}});
    const Value override_v = table(Table{{"tags", list({str("c")})}});

    const Value merged = deep_merge(base, override_v);

    EXPECT_EQ(std::get<List>(at_path(merged, {"tags"}).data).size(), 1u);
}

TEST(Part5Config, NullOverridesAndScalarTableMismatchesOverrideWins) {
    const Value base = table(Table{
        {"a", num(1)},
        {"b", table(Table{{"x", num(1)}})},
        {"c", str("s")},
    });
    const Value override_v = table(Table{
        {"a", null()},
        {"b", num(5)},
        {"c", table(Table{{"y", num(2)}})},
    });

    const Value merged = deep_merge(base, override_v);

    EXPECT_EQ(std::get<std::monostate>(at_path(merged, {"a"}).data), std::monostate{});
    EXPECT_EQ(std::get<std::int64_t>(at_path(merged, {"b"}).data), 5);
    EXPECT_EQ(std::get<std::int64_t>(at_path(merged, {"c", "y"}).data), 2);
}

TEST(Part5Config, DeepMergeSharesNoStorageWithInputs) {
    const Value base = table(Table{{"db", table(Table{{"host", str("a")}})}});
    const Value override_v = table(Table{{"extra", num(1)}});

    Value merged = deep_merge(base, override_v);
    at_path_mut(merged, {"db", "host"}) = str("changed");

    EXPECT_EQ(std::get<std::string>(at_path(base, {"db", "host"}).data), "a");
    EXPECT_EQ(std::get<std::string>(at_path(merged, {"db", "host"}).data), "changed");
}

TEST(Part5Config, ParseEnvKeepsOnlyPrefixAndNestsSegments) {
    const std::map<std::string, std::string> vars = {
        {"APP_DB__POOL__SIZE", "10"},
        {"APP_DB__HOST", "db.internal"},
        {"OTHER_X", "1"},
        {"APP", "no-underscore"},
    };

    const Table env = parse_env(vars, "APP");

    EXPECT_EQ(env.size(), 1u);
    EXPECT_EQ(std::get<std::int64_t>(at_path(table(env), {"db", "pool", "size"}).data), 10);
    EXPECT_EQ(std::get<std::string>(at_path(table(env), {"db", "host"}).data), "db.internal");
}

TEST(Part5Config, ParseEnvCoercesBoolsAndIntegersAndKeepsOtherStrings) {
    const std::map<std::string, std::string> vars = {
        {"APP_A", "TRUE"}, {"APP_B", "false"}, {"APP_C", "-42"}, {"APP_D", "007"},
        {"APP_E", "1.5"},  {"APP_F", "-"},     {"APP_G", "+3"},  {"APP_H", ""},
        {"APP_I", "12a"},
    };

    const Table env = parse_env(vars, "APP");
    const Value root = table(env);

    EXPECT_EQ(std::get<bool>(at_path(root, {"a"}).data), true);
    EXPECT_EQ(std::get<bool>(at_path(root, {"b"}).data), false);
    EXPECT_EQ(std::get<std::int64_t>(at_path(root, {"c"}).data), -42);
    EXPECT_EQ(std::get<std::int64_t>(at_path(root, {"d"}).data), 7);
    EXPECT_EQ(std::get<std::string>(at_path(root, {"e"}).data), "1.5");
    EXPECT_EQ(std::get<std::string>(at_path(root, {"f"}).data), "-");
    EXPECT_EQ(std::get<std::string>(at_path(root, {"g"}).data), "+3");
    EXPECT_EQ(std::get<std::string>(at_path(root, {"h"}).data), "");
    EXPECT_EQ(std::get<std::string>(at_path(root, {"i"}).data), "12a");
}

TEST(Part5Config, ParseEnvRejectsEmptySegments) {
    EXPECT_THROW(parse_env({{"APP_", "x"}}, "APP"), std::invalid_argument);
    EXPECT_THROW(parse_env({{"APP_DB____HOST", "x"}}, "APP"), std::invalid_argument);
    EXPECT_THROW(parse_env({{"APP_DB__", "x"}}, "APP"), std::invalid_argument);
}

TEST(Part5Config, PrecedenceIsCliThenEnvThenFileThenDefaults) {
    ConfigLayers layers;
    layers.defaults = Table{{"db", table(Table{{"host", str("d")}, {"pool", table(Table{{"size", num(1)}})}})},
                            {"feature", str("off")}};
    layers.file = Table{{"db", table(Table{{"host", str("f")}, {"pool", table(Table{{"size", num(2)}})}})}};
    layers.env = Table{{"db", table(Table{{"host", str("e")}})}};
    layers.cli = Table{{"db", table(Table{{"host", str("c")}})}};

    const Config config(layers);

    EXPECT_EQ(std::get<std::string>(config.get("db.host")->data), "c");
    EXPECT_EQ(std::get<std::int64_t>(config.get("db.pool.size")->data), 2);
    EXPECT_EQ(std::get<std::string>(config.get("feature")->data), "off");
    EXPECT_EQ(config.source("db.host"), Layer::cli);
    EXPECT_EQ(config.source("db.pool.size"), Layer::file);
    EXPECT_EQ(config.source("feature"), Layer::defaults);
}

TEST(Part5Config, FullPathMustExistInLayerAndNonTablesAreAbsent) {
    ConfigLayers layers;
    layers.defaults = Table{{"db", table(Table{{"host", str("d")}})}};
    layers.env = Table{{"db", num(5)}};  // env's `db` is a scalar, so it has no `db.host`

    const Config config(layers);

    EXPECT_EQ(std::get<std::string>(config.get("db.host")->data), "d");
    EXPECT_EQ(config.source("db.host"), Layer::defaults);
    EXPECT_EQ(config.get("db.host.port"), nullptr);  // runs through a string
}

TEST(Part5Config, NullValueCountsAsPresentAndMissingPathIsAbsent) {
    ConfigLayers layers;
    layers.defaults = Table{{"timeout", num(30)}};
    layers.cli = Table{{"timeout", null()}};

    const Config config(layers);

    const Value* timeout = config.get("timeout");
    ASSERT_NE(timeout, nullptr);
    EXPECT_TRUE(std::holds_alternative<std::monostate>(timeout->data));
    EXPECT_EQ(config.source("timeout"), Layer::cli);

    EXPECT_EQ(config.get("nope"), nullptr);
    EXPECT_EQ(config.source("nope"), std::nullopt);
}

TEST(Part5Config, ConfigSnapshotsLayersAtConstruction) {
    ConfigLayers layers;
    layers.defaults = Table{{"name", str("original")}};

    const Config config(layers);
    layers.defaults["name"] = str("mutated");
    layers.defaults["added"] = num(1);

    EXPECT_EQ(std::get<std::string>(config.get("name")->data), "original");
    EXPECT_EQ(config.get("added"), nullptr);
}

TEST(Part5Config, AsDictIsEffectiveMergeAndIndependentOfConfig) {
    ConfigLayers layers;
    layers.defaults = Table{{"db", table(Table{{"host", str("d")}, {"port", num(5432)}})}};
    layers.env = Table{{"db", table(Table{{"host", str("e")}})}};

    const Config config(layers);
    Table dict = config.as_dict();

    EXPECT_EQ(std::get<std::string>(at_path(table(dict), {"db", "host"}).data), "e");
    EXPECT_EQ(std::get<std::int64_t>(at_path(table(dict), {"db", "port"}).data), 5432);

    // Mutating the returned dict must not leak back into the Config.
    dict["db"] = null();
    EXPECT_EQ(std::get<std::string>(config.get("db.host")->data), "e");
}
