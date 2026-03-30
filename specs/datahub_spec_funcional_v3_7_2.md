# DataHub — Spec Funcional V3.7.2

## 1. Objetivo

O DataHub é uma biblioteca de integração e orquestração de variáveis para aplicações industriais.

Seu papel é:

- manter um **registro central de variáveis**;
- permitir que cada variável seja **alimentada por exatamente 1 produtor lógico**;
- permitir que cada variável alimente **0 a N consumidores**;
- permitir que a ligação entre variáveis e conectores seja feita por **YAML**, sem necessidade de alterar código da aplicação;
- permitir acesso a **valor e metadados de estado** por **seletores e expressões**;
- permitir **salvamento em arquivos com múltiplas colunas**, de forma **periódica** ou **sob demanda**, também configurado por YAML;
- permitir que a própria aplicação publique variáveis internas **apenas por bindings internos autorizados**, sem um `submit()` público irrestrito.

Esta spec define o **comportamento funcional esperado**, a **semântica dos blocos YAML** e a **API mínima pública** para integração em aplicações C++.

---

## 2. Escopo

### 2.1 Esta spec cobre

- modelo conceitual do hub;
- invariantes funcionais;
- definição de variáveis por YAML;
- definição de conectores por YAML;
- direção do fluxo entre meio externo e hub;
- ligação entre variáveis e conectores por YAML;
- seletores e expressões oficiais;
- exportação de arquivos multicoluna;
- uso de arquivo como fonte de dados;
- semântica de ativação de exports;
- semântica de falha para exports e conectores;
- semântica de `default_value`;
- API C++ pública para leitura, resolução textual, export manual e publicação por produtores internos autorizados.

### 2.2 Esta spec não congela

Esta spec não congela:

- estrutura de pastas do projeto;
- classes concretas de runtime;
- classes concretas de adapters;
- estratégia interna de filas, retry, reconexão e pooling;
- layout interno exato de memória;
- estratégia interna de logging e métricas;
- hot-reload de YAML.

Esses pontos ficam no **Anexo Técnico**.

### 2.3 Fora de escopo desta versão

Estão fora do escopo desta versão:

- criação de variáveis em runtime;
- recarga dinâmica de configuração sem reinício;
- escrita multicoluna em banco;
- query language genérica para histórico;
- janelas reativas de ativação baseadas em subscriptions internas;
- disparo manual de `consumer_bindings` para sinks diretos.

---

## 3. Modelo conceitual

### 3.1 Variável do hub

Uma variável do hub representa um ponto lógico centralizado no sistema.

Exemplos:

- `NUMERO_CORRIDA`
- `CORRIDA_ATIVA`
- `TEMP_MAX_FUNDO_PANELA`
- `ALARME_ESCORIA`

Cada variável possui:

- nome único;
- tipo de dado;
- valor atual;
- qualidade;
- timestamps;
- versão;
- metadados de definição.

### 3.2 Produtor

É a origem autorizada da atualização de uma variável.

Exemplos:

- um item OPC DA;
- um node OPC UA;
- um leitor de arquivo;
- a própria aplicação C++ por meio de um **binding interno**.

### 3.3 Consumidor

É um destino que recebe ou utiliza o valor da variável.

Exemplos:

- escrita em OPC UA;
- escrita em OPC DA;
- exportação para arquivo;
- leitura pela aplicação.

### 3.4 Conector

É a configuração de acesso a um meio externo.

Exemplos:

- um servidor OPC UA;
- um servidor OPC DA;
- um diretório de arquivos;
- um banco de dados.

### 3.5 Direção do fluxo

**Conector não é “de leitura” nem “de escrita” por si só.**

Conector representa um acesso externo reutilizável.
A direção do fluxo é definida pelo bloco YAML que o usa:

- `producer_bindings` = **externo → hub**;
- `consumer_bindings` = **hub → externo**;
- `file_exports` = **hub → arquivo multicoluna**.

### 3.6 Exportação de arquivo

É uma operação que gera registros em arquivo a partir de uma ou mais variáveis do hub.

A exportação pode ser:

- **periódica**;
- **manual/sob demanda**.

### 3.7 Produtor interno da aplicação

A aplicação não publica valores por um `submit(variable_name, ...)` genérico.

Em vez disso, ela obtém, no runtime, um **writer interno vinculado a um binding interno específico**. Assim:

- a escrita fica restrita às variáveis configuradas como `producer_kind: internal`;
- o invariante de **1 produtor por variável** é verificável em runtime;
- a aplicação não consegue escrever em variáveis com produtor externo usando a API pública.

---

## 4. Invariantes funcionais

As regras abaixo são obrigatórias.

### 4.1 Registro central

Todas as variáveis observáveis pelo sistema passam pelo DataHub.

### 4.2 Um produtor por variável

Cada variável do hub pode receber dados de **exatamente 1 produtor lógico**.

Esse invariante deve ser reforçado por configuração e por runtime.

### 4.3 Zero a N consumidores por variável

Cada variável do hub pode alimentar **0 a N consumidores**.

Exemplos válidos:

- uma variável existir apenas no hub;
- uma variável alimentar arquivo e OPC UA ao mesmo tempo;
- uma variável alimentar múltiplos arquivos;
- uma variável ser lida pela aplicação e também exportada.

### 4.4 YAML como contrato oficial de ligação

A ligação entre variáveis e conectores deve ser feita por YAML.

A aplicação não deve precisar alterar código para:

- criar uma nova variável declarativa;
- trocar um endpoint OPC;
- ligar uma variável a um novo arquivo;
- alterar colunas de um CSV;
- mudar um export de periódico para manual;
- mudar a condição de ativação de um export.

### 4.5 Publicação interna controlada

A aplicação pode produzir valores diretamente para o hub **somente** por meio de bindings configurados como `producer_kind: internal`.

### 4.6 Valor e metadados de estado acessíveis por seletores

Toda variável deve expor pelo menos:

- `value`
- `quality`
- `source_timestamp`
- `hub_timestamp`
- `version`
- `initialized`

### 4.7 Falha cedo para configuração inválida

Erros de configuração devem aparecer no bootstrap, não apenas em runtime.

### 4.8 Tipagem consistente entre definição e estado

O runtime deve validar toda atualização recebida contra o `data_type` da variável.

