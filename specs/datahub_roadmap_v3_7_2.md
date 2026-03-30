# Roadmap Completo — Da Baseline DataHub v3.7.2 a uma Biblioteca em Produção (com Plano de Testes)

## 1. Objetivo

Transformar a baseline v3.7.2 em uma biblioteca C++ estável, testada e integrável, capaz de suportar:

- registro central de variáveis com `1 produtor por variável`;
- `0..N consumidores` por variável;
- configuração oficial por YAML (`connectors`, `variables`, `producer_bindings`, `consumer_bindings`, `file_exports`);
- publicação interna controlada por `IInternalProducer`;
- leitura pública por `IDataHub`;
- resolução textual por seletores e templates;
- file source (`file.text`, `file.csv_last_row_column`);
- file export multicoluna em `csv`, periódico ou manual;
- integração com OPC UA e OPC DA;
- operação robusta em aplicações industriais de visão computacional, automação e processamento de sinais.

A meta não é apenas “ter código compilando”, mas ter uma lib com:

- API pública estável;
- cobertura de testes forte nos fluxos críticos;
- comportamento previsível em runtime;
- diagnóstico bom de falhas;
- documentação suficiente para adoção por outros módulos do sistema.

---

## 2. Princípios de execução

### 2.1 Macroestrutura
A execução deve seguir **fases como ordem técnica** e **sprints como ritmo de entrega**.

- **Fases** definem precedência arquitetural
- **Sprints** quebram o trabalho em incrementos curtos e testáveis

### 2.2 Regra de teste por sprint
Cada sprint deve cobrir três camadas:

1. **Unitários do que foi criado**
2. **Regressões do que foi tocado e podia quebrar**
3. **Pelo menos 1 smoke/integration curto** do fluxo novo

### 2.3 Regra de teste por fase
Ao fechar cada fase, deve-se rodar:

- suíte unitária completa do que já existe;
- integração principal impactada;
- sanitizers compatíveis com o toolchain;
- análise estática mínima;
- benchmark quando a fase tocar performance/escala.

### 2.4 Definição de pronto de uma sprint
Uma sprint só é considerada concluída quando:

- os entregáveis compilam;
- os testes previstos da sprint passam;
- não há regressão conhecida aberta na área tocada;
- existe pelo menos um fluxo curto provando o comportamento integrado;
- o código novo entrou no CI local da lib.

---

## 3. Diretriz transversal de documentação de código

A documentação do código deve ser produzida desde as primeiras sprints, acompanhando a implementação da biblioteca. O objetivo não é comentar tudo, mas garantir que a API pública e os pontos de comportamento não óbvios permaneçam claros, coerentes com a baseline e fáceis de manter ao longo da evolução do projeto.

### 3.1 Regra geral
Toda API pública introduzida em uma sprint deve sair acompanhada de documentação mínima no próprio código. Essa documentação deve explicar a responsabilidade do componente, o contrato da função ou classe, as invariantes relevantes, regras de ownership/lifecycle e o comportamento esperado em caso de erro, rejeição ou uso incorreto.

### 3.2 Onde a documentação é obrigatória
A documentação é obrigatória para:
- classes e structs públicas da biblioteca;
- interfaces de integração, como `IDataHub`, `IDataHubRuntime` e `IInternalProducer`;
- funções com contrato sensível ou comportamento não trivial;
- enums e políticas que afetam comportamento do sistema;
- componentes com lifecycle delicado ou integração com runtime assíncrono;
- pontos onde clocks, stale, quality, templates ou ownership possam ser mal interpretados.

### 3.3 Itens que devem sair documentados desde a introdução
Dar prioridade para documentação de:
- `IDataHub`
- `IDataHubRuntime`
- `IInternalProducer`
- `VariableDefinition`
- `VariableState`
- `UpdateRequest`
- `ResolveError`, `SubmitError`, `TriggerError`
- `IRuntimeHubAccess`
- `ConnectorRuntime`
- `resolveText(...)`
- `openInternalProducer(...)`
- `triggerFileExport(...)`
- `PeriodicScheduler`

