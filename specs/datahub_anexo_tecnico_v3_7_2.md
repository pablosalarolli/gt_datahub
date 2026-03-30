# DataHub — Anexo Técnico V3.7.2

## 1. Finalidade deste anexo

Este documento complementa a spec funcional e descreve uma arquitetura recomendada para implementação.

Ele existe para:

- organizar o código sem contaminar a spec funcional;
- registrar decisões internas de engenharia;
- orientar implementação incremental;
- reduzir risco de retrabalho;
- documentar decisões de concorrência, performance e lifecycle;
- formalizar a relação entre YAML, runtime, connectors e adapters.

A spec funcional continua sendo o documento que define o comportamento do produto.

---

## 2. Diretriz geral de arquitetura

A implementação deve separar claramente:

- **core**: domínio, tipos canônicos, API pública de leitura e lógica de estado;
- **runtime**: bootstrap, leitura do YAML, validação, criação de tokens/handles, agendamento e lifecycle;
- **connector runtimes**: objetos internos que representam cada `connector_id` compilado;
- **adapters**: facetas de integração concreta para source, sink e record sink.

### 2.1 Regra de dependência

- `core` não depende de `runtime`;
- `core` não depende de `adapters`;
- `runtime` pode depender de `core` e `adapters`;
- `adapters` dependem de `core`.

### 2.2 Princípio importante

O **DataHub core** deve ser um componente **passivo e thread-safe**.
Quem “roda” é o **runtime**, não o core.

---

## 3. Política de concorrência e threads

### 3.1 Regra geral

Se a implementação precisar de threads internas, a base recomendada e especificada para esta arquitetura é:

- `std::jthread`
- `std::stop_token`

A implementação não deve depender de `detach()` nem de parada não estruturada.

### 3.2 Onde threads podem existir

Threads internas podem existir, por exemplo, em:

- scheduler de exports periódicos;
- polling de source adapters;
- flush assíncrono de arquivo;
- workers de reconexão;
- filas seriais de export manual.

### 3.3 Onde threads não são obrigatórias

Não é obrigatório haver thread própria para:

- o core do hub;
- `getState()`;
- `getDefinition()`;
- `resolveText()`;
- estruturas de catálogo.

### 3.4 Diretriz de ciclo de vida

Toda thread interna deve:

- ser encapsulada em `std::jthread`;
- observar `std::stop_token`;
- parar de forma cooperativa;
- ser encerrada durante `runtime.stop()`;
- não sobreviver ao objeto que a possui.

### 3.5 Scheduler periódico — diretriz de drift

Para evitar drift acumulado, o scheduler periódico deve preferir `sleep_until()` com base em um próximo deadline monotônico, e não `sleep_for(period_)` em loop simples.

Para manter o comportamento previsível, esta versão **não faz catch-up de ticks atrasados**. Se o processamento atrasar além de `period_`, os ticks perdidos são descartados e o scheduler salta para o próximo deadline futuro.

Exemplo conceitual:

```cpp
class PeriodicScheduler {
public:
    explicit PeriodicScheduler(std::chrono::milliseconds period)
        : period_(period) {}

    void start() {
        worker_ = std::jthread([this](std::stop_token st) { run(st); });
    }

    void stop() {
        if (worker_.joinable()) {
            worker_.request_stop();
        }
    }

private:
    void run(std::stop_token st) {
        auto next = std::chrono::steady_clock::now() + period_;
        while (!st.stop_requested()) {
            std::this_thread::sleep_until(next);
            tick();
            next += period_;

            auto now = std::chrono::steady_clock::now();
            if (now > next) {
                auto missed = ((now - next) / period_) + 1;
                next += missed * period_;
            }
        }
    }

    void tick() {
        // avaliar exports periódicos elegíveis
    }

    std::chrono::milliseconds period_;
    std::jthread worker_;
};
```

---

## 4. Estrutura sugerida

Uma organização possível é:

```text
datahub/
  core/
    api/
    model/
    selectors/
    state/
  runtime/
    bootstrap/
    validation/
    scheduling/
    exports/
    connectors/
  adapters/
    opc_ua/
    opc_da/
    file/
```

O nome das pastas não é parte do contrato; a separação conceitual acima é.

---

## 5. Tipos centrais recomendados

### 5.1 Valor canônico

O `Value` pode seguir o variant da spec funcional.

### 5.1.1 Regra de clocks

A API pública expõe:

```cpp
using Timestamp = std::chrono::system_clock::time_point;
```

Isso é correto para representar **quando algo aconteceu no mundo real** e para serializar timestamps em CSV e logs.

Entretanto, o runtime **não deve** usar `system_clock` para calcular intervalos de:

- `stale_after_ms`
- `period_ms`
- timeouts
- backoff
- deadlines de scheduler

Esses cálculos internos devem usar `std::chrono::steady_clock`, pois ele é monotônico e não sofre saltos de NTP ou ajuste manual do relógio do SO.

### 5.2 Definição tipada

A definição compilada da variável deve usar `DataType` como enum tipado, não `std::string`.

### 5.3 Estado canônico

Uma estrutura típica é:

```cpp
struct VariableStateEntry {
    mutable std::shared_mutex mtx;

    Value value;
    Quality raw_quality{Quality::Uncertain};
    std::optional<Timestamp> source_timestamp;
    std::optional<Timestamp> hub_timestamp;
    std::uint64_t version{0};
    bool initialized{false};

    std::optional<std::chrono::steady_clock::time_point> last_update_steady;
};
```

Observação importante:

- a store mantém `raw_quality`;
- a API pública `getState()` retorna `quality` **efetiva**, já aplicando stale lazy.

### 5.4 Definição compilada

Uma estrutura compilada útil é:

```cpp
struct CompiledVariableDefinition {
    VariableDefinition public_definition;
    std::size_t variable_index;
};
```

Nesta versão, para evitar duplicação de contrato, valores como `default_value` e `stale_after_ms` permanecem apenas em `public_definition`. Se a implementação quiser cachear formas normalizadas internamente, isso deve ocorrer em estruturas auxiliares privadas, não duplicando o contrato compilado principal.

---

## 6. Catálogo, bootstrap de estado e lookup

### 6.1 Catálogo de definição

Após o bootstrap, o catálogo deve ser imutável.

Ele contém:

- definições compiladas;
- índices por nome;
- ownership de producer;
- referências para exports e bindings compilados.

### 6.2 Bootstrap de estado

A store de estado deve ser materializada durante o bootstrap por um componente dedicado, por exemplo `StateBootstrapper`.

Regra recomendada:

- o bootstrap lê o catálogo compilado;
- cria uma entrada de estado para cada variável;
- aplica `default_value` diretamente na store, sem passar por `submit()`;
- quando isso ocorrer, inicializa também `last_update_steady = steady_clock::now()` para que `stale_after_ms` não marque a variável como stale imediatamente.

Isso resolve a tensão entre catálogo imutável e store mutável:

- o catálogo continua sendo apenas definição;
- a store nasce inicializada com base no catálogo;
- não é necessário “submit sintético” para default.

### 6.3 Store de estado

A store pode ser um `std::unordered_map` ou vetor indexado por `variable_index`, desde que:

- a topologia não mude após o bootstrap;
- o lookup seja previsível;
- o hot path não precise travar o mapa inteiro.

### 6.4 Lookup heterogêneo em C++20

Se a store usar `std::unordered_map<std::string, ...>`, a implementação deve considerar lookup heterogêneo para evitar alocação temporária ao buscar por `std::string_view`.

Exemplo conceitual:

```cpp
struct TransparentHash {
    using is_transparent = void;

    std::size_t operator()(std::string_view v) const noexcept {
        return std::hash<std::string_view>{}(v);
    }

    std::size_t operator()(const std::string& v) const noexcept {
        return std::hash<std::string_view>{}(v);
    }
};

using StateMap = std::unordered_map<
    std::string,
    VariableStateEntry,
    TransparentHash,
    std::equal_to<>
>;
```