Atualizações com tipo incompatível devem ser rejeitadas, salvo coercões restritas explicitamente suportadas.

---

## 5. Modelo mínimo de dados da variável

### 5.1 Campos obrigatórios de estado

Cada variável deve manter, no mínimo:

- `value`: valor canônico atual;
- `quality`: qualidade efetiva atual;
- `source_timestamp`: timestamp de origem, quando houver;
- `hub_timestamp`: timestamp em que o hub aceitou ou sintetizou a atualização, quando existir;
- `version`: contador monotônico de atualização aceita;
- `initialized`: indica se a variável já possui valor inicializável.

### 5.2 Metadados de definição

Cada variável pode ter, opcionalmente:

- `role`
- `description`
- `unit`
- `groups`
- `labels` (lista simples de tags textuais, não mapa chave-valor)
- `default_value`
- `min_value`
- `max_value`
- `precision`
- `historize`
- `stale_after_ms`

### 5.3 Tipos mínimos suportados

O conjunto mínimo suportado é:

- `Bool`
- `Int32`
- `UInt32`
- `Int64`
- `UInt64`
- `Float`
- `Double`
- `String`
- `DateTime`

`Duration` e `Json` não fazem parte do conjunto funcional mínimo desta versão.

### 5.4 Qualidade mínima suportada

O conjunto mínimo de qualidade suportado é:

- `good`
- `bad`
- `stale`
- `uncertain`

Regra funcional importante:

- `bad` tem precedência sobre `stale`;
- `stale` não deve mascarar `bad`;
- a qualidade exposta por `getState()` é a **qualidade efetiva**.

### 5.5 Semântica de `default_value`

Se uma variável declarar `default_value`, o runtime deve inicializar o estado dessa variável no bootstrap com:

- `initialized = true`
- `value = default_value`
- `quality = uncertain`
- `source_timestamp` ausente
- `hub_timestamp` ausente
- `version = 0`

Para fins de `stale_after_ms`, essa inicialização conta como atualização inicial interna do runtime.
Assim, a variável não deve nascer imediatamente `stale`.

Se `default_value` não existir, a variável começa com:

- `initialized = false`
- `value` ausente
- `quality = uncertain`
- timestamps ausentes
- `version = 0`

`default_value` é, portanto, **semântica funcional de inicialização de estado**, e não apenas metadado informativo.

### 5.6 Semântica de `role`

Nesta versão, `role` no YAML não é texto livre.

Os valores suportados são exatamente:

- `State`
- `Measurement`
- `Alarm`
- `Command`
- `Calculated`
- `Other`

Valores não reconhecidos devem falhar no bootstrap com erro de configuração, em vez de serem mapeados silenciosamente.

### 5.7 Semântica de `labels`

Para manter o sistema simples, `labels` é uma **lista de tags textuais simples**.

Exemplo:

```yaml
labels: [processo, corrida, online]
```

Semântica chave-valor para labels fica fora do escopo desta versão.

---

## 6. Seletores e expressões

### 6.1 Dois formatos oficiais

Esta versão possui **dois formatos oficiais complementares**.

#### a) Seletor canônico puro

Usado quando o campo inteiro representa apenas uma referência.

Sintaxe:

```text
namespace.identificador.campo
```

Exemplos:

```text
hub.NUMERO_CORRIDA.value
hub.TEMP_MAX_FUNDO_PANELA.quality
export.captured_at
context.export_id
system.now
```

#### b) Seletor interpolado em texto

Usado quando a referência aparece dentro de uma string maior.

Sintaxe:

```text
${namespace.identificador.campo}
```

Exemplos:

```text
${hub.NUMERO_CORRIDA.value}
corridas/${hub.NUMERO_CORRIDA.value}.csv
${export.captured_at};${hub.ALARME_ESCORIA.value}
```

### 6.2 Regra de uso por campo YAML

- `source`: aceita **seletor canônico puro**, sem `${...}`;
- `run_while.<leaf>.source`: aceita **seletor canônico puro**, sem `${...}`;
- `expression`: aceita **texto interpolado** com `${...}`;
- `target_template`: aceita **texto interpolado** com `${...}`;
- `path_template`: aceita **texto interpolado** com `${...}`.

Em `columns`, para manter a configuração simples, a coluna pode usar **exatamente um** dos campos abaixo:

- `source` para referência simples;
- `expression` para texto interpolado.

Exemplos válidos:

```yaml
source: hub.TEMP_MAX_FUNDO_PANELA.value
expression: "${hub.NUMERO_CORRIDA.value}#${hub.TEMP_MAX_FUNDO_PANELA.quality}"
target_template: "corridas/${hub.NUMERO_CORRIDA.value}_${system.now}.csv"
path_template: "entrada/${hub.NUMERO_CORRIDA.value}.txt"
```

### 6.3 Namespaces oficiais

Os namespaces funcionais desta versão são:

- `hub`
- `context`
- `export`
- `system`

`internal.*` é reservado para uso interno do runtime e **não faz parte do contrato YAML desta versão**.

### 6.4 Campos oficiais para `hub.<variavel>`

Os campos oficiais acessíveis por seletor são:

- `value`
- `quality`
- `source_timestamp`
- `hub_timestamp`
- `version`
- `initialized`

### 6.5 Campos oficiais para `context.*`

Dentro da execução de `file_exports`, os campos mínimos suportados são:

- `context.export_id`
- `context.export_session_id`
- `context.row_index`
- `context.trigger_mode`
- `context.target_path`
- `context.session_started_at`

### 6.6 Campos oficiais para `export.*`

Os campos mínimos suportados são:

- `export.captured_at`

`export.captured_at` representa o instante lógico de captura da linha ou da resolução atual.

### 6.7 Campos de definição não acessíveis por seletor

Campos de definição como:

- `role`
- `unit`
- `description`
- `labels` (lista simples de tags textuais, não mapa chave-valor)
- `groups`
- `default_value`

**não são acessíveis por seletor nesta versão**.

### 6.8 Contexto suportado por `resolveText()`

A chamada pública `resolveText()` da API C++ suporta apenas:

- `hub.*`
- `system.*`
- `export.*`

Nesta chamada pública:

- `export.captured_at` representa o instante da própria resolução;
- `context.*` **não é válido**;
- `internal.*` **não é válido**.

### 6.9 Regras de serialização textual

