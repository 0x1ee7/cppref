// Part 7a: lazy proxy (Lazy<T>). Part 7b: virtual-base diamond (Device / Reader / Writer).
#include <gtest/gtest.h>

#include <memory>
#include <stdexcept>
#include <string>

#include "devices.hpp"
#include "lazy.hpp"

namespace poly = cppref::polymorphism;

namespace {

struct Thing {
    int value = 7;
};

}  // namespace

// ---- Part 7a: Lazy ---------------------------------------------------------

TEST(Part7Lazy, FactoryIsNotCalledOnConstruction) {
    int calls = 0;
    poly::Lazy<Thing> lazy([&calls] {
        ++calls;
        return std::make_unique<Thing>();
    });
    EXPECT_EQ(calls, 0);
    EXPECT_FALSE(lazy.loaded());
    EXPECT_EQ(lazy.init_count(), 0U);
}

TEST(Part7Lazy, FirstAccessBuildsOnceAndIsReused) {
    int calls = 0;
    poly::Lazy<Thing> lazy([&calls] {
        ++calls;
        return std::make_unique<Thing>();
    });
    EXPECT_EQ(lazy->value, 7);
    lazy->value = 9;
    EXPECT_EQ((*lazy).value, 9);

    const poly::Lazy<Thing>& view = lazy;
    EXPECT_EQ(view->value, 9);

    EXPECT_EQ(calls, 1);
    EXPECT_EQ(lazy.init_count(), 1U);
    EXPECT_TRUE(lazy.loaded());
}

TEST(Part7Lazy, ResetDiscardsTargetAndNextAccessRebuilds) {
    int calls = 0;
    poly::Lazy<Thing> lazy([&calls] {
        ++calls;
        return std::make_unique<Thing>();
    });
    lazy->value = 1;
    lazy.reset();
    EXPECT_FALSE(lazy.loaded());
    EXPECT_EQ(lazy->value, 7);
    EXPECT_EQ(calls, 2);
    EXPECT_EQ(lazy.init_count(), 2U);
}

TEST(Part7Lazy, FactoryExceptionLeavesProxyUnloadedAndUncounted) {
    int calls = 0;
    poly::Lazy<Thing> lazy([&calls]() -> std::unique_ptr<Thing> {
        if (++calls == 1) {
            throw std::runtime_error("connect failed");
        }
        return std::make_unique<Thing>();
    });
    EXPECT_THROW((void)lazy->value, std::runtime_error);
    EXPECT_FALSE(lazy.loaded());
    EXPECT_EQ(lazy.init_count(), 0U);

    EXPECT_EQ(lazy->value, 7);
    EXPECT_EQ(lazy.init_count(), 1U);
    EXPECT_EQ(calls, 2);
}

TEST(Part7Lazy, NullFromFactoryThrowsRuntimeError) {
    poly::Lazy<Thing> lazy([]() -> std::unique_ptr<Thing> { return nullptr; });
    EXPECT_THROW((void)lazy->value, std::runtime_error);
    EXPECT_FALSE(lazy.loaded());
}

TEST(Part7Lazy, DerefAndArrowRefersToTheSameObject) {
    poly::Lazy<Thing> lazy([] { return std::make_unique<Thing>(); });
    EXPECT_EQ(&*lazy, lazy.operator->());
}

// ---- Part 7b: diamond ------------------------------------------------------

TEST(Part7Diamond, InitialStateIsEmptyAndIdIsShared) {
    poly::ReadWriter rw("disk0");
    EXPECT_EQ(rw.id(), "disk0");
    EXPECT_EQ(rw.data(), "");
    EXPECT_EQ(rw.read(), "");
}

TEST(Part7Diamond, WriteThroughWriterIsVisibleThroughReaderAndBase) {
    poly::ReadWriter rw("disk0");
    poly::Writer& w = rw;
    poly::Reader& r = rw;
    poly::Device& d = rw;
    w.write("hello");
    EXPECT_EQ(r.read(), "hello");
    EXPECT_EQ(d.data(), "hello");
    EXPECT_EQ(rw.data(), "hello");
}

TEST(Part7Diamond, LatestWriteWinsAndBasePointerDeletesDerivedObject) {
    auto rw = std::make_unique<poly::ReadWriter>("nic1");
    rw->write("a");
    rw->write("b");
    EXPECT_EQ(rw->read(), "b");

    std::unique_ptr<poly::Device> dev = std::move(rw);
    EXPECT_EQ(dev->id(), "nic1");
    EXPECT_EQ(dev->data(), "b");
}
