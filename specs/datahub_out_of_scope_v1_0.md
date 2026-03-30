# Out of Scope for v1.0 — DataHub

Este documento registra o que não faz parte do escopo obrigatório para a primeira versão publicável da biblioteca `gt_datahub`.

## Objetivo

Evitar que a fase inicial da implementação perca foco por expansão prematura de escopo.

## Fora do escopo obrigatório da v1.0

- hot-reload de YAML;
- criação ou remoção dinâmica de variáveis em runtime;
- barramento interno reativo genérico de subscriptions;
- expressões aritméticas textuais além dos seletores e templates fechados pela baseline;
- suporte a formatos de export diferentes de `csv`;
- sinks multicoluna para banco de dados;
- histórico persistente embutido no DataHub;
- disparo manual de `consumer_bindings` para OPC UA/OPC DA;
- query language de histórico;
- clusterização/distribuição do hub entre processos ou máquinas;
- compatibilidade retroativa com specs anteriores à baseline sem migração formal;
- otimizações de performance não sustentadas por benchmark ou gargalo real;
- abstrações genéricas extras para suportar domínios ainda não consumidores da lib;
- UI própria da biblioteca;
- métricas avançadas, tracing distribuído ou telemetria remota antes do core estar fechado.

## Também não entra antes da hora

Enquanto as fases anteriores não estiverem fechadas, não antecipar:

- adapters OPC antes de catálogo/store/runtime básico estarem sólidos;
- refinamentos de reconexão antes de source/sink básicos funcionarem ponta a ponta;
- hardening pesado de performance antes de existir benchmark mínimo;
- integrações piloto reais antes de a suíte mínima, export e file producer estarem verdes;
- refinamentos estéticos de documentação antes de fechar contrato e comportamento.

## Como tratar novos pedidos fora do escopo

Quando surgir uma demanda fora deste recorte:

1. registrar como backlog futuro, se fizer sentido;
2. avaliar impacto na baseline;
3. só puxar para o plano ativo se houver mudança consciente de prioridade.