### 3.4 Conteúdo esperado das docstrings
As docstrings e comentários de API pública devem cobrir, quando aplicável:
- propósito da classe/função;
- diferença entre contrato público e contrato interno;
- quando a operação é síncrona, assíncrona, deferida ou apenas aceita a requisição;
- significado de `quality`, `version`, `initialized` e timestamps;
- impacto sobre ownership e lifetime;
- formato canônico de entrada/saída;
- comportamento em falhas, rejeições e valores ausentes.

### 3.5 Comentários obrigatórios para gotchas de implementação
Além das docstrings da API, o código deve conter comentários objetivos nos pontos em que a implementação pode facilmente divergir da baseline. Em especial:
- `system_clock` vs `steady_clock`;
- stale lazy;
- `raw_quality` vs `quality` efetiva;
- `AlreadyOpen` e reabertura de `IInternalProducer`;
- `on_change` = toda atualização aceita;
- fila/serialização por `export_id`;
- `path_template` vs `target_template`;
- movimento de `UpdateRequest` no hot path;
- `evaluate() const` nos nós de predicado.

### 3.6 Escopo da documentação
Não é necessário comentar código trivial, getters/setters óbvios, helpers locais simples ou testes autoexplicativos. A prioridade é documentar contrato, semântica e decisões que não sejam imediatamente óbvias pela leitura do código.

### 3.7 Complemento com documentação auxiliar
Quando um fluxo ou subsistema ficar grande demais para ser bem explicado apenas com docstrings, deve-se criar documentação auxiliar em Markdown dentro do repositório. Exemplos recomendados:
- `docs/bootstrap-and-validation.md`
- `docs/internal-producer-lifecycle.md`
- `docs/selectors-and-text-resolution.md`
- `docs/file-export-lifecycle.md`
- `docs/connector-runtime-and-adapters.md`
- `docs/quality-and-stale.md`

### 3.8 Critério de pronto relacionado à documentação
Nenhuma sprint que introduza ou altere API pública deve ser considerada concluída sem:
- docstrings mínimas atualizadas nas APIs tocadas;
- comentários adicionados nos gotchas relevantes;
- remoção de comentários obsoletos que contrariem a baseline vigente.

---

## 4. Definição de pronto da baseline de produção

A biblioteca só deve ser considerada “em produção” quando atender simultaneamente a estes critérios:

### 4.1 Funcional
- registra variáveis definidas em YAML;
- compila `connectors`, `variables`, `producer_bindings`, `consumer_bindings` e `file_exports`;
- reforça `1 produtor por variável` em runtime;
- expõe `IDataHub` com `getState()`, `getDefinition()`, `listVariables()` e `resolveText()`;
- expõe `IDataHubRuntime` com `start()`, `stop()`, `openInternalProducer()` e `triggerFileExport()`;
- aplica stale lazy e qualidade efetiva corretamente;
- executa file export periódico e manual em `csv`;
- suporta `file.text` e `file.csv_last_row_column`;
- suporta OPC UA e OPC DA em source/sink;
- lida com perda de conexão e qualidade sintética;
- respeita o modelo de `activation.run_while` da baseline.

### 4.2 Qualidade
- suíte de testes cobrindo fluxos críticos;
- sanitizers e análise estática rodando em CI;
- logging claro para bootstrap, validação, export e conectores;
- sem corrupção em export manual concorrente;
- sem uso incorreto de `system_clock` para intervalos;
- sem regressão no enforcement de ownership de escrita.

### 4.3 Operacional
- API pública minimamente documentada;
- exemplo end-to-end compilável;
- changelog inicial e versionamento semântico;
- pacote reutilizável por pelo menos 2 aplicações reais internas.

---

## 5. Estratégia geral de entrega

### Etapa A — Foundation
Construir os tipos públicos, o catálogo, a store, o bootstrap e o runtime mínimo. Aqui o foco é fechar o núcleo sem adapters externos pesados.

