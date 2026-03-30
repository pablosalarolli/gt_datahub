# Padrão de Documentação de Código — DataHub

Este documento consolida a diretriz transversal de documentação aplicada à biblioteca `gt_datahub`.

## Objetivo

Garantir que API pública, comportamento não óbvio e decisões sensíveis de contrato permaneçam claros desde as primeiras sprints.

## Regra geral

Toda API pública introduzida ou alterada deve sair documentada no mesmo merge.

A documentação mínima deve explicar, quando aplicável:

- responsabilidade do componente;
- contrato da função, enum, classe ou struct;
- invariantes relevantes;
- ownership e expectativas de lifetime;
- comportamento esperado em rejeição, falha ou uso incorreto;
- diferenças entre contrato público e contrato interno do runtime;
- o que é timestamp público e o que é cálculo monotônico interno.

## Onde a documentação é obrigatória

- classes e structs públicas;
- interfaces de integração;
- funções com contrato sensível ou comportamento não trivial;
- enums e políticas com impacto comportamental;
- componentes com lifecycle delicado;
- fluxos com comportamento deferido ou serializado;
- pontos onde a spec funcional e o anexo técnico se encontram.

## O que não precisa ser comentado

- getters/setters triviais;
- helpers locais autoexplicativos;
- testes cujo nome e corpo já expressem claramente a intenção.

## Comentários obrigatórios para gotchas

Quando o código tocar estes pontos, eles devem ser comentados de forma objetiva:

- invariante de `1 produtor por variável` e ausência de `submit()` público irrestrito;
- diferença entre `raw_quality` interno e `quality` efetiva exposta por `getState()`;
- `stale_after_ms` avaliado de forma lazy;
- `system_clock` para timestamps públicos e `steady_clock` para intervalos internos;
- `openInternalProducer()` com `AlreadyOpen` e reabertura após destruição do handle;
- `on_change` significando “toda atualização aceita”, isto é, incremento de `version`;
- `resolveText()` falhando em erro estrutural, mas interpolando string vazia para valor ausente;
- diferença semântica entre `path_template` de producer file e `target_template` de export;
- serialização por `export_id` em `triggerFileExport()`;
- `append: false` significando truncamento da nova sessão;
- callbacks/filas internas do runtime não devem capturar `this` de forma insegura;
- uso de `std::move` no hot path de `UpdateRequest` e `Value`.

## Documentação auxiliar

Quando docstrings não bastarem, criar documentação auxiliar em `docs/` ou `specs/` com foco em subsistemas específicos.

Exemplos recomendados:

- `docs/bootstrap-and-validation.md`
- `docs/internal-producer-lifecycle.md`
- `docs/selectors-and-text-resolution.md`
- `docs/file-export-lifecycle.md`
- `docs/connector-runtime-and-adapters.md`
- `docs/quality-and-stale.md`

## Critério de pronto relacionado à documentação

Nenhuma sprint que introduza ou altere API pública deve ser considerada concluída sem:

- docstrings mínimas atualizadas nas APIs tocadas;
- comentários adicionados nos gotchas relevantes;
- remoção de comentários obsoletos que contrariem a spec vigente;
- atualização de documentação auxiliar quando a docstring não for suficiente para explicar lifecycle, filas, ownership ou semântica de erro.

## Status esperado

Este padrão deve ser refletido desde o início nos headers públicos e nas classes de runtime com contrato sensível.
