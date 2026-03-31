# Checklist de Avanço — Implementação da Lib a partir da Baseline DataHub v3.7.2

Este checklist foi derivado do roadmap `datahub_roadmap_v3_7_2.md` e serve como documento operacional para acompanhar a evolução da implementação.

## Como usar

- marque `[x]` quando o item estiver concluído;
- marque `[~]` quando estiver em andamento;
- mantenha observações curtas ao lado de itens críticos;
- não avance de fase sem fechar os critérios de saída da fase anterior;
- em cada sprint, conclua também o bloco de testes correspondente.

---

## Visão geral de progresso

- [x] Fase 0 — Congelamento da baseline e governança
- [x] Fase 1 — API pública e tipos centrais
- [x] Fase 2 — Catálogo, store e bootstrap de estado
- [x] Fase 3 — YAML compiler e validação
- [x] Fase 4 — Runtime, ownership e lifecycle
- [x] Fase 5 — Seletores, `resolveText()` e predicados
- [ ] Fase 6 — File source e file export
- [ ] Fase 7 — Adapters OPC UA / OPC DA
- [ ] Fase 8 — Testes automatizados, CI e hardening
- [ ] Fase 9 — Integração piloto em aplicação real
- [ ] Fase 10 — Release 1.0 e rollout gradual

---

## Checklist transversal — documentação de código

### Regra geral
- [x] Toda API pública nova sai com docstring mínima no mesmo merge em que for introduzida
- [ ] Toda API pública alterada tem docstring revisada no mesmo merge em que for modificada
- [ ] Comentários obsoletos que contrariem a baseline vigente são removidos
- [x] Decisões não óbvias são registradas com comentário curto no código ou nota em `docs/`

### Docstrings obrigatórias
- [x] Classes/structs públicas novas estão documentadas
- [x] Interfaces como `IDataHub`, `IDataHubRuntime` e `IInternalProducer` estão documentadas
- [x] Funções com contrato sensível estão documentadas
- [x] Enums e políticas com impacto comportamental estão documentados
- [x] Ownership/lifecycle e comportamento deferido estão descritos onde aplicável

### Gotchas que devem ter comentário explícito no código
- [x] `raw_quality` vs `quality` efetiva
- [x] stale lazy baseado em `steady_clock`
- [x] `system_clock` exposto publicamente apenas para timestamps reais
- [x] `openInternalProducer()` e liberação de `AlreadyOpen`
- [x] `on_change` = atualização aceita / incremento de `version`
- [ ] `resolveText()` estruturalmente inválido retorna erro
- [ ] `target_template` e `path_template` têm semânticas diferentes para valor ausente
- [ ] fila/serialização por `export_id` em `triggerFileExport()`
- [x] movimento de `UpdateRequest` / `Value` no hot path

### Documentação auxiliar em Markdown
- [ ] Criar `docs/bootstrap-and-validation.md`
- [ ] Criar `docs/internal-producer-lifecycle.md`
- [ ] Criar `docs/selectors-and-text-resolution.md`
- [ ] Criar `docs/file-export-lifecycle.md`
- [ ] Criar `docs/connector-runtime-and-adapters.md`
- [ ] Criar `docs/quality-and-stale.md`

---

## Fase 0 — Congelamento da baseline e governança

### Implementação
- [x] Declarar a baseline v3.7.2 como única referência de contrato
- [x] Criar documento `Out of Scope for v1.0`
- [x] Definir convenções de nomenclatura
- [x] Definir política de versionamento
- [x] Definir política de mudanças pós-baseline
- [x] Registrar decisões obrigatórias:
  - [x] 1 produtor por variável com enforcement em runtime
  - [x] sem `submit()` público irrestrito
  - [x] stale lazy em `getState()`
  - [x] `consumer_bindings` apenas `on_change`
  - [x] `file_exports` apenas `csv`
  - [x] hot-reload fora do escopo

### Testes / validação
- [x] Repositório cria e builda localmente
- [x] Pipeline mínimo local roda sem erro
- [x] Backlog inicial está vinculado às fases e sprints