- `hub.<variavel>.quality` deve ser serializada em texto canônico: `good`, `bad`, `stale`, `uncertain`;
- `source_timestamp` e `hub_timestamp` devem ser serializados em ISO-8601 UTC com milissegundos;
- `export.captured_at` deve ser serializado em ISO-8601 UTC com milissegundos;
- `context.session_started_at` deve ser serializado em ISO-8601 UTC com milissegundos;
- `context.row_index` deve ser serializado como inteiro decimal sem sinal, base 10;
- `hub.<variavel>.initialized` deve ser serializado como `true` ou `false`;
- `hub.<variavel>.version` deve ser serializado como inteiro decimal sem sinal;
- `system.now` deve ser serializado em formato compacto **seguro para nomes de arquivo**:

```text
YYYYMMDDTHHMMSSmmm
```

Exemplo:

```text
20260327T214501123
```

### 6.10 Semântica de erro e de valor ausente

Há duas situações diferentes:

1. **Erro estrutural**
   - namespace inválido;
   - variável inexistente;
   - campo inexistente;
   - contexto não permitido.
   Nesse caso, `resolveText()` deve retornar erro explícito.

2. **Valor válido, porém ausente ou não inicializado**
   - seletor correto;
   - variável existente;
   - valor nulo ou não inicializado.
   Nesse caso, a interpolação deve resultar em **string vazia**, e não em erro estrutural.

---

## 7. Modelo YAML

A configuração deve permitir, no mínimo, os seguintes blocos:

```yaml
datahub:
  schema_version: 1
  runtime: {}
  connectors: []
  variables: []
  producer_bindings: []
  consumer_bindings: []
  file_exports: []
```

### 7.1 Connectors

Cada conector define um acesso externo reutilizável.

Exemplos de `kind`:

- `opc_ua`
- `opc_da`
- `file`
- `db`

Exemplo:

```yaml
connectors:
  - id: opc_ua_main
    kind: opc_ua
    enabled: true
    settings:
      server_url: "opc.tcp://127.0.0.1:49310"
      reconnect_ms: 2000
      request_timeout_ms: 1000

  - id: opc_da_main
    kind: opc_da
    enabled: true
    settings:
      server_name: "Kepware.KEPServerEX.V6"
      host: "192.168.0.50"
      reconnect_ms: 2000
      request_timeout_ms: 1000

  - id: file_main
    kind: file
    enabled: true
    settings:
      base_path: "C:/dados/processo"
      flush_ms: 1000
```

### 7.2 Matriz mínima de capacidades por `kind`

| kind   | `producer_bindings` | `consumer_bindings` | `file_exports` |
|--------|----------------------|---------------------|----------------|
| `opc_ua` | sim | sim | não |
| `opc_da` | sim | sim | não |
| `file`   | sim | não | sim |
| `db`     | reservado | reservado | não |

Observações:

- `consumer_bindings` desta versão modelam apenas escrita simples do hub para endpoints diretos;
- saída para arquivo multicoluna é feita exclusivamente por `file_exports`;
- entrada a partir de arquivo é feita exclusivamente por `producer_bindings` com `binding.type` do namespace `file.*`.

### 7.3 Modos válidos por `kind`

#### Producer bindings

| connector kind | `acquisition.mode` válidos |
|---|---|
| `opc_ua` | `subscription`, `polling` |
| `opc_da` | `polling` |
| `file` | `polling` |
| `db` | reservado |

#### Consumer bindings

| connector kind | `trigger.mode` válidos |
|---|---|
| `opc_ua` | `on_change` |
| `opc_da` | `on_change` |
| `file` | não aplicável |
| `db` | reservado |

Observação: `manual` em `consumer_bindings` fica fora do escopo desta versão. Escritas sob demanda para sinks diretos devem ser modeladas pela própria aplicação ou por versão futura da API, não por `consumer_bindings` nesta V3.7.2.

Semântica de `on_change` nesta versão: o consumer binding deve tentar escrever sempre que a variável associada receber uma atualização aceita pelo hub, isto é, sempre que `version` for incrementado para aquela variável, mesmo que o novo `value` seja igual ao anterior. O gatilho não depende de comparar com o último valor enviado ao endpoint externo.

### 7.4 Variables

As variáveis devem poder ser definidas integralmente por YAML.

Exemplo:

```yaml
variables:
  - name: NUMERO_CORRIDA
    data_type: UInt32
    role: State
    description: "Número da corrida em andamento"

  - name: CORRIDA_ATIVA
    data_type: Bool
    role: State
    default_value: false

  - name: EM_MANUTENCAO
    data_type: Bool
    role: State
    default_value: false

  - name: TEMP_MAX_FUNDO_PANELA
    data_type: Float
    role: Measurement
    unit: "°C"
    precision: 1
    stale_after_ms: 5000

  - name: ALARME_ESCORIA
    data_type: Bool
    role: Alarm
    default_value: false
```

### 7.5 Producer bindings

Cada variável deve apontar para exatamente 1 produtor lógico.

#### Regras importantes

- `producer_kind` pode ser `internal` ou `connector`;
- se `producer_kind: internal`, então:
  - `connector_id` deve estar ausente;
  - `acquisition` deve estar ausente;
  - `binding` externo deve estar ausente;
- se `producer_kind: connector`, então:
  - `connector_id` é obrigatório;
  - `binding` é obrigatório.

#### Exemplo 1 — producer interno

```yaml
producer_bindings:
  - id: pb_temp_interna
    variable_name: TEMP_MAX_FUNDO_PANELA
    producer_kind: internal
    enabled: true
```

#### Exemplo 2 — OPC UA como produtor

```yaml
producer_bindings:
  - id: pb_numero_corrida_opcua
    variable_name: NUMERO_CORRIDA
    producer_kind: connector
    connector_id: opc_ua_main
    enabled: true
    acquisition:
      mode: subscription
    binding:
      type: opc_ua.node
      ns: 2
      item_id: "N1-ACI.CONV1.NUMERO_CORRIDA"
```

#### Exemplo 3 — OPC DA como produtor

```yaml
producer_bindings:
  - id: pb_corrida_ativa_opcda
    variable_name: CORRIDA_ATIVA
    producer_kind: connector
    connector_id: opc_da_main
    enabled: true
    acquisition:
      mode: polling
      poll_interval_ms: 500
    binding:
      type: opc_da.item
      item_id: "ACIARIA.CONV1.CORRIDA_ATIVA"
      access_path: ""
```

