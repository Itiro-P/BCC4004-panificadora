# Panificadora – Atividade 2

Este repositório contém a solução para a Atividade 2 da disciplina BCC4004, baseada no arquivo `ACCH_Atividade2_Panificadora.pdf`.

## Descrição

O objetivo da atividade é desenvolver um sistema para gerenciar uma panificadora, conforme os requisitos detalhados no PDF. O sistema abrange funcionalidades como cadastro de produtos, controle de estoque, registro de vendas e geração de relatórios.

## Estrutura do Projeto

-   `src/` – Código-fonte do sistema
-   `ACCH_Atividade2_Panificadora.pdf` – Enunciado da atividade

---

### Variáveis importantes e papel

-   **`int tempoSova, tempoCrescer, tempoAssar`** — tempos configuráveis em **minutos**; valores iniciais no código (1, 1, 1) mas comentados para 25, 90, 40.
-   **`int temperatura`** — valor estimado em graus Celsius a partir do ADC.
-   **`enum Estado estadoAtual`** — estado atual da máquina de estados (CONFIG*\*, RODANDO*\*, FINALIZADO).
-   **`unsigned long tempoRestanteSegundos`** — tempo restante da fase atual em **segundos**.
-   **`unsigned long contadorSegundos`** — contador global de segundos usado para detectar mudança de segundo.
-   **`unsigned int contadorMs, contMs`** — contadores de milissegundos para temporização interna.
-   **`unsigned int debounceCounter`** — contador para debounce de botões.
-   **`bool botaoProcessado`** — flag para evitar múltiplos disparos por um único pressionamento.

---

### Funções principais com assinaturas, parâmetros, retorno e efeitos colaterais

> **Observação**: as assinaturas abaixo são apresentadas no estilo C/C++ para facilitar leitura.

#### `void setup(void)`

-   **Parâmetros**: nenhum
-   **Retorno**: `void`
-   **Descrição**: configura DDRx, pull-ups, inicializa ADC e LCD, limpa display e mostra tela inicial.
-   **Efeitos colaterais**: altera registradores `DDRx`, `PORTx`, `ADMUX`, `ADCSRA` e escreve no LCD.
-   **Verificações**: não há verificação explícita de falha; assume hardware presente.

#### `void loop(void)`

-   **Parâmetros**: nenhum
-   **Retorno**: `void`
-   **Descrição**: laço principal que: atualiza debounce, conta tempo, lê temperatura periodicamente (em ASSAR), detecta botões e faz transições de estado.
-   **Efeitos colaterais**: chama `gerencia_fase()` e funções de LCD; altera saídas (motor/resistência).
-   **Verificações**: usa `debounceCounter` e `botaoProcessado` para evitar leituras falsas; bloqueia leitura de botões durante execução (`emExecucao`).

#### `void gerencia_fase(const char* titulo)`

-   **Parâmetros**: `const char* titulo` — string com o nome da fase exibida no LCD.
-   **Retorno**: `void`
-   **Descrição**: executa a contagem regressiva segundo a segundo; atualiza o LCD; faz transições entre fases e aciona/desaciona motor, resistência e buzzer.
-   **Efeitos colaterais**: modifica `estadoAtual`, `tempoRestanteSegundos`, `contadorSegundos`, `PORTD`, `PORTC` e escreve no LCD.
-   **Verificações**: compara `contadorSegundos` com `ultimoSegundo` para atualizar apenas quando o segundo muda; verifica `tempoRestanteSegundos > 0` para decrementar; trata transições de fase.
-   **Pontos de atenção**: o laço de buzzer no fim (`for (i = 0; i < 100; i++)`) é bloqueante e pode impedir outras tarefas; não há timeout ou verificação de falha do buzzer.

#### `int lerTemperatura(void)`

-   **Parâmetros**: nenhum
-   **Retorno**: `int` — temperatura estimada em °C
-   **Descrição**: seleciona canal ADC A0, inicia conversão, espera término, lê `ADC`, aplica `map()` para converter leitura em temperatura e limita o resultado.
-   **Efeitos colaterais**: altera `ADMUX` e `ADCSRA` (inicia conversão).
-   **Verificações**: espera ativamente `while (ADCSRA & (1 << ADSC));` até a conversão terminar — isto é uma espera bloqueante; aplica limites finais (`if (temp < 0) temp = 0; if (temp > 150) temp = 150;`).
-   **Risco detectado**: `map()` pode dividir por zero se `in_max == in_min`; o código atual não verifica esse caso.