---

## 7. Concorrência, locking e hot path

### 7.1 Objetivo

Permitir alto throughput de escrita sem contenção desnecessária, especialmente em cenários industriais com polling/subscription frequentes.

### 7.2 Regra principal

Como a topologia da store não muda após o bootstrap, o runtime não precisa usar um lock global do mapa em cada update.

A recomendação é:

- lookup sem lock global em runtime;
- lock **por variável**, dentro da própria entrada de estado.

### 7.3 Estrutura sugerida

```cpp
struct VariableStateEntry {
    mutable std::shared_mutex mtx;
    // campos do estado
};
```

### 7.4 Consequência prática

Dois `submit` em variáveis diferentes devem poder ocorrer em paralelo, cada um travando apenas a entrada correspondente.

### 7.5 Política de acesso

- leitura pública de uma variável: lock compartilhado da entrada;
- escrita/submit daquela variável: lock exclusivo da entrada;
- estruturas imutáveis de definição: sem lock no hot path.

### 7.6 Observação importante

Se o container escolhido não garantir segurança de leitura concorrente sem mutação estrutural na forma desejada, uma alternativa robusta é:

- catálogo nome → índice imutável;
- store por índice em `std::vector<VariableStateEntry>`.

### 7.7 Movimento no hot path

A interface pública do writer interno recebe:

```cpp
std::expected<void, SubmitError> submit(UpdateRequest req);
```

Isso é correto e intencional.

No hot path, a implementação interna deve mover recursivamente os campos do `UpdateRequest` para a store trancada, evitando cópias desnecessárias, especialmente para `std::string`.

Em outras palavras:

- receber por valor;
- validar;
- fazer `std::move(req.value)` e mover demais campos para a entrada de estado.

---

## 8. Ownership, API pública e API interna

### 8.1 Problema a resolver

O invariante “1 producer por variável” fica fraco se existir um `submit(variable_name, ...)` público e genérico.

### 8.2 Solução recomendada

A API pública **não** deve expor `submit()` genérico por nome de variável.

Em vez disso:

- a aplicação obtém um `IInternalProducer` apenas para bindings `producer_kind: internal`;
- enquanto o handle existir, o `binding_id` correspondente permanece marcado como aberto;
- a destruição do handle ou `runtime.stop()` libera esse estado de abertura.
- adapters externos escrevem por um contrato interno, baseado em `ProducerToken`;
- cada token/handle está vinculado a exatamente uma variável e um binding configurado.

### 8.3 Interface interna recomendada

```cpp
struct ProducerToken {
    std::size_t binding_index;
    std::size_t variable_index;
};

struct RuntimeUpdateRequest {
    Value value;
    Quality quality{Quality::Good};
    std::optional<Timestamp> source_timestamp;
    bool synthetic{false};
};

class IRuntimeHubAccess {
public:
    virtual ~IRuntimeHubAccess() = default;

    virtual std::expected<void, SubmitError>
    submitFromProducer(ProducerToken token, RuntimeUpdateRequest req) = 0;

    virtual void markProducerConnectionBad(
        ProducerToken token,
        std::string_view reason) = 0;
};
```

### 8.4 Enforcement em runtime

`submitFromProducer()` deve validar, no mínimo:

- token válido;
- runtime iniciado;
- binding habilitado;
- token aponta para o owner configurado daquela variável.

Se houver divergência, o resultado deve ser `OwnershipViolation`.

### 8.5 API pública da aplicação

`openInternalProducer(binding_id)` só deve abrir bindings internos.

O objeto retornado deve encapsular internamente um `ProducerToken` privado, não exposto à aplicação.

Assim:

- a aplicação não consegue fabricar tokens;
- a aplicação não consegue escrever em variável externa;
- a aplicação não escolhe o nome da variável no submit.