#### Exemplo 4 — arquivo texto como produtor

```yaml
producer_bindings:
  - id: pb_receita_texto
    variable_name: RECEITA_ATUAL
    producer_kind: connector
    connector_id: file_main
    enabled: true
    acquisition:
      mode: polling
      poll_interval_ms: 1000
    binding:
      type: file.text
      path_template: "entrada/receita_${hub.NUMERO_CORRIDA.value}.txt"
      encoding: "utf-8"
      read_mode: whole_file
```

#### Exemplo 5 — coluna da última linha de CSV como produtor

```yaml
producer_bindings:
  - id: pb_temp_csv
    variable_name: TEMP_MAX_FUNDO_PANELA
    producer_kind: connector
    connector_id: file_main
    enabled: true
    acquisition:
      mode: polling
      poll_interval_ms: 1000
    binding:
      type: file.csv_last_row_column
      path_template: "entrada/processo.csv"
      column_name: "temp_max_fundo_panela"
      has_header: true
      delimiter: ","
      skip_if_missing: true
```

### 7.6 Consumer bindings

`consumer_bindings` modelam escrita simples do hub para endpoints diretos.

#### Exemplo 1 — OPC UA como consumidor

```yaml
consumer_bindings:
  - id: cb_alarme_escoria_opcua
    variable_name: ALARME_ESCORIA
    connector_id: opc_ua_main
    enabled: true
    trigger:
      mode: on_change
    binding:
      type: opc_ua.node
      ns: 2
      item_id: "N1-ACI.CONV1.ALARME_ESCORIA"
```

#### Exemplo 2 — OPC DA como consumidor

```yaml
consumer_bindings:
  - id: cb_alarme_escoria_opcda
    variable_name: ALARME_ESCORIA
    connector_id: opc_da_main
    enabled: true
    trigger:
      mode: on_change
    binding:
      type: opc_da.item
      item_id: "ACIARIA.CONV1.ALARME_ESCORIA"
      access_path: ""
```

### 7.7 File exports

`file_exports` modelam escrita multicoluna do hub para arquivo.

#### Regra semântica importante

- `trigger` define **quando o runtime tenta gravar**;
- `activation` define **em que janela a gravação é permitida**;
- se `activation` for omitido, o export é considerado **sempre ativo**.

#### Formato

Nesta versão, o campo `format` é obrigatório para extensibilidade futura, porém apenas o valor `csv` é aceito.

Para manter a versão simples, o CSV desta versão usa convenções fixas:

- `delimiter = ","`
- `newline = "\n"`
- `null_text = ""`

Esses três itens não são configuráveis nesta versão.

#### Política de valor nulo ou não inicializado

Se uma coluna referenciar uma variável válida, mas sem valor inicializado, a célula exportada deve ser **string vazia**.

Isso **não** aborta a gravação da linha.

#### Exemplo 1 — log periódico contínuo, sem ativação

```yaml
file_exports:
  - id: exp_log_continuo
    enabled: true
    connector_id: file_main
    format: csv
    target_template: "logs/continuo_${system.now}.csv"
    append: false
    write_header_if_missing: true

    trigger:
      mode: periodic
      period_ms: 1000

    columns:
      - name: ts
        source: export.captured_at
      - name: numero_corrida
        source: hub.NUMERO_CORRIDA.value
      - name: temp_max
        expression: "${hub.TEMP_MAX_FUNDO_PANELA.value}"
```

#### Exemplo 2 — export periódico durante a corrida

```yaml
file_exports:
  - id: exp_corrida
    enabled: true
    connector_id: file_main
    format: csv
    target_template: "corridas/corrida_${hub.NUMERO_CORRIDA.value}.csv"
    append: true
    write_header_if_missing: true

    trigger:
      mode: periodic
      period_ms: 1000

    activation:
      run_while:
        all:
          - source: hub.CORRIDA_ATIVA.value
            op: eq
            value: true
          - not:
              source: hub.EM_MANUTENCAO.value
              op: eq
              value: true
      finalize_on_stop: true

    columns:
      - name: ts
        expression: "${export.captured_at}"
      - name: row_index
        expression: "${context.row_index}"
      - name: session_id
        expression: "${context.export_session_id}"
      - name: numero_corrida
        expression: "${hub.NUMERO_CORRIDA.value}"
      - name: temp_max
        expression: "${hub.TEMP_MAX_FUNDO_PANELA.value}"
      - name: quality
        expression: "${hub.TEMP_MAX_FUNDO_PANELA.quality}"
```

#### Exemplo 3 — snapshot manual

```yaml
file_exports:
  - id: exp_snapshot_manual
    enabled: true
    connector_id: file_main
    format: csv
    target_template: "snapshots/snapshot_${system.now}.csv"
    append: false
    write_header_if_missing: true

    trigger:
      mode: manual

    columns:
      - name: ts
        expression: "${export.captured_at}"
      - name: export_id
        expression: "${context.export_id}"
      - name: trigger_mode
        expression: "${context.trigger_mode}"
      - name: numero_corrida
        expression: "${hub.NUMERO_CORRIDA.value}"
      - name: alarme
        expression: "${hub.ALARME_ESCORIA.value}"
```

### 7.8 Colunas de export

Cada coluna deve definir:

- `name`
- exatamente um entre `source` e `expression`

Exemplo:

```yaml
columns:
  - name: ts
    source: export.captured_at

  - name: corrida
    source: hub.NUMERO_CORRIDA.value

  - name: q_temp
    expression: "${hub.TEMP_MAX_FUNDO_PANELA.quality}"
```

### 7.9 Predicados de ativação

`activation.run_while` é uma expressão booleana estruturada em YAML.

Os exemplos desta seção são ilustrativos e assumem que todas as variáveis referenciadas já foram declaradas no bloco `variables:` da configuração real.

#### a) Nó folha

```yaml
run_while:
  source: hub.CORRIDA_ATIVA.value
  op: eq
  value: true
```

#### b) Composição lógica

```yaml
run_while:
  all:
    - source: hub.CORRIDA_ATIVA.value
      op: eq
      value: true
    - source: hub.MODO_OPERACAO.value
      op: in
      value: [AUTO, TESTE]
```

