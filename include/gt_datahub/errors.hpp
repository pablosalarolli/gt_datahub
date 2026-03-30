#pragma once

#include <string>

namespace gt::datahub {

/**
 * Error categories returned when textual resolution cannot be completed.
 */
enum class ResolveErrorCode {
  InvalidSyntax,
  InvalidNamespace,
  UnknownVariable,
  UnknownField,
  InvalidContext
};

/**
 * Public error payload for `resolveText(...)`.
 */
struct ResolveError {
  ResolveErrorCode code{ResolveErrorCode::InvalidSyntax};
  std::string message;
};

/**
 * Error categories returned when a producer update is rejected.
 */
enum class SubmitErrorCode {
  InvalidType,
  TypeCoercionFailed,
  BindingDisabled,
  OwnershipViolation,
  RuntimeStopped
};

/**
 * Public error payload for producer submissions.
 */
struct SubmitError {
  SubmitErrorCode code{SubmitErrorCode::InvalidType};
  std::string message;
};

/**
 * Error categories returned when a manual export trigger is rejected.
 */
enum class TriggerErrorCode {
  UnknownExport,
  ExportDisabled,
  InvalidTriggerMode,
  ActivationInactive,
  RuntimeStopped
};

/**
 * Public error payload for `triggerFileExport(...)`.
 */
struct TriggerError {
  TriggerErrorCode code{TriggerErrorCode::UnknownExport};
  std::string message;
};

/**
 * Error categories returned when opening an internal producer handle fails.
 */
enum class OpenProducerErrorCode {
  UnknownBinding,
  NotInternalProducer,
  BindingDisabled,
  AlreadyOpen,
  RuntimeNotStarted,
  RuntimeStopped
};

/**
 * Public error payload for `openInternalProducer(...)`.
 */
struct OpenProducerError {
  OpenProducerErrorCode code{OpenProducerErrorCode::UnknownBinding};
  std::string message;
};

/**
 * Error categories returned by runtime lifecycle operations.
 */
enum class RuntimeErrorCode {
  InvalidConfiguration,
  BootstrapFailed,
  AlreadyStarted
};

/**
 * Public error payload for runtime start failures.
 */
struct RuntimeError {
  RuntimeErrorCode code{RuntimeErrorCode::InvalidConfiguration};
  std::string message;
};

}  // namespace gt::datahub