O rastreamento interno de bindings internos abertos/fechados deve ser protegido por sincronização dedicada. A verificação de `AlreadyOpen`, a marcação de abertura e a liberação do estado na destruição do handle devem ocorrer sob a mesma proteção para evitar corrida entre destruição de um `IInternalProducer` e nova chamada de `openInternalProducer()` para o mesmo `binding_id`.

### 8.6 Updates sintéticos

A API interna precisa aceitar updates sintéticos do runtime, por exemplo:

- perda de conexão;
- valor invalidado pelo source;
- eventos internos de degradação.

Isso **não** deve ser exposto como API pública da aplicação.

---

## 9. Tipagem e coerção

### 9.1 Coerção restrita recomendada

O runtime deve validar o `Value` recebido contra o `DataType` da variável.

Coerções restritas aceitáveis, a critério da implementação, podem incluir:

- `int32_t` → `int64_t`
- `uint32_t` → `uint64_t`
- `float` → `double`

Coerções perigosas devem ser rejeitadas, por exemplo:

- `std::string` → número
- `double` → `uint32_t` sem regra explícita
- `bool` → inteiro por convenção implícita

### 9.2 Consequência prática na leitura tipada

Se a variável é `UInt32`, o runtime deve garantir que o estado armazenado efetivamente use `std::uint32_t`.

Isso evita cenários em que o valor “numericamente equivalente” entra como outro tipo do variant e falha em `std::get_if<std::uint32_t>()`.

---

## 10. Engine de seletores e `resolveText()`

### 10.1 Separação entre parser e resolução

Recomendação:

- parser no bootstrap para campos declarativos do YAML;
- avaliação/resolução em runtime sobre estruturas compiladas.

### 10.2 Contextos suportados

#### Chamada pública `resolveText()`

Suporta somente:

- `hub.*`
- `system.*`
- `export.*`

#### Execução de file export

Suporta:

- `hub.*`
- `system.*`
- `export.*`
- `context.*`

### 10.3 Falha em `resolveText()` público

A API pública deve retornar erro explícito para:

- sintaxe inválida;
- namespace inválido;
- variável inexistente;
- campo inexistente;
- `context.*` fora de export.

Valores ausentes ou não inicializados devem interpolar como string vazia.

### 10.4 `path_template` de file producer

O `path_template` deve ser compilado como template textual do mesmo motor de `target_template`, mas com restrição de namespaces válidos:

- `hub.*`
- `system.*`

`context.*` não é permitido.

---

## 11. Predicados de ativação

### 11.1 Representação recomendada

Uma representação eficiente é uma AST imutável.

Exemplo:

```cpp
struct ConditionNode;

struct ConditionLeaf {
    CompiledSelector source;
    Operator op;
    Value value;
    bool evaluate(const EvalContext& ctx) const;
};

struct ConditionAll {
    std::vector<std::unique_ptr<ConditionNode>> children;
    bool evaluate(const EvalContext& ctx) const;
};

struct ConditionAny {
    std::vector<std::unique_ptr<ConditionNode>> children;
    bool evaluate(const EvalContext& ctx) const;
};

struct ConditionNot {
    std::unique_ptr<ConditionNode> child;
    bool evaluate(const EvalContext& ctx) const;
};

struct ConditionNode {
    using Variant = std::variant<ConditionLeaf, ConditionAll, ConditionAny, ConditionNot>;
    Variant value;

    bool evaluate(const EvalContext& ctx) const;
};
```

### 11.2 `unique_ptr` em vez de `shared_ptr`

Como a árvore pertence unicamente ao export compilado e não requer ownership compartilhado, `std::unique_ptr` é preferível a `std::shared_ptr`.

Isso evita overhead atômico de contagem de referência.

A combinação `std::variant` + `std::unique_ptr` é uma escolha pragmática desta versão, não um compromisso definitivo de longo prazo. Se medições reais mostrarem custo relevante de indireção, a implementação pode migrar depois para outro formato interno equivalente.

### 11.3 Const-correctness obrigatória