### Etapa B — Feature-complete
Adicionar YAML compiler, selectors, predicates, file source/export e ownership enforcement, já alinhados ao runtime.

### Etapa C — Production hardening
Cobrir adapters OPC, testes pesados, integração real, performance, logging, docs, empacotamento e rollout controlado.

A ordem importa. Não comece por adapters complexos ou integrações piloto antes de o núcleo do runtime estar sólido.

---

## 6. Matriz de testes obrigatórios

### 6.1 Tipos de teste usados no roadmap
- **UT**: unit test
- **RT**: regression test
- **IT**: integration test
- **ST**: smoke test
- **PT**: performance test / benchmark
- **SAT**: sanitizer/análise estática
- **CT**: concurrency test

### 6.2 Áreas críticas que devem sempre ter cobertura explícita
- `1 produtor por variável`
- `default_value` e bootstrap de estado
- `raw_quality` vs `quality` efetiva
- stale lazy
- `resolveText()`
- serialização textual dos seletores
- predicados de ativação
- file source
- file export periódico/manual
- `triggerFileExport()` concorrente
- `openInternalProducer()` / `AlreadyOpen`
- perda de conexão e update sintético
- `on_change` para sinks
- reconexão por `ConnectorRuntime`

---

## 7. Fase 0 — Congelamento da baseline e governança

### Objetivo
Parar de alterar a arquitetura e preparar um terreno seguro para começar a codar.

### Entregáveis
- baseline v3.7.2 congelada como referência única;
- lista curta de decisões não reabertas;
- backlog inicial classificado por prioridade;
- convenções de código e versionamento.

### Tarefas
1. Declarar formalmente a v3.7.2 como única referência de contrato.
2. Criar `Out of Scope for v1.0`.
3. Definir convenções de nomenclatura e versionamento.
4. Definir política de mudanças.
5. Registrar decisões fechadas:
   - 1 produtor por variável com enforcement em runtime
   - sem `submit()` público irrestrito
   - stale lazy
   - `consumer_bindings` apenas `on_change`
   - `file_exports` apenas `csv`
   - hot-reload fora do escopo

### Testes da fase
**UT**: não aplicável como foco principal.
**RT**: checklist manual de coerência entre documentos.
**ST**: compilar um esqueleto mínimo do projeto com dependências básicas.

### Critério de saída
- ninguém mais usa a spec como rascunho em aberto;
- backlog de implementação inicial aprovado.

---

## 8. Fase 1 — API pública e tipos centrais

### Objetivo
Criar a superfície pública da biblioteca, mesmo com algumas implementações internas ainda vazias.

### Entregáveis
- headers públicos organizados;
- namespaces definidos;
- tipos auxiliares padronizados;
- exemplo mínimo compilando contra a API.

### Tarefas detalhadas
Criar:
- `data_type.hpp`
- `quality.hpp`
- `variable_role.hpp`
- `value.hpp`
- `variable_definition.hpp`
- `variable_state.hpp`
- `update_request.hpp`
- `errors.hpp`
- `i_datahub.hpp`
- `i_internal_producer.hpp`
- `i_datahub_runtime.hpp`

### Sprints sugeridas
#### Sprint 1.1 — Enums e aliases
**Implementação**
- `DataType`
- `Quality`
- `VariableRole`
- `Timestamp`
- códigos/estruturas de erro públicos

**Testes obrigatórios**
- **UT**: construção e comparação de enums/valores padrão
- **UT**: helpers/conversões básicos dos tipos públicos
- **RT**: nenhuma dependência quebrada ao incluir headers
- **ST**: TU simples compilando e instanciando tipos

#### Sprint 1.2 — `Value`, definição e estado
**Implementação**
- `Value` com `std::variant`
- `VariableDefinition`
- `VariableState`
- `UpdateRequest`

**Testes obrigatórios**
- **UT**: round-trip de armazenamento/extração por tipo
- **UT**: acesso errado gera erro esperado
- **UT**: defaults e opcionais de `VariableDefinition`
- **RT**: cópia/movimentação de `Value`
- **ST**: criar variável fictícia e estado mínimo