#### `long map(long x, long in_min, long in_max, long out_min, long out_max)`

-   **Parâmetros**: valores de entrada e saída para mapeamento linear
-   **Retorno**: `long` — valor mapeado
-   **Descrição**: realiza mapeamento linear clássico.
-   **Verificações**: **não** há verificação de divisão por zero; recomenda-se adicionar checagem para `in_max != in_min` antes de calcular.

#### LCD e utilitários

-   **`void inic_LCD_4bits(void)`**
    -   Inicializa o HD44780 em modo 4 bits. Retorno `void`. Realiza sequência de comandos e delays conforme datasheet.
    -   **Verificações**: usa delays para garantir estabilidade; não verifica presença física do LCD.
-   **`void cmd_LCD(unsigned char c, char cd)`**
    -   **Parâmetros**: `c` — byte a enviar; `cd` — 0 para instrução, 1 para caractere.
    -   **Retorno**: `void`
    -   **Descrição**: envia dois nibbles ao LCD, controla RS e E. Para instruções de limpeza/retorno (`c < 4`) aplica delay de 2 ms.
    -   **Verificações**: aplica delays mínimos; assume que escrita direta em `PORTD` e `PORTB` é segura.
-   **`void pulso_enable(void)`**
    -   Gera pulso no pino E do LCD. Retorno `void`.
-   **`void lcd_string(const char *str)`**
    -   Envia string terminada em `\0` ao LCD. Retorno `void`.
-   **`void lcd_set_cursor(int x, int y)`**
    -   Posiciona cursor no LCD. Retorno `void`.

#### `void exibe_tempo(unsigned long segundos)`

-   **Parâmetros**: `segundos` — tempo em segundos a ser exibido
-   **Retorno**: `void`
-   **Descrição**: converte segundos para formato **hh:mm** (horas = `segundos/60`, minutos = `segundos - h*60`) e escreve no LCD com zero à esquerda quando necessário.
-   **Verificações**: não valida limites de `segundos` (assume valor razoável).

#### `void atualiza_display_config(void)`

-   **Parâmetros**: nenhum
-   **Retorno**: `void`
-   **Descrição**: exibe a tela de configuração correspondente ao `estadoAtual` e mostra o tempo configurado (chama `exibe_tempo`).
-   **Verificações**: seleciona `t` com operador ternário; assume `estadoAtual` válido.

---

### Trechos do código e pontos de atenção

```c
// Exemplo de espera bloqueante para ADC
ADCSRA |= (1 << ADSC);
while (ADCSRA & (1 << ADSC)); // bloqueante até conversão terminar
```

-   **Impacto**: impede execução concorrente; aceitável para leituras esporádicas, mas não para sistemas que exigem multitarefa.

```c
int temp = map(leitura, 150, 600, 29, 120);
// sem verificação de in_max == in_min -> risco de divisão por zero
```

-   **Correção sugerida**:

```c
if (in_max == in_min) return out_min; // ou outro valor seguro
```

```c
// Debounce simples
if (!(PINC & (1 << PC2))) {
  btn = 1;
  debounceCounter = 10; // valor em ms depende do _delay_ms(1) no loop
  botaoProcessado = true;
}
```

-   **Observação**: ajuste `debounceCounter` e a lógica de temporização para garantir o tempo de debounce desejado (ex.: 50–200 ms).

---

## Como Executar

1. Clone este repositório:
    ```bash
    git clone https://github.com/seu-usuario/BCC4004-panificadora.git
    ```
2. Siga as instruções no arquivo `docs/INSTALL.md` para instalar dependências e executar o sistema.

## Requisitos

-   Python 3.x (ou outra linguagem, conforme especificado)
-   Bibliotecas descritas em `requirements.txt`

## Autoria

Desenvolvido para a disciplina BCC4004 por Matheus, Arthur, Lucas Marcão, Pedro Nagao e João Zulato.