### Critério de saída
- [x] A baseline deixou de ser tratada como rascunho aberto
- [x] Backlog inicial foi aprovado
- [x] Padrão de documentação de código foi aceito pela equipe

---

## Fase 1 — API pública e tipos centrais

### Sprint 1.1 — Enums e aliases públicos
#### Implementação
- [x] Criar `DataType`
- [x] Criar `Quality`
- [x] Criar `VariableRole`
- [x] Criar enums/structs de erro públicos
- [x] Criar alias `Timestamp`

#### Testes
- [x] UT: construção e comparação de enums
- [x] UT: códigos de erro têm valores/defaults coerentes
- [x] RT: includes públicos não quebram dependências
- [x] ST: TU simples compila e instancia tipos

### Sprint 1.2 — `Value`, definições e estado
#### Implementação
- [x] Implementar `Value` com `std::variant`
- [x] Implementar `VariableDefinition`
- [x] Implementar `VariableState`
- [x] Implementar `UpdateRequest`

#### Testes
- [x] UT: round-trip de armazenamento/extração por tipo
- [x] UT: acesso errado falha de forma limpa
- [x] UT: `VariableDefinition.default_value` aceita tipos compatíveis
- [x] RT: cópia/movimentação de `Value`
- [x] ST: exemplo mínimo com definição e estado

### Sprint 1.3 — Interfaces públicas
#### Implementação
- [x] Implementar headers de `IDataHub`
- [x] Implementar headers de `IDataHubRuntime`
- [x] Implementar headers de `IInternalProducer`
- [x] Fechar assinaturas públicas da baseline

#### Testes
- [x] UT: mocks simples compõem contra as interfaces
- [x] RT: includes públicos continuam independentes
- [x] ST: app mínima compila contra a API

### Critério de saída da fase
- [x] A API pública compila
- [x] Os nomes principais estão estabilizados
- [x] Os tipos base não estão mais em disputa
- [x] Docstrings mínimas das APIs públicas foram adicionadas

---

## Fase 2 — Catálogo, store e bootstrap de estado

### Sprint 2.1 — Catálogo compilado
#### Implementação
- [x] Implementar `CompiledVariableDefinition`
- [x] Implementar índice por nome
- [x] Implementar lookup heterogêneo por `std::string_view`
- [x] Fechar mapa imutável após bootstrap

#### Testes
- [x] UT: lookup por nome existente
- [x] UT: lookup por nome inexistente
- [x] UT: heterogeneous lookup não exige alocação temporária observável
- [x] ST: catálogo mínimo compila e consulta

### Sprint 2.2 — State store e bootstrap
#### Implementação
- [x] Implementar `VariableStateEntry`
- [x] Implementar bootstrap do estado a partir do catálogo
- [x] Aplicar `default_value` diretamente na store
- [x] Implementar `getState`, `getDefinition`, `listVariables`

#### Testes
- [x] UT: `default_value` inicializa variável como `initialized=true`
- [x] UT: `getState()` retorna `std::nullopt` para variável inexistente
- [x] UT: `listVariables()` retorna conjunto estável pós-bootstrap
- [x] ST: bootstrap completo com duas variáveis

### Sprint 2.3 — Qualidade efetiva e stale lazy
#### Implementação
- [x] Implementar `raw_quality` interno
- [x] Implementar avaliação lazy de stale
- [x] Garantir regra `Bad` vence `Stale`

#### Testes
- [x] UT: variável sem stale configurado não envelhece
- [x] UT: stale é derivado em leitura, não por thread de varredura
- [x] UT: `Bad` prevalece sobre `Stale`
- [x] ST: leitura de qualidade efetiva ponta a ponta

### Critério de saída da fase
- [x] Catálogo e store funcionam corretamente
- [x] `getState()` já reflete a semântica pública da baseline

---

## Fase 3 — YAML compiler e validação

### Sprint 3.1 — Parsing de `connectors` e `variables`
#### Implementação
- [x] Implementar loader YAML básico
- [x] Implementar parsing de `connectors`
- [x] Implementar parsing de `variables`
- [x] Validar unicidade de IDs e nomes