#### Operadores suportados nesta versão

- `eq`
- `ne`
- `gt`
- `ge`
- `lt`
- `le`
- `in`
- `not_in`
- `is_true`
- `is_false`
- `is_null`
- `is_not_null`

Regra do campo `value` nesta versão:

- `value` é **obrigatório** para operadores binários: `eq`, `ne`, `gt`, `ge`, `lt`, `le`, `in`, `not_in`;
- `value` deve estar **ausente** para operadores unários: `is_true`, `is_false`, `is_null`, `is_not_null`.

#### Exemplo simples

```yaml
activation:
  run_while:
    source: hub.CORRIDA_ATIVA.value
    op: eq
    value: true
  finalize_on_stop: true
```

#### Exemplo com `all`

```yaml
activation:
  run_while:
    all:
      - source: hub.CORRIDA_ATIVA.value
        op: eq
        value: true
      - source: hub.ALARME_ESCORIA.value
        op: is_false
  finalize_on_stop: true
```

#### Exemplo com `any`

```yaml
activation:
  run_while:
    any:
      - source: hub.FORCAR_SALVAMENTO.value
        op: eq
        value: true
      - all:
          - source: hub.CORRIDA_ATIVA.value
            op: eq
            value: true
          - source: hub.NIVEL_LOG.value
            op: ge
            value: 2
  finalize_on_stop: true
```

#### Exemplo com `not`

```yaml
activation:
  run_while:
    all:
      - source: hub.CORRIDA_ATIVA.value
        op: eq
        value: true
      - not:
          source: hub.EM_MANUTENCAO.value
          op: eq
          value: true
  finalize_on_stop: true
```

### 7.10 Exemplo YAML consolidado

```yaml
datahub:
  schema_version: 1

  connectors:
    - id: opc_ua_main
      kind: opc_ua
      enabled: true
      settings:
        server_url: "opc.tcp://127.0.0.1:49310"
        reconnect_ms: 2000
        request_timeout_ms: 1000

    - id: opc_da_main
      kind: opc_da
      enabled: true
      settings:
        server_name: "Kepware.KEPServerEX.V6"
        host: "192.168.0.50"
        reconnect_ms: 2000
        request_timeout_ms: 1000

    - id: file_main
      kind: file
      enabled: true
      settings:
        base_path: "C:/dados/processo"
        flush_ms: 1000

  variables:
    - name: NUMERO_CORRIDA
      data_type: UInt32
      role: State
      description: "Número da corrida em andamento"

    - name: CORRIDA_ATIVA
      data_type: Bool
      role: State
      default_value: false

    - name: EM_MANUTENCAO
      data_type: Bool
      role: State
      default_value: false

    - name: TEMP_MAX_FUNDO_PANELA
      data_type: Float
      role: Measurement
      unit: "°C"
      precision: 1
      stale_after_ms: 5000

    - name: RECEITA_ATUAL
      data_type: String
      role: State

    - name: ALARME_ESCORIA
      data_type: Bool
      role: Alarm
      default_value: false

  producer_bindings:
    - id: pb_numero_corrida_opcua
      variable_name: NUMERO_CORRIDA
      producer_kind: connector
      connector_id: opc_ua_main
      enabled: true
      acquisition:
        mode: subscription
      binding:
        type: opc_ua.node
        ns: 2
        item_id: "N1-ACI.CONV1.NUMERO_CORRIDA"

    - id: pb_corrida_ativa_opcda
      variable_name: CORRIDA_ATIVA
      producer_kind: connector
      connector_id: opc_da_main
      enabled: true
      acquisition:
        mode: polling
        poll_interval_ms: 500
      binding:
        type: opc_da.item
        item_id: "ACIARIA.CONV1.CORRIDA_ATIVA"
        access_path: ""

    - id: pb_receita_texto
      variable_name: RECEITA_ATUAL
      producer_kind: connector
      connector_id: file_main
      enabled: true
      acquisition:
        mode: polling
        poll_interval_ms: 1000
      binding:
        type: file.text
        path_template: "entrada/receita_${hub.NUMERO_CORRIDA.value}.txt"
        encoding: "utf-8"
        read_mode: whole_file

    - id: pb_temp_interna
      variable_name: TEMP_MAX_FUNDO_PANELA
      producer_kind: internal
      enabled: true

  consumer_bindings:
    - id: cb_alarme_escoria_opcua
      variable_name: ALARME_ESCORIA
      connector_id: opc_ua_main
      enabled: true
      trigger:
        mode: on_change
      binding:
        type: opc_ua.node
        ns: 2
        item_id: "N1-ACI.CONV1.ALARME_ESCORIA"

    - id: cb_alarme_escoria_opcda
      variable_name: ALARME_ESCORIA
      connector_id: opc_da_main
      enabled: true
      trigger:
        mode: on_change
      binding:
        type: opc_da.item
        item_id: "ACIARIA.CONV1.ALARME_ESCORIA"
        access_path: ""

  file_exports:
    - id: exp_corrida
      enabled: true
      connector_id: file_main
      format: csv
      target_template: "corridas/corrida_${hub.NUMERO_CORRIDA.value}.csv"
      append: true
      write_header_if_missing: true
      trigger:
        mode: periodic
        period_ms: 1000
      activation:
        run_while:
          all:
            - source: hub.CORRIDA_ATIVA.value
              op: eq
              value: true
            - not:
                source: hub.EM_MANUTENCAO.value
                op: eq
                value: true
        finalize_on_stop: true
      columns:
        - name: ts
          expression: "${export.captured_at}"
        - name: export_id
          expression: "${context.export_id}"
        - name: session_id
          expression: "${context.export_session_id}"
        - name: row_index
          expression: "${context.row_index}"
        - name: numero_corrida
          expression: "${hub.NUMERO_CORRIDA.value}"
        - name: receita
          expression: "${hub.RECEITA_ATUAL.value}"
        - name: temp_max
          expression: "${hub.TEMP_MAX_FUNDO_PANELA.value}"
        - name: quality
          expression: "${hub.TEMP_MAX_FUNDO_PANELA.quality}"

    - id: exp_snapshot_manual
      enabled: true
      connector_id: file_main
      format: csv
      target_template: "snapshots/snapshot_${system.now}.csv"
      append: false
      write_header_if_missing: true
      trigger:
        mode: manual
      columns:
        - name: ts
          expression: "${export.captured_at}"
        - name: trigger_mode
          expression: "${context.trigger_mode}"
        - name: numero_corrida
          expression: "${hub.NUMERO_CORRIDA.value}"
        - name: alarme
          expression: "${hub.ALARME_ESCORIA.value}"
```

