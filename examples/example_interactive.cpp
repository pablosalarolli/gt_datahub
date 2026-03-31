#include "gt_datahub/i_internal_producer.hpp"
#include "gt_datahub/quality.hpp"
#include "gt_datahub/update_request.hpp"
#include "runtime/datahub_runtime.hpp"

#include <chrono>
#include <iostream>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <variant>

namespace {

using gt::datahub::IInternalProducer;
using gt::datahub::Quality;
using gt::datahub::TriggerErrorCode;
using gt::datahub::UpdateRequest;
using gt::datahub::VariableState;
using gt::datahub::runtime::DataHubRuntime;

constexpr std::string_view kInteractiveYaml = R"yaml(
datahub:
  schema_version: 1
  connectors:
    - id: file_main
      kind: file
  variables:
    - name: TEMP_MAX_FUNDO_PANELA
      data_type: Double
      role: Measurement
    - name: CORRIDA_ATIVA
      data_type: Bool
      role: State
      default_value: false
  producer_bindings:
    - id: pb_temp_interna
      variable_name: TEMP_MAX_FUNDO_PANELA
      producer_kind: internal
    - id: pb_corrida_interna
      variable_name: CORRIDA_ATIVA
      producer_kind: internal
  consumer_bindings: []
  file_exports:
    - id: exp_snapshot_manual
      connector_id: file_main
      format: csv
      target_template: "snapshots/snapshot_${system.now}.csv"
      append: false
      write_header_if_missing: true
      trigger:
        mode: manual
      activation:
        run_while:
          source: hub.CORRIDA_ATIVA.value
          op: eq
          value: true
        finalize_on_stop: true
      columns:
        - name: ts
          expression: "${export.captured_at}"
        - name: corrida
          expression: "${hub.CORRIDA_ATIVA.value}"
        - name: temp_max
          expression: "${hub.TEMP_MAX_FUNDO_PANELA.value}"
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

std::optional<bool> parseBool(std::string_view text) {
  if (text == "1" || text == "true" || text == "on") {
    return true;
  }
  if (text == "0" || text == "false" || text == "off") {
    return false;
  }

  return std::nullopt;
}

std::string formatStateValue(const VariableState& state) {
  if (!state.initialized ||
      std::holds_alternative<std::monostate>(state.value)) {
    return "<uninitialized>";
  }

  if (const auto* value = std::get_if<bool>(&state.value)) {
    return *value ? "true" : "false";
  }
  if (const auto* value = std::get_if<double>(&state.value)) {
    std::ostringstream stream;
    stream << *value;
    return stream.str();
  }

  return "<unsupported>";
}

void printStatus(const DataHubRuntime& runtime) {
  const auto temp = runtime.hub().getState("TEMP_MAX_FUNDO_PANELA");
  const auto corrida = runtime.hub().getState("CORRIDA_ATIVA");

  if (!temp.has_value() || !corrida.has_value()) {
    std::cout << "Estado do hub indisponivel." << '\n';
    return;
  }

  std::cout << "TEMP_MAX_FUNDO_PANELA=" << formatStateValue(*temp)
            << " | quality=" << qualityToString(temp->quality)
            << " | version=" << temp->version << '\n';
  std::cout << "CORRIDA_ATIVA=" << formatStateValue(*corrida)
            << " | quality=" << qualityToString(corrida->quality)
            << " | version=" << corrida->version << '\n';
}

void printHelp() {
  std::cout << "Comandos:" << '\n';
  std::cout << "  help                     mostra esta ajuda" << '\n';
  std::cout << "  status                   mostra o estado atual do hub" << '\n';
  std::cout << "  set-temp <double>        publica TEMP_MAX_FUNDO_PANELA" << '\n';
  std::cout << "  set-corrida <on|off>     publica CORRIDA_ATIVA" << '\n';
  std::cout << "  resolve <expr>           resolve uma expressao informada pelo usuario" << '\n';
  std::cout << "  snapshot                 tenta trigger manual do export" << '\n';
  std::cout << "  quit                     encerra o exemplo" << '\n';
}

bool submitTemp(IInternalProducer& producer, double value) {
  UpdateRequest request;
  request.value = value;
  request.quality = Quality::Good;
  request.source_timestamp = std::chrono::system_clock::now();

  const auto submitted = producer.submit(std::move(request));
  if (!submitted.has_value()) {
    std::cout << "submit TEMP_MAX_FUNDO_PANELA falhou: "
              << submitted.error().message << '\n';
    return false;
  }

  std::cout << "TEMP_MAX_FUNDO_PANELA atualizado para " << value << '\n';
  return true;
}

bool submitCorrida(IInternalProducer& producer, bool value) {
  UpdateRequest request;
  request.value = value;
  request.quality = Quality::Good;
  request.source_timestamp = std::chrono::system_clock::now();

  const auto submitted = producer.submit(std::move(request));
  if (!submitted.has_value()) {
    std::cout << "submit CORRIDA_ATIVA falhou: "
              << submitted.error().message << '\n';
    return false;
  }

  std::cout << "CORRIDA_ATIVA atualizado para "
            << (value ? "true" : "false") << '\n';
  return true;
}

void resolveExpression(const DataHubRuntime& runtime,
                       std::string_view expression) {
  const auto resolved = runtime.hub().resolveText(expression);

  if (!resolved.has_value()) {
      std::cout << "resolveText falhou: " << resolved.error().message << '\n';
    return;
  }

  std::cout << *resolved << '\n';
}

void trySnapshot(DataHubRuntime& runtime) {
  const auto triggered = runtime.triggerFileExport("exp_snapshot_manual");
  if (!triggered.has_value()) {
    std::cout << "triggerFileExport rejeitado: " << triggered.error().message
              << '\n';
    if (triggered.error().code == TriggerErrorCode::ActivationInactive) {
      std::cout << "Dica: use `set-corrida on` para ativar a janela do export."
                << '\n';
    }
    return;
  }

  std::cout << "triggerFileExport aceito." << '\n';
  std::cout << "Observacao: nesta fase o exemplo demonstra o contrato da API; "
               "a escrita real em CSV entra na fase 6."
            << '\n';
}

}  // namespace