#### Testes
- [x] UT: YAML mínimo válido
- [x] UT: `connector_id` duplicado falha
- [x] UT: `variable.name` duplicado falha
- [x] ST: carregamento de configuração simples

### Sprint 3.2 — Parsing e validação de bindings
#### Implementação
- [x] Implementar parsing de `producer_bindings`
- [x] Implementar parsing de `consumer_bindings`
- [x] Implementar parsing de `file_exports`
- [x] Validar capacidades por `kind`
- [x] Validar ownership de producer

#### Testes
- [x] UT: binding para connector inexistente falha
- [x] UT: mais de 1 producer para a mesma variável falha
- [x] UT: `producer_kind: internal` sem `connector_id` é aceito
- [x] UT: `consumer_bindings` com modo inválido falham
- [x] ST: arquivo consolidado da spec valida

### Sprint 3.3 — Templates e predicados compilados
#### Implementação
- [x] Compilar `source` canônico
- [x] Compilar templates interpolados
- [x] Compilar AST de predicados
- [x] Validar operadores unários/binários

#### Testes
- [x] UT: seletor puro válido
- [x] UT: template válido com `${...}`
- [x] UT: operador unário sem `value` é aceito
- [x] UT: operador binário sem `value` falha
- [x] ST: predicado `all/any/not` compilado corretamente

### Critério de saída da fase
- [x] Configuração YAML relevante da baseline compila e valida
- [x] Erros de configuração surgem no bootstrap

---

## Fase 4 — Runtime, ownership e lifecycle

### Sprint 4.1 — Runtime core
#### Implementação
- [x] Implementar `DataHubRuntime`
- [x] Separar bootstrap de `start()`
- [x] Implementar `start()` e `stop()`
- [x] Inicializar connector runtimes e scheduler apenas no `start()`

#### Testes
- [x] UT: `start()` duplo falha com erro coerente
- [x] UT: `stop()` é idempotente
- [x] ST: runtime sobe e desce sem adapters reais

### Sprint 4.2 — Ownership de escrita e internal producer
#### Implementação
- [x] Implementar `IRuntimeHubAccess`
- [x] Implementar `openInternalProducer()`
- [x] Implementar estado `AlreadyOpen` protegido contra corrida
- [x] Liberar abertura ao destruir o handle

#### Testes
- [x] UT: abrir binding interno válido
- [x] UT: segunda abertura do mesmo binding falha com `AlreadyOpen`
- [x] UT: destruir handle libera nova abertura
- [x] UT: `submit()` com tipo incompatível é rejeitado
- [x] ST: app publica por `IInternalProducer`

### Sprint 4.3 — Dispatch de `on_change`
#### Implementação
- [x] Implementar despacho interno para sinks após submit aceito
- [x] Enfileirar notificações leves por binding consumidor
- [x] Garantir que hub permaneça passivo

#### Testes
- [x] UT: update aceito gera notificação interna para sink correspondente
- [x] UT: variável sem consumer binding não gera trabalho adicional
- [x] ST: update interno aciona sink fake

### Critério de saída da fase
- [x] Runtime básico já sobe, aceita producer interno e despacha `on_change`
- [x] Invariante de 1 produtor está realmente reforçado

---

## Fase 5 — Seletores, `resolveText()` e predicados

### Sprint 5.1 — Resolução textual pública
#### Implementação
- [x] Implementar `resolveText()` público
- [x] Fechar namespaces públicos permitidos
- [x] Implementar serialização textual canônica dos campos selecionáveis

#### Testes
- [x] UT: `hub.<var>.value`
- [x] UT: `hub.<var>.quality`
- [x] UT: `hub.<var>.initialized` serializa como `true/false`
- [x] UT: `hub.<var>.version` serializa como decimal
- [x] UT: `context.row_index` serializa como inteiro decimal sem sinal
- [x] ST: template textual completo resolve corretamente

### Sprint 5.2 — Semântica de erro e valor ausente
#### Implementação
- [x] Retornar erro estrutural em `resolveText()`
- [x] Interpolar string vazia para valor ausente
- [x] Diferenciar `path_template` e `target_template`