#### Sprint 1.3 — Interfaces públicas
**Implementação**
- `IDataHub`
- `IDataHubRuntime`
- `IInternalProducer`

**Testes obrigatórios**
- **UT**: mocks/stubs compilam contra as interfaces
- **RT**: includes públicos continuam independentes
- **ST**: app mínima compila contra a API

### Critério de saída
- API compila;
- principais tipos já têm forma estável;
- nenhum nome importante ainda está em disputa.

---

## 9. Fase 2 — Catálogo, store e bootstrap de estado

### Objetivo
Implementar o estado real de runtime do hub.

### Entregáveis
- catálogo compilado;
- state store thread-safe;
- bootstrap de estado;
- `getState/getDefinition/listVariables` funcionando.

### Tarefas detalhadas
- estruturar `CompiledVariableDefinition`;
- índice heterogêneo por nome;
- `VariableStateEntry` com locking por variável;
- `default_value` aplicado no bootstrap;
- stale lazy e qualidade efetiva.

### Sprints sugeridas
#### Sprint 2.1 — Catálogo compilado
**Testes obrigatórios**
- **UT**: lookup por nome e falha limpa para inexistente
- **UT**: índices por nome são estáveis pós-bootstrap
- **RT**: headers públicos seguem compilando
- **ST**: catálogo mínimo consulta definições

#### Sprint 2.2 — Store e bootstrap
**Testes obrigatórios**
- **UT**: bootstrap cria uma entrada por variável
- **UT**: `default_value` inicializa `initialized=true`
- **UT**: `listVariables()` retorna conjunto correto
- **ST**: runtime fake lê definições e estados

#### Sprint 2.3 — stale lazy e qualidade efetiva
**Testes obrigatórios**
- **UT**: cálculo lazy com `steady_clock`
- **UT**: `Bad` vence `Stale`
- **UT**: sem `stale_after_ms`, qualidade não degrada
- **ST**: leitura ponta a ponta da qualidade efetiva

### Critério de saída
- hub já armazena e consulta variáveis corretamente, ainda sem YAML completo nem adapters.

---

## 10. Fase 3 — YAML compiler e validação

### Objetivo
Transformar o YAML fechado pela baseline em runtime compilado e validado.

### Entregáveis
- loader YAML;
- estruturas compiladas;
- validação de contrato;
- mensagens de erro coerentes de bootstrap.

### Tarefas detalhadas
- parsing de `connectors`, `variables`, `producer_bindings`, `consumer_bindings`, `file_exports`;
- validação de IDs, nomes, capabilities e ownership;
- compilação de seletores, templates e predicados.

### Sprints sugeridas
#### Sprint 3.1 — `connectors` e `variables`
**Testes obrigatórios**
- **UT**: YAML mínimo válido
- **UT**: duplicação de IDs falha
- **UT**: nomes de variáveis duplicados falham
- **ST**: carregar config simples

#### Sprint 3.2 — bindings e exports
**Testes obrigatórios**
- **UT**: capacidade inválida por `kind` falha
- **UT**: mais de 1 producer para mesma variável falha
- **UT**: `producer_kind: internal` mal formado falha
- **ST**: config média com source, sink e export valida

#### Sprint 3.3 — seletores, templates e predicados compilados
**Testes obrigatórios**
- **UT**: seletor canônico puro
- **UT**: template textual com `${...}`
- **UT**: operadores unários/binários validados corretamente
- **ST**: AST de ativação compilada com `all/any/not`

### Critério de saída
- toda configuração relevante da baseline compila com falha cedo para erro de contrato.

---

## 11. Fase 4 — Runtime, ownership e lifecycle

### Objetivo
Implementar o runtime real, com start/stop, ownership enforcement e contrato interno separado do contrato público.

### Entregáveis
- `DataHubRuntime` funcional;
- `IRuntimeHubAccess`;
- `openInternalProducer()`;
- dispatch interno de `on_change`.

