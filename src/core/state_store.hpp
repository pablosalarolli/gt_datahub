#pragma once

#include "core/compiled_catalog.hpp"
#include "gt_datahub/variable_state.hpp"

#include <chrono>
#include <cstdint>
#include <optional>
#include <shared_mutex>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace gt::datahub::core {

/**
 * Mutable runtime state for one variable, protected by a per-variable lock.
 *
 * The store keeps `raw_quality` internally; the public snapshot returned by
 * `getState()` is derived from this entry and will later incorporate stale lazy
 * evaluation on top of `last_update_steady`.
 */
struct VariableStateEntry {
  mutable std::shared_mutex mtx;

  Value value;
  Quality raw_quality{Quality::Uncertain};
  std::optional<Timestamp> source_timestamp;
  std::optional<Timestamp> hub_timestamp;
  std::uint64_t version{0};
  bool initialized{false};

  std::optional<std::chrono::steady_clock::time_point> last_update_steady;

  VariableStateEntry() = default;
  VariableStateEntry(const VariableStateEntry&) = delete;
  VariableStateEntry& operator=(const VariableStateEntry&) = delete;

  VariableStateEntry(VariableStateEntry&& other) noexcept
      : value(std::move(other.value)),
        raw_quality(other.raw_quality),
        source_timestamp(std::move(other.source_timestamp)),
        hub_timestamp(std::move(other.hub_timestamp)),
        version(other.version),
        initialized(other.initialized),
        last_update_steady(std::move(other.last_update_steady)) {}

  VariableStateEntry& operator=(VariableStateEntry&& other) noexcept {
    if (this == &other) {
      return *this;
    }

    value = std::move(other.value);
    raw_quality = other.raw_quality;
    source_timestamp = std::move(other.source_timestamp);
    hub_timestamp = std::move(other.hub_timestamp);
    version = other.version;
    initialized = other.initialized;
    last_update_steady = std::move(other.last_update_steady);
    return *this;
  }
};

/**
 * Bootstrapped immutable catalog plus mutable state entries.
 *
 * The catalog topology is frozen after bootstrap. Lookups use the compiled
 * catalog index and then touch only the corresponding state entry.
 */
class StateStore {
 public:
  /**
   * Materializes a state entry for each compiled variable definition.
   *
   * `default_value` is applied directly during bootstrap instead of going
   * through `submit()`, which keeps bootstrap independent from runtime write
   * ownership while still honoring the functional initialization semantics.
   */
  static StateStore bootstrap(CompiledCatalog catalog) {
    std::vector<VariableStateEntry> entries;
    entries.reserve(catalog.size());

    const auto bootstrap_now = std::chrono::steady_clock::now();

    for (const auto& definition : catalog.definitions()) {
      VariableStateEntry entry;

      if (definition.public_definition.default_value.has_value()) {
        entry.value = *definition.public_definition.default_value;
        entry.initialized = true;
        entry.last_update_steady = bootstrap_now;
      }

      entries.push_back(std::move(entry));
    }

    return StateStore(std::move(catalog), std::move(entries));
  }

  /**
   * Returns the public state snapshot for `variable_name`, when present.
   */
  std::optional<VariableState> getState(
      std::string_view variable_name) const noexcept {
    const auto index = m_catalog.findIndexByName(variable_name);
    if (!index.has_value()) {
      return std::nullopt;
    }

    const VariableStateEntry& entry = m_entries[*index];
    std::shared_lock lock(entry.mtx);

    VariableState state;
    state.value = entry.value;
    state.quality = entry.raw_quality;
    state.source_timestamp = entry.source_timestamp;
    state.hub_timestamp = entry.hub_timestamp;
    state.version = entry.version;
    state.initialized = entry.initialized;
    return state;
  }

  /**
   * Returns the public definition for `variable_name`, when present.
   */
  std::optional<VariableDefinition> getDefinition(
      std::string_view variable_name) const {
    const auto* definition = m_catalog.findByName(variable_name);
    if (definition == nullptr) {
      return std::nullopt;
    }

    return definition->public_definition;
  }

  /**
   * Returns the stable list of variables registered during bootstrap.
   */
  std::vector<std::string> listVariables() const {
    std::vector<std::string> variables;
    variables.reserve(m_catalog.size());

    for (const auto& definition : m_catalog.definitions()) {
      variables.push_back(definition.public_definition.name);
    }

    return variables;
  }

  /**
   * Returns the number of state entries created during bootstrap.
   */
  std::size_t size() const noexcept { return m_entries.size(); }

  /**
   * Returns the compiled catalog owned by this store.
   */
  const CompiledCatalog& catalog() const noexcept { return m_catalog; }

  /**
   * Returns an internal entry by name for runtime code and internal tests.
   */
  const VariableStateEntry* findEntryByName(
      std::string_view variable_name) const noexcept {
    const auto index = m_catalog.findIndexByName(variable_name);
    if (!index.has_value()) {
      return nullptr;
    }

    return &m_entries[*index];
  }

 private:
  StateStore(CompiledCatalog catalog, std::vector<VariableStateEntry> entries)
      : m_catalog(std::move(catalog)), m_entries(std::move(entries)) {}

  CompiledCatalog m_catalog;
  std::vector<VariableStateEntry> m_entries;
};

}  // namespace gt::datahub::core
