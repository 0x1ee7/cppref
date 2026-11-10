// Part 7b - multiple inheritance with a virtual base (the diamond).
#pragma once

#include <string>
#include <utility>

namespace cppref::polymorphism {

/// Shared base of the diamond. Holds the id and the last payload written.
class Device {
public:
    explicit Device(std::string id) : id_(std::move(id)) {}
    virtual ~Device() = default;

    [[nodiscard]] const std::string& id() const { return id_; }

    /// Payload most recently stored through set_data(); empty initially.
    [[nodiscard]] const std::string& data() const { return data_; }

protected:
    void set_data(std::string value) { data_ = std::move(value); }

private:
    std::string id_;
    std::string data_;
};

/// Reads the shared Device state. Inherits Device virtually.
class Reader : public virtual Device {
public:
    explicit Reader(std::string id) : Device(std::move(id)) {}

    [[nodiscard]] std::string read() const { return data(); }
};

/// Writes the shared Device state. Inherits Device virtually.
class Writer : public virtual Device {
public:
    explicit Writer(std::string id) : Device(std::move(id)) {}

    void write(std::string payload) { set_data(std::move(payload)); }
};

/// Reader and Writer both inherit the same Device subobject. ReadWriter initialises
/// that single shared base directly. The Device initialisers passed through Reader
/// and Writer are ignored, because the most-derived class constructs virtual bases.
class ReadWriter final : public Reader, public Writer {
public:
    explicit ReadWriter(std::string id) : Device(id), Reader(id), Writer(id) {}
};

}  // namespace cppref::polymorphism
