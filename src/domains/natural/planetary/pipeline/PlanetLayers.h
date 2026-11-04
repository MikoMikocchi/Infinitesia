#pragma once

#include <any>
#include <cstddef>
#include <map>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace Infinitesia {
// One value per cell of the planet grid, row-major by latitude band then
// longitude.
template <typename T>
class Field {
 public:
  Field(int latitudeBands, int longitudeSegments, T initial = T{})
      : m_latitudeBands(latitudeBands),
        m_longitudeSegments(longitudeSegments),
        m_values(static_cast<std::size_t>(latitudeBands) *
                     static_cast<std::size_t>(longitudeSegments),
                 initial) {}

  int LatitudeBands() const noexcept { return m_latitudeBands; }
  int LongitudeSegments() const noexcept { return m_longitudeSegments; }

  T& At(int latitude, int longitude) {
    return m_values[Index(latitude, longitude)];
  }
  const T& At(int latitude, int longitude) const {
    return m_values[Index(latitude, longitude)];
  }

 private:
  std::size_t Index(int latitude, int longitude) const {
    return static_cast<std::size_t>(latitude) *
               static_cast<std::size_t>(m_longitudeSegments) +
           static_cast<std::size_t>(longitude);
  }

  int m_latitudeBands;
  int m_longitudeSegments;
  std::vector<T> m_values;
};

// The extra data of a planet, by layer name ("biology.vegetation"): what stages
// added beyond the fixed PlanetRegion fields. A layer is created by the stage
// that provides it and read by the stages and code that need it; the type is
// fixed at creation.
class PlanetLayers {
 public:
  // Throws std::logic_error when the name is taken.
  template <typename T>
  Field<T>& Create(const std::string& name, int latitudeBands,
                   int longitudeSegments, T initial = T{}) {
    if (m_fields.contains(name)) {
      throw std::logic_error("Planet layer '" + name + "' already exists");
    }
    std::any& slot = m_fields[name];
    slot = Field<T>(latitudeBands, longitudeSegments, std::move(initial));
    return *std::any_cast<Field<T>>(&slot);
  }

  bool Contains(std::string_view name) const noexcept {
    return m_fields.find(name) != m_fields.end();
  }

  // nullptr when there is no such layer; throws std::logic_error when it holds
  // another type.
  template <typename T>
  const Field<T>* Find(std::string_view name) const {
    const auto found = m_fields.find(name);
    if (found == m_fields.end()) {
      return nullptr;
    }
    const Field<T>* field = std::any_cast<Field<T>>(&found->second);
    if (field == nullptr) {
      throw std::logic_error("Planet layer '" + std::string(name) +
                             "' has another type");
    }
    return field;
  }

  template <typename T>
  Field<T>* FindMutable(std::string_view name) {
    return const_cast<Field<T>*>(std::as_const(*this).template Find<T>(name));
  }

  // Throws std::logic_error when the layer is missing or has another type.
  template <typename T>
  const Field<T>& Require(std::string_view name) const {
    const Field<T>* field = Find<T>(name);
    if (field == nullptr) {
      throw std::logic_error("Planet layer '" + std::string(name) +
                             "' does not exist");
    }
    return *field;
  }

 private:
  std::map<std::string, std::any, std::less<>> m_fields;
};
}  // namespace Infinitesia
