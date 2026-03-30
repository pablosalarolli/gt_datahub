# Backlog Inicial — DataHub v3.7.2

Este backlog traduz a baseline v3.7.2 em um conjunto inicial de entregas vinculadas explicitamente às fases e sprints do roadmap.

## Prioridade imediata

### P0 — Foundation

- Fase 1 / Sprint 1.1: enums e tipos-base da API pública
- Fase 1 / Sprint 1.2: `Value`, `VariableDefinition`, `VariableState`, erros públicos
- Fase 1 / Sprint 1.3: `IDataHub`, `IDataHubRuntime`, `IInternalProducer`
- Fase 2 / Sprint 2.1: catálogo compilado e índice por nome
- Fase 2 / Sprint 2.2: state store, bootstrap por `default_value`, `getState/getDefinition/listVariables`
- Fase 2 / Sprint 2.3: qualidade efetiva e stale lazy
- Fase 3 / Sprint 3.1: parsing de `connectors` e `variables`
- Fase 3 / Sprint 3.2: parsing/validação de bindings e `file_exports`
- Fase 3 / Sprint 3.3: compilação de templates e predicados

### P1 — Core behavior

- Fase 4 / Sprint 4.1: runtime core, `start()/stop()` e bootstrap validado
- Fase 4 / Sprint 4.2: ownership enforcement e `openInternalProducer()`
- Fase 4 / Sprint 4.3: dispatch interno de `on_change` para sinks
- Fase 5 / Sprint 5.1: parser e resolução de seletores
- Fase 5 / Sprint 5.2: `resolveText()` público e contexto de export
- Fase 5 / Sprint 5.3: predicados de ativação e avaliação no tick
- Fase 6 / Sprint 6.1: `file.text` e `file.csv_last_row_column`
- Fase 6 / Sprint 6.2: `file_exports` periódicos/manuais e sessão CSV

### P2 — Integração externa e robustez

- Fase 6 / Sprint 6.3: perda de conexão, qualidade sintética e diagnósticos
- Fase 7 / Sprint 7.1: OPC UA source/sink
- Fase 7 / Sprint 7.2: OPC DA source/sink
- Fase 7 / Sprint 7.3: reconexão por `ConnectorRuntime`
- Fase 8 / Sprint 8.1: suíte unitária e integração base
- Fase 8 / Sprint 8.2: concorrência, sanitizers e análise estática
- Fase 9: integração piloto em aplicação real
- Fase 10: release 1.0 e rollout gradual

## Dependências principais

- Fase 1 precede todas as demais porque fecha a superfície pública mínima;
- Fase 2 depende da estabilização dos tipos-base da fase 1;
- Fase 3 depende de catálogo e store utilizáveis;
- Fase 4 depende de bootstrap/validação fechados;
- Fase 5 depende do runtime core existir;
- Fase 6 depende de templates, predicados e scheduler básico;
- Fase 7 depende do contrato de source/sink e do runtime interno já estarem estáveis;
- Fases 8 a 10 são de hardening, integração e release.

## Situação atual

- baseline funcional e técnica v3.7.2 fechada;
- pacote operacional de governança criado;
- implementação da biblioteca ainda deve assumir que o código parte praticamente do zero, salvo eventuais esqueletos locais;
- decisões mais sensíveis de contrato já estão fechadas: ownership de escrita, clocks, stale lazy, `on_change`, export manual e falta de hot-reload.