Como múltiplos schedulers ou threads podem avaliar a mesma árvore compilada simultaneamente, o método `evaluate()` dos nós deve ser `const`.

A árvore compilada deve ser tratada como imutável durante avaliação.

### 11.4 Modelo de avaliação

Nesta versão:

- export periódico: `run_while` é avaliado no tick do scheduler, imediatamente antes da tentativa de gravação;
- export manual: `run_while` é avaliado no momento de `triggerFileExport(export_id)`;
- não há avaliação reativa baseada em subscriptions internas.

Essa escolha é deliberada para simplificar a arquitetura e evitar dependência de um barramento interno de eventos.

---

## 12. Avaliação de `stale_after_ms`

### 12.1 Diretriz da versão

A avaliação de stale deve ser **lazy**, e não feita por thread dedicada de varredura.

### 12.2 Regra de cálculo

Quando `getState()` ou o motor de export lê uma variável com `stale_after_ms` configurado:

- lê a `raw_quality`;
- compara `steady_clock::now()` com `last_update_steady`;
- se o intervalo exceder `stale_after_ms`, a qualidade efetiva exposta vira `Stale`.

### 12.3 Vantagem

Essa abordagem evita uma thread global de varredura e reduz custo em sistemas com muitas variáveis.

### 12.4 Observação

`raw_quality` pode ser `Bad` por perda de conexão. Nesse caso, a qualidade efetiva continua `Bad`; `Stale` não deve mascarar `Bad`.

---

## 13. Bootstrap, connector runtimes e validação

### 13.1 Pipeline sugerido

Fase de bootstrap (antes de `start()`):

1. carregar YAML;
2. validar schema básico;
3. compilar `variables`;
4. compilar `connectors`;
5. compilar `producer_bindings`;
6. compilar `consumer_bindings`;
7. compilar `file_exports`;
8. validar invariantes globais;
9. construir catálogo imutável;
10. materializar store inicial a partir do catálogo e de `default_value`;
11. criar `ConnectorRuntime` por `connector_id`;
12. criar adapters/facetas por uso compilado.

Abertura de recursos externos e início de threads/schedulers pertencem ao `start()`, não ao bootstrap.

### 13.2 Validações adicionais importantes

No bootstrap, validar explicitamente:

- `producer_kind: internal` não referencia `connector_id`;
- `producer_kind: internal` não tem `binding` externo;
- `producer_kind: internal` não tem `acquisition`;
- `connector.kind` suporta o modo configurado;
- `file_exports.connector_id` é realmente `kind: file`.

### 13.3 Relação formal entre connector YAML e adapters

O YAML de um `connector_id` deve compilar para um **ConnectorRuntime** interno.

Esse objeto representa o acesso lógico ao meio externo e pode expor uma ou mais facetas:

- source facet;
- sink facet;
- record sink facet.

### 13.4 Quando o mesmo connector é usado em múltiplos papéis

Se um mesmo `connector_id` for usado como producer e consumer:

- o recomendado é compartilhar a mesma conexão/sessão subjacente quando o protocolo e a biblioteca permitirem;
- as facetas `ISourceAdapter` e `ISinkAdapter` podem ser objetos distintos sobre o mesmo `ConnectorRuntime`.

Se o protocolo ou SDK não permitir compartilhamento real, a implementação pode usar sessões separadas, desde que:

- o `connector_id` continue sendo uma unidade lógica única;
- saúde, reconexão e habilitação sejam coordenadas nesse nível lógico.

---

## 14. Runtime e lifecycle

### 14.1 Start

`runtime.start()` deve:

- validar que o bootstrap foi concluído;
- abrir recursos externos;
- iniciar threads e schedulers necessários;
- permitir novas chamadas bem-sucedidas de `openInternalProducer()` somente **após** o start.

Nesta versão, `openInternalProducer()` antes de `start()` deve falhar com `RuntimeNotStarted`. Não existe handle “pré-aberto” pendente de ativação. Recursos externos e workers não são abertos no bootstrap; eles passam a existir somente a partir de `start()`.

