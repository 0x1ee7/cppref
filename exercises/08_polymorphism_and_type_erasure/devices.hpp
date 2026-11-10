// Part 7b - multiple inheritance with a virtual base (the diamond).
#pragma once

#include <stdexcept>
#include <string>

namespace cppref::polymorphism {

/// Shared base of the diamond. Holds the id and the last payload written.
///
/// Engineer: add data members (id, data).
class Device {
public:
    explicit Device(std::string /*id*/) { throw std::logic_error("not implemented: Device::Device"); }
    virtual ~Device() = default;

    [[nodiscard]] const std::string& id() const { throw std::logic_error("not implemented: Device::id"); }

    /// Payload most recently stored through set_data(); empty initially.
    [[nodiscard]] const std::string& data() const { throw std::logic_error("not implemented: Device::data"); }

protected:
    void set_data(std::string /*value*/) { throw std::logic_error("not implemented: Device::set_data"); }
};

/// Reads the shared Device state. Inherits Device virtually.
class Reader : public virtual Device {
public:
    explicit Reader(std::string id) : Device(std::move(id)) {}

    [[nodiscard]] std::string read() const { throw std::logic_error("not implemented: Reader::read"); }
};

/// Writes the shared Device state. Inherits Device virtually.
class Writer : public virtual Device {
public:
    explicit Writer(std::string id) : Device(std::move(id)) {}

    void write(std::string /*payload*/) { throw std::logic_error("not implemented: Writer::write"); }
};

/// Reader and Writer both inherit the same Device subobject. ReadWriter initialises
/// that single shared base directly. The Device initialisers passed through Reader
/// and Writer are ignored, because the most-derived class constructs virtual bases.
class ReadWriter final : public Reader, public Writer {
public:
    explicit ReadWriter(std::string id) : Device(id), Reader(id), Writer(id) {}
};

}  // namespace cppref::polymorphism