### Tarefas detalhadas
- separar bootstrap de `start()`;
- inicializar connector runtimes e schedulers somente no `start()`;
- criar mecanismo de abertura protegida para `IInternalProducer`;
- despachar notificação interna para sinks a cada submit aceito.

### Sprints sugeridas
#### Sprint 4.1 — `start()` / `stop()`
**Testes obrigatórios**
- **UT**: `start()` bem-sucedido
- **UT**: `AlreadyStarted` coerente
- **UT**: `stop()` idempotente
- **ST**: runtime sobe/desce sem adapters reais

#### Sprint 4.2 — internal producer e ownership enforcement
**Testes obrigatórios**
- **UT**: binding interno válido abre
- **UT**: binding aberto duas vezes retorna `AlreadyOpen`
- **UT**: destruir handle libera reabertura
- **UT**: `submit()` com tipo incompatível falha
- **ST**: app publica valor por `IInternalProducer`

#### Sprint 4.3 — `on_change` para sinks
**Testes obrigatórios**
- **UT**: submit aceito incrementa `version` e gera notificação interna
- **UT**: sink recebe toda atualização aceita, mesmo com valor igual
- **ST**: sink fake é acionado após update

### Critério de saída
- runtime mínimo já funciona com ownership real e sem workarounds públicos.

---

## 12. Fase 5 — Seletores, `resolveText()` e predicados

### Objetivo
Fechar a camada de leitura simbólica e ativação configurável.

### Entregáveis
- parser/resolvedor de seletores;
- `resolveText()` público;
- avaliação de predicados;
- serialização textual canônica.

### Tarefas detalhadas
- namespaces `hub`, `context`, `export`, `system` conforme baseline;
- serialização de timestamps, bools, versões e contextos;
- erro estrutural vs valor ausente;
- `evaluate() const` da AST.

### Sprints sugeridas
#### Sprint 5.1 — `resolveText()` público
**Testes obrigatórios**
- **UT**: variáveis conhecidas e campos válidos
- **UT**: namespaces inválidos geram erro
- **UT**: valor ausente gera string vazia
- **ST**: template completo resolve corretamente

#### Sprint 5.2 — serialização textual canônica
**Testes obrigatórios**
- **UT**: `initialized` serializa como `true/false`
- **UT**: `version` serializa como decimal
- **UT**: `row_index` serializa como decimal sem sinal
- **UT**: timestamps seguem formato da baseline
- **ST**: CSV com contextos e timestamps coerentes

#### Sprint 5.3 — predicados de ativação
**Testes obrigatórios**
- **UT**: folha binária
- **UT**: folha unária
- **UT**: `all`, `any`, `not`
- **ST**: export periódico respeita a janela de ativação

### Critério de saída
- seletores e predicados funcionam como contrato central da biblioteca.

---

## 13. Fase 6 — File source e file export

### Objetivo
Implementar as capacidades de arquivo que fazem parte do núcleo da baseline.

### Entregáveis
- `file.text`;
- `file.csv_last_row_column`;
- export CSV periódico/manual;
- sessão de export por `export_id`.

### Tarefas detalhadas
- resolução de `path_template` a cada tentativa;
- serialização de colunas com `source` ou `expression`;
- `activation`, `finalize_on_stop`, `append`, header e row index;
- `triggerFileExport()` e fila/serialização por `export_id`.

### Sprints sugeridas
#### Sprint 6.1 — file producer
**Testes obrigatórios**
- **UT**: leitura de texto integral
- **UT**: leitura da última linha de CSV
- **UT**: caminho ausente ignora tentativa sem tocar estado
- **ST**: arquivo atualiza variável no hub

#### Sprint 6.2 — file export periódico/manual
**Testes obrigatórios**
- **UT**: export contínuo sem `activation`
- **UT**: export manual com `ActivationInactive` quando aplicável
- **UT**: `append: false` trunca
- **ST**: export consolidado da spec gera CSV esperado

