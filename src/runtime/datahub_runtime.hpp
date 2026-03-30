#pragma once

#include "core/compiled_catalog.hpp"
#include "core/state_store.hpp"
#include "gt_datahub/i_datahub_runtime.hpp"
#include "runtime/yaml_loader.hpp"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace gt::datahub::runtime {

/**
 * In-memory runtime core bootstrapped from the compiled YAML configuration.
 *
 * Bootstrap remains separate from lifecycle: the object is fully validated and
 * owns catalog/store state immediately after creation, while connector runtime
 * slots and the scheduler placeholder only become active during `start()`.
 */
class DataHubRuntime final : public IDataHubRuntime {
 public:
  /**
   * Builds one runtime instance from a YAML document already available in
   * memory.
   */
  static std::expected<std::unique_ptr<DataHubRuntime>, RuntimeError>
  createFromString(std::string_view yaml_text) {
    auto config = YamlLoader::loadFromString(yaml_text);
    if (!config.has_value()) {
      return std::unexpected(RuntimeError{
          RuntimeErrorCode::InvalidConfiguration, config.error().message});
    }

    return create(std::move(*config));
  }

  /**
   * Builds one runtime instance from a YAML file path.
   */
  static std::expected<std::unique_ptr<DataHubRuntime>, RuntimeError>
  createFromFile(std::string_view yaml_path) {
    auto config = YamlLoader::loadFromFile(yaml_path);
    if (!config.has_value()) {
      return std::unexpected(RuntimeError{
          RuntimeErrorCode::InvalidConfiguration, config.error().message});
    }

    return create(std::move(*config));
  }

  DataHubRuntime(const DataHubRuntime&) = delete;
  DataHubRuntime& operator=(const DataHubRuntime&) = delete;
  DataHubRuntime(DataHubRuntime&&) = delete;
  DataHubRuntime& operator=(DataHubRuntime&&) = delete;
  ~DataHubRuntime() override = default;

  std::expected<void, RuntimeError> start() override {
    std::scoped_lock lock(m_lifecycle_mtx);

    if (m_started) {
      return std::unexpected(RuntimeError{
          RuntimeErrorCode::AlreadyStarted, "runtime is already started"});
    }

    m_connector_runtimes.clear();
    m_connector_runtimes.reserve(m_config.connectors.size());
    for (const auto& connector : m_config.connectors) {
      m_connector_runtimes.push_back(
          ConnectorRuntimeSlot{connector.id, connector.kind, connector.enabled});
    }

    m_scheduler_active = runtimeNeedsScheduler();
    m_started = true;
    m_ever_started = true;
    ++m_start_generation;
    return {};
  }

  void stop() override {
    std::scoped_lock lock(m_lifecycle_mtx);

    if (!m_started) {
      return;
    }

    m_started = false;
    m_scheduler_active = false;
    m_connector_runtimes.clear();
  }

  IDataHub& hub() noexcept override { return m_hub; }

  const IDataHub& hub() const noexcept override { return m_hub; }

  std::expected<std::unique_ptr<IInternalProducer>, OpenProducerError>
  openInternalProducer(std::string_view binding_id) override {
    std::scoped_lock lock(m_lifecycle_mtx);

    if (!m_started) {
      return std::unexpected(OpenProducerError{
          m_ever_started ? OpenProducerErrorCode::RuntimeStopped
                         : OpenProducerErrorCode::RuntimeNotStarted,
          m_ever_started ? "runtime is stopped" : "runtime is not started"});
    }

    const auto binding_index = findProducerBindingIndex(binding_id);
    if (!binding_index.has_value()) {
      return std::unexpected(OpenProducerError{
          OpenProducerErrorCode::UnknownBinding,
          "unknown producer binding: " + std::string(binding_id)});
    }

    const auto& binding = m_config.producer_bindings[*binding_index];
    if (binding.producer_kind != ProducerKind::Internal) {
      return std::unexpected(OpenProducerError{
          OpenProducerErrorCode::NotInternalProducer,
          "binding is not configured as an internal producer: " +
              std::string(binding.id)});
    }

    if (!binding.enabled) {
      return std::unexpected(OpenProducerError{
          OpenProducerErrorCode::BindingDisabled,
          "binding is disabled: " + std::string(binding.id)});
    }

    // Sprint 4.1 gates internal writers by runtime lifecycle only. The
    // dedicated `AlreadyOpen` ownership guard is introduced in sprint 4.2.
    std::unique_ptr<IInternalProducer> producer =
        std::make_unique<RuntimeInternalProducer>(
            this, *binding_index, m_start_generation, binding.id,
            binding.variable_name);
    return std::move(producer);
  }

