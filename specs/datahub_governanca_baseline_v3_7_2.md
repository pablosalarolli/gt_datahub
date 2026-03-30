# Governança da Baseline — DataHub v3.7.2

Este documento congela a baseline arquitetural usada pela biblioteca `gt_datahub` a partir de março de 2026.

## Status da baseline

Os documentos abaixo passam a ser a única referência normativa de contrato para a evolução funcional e técnica da biblioteca:

- `datahub_spec_funcional_v3_7_2.md`
- `datahub_anexo_tecnico_v3_7_2.md`

Regras decorrentes:

- nenhuma versão anterior da spec deve ser usada como fonte de verdade concorrente;
- ambiguidades devem ser resolvidas por atualização explícita da baseline e não por interpretação local em código;
- qualquer implementação que divergir da v3.7.2 deve ser tratada como defeito, dívida técnica registrada ou mudança formal de contrato;
- o código deve refletir o contrato fechado de `1 produtor por variável`, `0..N consumidores`, YAML como vínculo oficial e ausência de `submit()` público irrestrito.

## Documentos operacionais vinculados a esta baseline

- `datahub_roadmap_v3_7_2.md`: plano mestre por fases e sprints;
- `datahub_checklist_avanco_implementacao_v3_7_2.md`: acompanhamento operacional do avanço;
- `datahub_out_of_scope_v1_0.md`: exclusões explícitas da release 1.0;
- `datahub_backlog_inicial_v3_7_2.md`: backlog inicial vinculado às fases e sprints;
- `datahub_padrao_documentacao_codigo.md`: consolidado da política de documentação de código.

## Convenções de nomenclatura

### YAML

- `connector_id`, `binding_id`, `export_id` e demais IDs técnicos usam `snake_case` minúsculo;
- `kind`, `type`, `mode`, `op` e chaves estruturais seguem `snake_case` minúsculo;
- nomes de variáveis de processo expostas ao hub preferem `UPPER_SNAKE_CASE` para facilitar leitura operacional e aderir aos exemplos da baseline;
- labels descritivos não substituem IDs técnicos.

### Código C++

- tipos, classes, structs e enums públicos usam `PascalCase`;
- funções públicas e helpers usam `camelCase`;
- variáveis locais e parâmetros usam `snake_case`;
- member variables usam prefixo `m_` seguido de `snake_case`;
- constantes locais `constexpr` podem usar prefixo `k` quando melhorarem leitura;
- arquivos `*.hpp` e `*.cpp` usam `snake_case`;
- subpastas em `include/`, `src/` e `test/` usam `snake_case`;
- namespaces públicos permanecem sob `gt::datahub`.

### Testes

- a árvore de `test/` deve espelhar a árvore pública de `include/gt_datahub/`;
- `test/main_test.cpp` permanece como entrypoint único da suíte;
- nomes de arquivos de teste seguem `<componente>_test.cpp`.

## Política de versionamento

A biblioteca adota versionamento semântico.

Regras iniciais:

- mudanças incompatíveis com a API pública incrementam `MAJOR`;
- funcionalidade nova compatível incrementa `MINOR`;
- correções compatíveis incrementam `PATCH`;
- enquanto a biblioteca estiver em `0.x`, quebras de API ainda podem acontecer, mas continuam exigindo registro formal de mudança de contrato;
- o manifesto de pacote e qualquer cabeçalho gerado com informações de versão devem refletir a mesma versão publicada.

## Política de mudanças pós-baseline

Depois do congelamento da v3.7.2, qualquer mudança proposta deve ser classificada em uma destas categorias:

1. implementação aderente à spec, sem mudança de contrato;
2. clarificação editorial da spec, sem mudança comportamental;
3. mudança de contrato, com atualização explícita da baseline, roadmap, checklist e backlog;
4. adiamento formal para backlog futuro ou `Out of Scope`.

Regras operacionais:

- não introduzir comportamento novo apenas por conveniência local;
- quando houver impacto em API pública, atualizar docstrings no mesmo merge;
- quando houver impacto em planejamento, atualizar o checklist de avanço no mesmo merge;
- comentários e documentação auxiliar que contrariem a baseline vigente devem ser removidos ou corrigidos;
- decisões do anexo técnico não devem ser “otimizadas” localmente de forma incompatível, sobretudo em clocks, locking, lifecycle e ownership.

## Decisões não reabertas nesta baseline

As decisões abaixo ficam tratadas como fechadas para a linha v3.7.2:

- cada variável possui exatamente 1 produtor lógico;
- a aplicação não possui `submit(variable_name, ...)` público irrestrito;
- publicação interna ocorre somente por `IInternalProducer` aberto por `binding_id` interno;
- `getState()` retorna qualidade efetiva, já considerando stale lazy;
- intervalos internos usam `steady_clock`, enquanto timestamps públicos usam `system_clock`;
- `consumer_bindings` suportam apenas `on_change` nesta versão;
- `on_change` significa “toda atualização aceita pelo hub”, isto é, incremento de `version`;
- `file_exports` suportam apenas `csv` na v1.0;
- `triggerFileExport()` existe apenas para `file_exports` manuais;
- `activation.run_while` é avaliado no tick do scheduler ou no trigger manual, não de forma reativa por subscription interna;
- hot-reload de YAML não faz parte da baseline.

## Definição prática de “fase 0 concluída”

A fase 0 só pode ser considerada totalmente concluída quando, além destes documentos:

- a baseline v3.7.2 estiver aceita como única fonte de verdade;
- o projeto criar/buildar em um ambiente com dependências instaladas;
- o pipeline mínimo local rodar sem erro;
- houver aprovação humana explícita do backlog inicial e do padrão de documentação.