#### Testes
- [x] UT: namespace inválido retorna erro
- [x] UT: variável desconhecida retorna erro
- [x] UT: valor ausente interpola vazio
- [x] ST: `path_template` ausente ignora tentativa sem tocar estado

### Sprint 5.3 — Avaliação de predicados
#### Implementação
- [x] Implementar `evaluate() const`
- [x] Avaliar predicados no tick/trigger
- [x] Aplicar `finalize_on_stop`

#### Testes
- [x] UT: nó folha binário
- [x] UT: nó folha unário
- [x] UT: composição `all/any/not`
- [x] ST: export periódico respeita `activation`

### Critério de saída da fase
- [x] Seletores, templates e predicados funcionam como na baseline
- [x] `resolveText()` já é utilizável pela aplicação e pelo export

---

## Fase 6 — File source e file export

### Sprint 6.1 — Producers de arquivo
#### Implementação
- [ ] Implementar `file.text`
- [ ] Implementar `file.csv_last_row_column`
- [ ] Resolver `path_template` a cada tentativa
- [ ] Converter para `Value` compatível

#### Testes
- [ ] UT: leitura de arquivo texto
- [ ] UT: leitura da última linha de CSV com header
- [ ] UT: arquivo ausente pode ser ignorado quando configurado
- [ ] ST: arquivo alimenta variável do hub

### Sprint 6.2 — Export CSV
#### Implementação
- [ ] Implementar sessão de export por `export_id`
- [ ] Implementar `append`, cabeçalho e `row_index`
- [ ] Implementar `trigger.mode: periodic` e `manual`
- [ ] Implementar `triggerFileExport()`

#### Testes
- [ ] UT: log periódico contínuo sem `activation`
- [ ] UT: export manual aceita requisição e grava uma linha
- [ ] UT: `append: false` trunca nova sessão
- [ ] UT: `write_header_if_missing` só escreve em arquivo novo/truncado
- [ ] ST: export consolidado da spec gera CSV esperado

### Sprint 6.3 — Qualidade, falhas e serialização por `export_id`
#### Implementação
- [ ] Implementar fila/serialização por `export_id`
- [ ] Implementar falha de `target_template`
- [ ] Implementar updates sintéticos em perda de conexão

#### Testes
- [ ] UT: múltiplos triggers manuais do mesmo export não corrompem arquivo
- [ ] UT: perda de conexão marca `raw_quality = Bad`
- [ ] ST: finalização de sessão por `finalize_on_stop`

### Critério de saída da fase
- [ ] file source e file export funcionam ponta a ponta
- [ ] manual trigger e scheduler periódico estão fechados

---

## Fase 7 — Adapters OPC UA / OPC DA

### Sprint 7.1 — OPC UA source/sink
#### Implementação
- [ ] Implementar `ISourceAdapter` OPC UA
- [ ] Implementar `ISinkAdapter` OPC UA
- [ ] Integrar com `ConnectorRuntime`

#### Testes
- [ ] IT: OPC UA producer
- [ ] IT: OPC UA consumer
- [ ] ST: perda/reconexão básica de sessão OPC UA

### Sprint 7.2 — OPC DA source/sink
#### Implementação
- [ ] Implementar `ISourceAdapter` OPC DA
- [ ] Implementar `ISinkAdapter` OPC DA
- [ ] Integrar com `ConnectorRuntime`

#### Testes
- [ ] IT: OPC DA producer
- [ ] IT: OPC DA consumer
- [ ] ST: perda/reconexão básica de sessão OPC DA

### Sprint 7.3 — Reconexão coerente por connector runtime
#### Implementação
- [ ] Centralizar reconexão no `ConnectorRuntime`
- [ ] Propagar impacto coerente para facetas source/sink do mesmo connector
- [ ] Expor estado mínimo de diagnóstico

#### Testes
- [ ] IT: mesmo connector usado como source e sink reconecta de forma coerente
- [ ] ST: diagnóstico básico de connector indisponível

