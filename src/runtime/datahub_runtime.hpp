#pragma once

#include "core/compiled_catalog.hpp"
#include "core/state_store.hpp"
#include "core/text_resolver.hpp"
#include "gt_datahub/i_datahub_runtime.hpp"
#include "runtime/i_runtime_hub_access.hpp"
#include "runtime/on_change_dispatcher.hpp"
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
class DataHubRuntime final : public IDataHubRuntime, public IRuntimeHubAccess {
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
  ~DataHubRuntime() override { stop(); }

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
    std::scoped_lock lock(m_lifecycle_mtx, m_internal_producer_open_state->mtx);

    if (!m_started) {
      return;
    }

    m_started = false;
    m_scheduler_active = false;
    m_connector_runtimes.clear();
    m_on_change_dispatcher.clear();
    resetFileExportStatesOnStop();
    for (auto& binding_generation :
         m_internal_producer_open_state->open_generations) {
      binding_generation.reset();
    }
  }

  IDataHub& hub() noexcept override { return m_hub; }

  const IDataHub& hub() const noexcept override { return m_hub; }

  std::expected<std::unique_ptr<IInternalProducer>, OpenProducerError>
  openInternalProducer(std::string_view binding_id) override {
    std::scoped_lock lock(m_lifecycle_mtx, m_internal_producer_open_state->mtx);

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

    auto& open_generation =
        m_internal_producer_open_state->open_generations[*binding_index];
    if (open_generation.has_value()) {
      return std::unexpected(OpenProducerError{
          OpenProducerErrorCode::AlreadyOpen,
          "binding is already open: " + std::string(binding.id)});
    }

    std::unique_ptr<IInternalProducer> producer =
        std::make_unique<RuntimeInternalProducer>(
            this, m_internal_producer_open_state, m_producer_tokens[*binding_index],
            m_start_generation, binding.id, binding.variable_name);
    open_generation = m_start_generation;
    return std::move(producer);
  }

  std::expected<void, TriggerError> triggerFileExport(
      std::string_view export_id) override {
    std::scoped_lock lock(m_lifecycle_mtx);

    if (!m_started) {
      return std::unexpected(TriggerError{
          TriggerErrorCode::RuntimeStopped, "runtime is not started"});
    }

    const auto export_index = findFileExportIndex(export_id);
    if (!export_index.has_value()) {
      return std::unexpected(TriggerError{
          TriggerErrorCode::UnknownExport,
          "unknown file export: " + std::string(export_id)});
    }

    const auto& export_config = m_config.file_exports[*export_index];
    if (!export_config.enabled) {
      return std::unexpected(TriggerError{
          TriggerErrorCode::ExportDisabled,
          "file export is disabled: " + export_config.id});
    }

    if (export_config.trigger.mode != "manual") {
      return std::unexpected(TriggerError{
          TriggerErrorCode::InvalidTriggerMode,
          "file export is not configured for manual trigger: " +
              export_config.id});
    }

    return triggerManualFileExportLocked(*export_index,
                                         std::chrono::system_clock::now());
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

  /**
   * Returns pending `on_change` notifications for one consumer binding.
   *
   * Internal test hook until real sink workers are introduced.
   */
  std::size_t pendingOnChangeNotificationCount(
      std::string_view binding_id) const noexcept {
    return m_on_change_dispatcher.pendingCount(binding_id);
  }

  /**
   * Returns the total amount of pending `on_change` work.
   *
   * Internal test hook until sink adapters own their own queues/workers.
   */
  std::size_t totalPendingOnChangeNotificationCount() const noexcept {
    return m_on_change_dispatcher.totalPendingCount();
  }

  /**
   * Drains one consumer queue for internal tests.
   */
  std::vector<OnChangeNotification> drainOnChangeNotificationsForTesting(
      std::string_view binding_id) {
    return m_on_change_dispatcher.drainAll(binding_id);
  }

  /**
   * Executes one synthetic periodic scheduler tick for internal tests.
   */
  void runPeriodicExportTickForTesting() {
    std::scoped_lock lock(m_lifecycle_mtx);
    if (!m_started) {
      return;
    }

    runPeriodicExportsLocked(std::chrono::system_clock::now());
  }

  /**
   * Returns how many export attempts were accepted for one configured export.
   *
   * Internal test hook until real file sinks own the write path.
   */
  std::size_t acceptedFileExportAttemptCount(
      std::string_view export_id) const noexcept {
    std::scoped_lock lock(m_lifecycle_mtx);
    const auto export_index = findFileExportIndex(export_id);
    if (!export_index.has_value() ||
        *export_index >= m_file_export_states.size()) {
      return 0;
    }

    return m_file_export_states[*export_index].accepted_attempt_count;
  }

  /**
   * Returns whether one export currently has an open logical session.
   *
   * Internal test hook until real file sinks materialize session handles.
   */
  bool fileExportSessionOpen(std::string_view export_id) const noexcept {
    std::scoped_lock lock(m_lifecycle_mtx);
    const auto export_index = findFileExportIndex(export_id);
    if (!export_index.has_value() ||
        *export_index >= m_file_export_states.size()) {
      return false;
    }

    return m_file_export_states[*export_index].session_open;
  }

  /**
   * Returns the current logical export session id, when one is open.
   */
  std::optional<std::uint64_t> currentFileExportSessionId(
      std::string_view export_id) const noexcept {
    std::scoped_lock lock(m_lifecycle_mtx);
    const auto export_index = findFileExportIndex(export_id);
    if (!export_index.has_value() ||
        *export_index >= m_file_export_states.size()) {
      return std::nullopt;
    }

    return m_file_export_states[*export_index].current_session_id;
  }

  std::expected<void, SubmitError> submitFromProducer(
      ProducerToken token, RuntimeUpdateRequest req) override {
    std::scoped_lock lock(m_lifecycle_mtx);

    auto binding_result = findBindingForTokenLocked(token);
    if (!binding_result.has_value()) {
      return std::unexpected(binding_result.error());
    }

    const auto* compiled_definition =
        m_store.catalog().findByIndex(token.variable_index);
    auto* entry = m_store.findEntryByIndex(token.variable_index);
    if (compiled_definition == nullptr || entry == nullptr) {
      return std::unexpected(SubmitError{
          SubmitErrorCode::OwnershipViolation,
          "producer token references an unknown runtime variable"});
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
    const auto accepted_version = entry->version;
    entry_lock.unlock();

    dispatchAcceptedUpdate(token.variable_index, accepted_version);
    return {};
  }

  void markProducerConnectionBad(ProducerToken token,
                                 std::string_view reason) override {
    (void)reason;

    std::scoped_lock lock(m_lifecycle_mtx);
    auto binding_result = findBindingForTokenLocked(token);
    if (!binding_result.has_value()) {
      return;
    }

    auto* entry = m_store.findEntryByIndex(token.variable_index);
    if (entry == nullptr) {
      return;
    }

    std::unique_lock<std::shared_mutex> entry_lock(entry->mtx);
    entry->raw_quality = Quality::Bad;
    entry->hub_timestamp = std::chrono::system_clock::now();
    ++entry->version;
    entry->last_update_steady = std::chrono::steady_clock::now();
    const auto accepted_version = entry->version;
    entry_lock.unlock();

    dispatchAcceptedUpdate(token.variable_index, accepted_version);
  }

 private:
  struct ConnectorRuntimeSlot {
    std::string connector_id;
    std::string kind;
    bool enabled{true};
  };

  struct InternalProducerOpenState {
    // Open/close of internal producer handles is serialized here so
    // `AlreadyOpen` and handle destruction cannot race each other.
    std::mutex mtx;
    std::vector<std::optional<std::uint64_t>> open_generations;
  };

  struct FileExportRuntimeState {
    bool last_activation_result{false};
    bool session_open{false};
    std::uint64_t issued_session_count{0};
    std::optional<std::uint64_t> current_session_id;
    std::optional<std::string> current_target_path;
    std::optional<Timestamp> session_started_at;
    std::uint64_t accepted_attempt_count{0};
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
        std::string_view expression) const override {
      core::TextResolveContext context;
      return core::TextResolver::resolveExpression(
          expression, *this, core::SelectorContext::PublicResolve,
          std::move(context));
    }

   private:
    const core::StateStore* m_store;
  };

  class RuntimeInternalProducer final : public IInternalProducer {
   public:
    RuntimeInternalProducer(
        DataHubRuntime* runtime,
        std::shared_ptr<InternalProducerOpenState> open_state, ProducerToken token,
        std::uint64_t generation, std::string binding_id,
        std::string variable_name)
        : m_runtime(runtime),
          m_open_state(std::move(open_state)),
          m_token(token),
          m_generation(generation),
          m_binding_id(std::move(binding_id)),
          m_variable_name(std::move(variable_name)) {}

    ~RuntimeInternalProducer() override {
      releaseInternalProducer(m_open_state, m_token.binding_index, m_generation);
    }

    std::string_view bindingId() const noexcept override { return m_binding_id; }

    std::string_view variableName() const noexcept override {
      return m_variable_name;
    }

    std::expected<void, SubmitError> submit(UpdateRequest req) override {
      RuntimeUpdateRequest runtime_request;
      runtime_request.value = std::move(req.value);
      runtime_request.quality = req.quality;
      runtime_request.source_timestamp = std::move(req.source_timestamp);
      return m_runtime->submitFromInternalProducerHandle(
          m_generation, m_token, std::move(runtime_request));
    }

   private:
    DataHubRuntime* m_runtime;
    std::shared_ptr<InternalProducerOpenState> m_open_state;
    ProducerToken m_token;
    std::uint64_t m_generation;
    std::string m_binding_id;
    std::string m_variable_name;
  };

  DataHubRuntime(LoadedDatahubConfig config, core::StateStore store,
                 std::vector<ProducerToken> producer_tokens,
                 std::shared_ptr<InternalProducerOpenState> open_state,
                 OnChangeDispatcher on_change_dispatcher)
      : m_config(std::move(config)),
        m_store(std::move(store)),
        m_hub(&m_store),
        m_producer_tokens(std::move(producer_tokens)),
        m_internal_producer_open_state(std::move(open_state)),
        m_on_change_dispatcher(std::move(on_change_dispatcher)) {
    m_file_export_states.resize(m_config.file_exports.size());
  }

  static std::expected<std::unique_ptr<DataHubRuntime>, RuntimeError> create(
      LoadedDatahubConfig config) {
    std::vector<VariableDefinition> definitions = config.variables;
    auto catalog = core::CompiledCatalog::build(std::move(definitions));
    if (!catalog.has_value()) {
      return std::unexpected(RuntimeError{
          RuntimeErrorCode::InvalidConfiguration, catalog.error().message});
    }

    auto store = core::StateStore::bootstrap(std::move(*catalog));
    auto producer_tokens = buildProducerTokens(config, store);
    if (!producer_tokens.has_value()) {
      return std::unexpected(producer_tokens.error());
    }

    auto open_state = std::make_shared<InternalProducerOpenState>();
    open_state->open_generations.resize(config.producer_bindings.size());

    auto on_change_dispatcher = OnChangeDispatcher::build(config, store);
    if (!on_change_dispatcher.has_value()) {
      return std::unexpected(on_change_dispatcher.error());
    }

    return std::unique_ptr<DataHubRuntime>(
        new DataHubRuntime(std::move(config), std::move(store),
                           std::move(*producer_tokens), std::move(open_state),
                           std::move(*on_change_dispatcher)));
  }

  void dispatchAcceptedUpdate(std::size_t variable_index,
                              std::uint64_t accepted_version) noexcept {
    // The hub stays passive; `on_change` fan-out happens only on the internal
    // path of an already accepted runtime update.
    m_on_change_dispatcher.enqueueAcceptedUpdate(variable_index,
                                                 accepted_version);
  }

  static std::expected<std::vector<ProducerToken>, RuntimeError>
  buildProducerTokens(const LoadedDatahubConfig& config,
                      const core::StateStore& store) {
    std::vector<ProducerToken> tokens;
    tokens.reserve(config.producer_bindings.size());

    for (std::size_t i = 0; i < config.producer_bindings.size(); ++i) {
      const auto& binding = config.producer_bindings[i];
      const auto variable_index =
          store.catalog().findIndexByName(binding.variable_name);
      if (!variable_index.has_value()) {
        return std::unexpected(RuntimeError{
            RuntimeErrorCode::BootstrapFailed,
            "producer binding references an unknown compiled variable: " +
                binding.variable_name});
      }

      tokens.push_back(ProducerToken{i, *variable_index});
    }

    return tokens;
  }

  std::expected<void, SubmitError> submitFromInternalProducerHandle(
      std::uint64_t generation, ProducerToken token, RuntimeUpdateRequest req) {
    {
      std::scoped_lock lock(m_lifecycle_mtx);

      if (!m_started || generation != m_start_generation) {
        return std::unexpected(SubmitError{
            SubmitErrorCode::RuntimeStopped,
            "runtime generation is no longer active"});
      }
    }

    if (token.binding_index >= m_config.producer_bindings.size()) {
      return std::unexpected(SubmitError{
          SubmitErrorCode::OwnershipViolation,
          "internal producer binding index is invalid"});
    }

    const auto& binding = m_config.producer_bindings[token.binding_index];
    if (binding.producer_kind != ProducerKind::Internal) {
      return std::unexpected(SubmitError{
          SubmitErrorCode::OwnershipViolation,
          "binding is not configured as an internal producer: " + binding.id});
    }

    return submitFromProducer(token, std::move(req));
  }

  static void releaseInternalProducer(
      const std::shared_ptr<InternalProducerOpenState>& open_state,
      std::size_t binding_index, std::uint64_t generation) noexcept {
    if (open_state == nullptr) {
      return;
    }

    std::scoped_lock lock(open_state->mtx);
    if (binding_index >= open_state->open_generations.size()) {
      return;
    }

    auto& open_generation = open_state->open_generations[binding_index];
    if (open_generation == generation) {
      open_generation.reset();
    }
  }

  std::expected<const ProducerBindingConfig*, SubmitError> findBindingForTokenLocked(
      ProducerToken token) const {
    if (!m_started) {
      return std::unexpected(SubmitError{
          SubmitErrorCode::RuntimeStopped, "runtime is not started"});
    }

    if (token.binding_index >= m_config.producer_bindings.size() ||
        token.binding_index >= m_producer_tokens.size()) {
      return std::unexpected(SubmitError{
          SubmitErrorCode::OwnershipViolation,
          "producer token references an invalid binding index"});
    }

    const auto& expected_token = m_producer_tokens[token.binding_index];
    if (expected_token.variable_index != token.variable_index) {
      return std::unexpected(SubmitError{
          SubmitErrorCode::OwnershipViolation,
          "producer token does not own the configured variable"});
    }

    const auto& binding = m_config.producer_bindings[token.binding_index];
    if (!binding.enabled) {
      return std::unexpected(SubmitError{
          SubmitErrorCode::BindingDisabled,
          "binding is disabled: " + binding.id});
    }

    return &binding;
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

  std::optional<std::size_t> findFileExportIndex(
      std::string_view export_id) const noexcept {
    for (std::size_t i = 0; i < m_config.file_exports.size(); ++i) {
      if (m_config.file_exports[i].id == export_id) {
        return i;
      }
    }

    return std::nullopt;
  }

  std::expected<void, TriggerError> triggerManualFileExportLocked(
      std::size_t export_index, Timestamp now) {
    auto& export_state = m_file_export_states[export_index];
    const auto& export_config = m_config.file_exports[export_index];

    if (!evaluateExportActivationLocked(export_config, export_state, "manual",
                                        now)) {
      return std::unexpected(TriggerError{
          TriggerErrorCode::ActivationInactive,
          "file export activation is inactive: " + export_config.id});
    }

    ensureFileExportSessionOpenLocked(export_config, export_state, "manual",
                                      now);
    ++export_state.accepted_attempt_count;
    return {};
  }

  void runPeriodicExportsLocked(Timestamp now) {
    for (std::size_t i = 0; i < m_config.file_exports.size(); ++i) {
      const auto& export_config = m_config.file_exports[i];
      if (!export_config.enabled || export_config.trigger.mode != "periodic") {
        continue;
      }

      auto& export_state = m_file_export_states[i];
      if (!evaluateExportActivationLocked(export_config, export_state,
                                          "periodic", now)) {
        continue;
      }

      ensureFileExportSessionOpenLocked(export_config, export_state, "periodic",
                                        now);
      ++export_state.accepted_attempt_count;
    }
  }

  bool evaluateExportActivationLocked(const FileExportConfig& export_config,
                                      FileExportRuntimeState& export_state,
                                      std::string_view trigger_mode,
                                      Timestamp now) {
    core::EvalContext context{m_hub};
    context.export_id = export_config.id;
    context.export_session_id = export_state.current_session_id;
    context.trigger_mode = std::string(trigger_mode);
    context.target_path = export_state.current_target_path;
    context.session_started_at = export_state.session_started_at;
    context.export_captured_at = now;
    context.system_now = now;

    const bool active = export_config.compiled_activation == nullptr
                            ? true
                            : export_config.compiled_activation->evaluate(context);

    if (!active && export_state.last_activation_result &&
        export_config.finalize_on_stop) {
      closeFileExportSessionLocked(export_state);
    }

    export_state.last_activation_result = active;
    return active;
  }

  void ensureFileExportSessionOpenLocked(const FileExportConfig& export_config,
                                         FileExportRuntimeState& export_state,
                                         std::string_view trigger_mode,
                                         Timestamp now) {
    if (export_state.session_open) {
      return;
    }

    ++export_state.issued_session_count;
    export_state.current_session_id = export_state.issued_session_count;
    export_state.session_started_at = now;

    core::TextResolveContext context;
    context.export_id = export_config.id;
    context.export_session_id = export_state.current_session_id;
    context.row_index = std::uint64_t{0};
    context.trigger_mode = std::string(trigger_mode);
    context.session_started_at = now;
    context.export_captured_at = now;
    context.system_now = now;

    // The template is already structurally validated at bootstrap. At runtime
    // missing values interpolate to empty and should not abort session opening.
    const auto target_path = core::TextResolver::resolveTargetTemplate(
        export_config.compiled_target_template, m_hub, context);
    export_state.current_target_path =
        target_path.has_value() ? std::optional<std::string>{std::move(*target_path)}
                                : std::optional<std::string>{std::string{}};
    export_state.session_open = true;
  }

  static void closeFileExportSessionLocked(
      FileExportRuntimeState& export_state) noexcept {
    export_state.session_open = false;
    export_state.current_session_id.reset();
    export_state.current_target_path.reset();
    export_state.session_started_at.reset();
  }

  void resetFileExportStatesOnStop() noexcept {
    for (auto& export_state : m_file_export_states) {
      export_state.last_activation_result = false;
      export_state.session_open = false;
      export_state.current_session_id.reset();
      export_state.current_target_path.reset();
      export_state.session_started_at.reset();
      export_state.accepted_attempt_count = 0;
    }
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
  std::vector<ProducerToken> m_producer_tokens;
  std::shared_ptr<InternalProducerOpenState> m_internal_producer_open_state;
  OnChangeDispatcher m_on_change_dispatcher;
  std::vector<FileExportRuntimeState> m_file_export_states;
  bool m_started{false};
  bool m_ever_started{false};
  std::uint64_t m_start_generation{0};
  std::vector<ConnectorRuntimeSlot> m_connector_runtimes;
  bool m_scheduler_active{false};
};

}  // namespace gt::datahub::runtime
