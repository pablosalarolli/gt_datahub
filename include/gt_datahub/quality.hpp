#pragma once

namespace gt::datahub {

/**
 * Effective quality states exposed by the DataHub public API.
 *
 * Public reads observe the effective quality, which may already include stale
 * evaluation on top of the runtime's internal raw quality.
 */
enum class Quality {
  Good,
  Bad,
  Stale,
  Uncertain
};

}  // namespace gt::datahub
