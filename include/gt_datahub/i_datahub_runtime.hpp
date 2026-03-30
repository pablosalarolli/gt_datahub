#pragma once

#include "gt_datahub/errors.hpp"
#include "gt_datahub/i_datahub.hpp"
#include "gt_datahub/i_internal_producer.hpp"

#include <expected>
#include <memory>
#include <string_view>

namespace gt::datahub {

/**
 * Lifecycle and runtime services facade for the DataHub.
 *
 * The runtime owns startup/shutdown, opens internal producer handles and
 * exposes manual file-export triggering for configured manual exports only.
 */
class IDataHubRuntime {
 public:
  virtual ~IDataHubRuntime() = default;

  /**
   * Starts the runtime and its configured services.
   */
  virtual std::expected<void, RuntimeError> start() = 0;

  /**
   * Stops the runtime and releases runtime-owned resources.
   */
  virtual void stop() = 0;

  /**
   * Returns the mutable runtime-owned hub facade.
   */
  virtual IDataHub& hub() noexcept = 0;

  /**
   * Returns the const runtime-owned hub facade.
   */
  virtual const IDataHub& hub() const noexcept = 0;

  /**
   * Opens an application-facing writer handle for a configured internal
   * producer binding.
   */
  virtual std::expected<std::unique_ptr<IInternalProducer>, OpenProducerError>
  openInternalProducer(std::string_view binding_id) = 0;

  /**
   * Triggers a manual file export identified by `export_id`.
   */
  virtual std::expected<void, TriggerError> triggerFileExport(
      std::string_view export_id) = 0;
};

}  // namespace gt::datahub
