#include "gt_datahub/quality.hpp"
#include "gt_datahub/update_request.hpp"
#include "runtime/datahub_runtime.hpp"

#include <chrono>
#include <iostream>
#include <memory>
#include <string>
#include <string_view>
#include <variant>

namespace {

using gt::datahub::Quality;
using gt::datahub::UpdateRequest;
using gt::datahub::runtime::DataHubRuntime;

constexpr std::string_view kExampleYaml = R"yaml(
datahub:
  schema_version: 1
  connectors: []
  variables:
    - name: TEMP_MAX_FUNDO_PANELA
      data_type: Double
      role: Measurement
  producer_bindings:
    - id: pb_temp_interna
      variable_name: TEMP_MAX_FUNDO_PANELA
      producer_kind: internal
  consumer_bindings: []
  file_exports: []
)yaml";

std::string qualityToString(Quality quality) {
  switch (quality) {
    case Quality::Good:
      return "good";
    case Quality::Bad:
      return "bad";
    case Quality::Stale:
      return "stale";
    case Quality::Uncertain:
      return "uncertain";
  }

  return "unknown";
}

}  // namespace

int main() {
  // Repo-local example: until a public runtime factory exists, examples build
  // the in-memory runtime directly from the internal bootstrap class.
  auto runtime_result = DataHubRuntime::createFromString(kExampleYaml);
  if (!runtime_result.has_value()) {
    std::cerr << "runtime bootstrap failed: "
              << runtime_result.error().message << '\n';
    return 1;
  }

  std::unique_ptr<DataHubRuntime> runtime = std::move(*runtime_result);

  if (const auto started = runtime->start(); !started.has_value()) {
    std::cerr << "runtime start failed: " << started.error().message << '\n';
    return 1;
  }

  auto producer_result = runtime->openInternalProducer("pb_temp_interna");
  if (!producer_result.has_value()) {
    std::cerr << "openInternalProducer failed: "
              << producer_result.error().message << '\n';
    return 1;
  }

  std::unique_ptr<gt::datahub::IInternalProducer> producer =
      std::move(*producer_result);

  UpdateRequest request;
  request.value = 812.4;
  request.quality = Quality::Good;
  request.source_timestamp = std::chrono::system_clock::now();

  if (const auto submitted = producer->submit(std::move(request));
      !submitted.has_value()) {
    std::cerr << "submit failed: " << submitted.error().message << '\n';
    return 1;
  }

  const auto state = runtime->hub().getState("TEMP_MAX_FUNDO_PANELA");
  if (!state.has_value()) {
    std::cerr << "hub state not found after submit" << '\n';
    return 1;
  }

  const auto* value = std::get_if<double>(&state->value);
  if (value == nullptr) {
    std::cerr << "unexpected state value type" << '\n';
    return 1;
  }

  std::cout << "TEMP_MAX_FUNDO_PANELA=" << *value
            << ", quality=" << qualityToString(state->quality)
            << ", version=" << state->version << '\n';

  const auto resolved = runtime->hub().resolveText(
      "Leitura atual ${hub.TEMP_MAX_FUNDO_PANELA.value} C "
      "(q=${hub.TEMP_MAX_FUNDO_PANELA.quality})");
  if (!resolved.has_value()) {
    std::cerr << "resolveText failed: " << resolved.error().message << '\n';
    return 1;
  }

  std::cout << *resolved << '\n';

  runtime->stop();
  return 0;
}