int main() {
  auto runtime_result = DataHubRuntime::createFromString(kInteractiveYaml);
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

  auto temp_result = runtime->openInternalProducer("pb_temp_interna");
  if (!temp_result.has_value()) {
    std::cerr << "openInternalProducer(pb_temp_interna) failed: "
              << temp_result.error().message << '\n';
    return 1;
  }

  auto corrida_result = runtime->openInternalProducer("pb_corrida_interna");
  if (!corrida_result.has_value()) {
    std::cerr << "openInternalProducer(pb_corrida_interna) failed: "
              << corrida_result.error().message << '\n';
    return 1;
  }

  std::unique_ptr<IInternalProducer> temp_writer = std::move(*temp_result);
  std::unique_ptr<IInternalProducer> corrida_writer = std::move(*corrida_result);

  std::cout << "gt_datahub interactive example" << '\n';
  printHelp();
  printStatus(*runtime);

  std::string line;
  while (true) {
    std::cout << "> " << std::flush;
    if (!std::getline(std::cin, line)) {
      break;
    }

    std::istringstream input(line);
    std::string command;
    input >> command;

    if (command.empty()) {
      continue;
    }

    if (command == "quit" || command == "exit") {
      break;
    }

    if (command == "help") {
      printHelp();
      continue;
    }

    if (command == "status") {
      printStatus(*runtime);
      continue;
    }

    if (command == "resolve") {
      std::string expression;
      std::getline(input >> std::ws, expression);

      if (expression.empty()) {
        std::cout << "Expressao: " << std::flush;
        if (!std::getline(std::cin, expression) || expression.empty()) {
          std::cout << "Uso: resolve <expr>" << '\n';
          continue;
        }
      }

      resolveExpression(*runtime, expression);
      continue;
    }

    if (command == "snapshot") {
      trySnapshot(*runtime);
      continue;
    }

    if (command == "set-temp") {
      double value = 0.0;
      if (!(input >> value)) {
        std::cout << "Uso: set-temp <double>" << '\n';
        continue;
      }

      submitTemp(*temp_writer, value);
      continue;
    }

    if (command == "set-corrida") {
      std::string value;
      input >> value;
      const auto parsed = parseBool(value);
      if (!parsed.has_value()) {
        std::cout << "Uso: set-corrida <on|off|true|false|1|0>" << '\n';
        continue;
      }

      submitCorrida(*corrida_writer, *parsed);
      continue;
    }

    std::cout << "Comando desconhecido: " << command << '\n';
    std::cout << "Use `help` para listar os comandos." << '\n';
  }

  runtime->stop();
  return 0;
}