#### Sprint 6.3 — serialização por `export_id` e falhas
**Testes obrigatórios**
- **UT**: múltiplos triggers manuais do mesmo export não corrompem arquivo
- **UT**: falha estrutural de `target_template` não grava linha
- **ST**: `finalize_on_stop` encerra sessão e reabre depois

### Critério de saída
- file source e file export já entregam valor real para integração industrial.

---

## 14. Fase 7 — Adapters OPC UA / OPC DA

### Objetivo
Conectar o core do DataHub aos meios industriais previstos na baseline.

### Entregáveis
- adapters OPC UA source/sink;
- adapters OPC DA source/sink;
- `ConnectorRuntime` coerente para reconexão e diagnóstico.

### Tarefas detalhadas
- source adapters submetem via contrato interno;
- sink adapters recebem notificação interna por `on_change`;
- reconexão no nível do connector runtime;
- perda de conexão gera update sintético coerente.

### Sprints sugeridas
#### Sprint 7.1 — OPC UA
**Testes obrigatórios**
- **IT**: producer OPC UA
- **IT**: consumer OPC UA
- **ST**: perda e reconexão básica

#### Sprint 7.2 — OPC DA
**Testes obrigatórios**
- **IT**: producer OPC DA
- **IT**: consumer OPC DA
- **ST**: perda e reconexão básica

#### Sprint 7.3 — reconexão e diagnóstico
**Testes obrigatórios**
- **IT**: mesmo connector usado em source e sink reconecta de forma coerente
- **ST**: estado do connector é observável

### Critério de saída
- OPC UA e OPC DA operam com source/sink básicos aderentes à baseline.

---

## 15. Fase 8 — Testes automatizados, CI e hardening

### Objetivo
Criar confiança contínua para evolução futura da lib.

### Entregáveis
- pipeline de CI;
- testes unitários e de integração;
- sanitizers;
- benchmark básico.

### Tarefas detalhadas
- separar suítes por módulo;
- automatizar execução local e no CI;
- medir custo dos caminhos críticos;
- definir gates de merge.

### Sprints sugeridas
#### Sprint 8.1 — organização da suíte
**Testes obrigatórios**
- **UT**: suites por módulo rodando isoladamente
- **RT**: execução do conjunto mínimo por sprint
- **ST**: comando único local para smoke da lib

#### Sprint 8.2 — integração e concorrência
**Testes obrigatórios**
- **IT**: setup -> start -> producer -> sink/export -> stop
- **IT**: `triggerFileExport()` concorrente
- **CT**: submits concorrentes em variáveis diferentes
- **CT**: avaliação simultânea da mesma AST de ativação

#### Sprint 8.3 — sanitizers e análise estática
**Testes obrigatórios**
- **SAT**: ASan
- **SAT**: UBSan
- **SAT**: clang-tidy/cppcheck
- **SAT**: TSan, se viável

#### Sprint 8.4 — benchmarks
**Testes obrigatórios**
- **PT**: submit em variáveis diferentes
- **PT**: `resolveText()`
- **PT**: export periódico
- **PT**: trigger manual seriado por `export_id`

### Critério de saída
- CI bloqueia regressões reais;
- métricas básicas já são conhecidas.

---

## 16. Fase 9 — Integração piloto em aplicação real

### Objetivo
Validar a lib em um caso real antes de declarar produção ampla.

### Entregáveis
- integração em 1 app piloto;
- relatório de gaps reais;
- ajustes finais de API compatíveis.

### Estratégia recomendada
Escolher um caso real com complexidade média:
- 1 ou 2 variáveis publicadas pela aplicação via `IInternalProducer`;
- 1 source externo;
- 1 sink externo;
- 1 export CSV com `activation`;
- persistência operacional do YAML da aplicação.

### Testes da fase
- **IT**: comparar comportamento do sistema antigo vs novo em modo shadow, se possível
- **IT**: reconexão de connector
- **IT**: export durante corrida com `activation`
- **PT**: carga realista de updates

### Critério de saída
- 1 caso real operando de forma aceitável com a nova lib.