### Critério de saída da fase
- [ ] OPC UA e OPC DA funcionam em source/sink
- [ ] reconexão básica já é coerente por `connector_id`

---

## Fase 8 — Testes automatizados, CI e hardening

### Sprint 8.1 — Organização da suíte
#### Implementação
- [ ] Separar suites por módulo
- [ ] Criar comando único local para smoke
- [ ] Automatizar execução mínima por sprint

#### Testes
- [ ] UT: suites por módulo rodam isoladamente
- [ ] RT: execução do conjunto mínimo por sprint
- [ ] ST: comando único local para smoke

### Sprint 8.2 — Concorrência, sanitizers e análise estática
#### Implementação
- [ ] Integrar ASan
- [ ] Integrar UBSan
- [ ] Integrar clang-tidy/cppcheck
- [ ] Avaliar TSan

#### Testes
- [ ] SAT: ASan
- [ ] SAT: UBSan
- [ ] SAT: clang-tidy/cppcheck
- [ ] SAT: TSan, se viável
- [ ] CT: submits concorrentes em variáveis diferentes
- [ ] CT: trigger manual concorrente no mesmo `export_id`

### Critério de saída da fase
- [ ] CI bloqueia regressões reais
- [ ] métricas básicas e concorrência crítica são conhecidas

---

## Fase 9 — Integração piloto em aplicação real

### Implementação
- [ ] Escolher 1 aplicação piloto de complexidade média
- [ ] Integrar pelo menos 1 producer interno
- [ ] Integrar pelo menos 1 source externo e 1 sink externo
- [ ] Integrar pelo menos 1 `file_export`
- [ ] Rodar shadow mode quando possível

### Testes
- [ ] IT: comportamento antigo vs novo comparado em shadow mode, se possível
- [ ] IT: reconexão de connector
- [ ] IT: export durante corrida com `activation`
- [ ] PT: carga realista de updates

### Critério de saída
- [ ] 1 caso real operando de forma aceitável com a nova lib

---

## Fase 10 — Release 1.0 e rollout gradual

### Implementação
- [ ] Fechar versão `1.0.0`
- [ ] Criar guia de adoção
- [ ] Criar changelog inicial
- [ ] Criar exemplo oficial compilável
- [ ] Criar checklist de integração para novos consumidores

### Testes
- [ ] RT: suíte completa verde
- [ ] IT: exemplo oficial compila e roda
- [ ] SAT: CI final de release
- [ ] PT: benchmarks de referência congelados

### Critério de saída
- [ ] A lib é publicável e utilizável sem depender da spec a cada dúvida operacional

---

## Checklist transversal — pontos críticos da baseline que não podem ser esquecidos

### Semântica de estado
- [ ] `getState()` retorna qualidade efetiva
- [ ] stale é lazy
- [ ] `Bad` vence `Stale`
- [ ] `default_value` inicializa estado no bootstrap
- [ ] `version` incrementa em toda atualização aceita

### Ownership e lifecycle
- [ ] sem `submit()` público irrestrito
- [ ] `IInternalProducer` só abre para binding interno válido
- [ ] `AlreadyOpen` é protegido contra corrida
- [ ] destruir handle libera reabertura
- [ ] `stop()` invalida handles e encerra threads

### Seletores e serialização textual
- [ ] `resolveText()` falha em erro estrutural
- [ ] valor ausente interpola string vazia
- [ ] `context.row_index` é 0-based e serializa como decimal
- [ ] `context.export_session_id` serializa como texto decimal
- [ ] timestamps públicos seguem formato fechado da baseline

### File export e runtime
- [ ] `triggerFileExport()` existe apenas para exports manuais
- [ ] manual trigger respeita `activation`
- [ ] `append: false` trunca nova sessão
- [ ] serialização por `export_id` evita corrupção de arquivo
- [ ] scheduler usa `steady_clock` e não faz catch-up

### Conectores e adapters
- [ ] direction é definida pelo binding, não pelo connector
- [ ] capacidades por `kind` são respeitadas
- [ ] perda de conexão gera update sintético coerente
- [ ] reconexão é centralizada por `ConnectorRuntime`