---

## 8. API C++ pública mínima

### 8.1 Tipos públicos mínimos

```cpp
namespace gt::datahub {

using Timestamp = std::chrono::system_clock::time_point;

enum class DataType {
    Bool,
    Int32,
    UInt32,
    Int64,
    UInt64,
    Float,
    Double,
    String,
    DateTime
};

enum class Quality {
    Good,
    Bad,
    Stale,
    Uncertain
};

enum class VariableRole {
    State,
    Measurement,
    Alarm,
    Command,
    Calculated,
    Other
};

using Value = std::variant<
    std::monostate,
    bool,
    std::int32_t,
    std::uint32_t,
    std::int64_t,
    std::uint64_t,
    float,
    double,
    std::string,
    Timestamp
>;

struct VariableDefinition {
    std::string name;
    DataType data_type;
    VariableRole role{VariableRole::Other};
    std::string description;
    std::string unit;
    std::vector<std::string> groups;
    std::vector<std::string> labels; // tags textuais simples
    std::optional<Value> default_value;
    std::optional<Value> min_value;
    std::optional<Value> max_value;
    std::optional<int> precision;
    bool historize{false};
    std::optional<std::chrono::milliseconds> stale_after_ms;
};

struct VariableState {
    Value value;
    Quality quality{Quality::Uncertain};   // qualidade efetiva, já considerando stale lazy
    std::optional<Timestamp> source_timestamp;
    std::optional<Timestamp> hub_timestamp;
    std::uint64_t version{0};
    bool initialized{false};
};

struct UpdateRequest {
    Value value;
    Quality quality{Quality::Good};
    std::optional<Timestamp> source_timestamp;
};

enum class ResolveErrorCode {
    InvalidSyntax,
    InvalidNamespace,
    UnknownVariable,
    UnknownField,
    InvalidContext
};

struct ResolveError {
    ResolveErrorCode code;
    std::string message;
};

enum class SubmitErrorCode {
    InvalidType,
    TypeCoercionFailed,
    BindingDisabled,
    OwnershipViolation,
    RuntimeStopped
};

struct SubmitError {
    SubmitErrorCode code;
    std::string message;
};

enum class TriggerErrorCode {
    UnknownExport,
    ExportDisabled,
    InvalidTriggerMode,
    ActivationInactive,
    RuntimeStopped
};

struct TriggerError {
    TriggerErrorCode code;
    std::string message;
};

} // namespace gt::datahub
```

### 8.2 Interfaces públicas mínimas

```cpp
namespace gt::datahub {

class IDataHub {
public:
    virtual ~IDataHub() = default;

    virtual std::optional<VariableState> getState(std::string_view variable_name) const = 0;
    virtual std::optional<VariableDefinition> getDefinition(std::string_view variable_name) const = 0;
    virtual std::vector<std::string> listVariables() const = 0;

    virtual std::expected<std::string, ResolveError>
    resolveText(std::string_view expression) const = 0;
};

class IInternalProducer {
public:
    virtual ~IInternalProducer() = default;

    virtual std::string_view bindingId() const noexcept = 0;
    virtual std::string_view variableName() const noexcept = 0;

    virtual std::expected<void, SubmitError>
    submit(UpdateRequest req) = 0;
};

enum class OpenProducerErrorCode {
    UnknownBinding,
    NotInternalProducer,
    BindingDisabled,
    AlreadyOpen,
    RuntimeNotStarted,
    RuntimeStopped
};

struct OpenProducerError {
    OpenProducerErrorCode code;
    std::string message;
};

enum class RuntimeErrorCode {
    InvalidConfiguration,
    BootstrapFailed,
    AlreadyStarted
};

struct RuntimeError {
    RuntimeErrorCode code;
    std::string message;
};

class IDataHubRuntime {
public:
    virtual ~IDataHubRuntime() = default;

    virtual std::expected<void, RuntimeError> start() = 0;
    virtual void stop() = 0;

    virtual IDataHub& hub() noexcept = 0;
    virtual const IDataHub& hub() const noexcept = 0;

    virtual std::expected<std::unique_ptr<IInternalProducer>, OpenProducerError>
    openInternalProducer(std::string_view binding_id) = 0;

    virtual std::expected<void, TriggerError>
    triggerFileExport(std::string_view export_id) = 0;
};

} // namespace gt::datahub
```

### 8.3 Contrato público e contrato interno

A API acima é o contrato **público** para a aplicação.

O runtime e os adapters exigem um contrato **interno**, não exposto à aplicação, para:

- submissão por tokens de producer configurados;
- updates sintéticos de perda de conexão;
- marcação interna de qualidade;
- integração com scheduler e adapters.

Esse contrato interno é descrito no **Anexo Técnico**.

### 8.4 Semântica mínima da API

#### `hub().getState(variable_name)`

- retorna `std::nullopt` se a variável não existir;
- retorna a **qualidade efetiva**, já considerando `stale_after_ms` via avaliação lazy;
- não cria variáveis implicitamente.

#### `hub().getDefinition(variable_name)`

- retorna `std::nullopt` se a variável não existir;
- retorna o conjunto de metadados de definição expostos na API pública.

#### `hub().listVariables()`

- retorna a lista das variáveis registradas;
- a ordem pode ser de registro ou outra ordem documentada pela implementação;
- a spec não exige hot-reload, portanto a lista é estável após bootstrap nesta versão.

#### `hub().resolveText(expression)`

- retorna `std::expected<std::string, ResolveError>`;
- falha para erro estrutural de seletor ou contexto;
- não falha por valor ausente ou não inicializado, que deve interpolar como string vazia.

#### `openInternalProducer(binding_id)`

- só funciona para bindings configurados como `producer_kind: internal`;
- falha para binding inexistente, externo, desabilitado, runtime ainda não iniciado ou runtime parado;
- se o mesmo `binding_id` já estiver aberto, a segunda chamada falha com `AlreadyOpen`;
- retorna um writer vinculado a um único binding interno;
- quando o `std::unique_ptr<IInternalProducer>` retornado é destruído, o `binding_id` volta ao estado **não aberto** e pode ser reaberto;
- `runtime.stop()` invalida todos os handles abertos e também libera o estado de abertura para o próximo `start()`.

