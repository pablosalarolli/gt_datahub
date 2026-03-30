#pragma once

#include "core/state_store.hpp"
#include "gt_datahub/errors.hpp"
#include "gt_concurrency/queue/factory.hpp"
#include "runtime/yaml_loader.hpp"

#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace gt::datahub::runtime {

/**
 * Lightweight internal notification emitted for one accepted hub update.
 *
 * The payload keeps only stable indices plus the accepted `version`, so sink
 * adapters can react to `on_change` without polling or re-running ownership
 * checks in the hot path.
 */
struct OnChangeNotification {
  std::size_t consumer_binding_index{0};
  std::size_t variable_index{0};
  std::uint64_t version{0};
};

/**
 * Immutable routing table plus per-consumer notification queues for
 * `on_change`.
 *
 * The dispatcher remains an internal runtime detail. The hub stays passive:
 * accepted submits call into this dispatcher, which fans out lightweight
 * notifications to consumer-specific queues.
 */
class OnChangeDispatcher {
 public:
  OnChangeDispatcher(const OnChangeDispatcher&) = delete;
  OnChangeDispatcher& operator=(const OnChangeDispatcher&) = delete;
  OnChangeDispatcher(OnChangeDispatcher&&) noexcept = default;
  OnChangeDispatcher& operator=(OnChangeDispatcher&&) noexcept = default;

  /**
   * Builds one dispatcher aligned with the compiled variable catalog.
   */
  static std::expected<OnChangeDispatcher, RuntimeError> build(
      const LoadedDatahubConfig& config, const core::StateStore& store) {
    std::vector<ConsumerBindingQueueSlot> consumer_slots;
    consumer_slots.reserve(config.consumer_bindings.size());

    std::vector<std::vector<std::size_t>> variable_consumers(store.size());

    for (std::size_t i = 0; i < config.consumer_bindings.size(); ++i) {
      const auto& binding = config.consumer_bindings[i];
      const auto variable_index =
          store.catalog().findIndexByName(binding.variable_name);
      if (!variable_index.has_value()) {
        return std::unexpected(RuntimeError{
            RuntimeErrorCode::BootstrapFailed,
            "consumer binding references an unknown compiled variable: " +
                binding.variable_name});
      }

      ConsumerBindingQueueSlot slot;
      slot.binding_id = binding.id;
      slot.variable_index = *variable_index;
      slot.enabled = binding.enabled && binding.trigger.mode == "on_change";
      slot.queue = makeQueue();

      consumer_slots.push_back(std::move(slot));
      if (consumer_slots.back().enabled) {
        variable_consumers[*variable_index].push_back(i);
      }
    }

    return OnChangeDispatcher(std::move(consumer_slots),
                              std::move(variable_consumers));
  }

  /**
   * Enqueues one lightweight notification for every consumer bound to the
   * updated variable.
   */
  void enqueueAcceptedUpdate(std::size_t variable_index,
                             std::uint64_t version) noexcept {
    if (variable_index >= m_variable_consumers.size()) {
      return;
    }

    for (const auto consumer_binding_index : m_variable_consumers[variable_index]) {
      auto& slot = m_consumer_slots[consumer_binding_index];
      if (!slot.enabled || slot.queue == nullptr) {
        continue;
      }

      slot.queue->tryEnqueue(OnChangeNotification{
          consumer_binding_index, variable_index, version});
    }
  }

  /**
   * Returns the number of pending notifications for one consumer binding.
   */
  std::size_t pendingCount(std::string_view binding_id) const noexcept {
    const auto* slot = findSlot(binding_id);
    if (slot == nullptr || slot->queue == nullptr) {
      return 0;
    }

    return slot->queue->size();
  }

  /**
   * Returns the total amount of queued `on_change` work across all bindings.
   */
  std::size_t totalPendingCount() const noexcept {
    std::size_t total = 0;
    for (const auto& slot : m_consumer_slots) {
      if (slot.queue != nullptr) {
        total += slot.queue->size();
      }
    }

    return total;
  }

  /**
   * Drains one consumer queue into a vector for internal tests and future
   * adapter workers.
   */
  std::vector<OnChangeNotification> drainAll(
      std::string_view binding_id) noexcept {
    std::vector<OnChangeNotification> notifications;
    auto* slot = findSlot(binding_id);
    if (slot == nullptr || slot->queue == nullptr) {
      return notifications;
    }

    while (auto notification = slot->queue->tryDequeue()) {
      notifications.push_back(std::move(*notification));
    }

    return notifications;
  }

  /**
   * Clears all pending notifications, used during runtime stop.
   */
  void clear() noexcept {
    for (auto& slot : m_consumer_slots) {
      if (slot.queue == nullptr) {
        continue;
      }

      while (slot.queue->tryDequeue().has_value()) {
      }
    }
  }

 private:
  using NotificationQueue =
      gtfw::concurrency::queue::MPMCQueue<OnChangeNotification>;

  struct ConsumerBindingQueueSlot {
    std::string binding_id;
    std::size_t variable_index{0};
    bool enabled{true};
    std::unique_ptr<NotificationQueue> queue;
  };

  static std::unique_ptr<NotificationQueue> makeQueue() {
    namespace queue = gtfw::concurrency::queue;

    return std::make_unique<NotificationQueue>(queue::obtainMPMCQueue<
                                               OnChangeNotification>(
        queue::policies::Bounded{kDefaultQueueCapacity,
                                 queue::policies::OverflowPolicy::Block}));
  }

  ConsumerBindingQueueSlot* findSlot(std::string_view binding_id) noexcept {
    for (auto& slot : m_consumer_slots) {
      if (slot.binding_id == binding_id) {
        return &slot;
      }
    }

    return nullptr;
  }

  const ConsumerBindingQueueSlot* findSlot(
      std::string_view binding_id) const noexcept {
    for (const auto& slot : m_consumer_slots) {
      if (slot.binding_id == binding_id) {
        return &slot;
      }
    }

    return nullptr;
  }

  OnChangeDispatcher(std::vector<ConsumerBindingQueueSlot> consumer_slots,
                     std::vector<std::vector<std::size_t>> variable_consumers)
      : m_consumer_slots(std::move(consumer_slots)),
        m_variable_consumers(std::move(variable_consumers)) {}

  static constexpr std::size_t kDefaultQueueCapacity = 1024;

  std::vector<ConsumerBindingQueueSlot> m_consumer_slots;
  std::vector<std::vector<std::size_t>> m_variable_consumers;
};

}  // namespace gt::datahub::runtime