  std::expected<void, TriggerError> triggerFileExport(
      std::string_view export_id) override {
    std::scoped_lock lock(m_lifecycle_mtx);

    if (!m_started) {
      return std::unexpected(TriggerError{
          TriggerErrorCode::RuntimeStopped, "runtime is not started"});
    }

    const FileExportConfig* export_config = findFileExport(export_id);
    if (export_config == nullptr) {
      return std::unexpected(TriggerError{
          TriggerErrorCode::UnknownExport,
          "unknown file export: " + std::string(export_id)});
    }

    if (!export_config->enabled) {
      return std::unexpected(TriggerError{
          TriggerErrorCode::ExportDisabled,
          "file export is disabled: " + export_config->id});
    }

    if (export_config->trigger.mode != "manual") {
      return std::unexpected(TriggerError{
          TriggerErrorCode::InvalidTriggerMode,
          "file export is not configured for manual trigger: " +
              export_config->id});
    }

    return {};
  }

  /**
   * Exposes whether the runtime lifecycle is currently active.
   */
  bool started() const noexcept {
    std::scoped_lock lock(m_lifecycle_mtx);
    return m_started;
  }

  /**
   * Returns how many connector runtime placeholders are currently active.
   */
  std::size_t activeConnectorRuntimeCount() const noexcept {
    std::scoped_lock lock(m_lifecycle_mtx);
    return m_connector_runtimes.size();
  }

  /**
   * Returns whether the scheduler placeholder is active in the current start
   * generation.
   */
  bool schedulerActive() const noexcept {
    std::scoped_lock lock(m_lifecycle_mtx);
    return m_scheduler_active;
  }

 private:
  struct ConnectorRuntimeSlot {
    std::string connector_id;
    std::string kind;
    bool enabled{true};
  };

  class RuntimeHub final : public IDataHub {
   public:
    explicit RuntimeHub(const core::StateStore* store) : m_store(store) {}

    std::optional<VariableState> getState(
        std::string_view variable_name) const override {
      return m_store->getState(variable_name);
    }

    std::optional<VariableDefinition> getDefinition(
        std::string_view variable_name) const override {
      return m_store->getDefinition(variable_name);
    }

    std::vector<std::string> listVariables() const override {
      return m_store->listVariables();
    }

    std::expected<std::string, ResolveError> resolveText(
        std::string_view) const override {
      return std::unexpected(ResolveError{
          ResolveErrorCode::InvalidContext,
          "resolveText is introduced in phase 5 after the runtime core"});
    }

   private:
    const core::StateStore* m_store;
  };

  class RuntimeInternalProducer final : public IInternalProducer {
   public:
    RuntimeInternalProducer(DataHubRuntime* runtime, std::size_t binding_index,
                            std::uint64_t generation, std::string binding_id,
                            std::string variable_name)
        : m_runtime(runtime),
          m_binding_index(binding_index),
          m_generation(generation),
          m_binding_id(std::move(binding_id)),
          m_variable_name(std::move(variable_name)) {}

    std::string_view bindingId() const noexcept override { return m_binding_id; }

    std::string_view variableName() const noexcept override {
      return m_variable_name;
    }

    std::expected<void, SubmitError> submit(UpdateRequest req) override {
      return m_runtime->submitFromInternalProducer(m_binding_index, m_generation,
                                                  std::move(req));
    }

   private:
    DataHubRuntime* m_runtime;
    std::size_t m_binding_index;
    std::uint64_t m_generation;
    std::string m_binding_id;
    std::string m_variable_name;
  };

  DataHubRuntime(LoadedDatahubConfig config, core::StateStore store)
      : m_config(std::move(config)),
        m_store(std::move(store)),
        m_hub(&m_store) {}

  static std::expected<std::unique_ptr<DataHubRuntime>, RuntimeError> create(
      LoadedDatahubConfig config) {
    std::vector<VariableDefinition> definitions = config.variables;
    auto catalog = core::CompiledCatalog::build(std::move(definitions));
    if (!catalog.has_value()) {
      return std::unexpected(RuntimeError{
          RuntimeErrorCode::InvalidConfiguration, catalog.error().message});
    }

    auto store = core::StateStore::bootstrap(std::move(*catalog));
    return std::unique_ptr<DataHubRuntime>(
        new DataHubRuntime(std::move(config), std::move(store)));
  }