#### `IInternalProducer::submit(UpdateRequest)`

- aplica validação de tipo contra o `data_type` da variável;
- aceita coerções restritas quando suportadas;
- falha se o binding estiver desabilitado ou se o runtime estiver parado;
- não permite redirecionar a escrita para outra variável;
- não altera o ciclo de vida de abertura do handle; o binding permanece aberto até a destruição do handle ou `stop()`.

#### `triggerFileExport(export_id)`

- só funciona para exports com `trigger.mode: manual`;
- falha se o export não existir, estiver desabilitado ou o runtime estiver parado;
- se houver `activation` e o predicado estiver falso no momento do disparo, retorna erro `ActivationInactive` e **não grava nada**;
- sinaliza aceite da requisição, não conclusão síncrona do I/O.

---

## 9. Exemplos de integração em C++

### 9.1 Exemplo 1 — inicializar runtime

```cpp
#include <memory>
#include <expected>

std::unique_ptr<gt::datahub::IDataHubRuntime> make_runtime_from_yaml(
    std::string_view yaml_path);

int main() {
    auto runtime = make_runtime_from_yaml("config/datahub.yaml");

    auto started = runtime->start();
    if (!started) {
        // log started.error().message
        return 1;
    }

    return 0;
}
```

### 9.2 Exemplo 2 — abrir producer interno e publicar valor

```cpp
auto temp_writer_result = runtime->openInternalProducer("pb_temp_interna");
if (!temp_writer_result) {
    // log temp_writer_result.error().message
    return;
}

auto temp_writer = std::move(temp_writer_result.value());

gt::datahub::UpdateRequest req;
req.value = 812.4f;
req.quality = gt::datahub::Quality::Good;
req.source_timestamp = std::chrono::system_clock::now();

auto submitted = temp_writer->submit(std::move(req));
if (!submitted) {
    // log submitted.error().message
}
```

### 9.3 Exemplo 3 — ler definição e estado

```cpp
const auto& hub = runtime->hub();

auto def = hub.getDefinition("TEMP_MAX_FUNDO_PANELA");
if (def) {
    // def->data_type, def->role, def->unit, def->stale_after_ms...
}

auto state = hub.getState("TEMP_MAX_FUNDO_PANELA");
if (state) {
    // state->quality já é a qualidade efetiva
}
```

### 9.4 Exemplo 4 — resolver expressão textual

```cpp
auto text = runtime->hub().resolveText(
    "Corrida ${hub.NUMERO_CORRIDA.value} @ ${export.captured_at}");

if (!text) {
    // log text.error().message
} else {
    // usar text.value()
}
```

### 9.5 Exemplo 5 — disparar export manual

```cpp
auto triggered = runtime->triggerFileExport("exp_snapshot_manual");
if (!triggered) {
    // UnknownExport, InvalidTriggerMode, ActivationInactive...
}
```

### 9.6 Exemplo 6 — leitura tipada segura

```cpp
auto state = runtime->hub().getState("NUMERO_CORRIDA");
if (state && state->initialized) {
    if (auto value = std::get_if<std::uint32_t>(&state->value)) {
        // usar *value
    }
}
```

### 9.7 Exemplo 7 — classe de integração da aplicação

```cpp
class AppIntegration {
public:
    explicit AppIntegration(std::unique_ptr<gt::datahub::IDataHubRuntime> runtime)
        : runtime_(std::move(runtime)) {}

    bool start() {
        auto started = runtime_->start();
        if (!started) {
            return false;
        }

        auto writer_result = runtime_->openInternalProducer("pb_temp_interna");
        if (!writer_result) {
            return false;
        }

        temp_writer_ = std::move(writer_result.value());
        return true;
    }

    void publishTemperature(float temp_c) {
        if (!temp_writer_) {
            return;
        }

        gt::datahub::UpdateRequest req;
        req.value = temp_c;
        req.quality = gt::datahub::Quality::Good;
        req.source_timestamp = std::chrono::system_clock::now();

        auto result = temp_writer_->submit(std::move(req));
        if (!result) {
            // log result.error().message
        }
    }

    void snapshot() {
        auto result = runtime_->triggerFileExport("exp_snapshot_manual");
        if (!result) {
            // log result.error().message
        }
    }

    const gt::datahub::IDataHub& hub() const noexcept {
        return runtime_->hub();
    }

private:
    std::unique_ptr<gt::datahub::IDataHubRuntime> runtime_;
    std::unique_ptr<gt::datahub::IInternalProducer> temp_writer_;
};
```

### 9.8 Exemplo 8 — integração típica com export em fim de corrida

```cpp
void on_end_of_heat(AppIntegration& app) {
    app.snapshot();

    auto state = app.hub().getState("NUMERO_CORRIDA");
    if (state && state->initialized) {
        // registrar evento no log da aplicação
    }
}
```

### 9.9 Exemplo 9 — o que a aplicação não consegue fazer nesta versão

A aplicação **não** possui uma API pública deste tipo:

```cpp
// NÃO FAZ PARTE DO CONTRATO PÚBLICO
hub.submit("CORRIDA_ATIVA", req);
hub.submit("TEMP_MAX_FUNDO_PANELA", req);
```

Isso é proposital, para não quebrar o invariante de ownership de escrita.

---

## 10. Regras mínimas de validação

No bootstrap, o runtime deve validar pelo menos:

