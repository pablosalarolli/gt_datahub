#pragma once

#include "gt_datahub/variable_definition.hpp"

#include <cstddef>
#include <expected>
#include <functional>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace gt::datahub::core {

/**
 * Compiled immutable definition used by the runtime after bootstrap.
 *
 * The compiled form keeps the public definition intact and assigns a stable
 * `variable_index` that can later back vector-based state storage.
 */
struct CompiledVariableDefinition {
  VariableDefinition public_definition;
  std::size_t variable_index;
};

/**
 * Internal error returned when the compiled catalog cannot be materialized.
 */
struct CompiledCatalogBuildError {
  std::string message;
};

/**
 * Transparent hasher for heterogeneous lookup by `std::string_view`.
 */
struct TransparentStringHash {
  using is_transparent = void;

  std::size_t operator()(std::string_view value) const noexcept {
    return std::hash<std::string_view>{}(value);
  }

  std::size_t operator()(const std::string& value) const noexcept {
    return operator()(std::string_view{value});
  }
};

/**
 * Immutable catalog of compiled variable definitions.
 *
 * The catalog is materialized once during bootstrap and only exposes read-only
 * operations afterwards. Name lookup is backed by a transparent hash map so
 * runtime code can search by `std::string_view` without forcing a temporary
 * `std::string` key.
 */
class CompiledCatalog {
 public:
  using NameIndex =
      std::unordered_map<std::string, std::size_t, TransparentStringHash,
                         std::equal_to<>>;

  /**
   * Builds an immutable compiled catalog from public variable definitions.
   */
  static std::expected<CompiledCatalog, CompiledCatalogBuildError> build(
      std::vector<VariableDefinition> definitions) {
    std::vector<CompiledVariableDefinition> compiled_definitions;
    compiled_definitions.reserve(definitions.size());

    NameIndex name_index;
    name_index.reserve(definitions.size());

    for (auto& definition : definitions) {
      const std::size_t variable_index = compiled_definitions.size();
      auto [it, inserted] = name_index.emplace(definition.name, variable_index);
      if (!inserted) {
        return std::unexpected(CompiledCatalogBuildError{
            "duplicate variable name in compiled catalog: " + definition.name});
      }

      compiled_definitions.push_back(
          CompiledVariableDefinition{std::move(definition), variable_index});
    }

    return CompiledCatalog(std::move(compiled_definitions),
                           std::move(name_index));
  }

  /**
   * Returns the compiled definition for `variable_name`, when present.
   */
  const CompiledVariableDefinition* findByName(
      std::string_view variable_name) const noexcept {
    const auto it = m_name_index.find(variable_name);
    if (it == m_name_index.end()) {
      return nullptr;
    }

    return &m_definitions[it->second];
  }

  /**
   * Returns the stable index for `variable_name`, when present.
   */
  std::optional<std::size_t> findIndexByName(
      std::string_view variable_name) const noexcept {
    const auto it = m_name_index.find(variable_name);
    if (it == m_name_index.end()) {
      return std::nullopt;
    }

    return it->second;
  }

  /**
   * Returns the compiled definition stored at `variable_index`, when valid.
   */
  const CompiledVariableDefinition* findByIndex(
      std::size_t variable_index) const noexcept {
    if (variable_index >= m_definitions.size()) {
      return nullptr;
    }

    return &m_definitions[variable_index];
  }

  /**
   * Exposes the immutable sequence of compiled definitions.
   */
  std::span<const CompiledVariableDefinition> definitions() const noexcept {
    return m_definitions;
  }

  /**
   * Exposes the immutable name index for internal runtime code and tests.
   */
  const NameIndex& nameIndex() const noexcept { return m_name_index; }

  /**
   * Returns the number of compiled definitions in the catalog.
   */
  std::size_t size() const noexcept { return m_definitions.size(); }

 private:
  CompiledCatalog(std::vector<CompiledVariableDefinition> definitions,
                  NameIndex name_index)
      : m_definitions(std::move(definitions)),
        m_name_index(std::move(name_index)) {}

  std::vector<CompiledVariableDefinition> m_definitions;
  NameIndex m_name_index;
};

}  // namespace gt::datahub::core