### 14.2 Stop

`runtime.stop()` deve:

- solicitar parada cooperativa das `std::jthread`;
- drenar filas críticas quando necessário;
- fazer flush e fechamento de exports ativos;
- invalidar novos submits e novos triggers.

---

## 15. Adapters

### 15.1 Classificação recomendada

```cpp
class ISourceAdapter;
class ISinkAdapter;
class IRecordSinkAdapter;
```

### 15.2 `ISourceAdapter`

Responsável por:

- polling/subscription;
- transformação do dado externo em `RuntimeUpdateRequest`;
- submissão via `IRuntimeHubAccess`.

### 15.3 `ISinkAdapter`

Responsável por:

- escrita simples hub → endpoint;
- disparo por `on_change` nesta versão.

Semântica recomendada para `on_change` nesta versão: o adapter deve reagir a cada atualização aceita pelo hub para a variável associada, ou seja, a cada incremento de `version`. Não é necessário comparar o valor atual com o último valor enviado ao endpoint externo. Isso mantém comportamento consistente entre implementações OPC UA e OPC DA e evita estado extra no hot path.

Mecanismo recomendado nesta versão, sem polling adicional: após um `submitFromProducer()` aceito, o runtime deve consultar a lista pré-compilada de `consumer_bindings` associados à variável atualizada e enfileirar notificações internas leves para os `ISinkAdapter` correspondentes. O hub continua passivo; quem observa o resultado do submit é o runtime, no mesmo caminho interno da submissão aceita. Cada `ISinkAdapter` pode então consumir sua fila própria em worker dedicado ou execução serial equivalente.

Esse mecanismo é intencionalmente simples:

- não exige subscription pública no hub;
- não exige polling periódico para detectar mudança;
- centraliza a semântica de `on_change` no runtime compilado.

### 15.4 `IRecordSinkAdapter`

Responsável por:

- escrita multicoluna;
- gerenciamento de sessão lógica de export;
- cabeçalho, append e flush;
- serialização por `export_id`.

---

## 16. File source e file export

### 16.1 File como producer

Bindings `file.text` e `file.csv_last_row_column` devem:

- resolver `path_template` a cada tentativa de aquisição;
- ler o conteúdo desejado;
- converter para `Value` compatível;
- submeter via producer token correspondente.

### 16.2 Estratégia de export CSV

Para cada `file_export`, o runtime pode manter uma sessão com:

- `export_session_id`
- `current_target_path`
- `row_index`
- handle/stream do arquivo
- fila serial de gravação, se necessário

### 16.3 Semântica de `finalize_on_stop`

Quando `run_while` deixa de ser verdadeiro e `finalize_on_stop: true`:

- flush do stream;
- fechamento do handle;
- descarte da sessão atual;
- próxima ativação inicia nova sessão.

### 16.4 Falha em `target_template`

Se a resolução de `target_template` falhar por erro estrutural:

- o export daquela tentativa falha;
- nenhuma linha é gravada;
- o erro deve ser observável por log/métrica.

Se a resolução for válida, mas algum valor interpolado estiver ausente, o trecho correspondente vira string vazia.

Essa regra é intencionalmente diferente de `path_template` em file producers. Para `target_template`, interpolação vazia ainda pode resultar em destino controlado e observável. Para `path_template`, a implementação deve preferir ignorar a tentativa para não ler fonte errada por caminho degenerado.

### 16.5 Concorrência em export manual

`triggerFileExport(export_id)` pode ser chamado repetidamente e rapidamente pela aplicação.

Para evitar corrupção de arquivo, o `IRecordSinkAdapter` deve garantir serialização por `export_id`.

Duas estratégias aceitáveis:

- fila FIFO por `export_id`;
- lock serial por `export_id` com rejeição explícita quando já houver operação ativa.

A recomendação preferencial é **fila FIFO lógica por `export_id`**.

### 16.6 Consequência para a API

A API pública `triggerFileExport()` sinaliza aceite da requisição.