1. `variable.name` único;
2. `connector.id` único;
3. `producer_binding.id` único;
4. `consumer_binding.id` único;
5. `file_export.id` único;
6. toda variável referenciada em binding, export, coluna ou predicado deve existir;
7. todo `connector_id` referenciado deve existir;
8. cada variável deve ter **exatamente 1** `producer_binding` ativo;
9. `producer_kind: internal` não pode declarar `connector_id`, `acquisition` nem `binding`;
10. `producer_kind: connector` deve declarar `connector_id` e `binding`;
11. `binding.type` deve ser compatível com `connector.kind`;
12. `acquisition.mode` deve ser compatível com `connector.kind`;
13. `trigger.mode` de consumer binding deve ser compatível com `connector.kind` e, nesta versão, só pode ser `on_change`;
14. `file_exports.format` deve ser `csv`;
15. `file_exports.connector_id` deve referenciar connector de `kind: file`;
16. `triggerFileExport()` só pode ser usado com exports de `trigger.mode: manual`;
17. `target_template` e `path_template` devem conter apenas namespaces permitidos;
18. `source` e `run_while.*.source` devem usar seletor puro, sem `${...}`;
19. `expression`, `target_template` e `path_template` devem usar apenas sintaxe interpolada `${...}` quando houver referências;
20. `default_value`, `min_value` e `max_value` devem ser compatíveis com `data_type`;
21. `stale_after_ms`, quando informado, deve ser positivo;
22. `append`, `write_header_if_missing` e demais flags devem ser coerentes com o formato e o sink;
23. `role` deve ser um dos valores suportados nesta versão;
24. cada coluna deve declarar exatamente um entre `source` e `expression`.

---

## 11. Semântica de execução esperada

### 11.1 Sem `activation`

Sem `activation`, o export é considerado **sempre ativo**.

Isso significa que ele pode gravar sempre que o `trigger` disparar, enquanto:

- o runtime estiver iniciado;
- o export estiver habilitado;
- o connector estiver operacional.

Para manter a semântica simples, a sessão de escrita é aberta **de forma lazy na primeira gravação elegível** e permanece aberta até `runtime.stop()` ou até uma rotação explícita de sessão.
O runtime não deve abrir e fechar o arquivo a cada tick periódico.

### 11.2 Com `activation`

Com `activation`, o predicado `run_while` define a janela operacional.

#### Modelo de avaliação nesta versão

Esta versão adota o seguinte modelo:

- export periódico: o predicado é avaliado **em cada tick do scheduler**, imediatamente antes da tentativa de gravação;
- export manual: o predicado é avaliado **no momento da chamada** `triggerFileExport(export_id)`;
- não existe avaliação reativa por mudança de variável nesta versão.

Consequência:

- a latência máxima para perceber o fim de uma condição periódica é, em geral, da ordem de `period_ms`.

### 11.3 `finalize_on_stop`

Quando `finalize_on_stop: true` e o predicado de ativação muda de verdadeiro para falso:

- o runtime deve fazer `flush`;
- o runtime deve fechar o handle do arquivo da sessão atual;
- a próxima ativação deve abrir ou criar uma nova sessão de escrita, resolvendo novamente o `target_template` naquele momento.

### 11.4 Export manual

No export manual:

- `triggerFileExport(export_id)` aceita somente exports com `trigger.mode: manual`;
- se houver `activation` e o predicado estiver falso, a API deve retornar `ActivationInactive`;
- nesse caso, nenhuma linha deve ser gravada.

### 11.5 Resolução de contexto no export

Dentro de `file_exports`, `context.*` representa dados da sessão corrente de export, e não estado persistido do hub.

Exemplos:

- `context.export_id` → id do export configurado;
- `context.export_session_id` → identificador textual decimal da sessão lógica corrente, monotônico por `export_id` dentro da vida do processo;
- `context.row_index` → índice **0-based** da linha dentro da sessão atual;
- `context.trigger_mode` → `periodic` ou `manual`;
- `context.target_path` → caminho final resolvido do arquivo;
- `context.session_started_at` → instante de início da sessão atual.

### 11.6 `append: false`

Se `append: false`, o runtime deve abrir o arquivo em modo de truncamento ao iniciar a sessão de escrita daquele export.

Consequências:

- export manual repetido para o mesmo `target_template` deve sobrescrever o conteúdo anterior da sessão anterior;
- export periódico reativado para o mesmo caminho também pode sobrescrever, pois uma nova sessão é aberta;
- `write_header_if_missing: true` continua válido e deve escrever o cabeçalho no início da nova sessão.

### 11.6.1 `write_header_if_missing` com `append: true`

Nesta versão, para manter a implementação simples e previsível, `write_header_if_missing: true` **não** inspeciona conteúdo pré-existente do arquivo.

A regra é:

- escrever cabeçalho quando a sessão cria um arquivo novo;
- escrever cabeçalho quando a sessão abre o arquivo em truncamento;
- não escrever cabeçalho automaticamente ao anexar (`append: true`) em arquivo já existente.

Ou seja, o nome do campo é mantido por compatibilidade documental, mas a semântica prática desta versão é “escrever cabeçalho no início de uma nova sessão de arquivo vazio”.

### 11.7 Conector indisponível

Se um connector de sink estiver indisponível, o runtime deve tratar a operação como falha de entrega daquele destino.

Se um connector de source estiver indisponível, o runtime deve marcar as variáveis afetadas com qualidade ruim, conforme política do anexo.

### 11.8 Perda abrupta de comunicação com source connector

Se um source connector perder comunicação de forma abrupta, o runtime deve registrar atualização sintética interna para as variáveis afetadas.

O comportamento mínimo esperado é:

- manter o último valor aceito, se houver;
- alterar a qualidade bruta para `bad`;
- atualizar `hub_timestamp`;
- incrementar `version`.

Se a variável ainda não tinha valor, ela permanece não inicializada.

### 11.9 `path_template` em file producers

`path_template` pode usar `${...}`.

Nesta versão, os namespaces permitidos em `path_template` são:

- `hub.*`
- `system.*`

`context.*` não é válido em `path_template`.

O `path_template` deve ser resolvido a cada tentativa de aquisição do file producer.

Se a resolução depender de valor ausente ou não inicializado, a tentativa de aquisição deve ser **ignorada**, sem tocar o estado da variável de destino e sem gerar caminho degenerado silencioso.

Essa regra é intencionalmente mais rígida do que `target_template` em `file_exports`: para leitura de arquivo, um caminho degenerado pode fazer o runtime ler a fonte errada; para escrita, interpolação vazia controlada ainda pode produzir um destino válido e observável por log/métrica.

---

## 12. Conclusão funcional

A V3.7.2 consolida o DataHub como um contrato funcional enxuto, mas implementável, com foco em:

- registro central de variáveis;
- ownership real de escrita;
- YAML como contrato oficial de integração;
- seletores e expressões previsíveis;
- export multicoluna configurável;
- arquivo como source e sink;
- API C++ pública coerente com o invariante de 1 produtor por variável;
- separação clara entre contrato público e API interna do runtime.