---

## 17. Fase 10 — Release 1.0 e rollout gradual

### Objetivo
Transformar o piloto numa biblioteca adotável com confiança.

### Entregáveis
- release `1.0.0`;
- guia de adoção;
- changelog inicial;
- exemplo oficial;
- checklist de integração.

### Testes da fase
- **RT**: suíte completa verde
- **IT**: exemplo oficial compilando e rodando
- **SAT**: CI final de release
- **PT**: benchmarks de referência congelados para comparação futura
- **ST**: checklist de adoção validado em pelo menos um novo consumidor

### Critério de saída
- a lib é publicável e pode ser usada em novos projetos sem depender da spec para cada dúvida cotidiana.

---

## 18. Priorização realista

### P0 — indispensável antes do primeiro piloto
- Fase 1
- Fase 2
- Fase 3
- Fase 4
- Fase 5
- Fase 6

### P1 — indispensável antes de produção ampla
- Fase 7
- Fase 8
- Fase 9

### P2 — pode amadurecer depois do 1.0
- diagnósticos mais sofisticados
- métricas avançadas
- sinks adicionais
- formatos adicionais de export
- hot-reload e reatividade interna

---

## 19. Ordem recomendada de implementação por arquivos

### Sprint técnica 1
- `include/gt_datahub/data_type.hpp`
- `include/gt_datahub/quality.hpp`
- `include/gt_datahub/variable_role.hpp`
- `include/gt_datahub/value.hpp`
- `include/gt_datahub/variable_definition.hpp`
- `include/gt_datahub/variable_state.hpp`
- `include/gt_datahub/update_request.hpp`
- `include/gt_datahub/errors.hpp`

### Sprint técnica 2
- `include/gt_datahub/i_datahub.hpp`
- `include/gt_datahub/i_internal_producer.hpp`
- `include/gt_datahub/i_datahub_runtime.hpp`
- `src/core/compiled_catalog.hpp`
- `src/core/state_store.hpp`

### Sprint técnica 3
- `src/runtime/yaml_loader.hpp`
- `src/runtime/config_validator.hpp`
- `src/runtime/compiled_config.hpp`
- `src/runtime/state_bootstrapper.hpp`

### Sprint técnica 4
- `src/runtime/datahub_runtime.hpp`
- `src/runtime/runtime_hub_access.hpp`
- `src/runtime/internal_producer_handle.hpp`
- `src/runtime/on_change_dispatcher.hpp`

### Sprint técnica 5
- `src/core/selector_parser.hpp`
- `src/core/text_template_compiler.hpp`
- `src/core/text_resolver.hpp`
- `src/core/predicate_compiler.hpp`
- `src/core/predicate_evaluator.hpp`

### Sprint técnica 6
- `src/adapters/file/file_source_adapter.hpp`
- `src/runtime/file_export_session.hpp`
- `src/runtime/file_export_scheduler.hpp`
- `src/runtime/manual_export_dispatcher.hpp`

### Sprint técnica 7
- `src/adapters/opc_ua/opc_ua_source_adapter.hpp`
- `src/adapters/opc_ua/opc_ua_sink_adapter.hpp`
- `src/adapters/opc_da/opc_da_source_adapter.hpp`
- `src/adapters/opc_da/opc_da_sink_adapter.hpp`
- `src/runtime/connector_runtime.hpp`

### Sprint técnica 8
- `test/core/*`
- `test/runtime/*`
- `test/adapters/*`
- `bench/*`
- CI e sanitizers

---

## 20. Recomendação prática de execução

A forma mais segura de tocar o projeto é:

- **planejar por fase**
- **executar por sprint**
- **fechar cada sprint com seu pacote explícito de testes**
- **fechar cada fase com suíte ampliada + regressão + sanitizers**

Em resumo:

- **fases** preservam a ordem arquitetural
- **sprints** preservam a velocidade
- **testes por sprint** preservam a qualidade
- **testes por fase** preservam a estabilidade do programa inteiro