  std::expected<void, SubmitError> submitFromInternalProducer(
      std::size_t binding_index, std::uint64_t generation, UpdateRequest req) {
    const ProducerBindingConfig* binding = nullptr;
    {
      std::scoped_lock lock(m_lifecycle_mtx);

      if (!m_started || generation != m_start_generation) {
        return std::unexpected(SubmitError{
            SubmitErrorCode::RuntimeStopped,
            "runtime generation is no longer active"});
      }

      if (binding_index >= m_config.producer_bindings.size()) {
        return std::unexpected(SubmitError{
            SubmitErrorCode::OwnershipViolation,
            "internal producer binding index is invalid"});
      }

      binding = &m_config.producer_bindings[binding_index];
      if (!binding->enabled) {
        return std::unexpected(SubmitError{
            SubmitErrorCode::BindingDisabled,
            "binding is disabled: " + binding->id});
      }

      if (binding->producer_kind != ProducerKind::Internal) {
        return std::unexpected(SubmitError{
            SubmitErrorCode::OwnershipViolation,
            "binding is not configured as an internal producer: " +
                binding->id});
      }
    }

    const auto* compiled_definition = m_store.catalog().findByName(
        binding->variable_name);
    auto* entry = m_store.findEntryByName(binding->variable_name);
    if (compiled_definition == nullptr || entry == nullptr) {
      return std::unexpected(SubmitError{
          SubmitErrorCode::OwnershipViolation,
          "binding references an unknown runtime variable: " +
              binding->variable_name});
    }

    if (!isValueCompatible(req.value,
                           compiled_definition->public_definition.data_type)) {
      return std::unexpected(SubmitError{
          SubmitErrorCode::InvalidType,
          "update value is incompatible with variable data_type"});
    }

    std::unique_lock<std::shared_mutex> entry_lock(entry->mtx);
    entry->value = std::move(req.value);
    entry->raw_quality = req.quality;
    entry->source_timestamp = std::move(req.source_timestamp);
    entry->hub_timestamp = std::chrono::system_clock::now();
    ++entry->version;
    entry->initialized = true;
    entry->last_update_steady = std::chrono::steady_clock::now();
    return {};
  }

  std::optional<std::size_t> findProducerBindingIndex(
      std::string_view binding_id) const noexcept {
    for (std::size_t i = 0; i < m_config.producer_bindings.size(); ++i) {
      if (m_config.producer_bindings[i].id == binding_id) {
        return i;
      }
    }

    return std::nullopt;
  }

  const FileExportConfig* findFileExport(
      std::string_view export_id) const noexcept {
    for (const auto& export_config : m_config.file_exports) {
      if (export_config.id == export_id) {
        return &export_config;
      }
    }

    return nullptr;
  }

  bool runtimeNeedsScheduler() const noexcept {
    for (const auto& binding : m_config.producer_bindings) {
      if (binding.acquisition.has_value() &&
          binding.acquisition->mode == "polling") {
        return true;
      }
    }

    for (const auto& export_config : m_config.file_exports) {
      if (export_config.trigger.mode == "periodic") {
        return true;
      }
    }

    return false;
  }

  static bool isValueCompatible(const Value& value,
                                DataType data_type) noexcept {
    switch (data_type) {
      case DataType::Bool:
        return std::holds_alternative<bool>(value);
      case DataType::Int32:
        return std::holds_alternative<std::int32_t>(value);
      case DataType::UInt32:
        return std::holds_alternative<std::uint32_t>(value);
      case DataType::Int64:
        return std::holds_alternative<std::int64_t>(value);
      case DataType::UInt64:
        return std::holds_alternative<std::uint64_t>(value);
      case DataType::Float:
        return std::holds_alternative<float>(value);
      case DataType::Double:
        return std::holds_alternative<double>(value);
      case DataType::String:
        return std::holds_alternative<std::string>(value);
      case DataType::DateTime:
        return std::holds_alternative<Timestamp>(value);
    }

    return false;
  }

  mutable std::mutex m_lifecycle_mtx;
  LoadedDatahubConfig m_config;
  core::StateStore m_store;
  RuntimeHub m_hub;
  bool m_started{false};
  bool m_ever_started{false};
  std::uint64_t m_start_generation{0};
  std::vector<ConnectorRuntimeSlot> m_connector_runtimes;
  bool m_scheduler_active{false};
};

}  // namespace gt::datahub::runtime