Ela não implica que o I/O já terminou no momento do retorno.

### 16.7 `append: false`

Ao abrir uma nova sessão com `append: false`, o arquivo deve ser aberto em truncamento.

Se o mesmo `target_template` for resolvido em sessões sucessivas, o conteúdo anterior pode ser sobrescrito pela nova sessão.

---

## 17. Perda de conexão e qualidade

### 17.1 Source adapter indisponível

Quando um source connector perde comunicação:

- o adapter ou o connector runtime deve notificar a camada interna;
- o runtime deve propagar um update sintético para os producers afetados.

### 17.2 Política mínima recomendada

Para cada variável afetada:

- preservar o último valor aceito, se existir;
- alterar `raw_quality` para `Bad`;
- atualizar `hub_timestamp`;
- incrementar `version`;
- manter `source_timestamp` anterior, se houver.

### 17.3 Reconexão

A reconexão deve ser tratada no nível do `ConnectorRuntime`, para que todas as facetas derivadas do mesmo `connector_id` sejam impactadas de forma coerente.

---

## 18. Observabilidade e diagnóstico

É recomendável expor, no mínimo:

- estado dos connectors (`connected`, `degraded`, `reconnecting`, `stopped`);
- contadores de submit aceitos/rejeitados;
- contadores de exports executados/falhos;
- última falha por `export_id`;
- última falha por `connector_id`;
- contadores de manual trigger aceitos/enfileirados/rejeitados.

---

## 19. Estratégia de testes

### 19.1 Unitários

- parser de seletores;
- parser de predicados;
- coerção de tipos;
- `default_value` → estado inicial;
- cálculo lazy de stale;
- `resolveText()` com erro estrutural e valor ausente;
- `append: false` e `finalize_on_stop`.

### 19.2 Integração

- `opc_ua` producer;
- `opc_ua` consumer;
- `opc_da` producer;
- `opc_da` consumer;
- `file.text` producer;
- `file.csv_last_row_column` producer;
- `file_export` periódico;
- `file_export` manual;
- perda de conexão e update sintético de qualidade.

### 19.3 Concorrência

- submits concorrentes em variáveis diferentes;
- leitura e escrita simultâneas da mesma variável;
- múltiplos triggers manuais para o mesmo `export_id`;
- scheduler periódico junto com manual trigger;
- avaliação simultânea da mesma AST de ativação.

---

## 20. Plano de implementação sugerido

### Fase 1
- tipos públicos
- catálogo
- store
- `getState`, `getDefinition`, `listVariables`

### Fase 2
- bootstrap YAML
- validações
- inicialização por `default_value`

### Fase 3
- API pública `IDataHubRuntime`
- `IInternalProducer`
- `IRuntimeHubAccess`
- enforcement de ownership

### Fase 4
- seletores
- `resolveText`
- predicados de ativação

### Fase 5
- `file_exports`
- manual trigger
- serialização por `export_id`

### Fase 6
- `opc_da`
- `opc_ua`
- `file` producer
- reconexão e qualidade sintética

---

## 21. Decisões deliberadamente deixadas para depois

Ficam explicitamente para versões futuras:

- hot-reload de configuração;
- criação/remoção dinâmica de variáveis;
- barramento interno reativo de subscriptions;
- expressões aritméticas textuais;
- sinks multicoluna para banco;
- histórico persistente embutido;
- compartilhamento de conexão entre facetas quando o SDK externo não suportar isso de forma limpa.

---

## 22. Conclusão técnica

A V3.7.2 fecha os principais pontos de arquitetura e implementação necessários para um DataHub robusto:

- ownership real de escrita;
- API pública sem `submit()` irrestrito;
- API interna específica para runtime e adapters;
- store concorrente com lock por variável;
- cálculo temporal baseado em `steady_clock`;
- bootstrap limpo de `default_value` sem acoplamento indevido com `submit()`;
- relação formal entre YAML, connector runtime e facetas de adapter;
- política clara para export manual concorrente e perda de conexão.
