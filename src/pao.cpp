/*
 * Panificadora Automática
 * Hardware: Arduino UNO (ATmega328P)
 *
 * Mapeamento de Pinos (Conforme PDF/Esquemático):
 * - LCD D4-D7: PD4-PD7 (Pinos Digitais 4, 5, 6, 7)
 * - LCD RS:    PB0 (Pino Digital 8)
 * - LCD E:     PB1 (Pino Digital 9)
 * - Motor:     PD3 (Pino Digital 3)
 * - Resistência: PD2 (Pino Digital 2)
 * - Buzzer:    PC1 (Pino Analógico A1 usado como Digital)
 * - Botão Sel: PC2 (Pino Analógico A2)
 * - Botão Up:  PC3 (Pino Analógico A3)
 * - Botão Down: PC4 (Pino Analógico A4)
 * - Sensor Temp: PC0 (Pino Analógico A0)
 */

#include <avr/io.h>
#include <util/delay.h>
#include <stdlib.h>
#include <stdbool.h>

#define set_bit(y, bit) (y |= (1 << bit))
#define clr_bit(y, bit) (y &= ~(1 << bit))
#define cpl_bit(y, bit) (y ^= (1 << bit))
#define tst_bit(y, bit) (y & (1 << bit))

// CONTR_LCD: porta de controle (RS, E) -> PORTB
// DADOS_LCD: porta dos 4 bits D4-D7 -> PORTD
#define CONTR_LCD PORTB
#define DADOS_LCD PORTD

// Bits de controle
#define RS 0   // PB0
#define E  1   // PB1

// Se os pinos de dados do LCD estiverem nos 4 MSB do PORTx, defina 1.
// No seu mapeamento D4-D7 = PD4-PD7 => 4 MSB de PORTD => nibble_dados = 1
#define nibble_dados 1

enum Estado {
  CONFIG_SOVA,
  CONFIG_CRESCIMENTO,
  CONFIG_ASSAR,
  RODANDO_SOVA,
  RODANDO_CRESCIMENTO,
  RODANDO_ASSAR,
  FINALIZADO
};

enum Estado estadoAtual = CONFIG_SOVA;

unsigned long contadorSegundos = 0;
unsigned long tempoRestanteSegundos = 0;
unsigned int contadorMs = 0;

// Tempos padrão (em minutos)
int tempoSova = 1; // 25
int tempoCrescer = 1; // 90
int tempoAssar = 1; // 40
int temperatura = 25;

// Controle de botões
unsigned int debounceCounter = 0;
bool botaoProcessado = false;

void cmd_LCD(unsigned char c, char cd);
void inic_LCD_4bits(void);              
void pulso_enable(void);
void lcd_string(const char *str);
void lcd_set_cursor(int x, int y);
void exibe_tempo(unsigned long segundos);
void atualiza_display_config();
void gerencia_fase(const char* nomeFase);
int lerTemperatura();
long map(long x, long in_min, long in_max, long out_min, long out_max);

void setup() {
  // 1. Configuração de Portas (DDRx)
  DDRD |= 0xFC;  // Pinos 2-7 como saída (PD2-PD7)
  DDRB |= 0x03;  // Pinos 8-9 (PB0, PB1) como saída (RS, E)
  DDRC |= 0x02;  // PC1 (Buzzer) como saída
  DDRC &= ~0x1C; // PC2-PC4 como entrada (botões)

  // 2. Estado Inicial
  PORTC |= 0x1C;  // Pull-up nos botões (PC2-PC4)
  PORTD &= ~(1 << PD2); // Resistência OFF
  PORTD &= ~(1 << PD3); // Motor OFF
  PORTC &= ~(1 << PC1); // Buzzer OFF

  // 3. Configuração do ADC
  ADMUX = (1 << REFS0); // AVcc como referência
  ADCSRA = (1 << ADEN) | (1 << ADPS2) | (1 << ADPS1) | (1 << ADPS0); // Enable ADC, prescaler 128

  // 4. Inicializa LCD
  inic_LCD_4bits();

  // Tela Inicial
  cmd_LCD(0x01, 0);
  lcd_string("Panificadora");
  lcd_set_cursor(0, 1);
  lcd_string("UNO Iniciada");

  cmd_LCD(0x01, 0);
  atualiza_display_config();
}

void loop() {
  _delay_ms(1);
  contadorMs++;
  
  // Atualiza debounce
  if (debounceCounter > 0) {
    debounceCounter--;
  }

  static unsigned int contadorTemp = 0;
  
  if (estadoAtual == RODANDO_ASSAR) {
    contadorTemp++;
    if (contadorTemp >= 500) {
      contadorTemp = 0;
      temperatura = lerTemperatura();
    }
  } else {
    // reseta contador para evitar leitura imediata ao entrar em ASSAR
    contadorTemp = 0;
  }

  static unsigned int contMs = 0;
  
  // SÓ conta segundos durante execução
  if (estadoAtual == RODANDO_SOVA || 
      estadoAtual == RODANDO_CRESCIMENTO || 
      estadoAtual == RODANDO_ASSAR) {
    contMs++;
    if (contMs >= 1000) {
      contMs = 0;
      contadorSegundos++;
    }
  } else {
    contMs = 0;
  }

  int btn = 0; // 0:Nada, 1:Sel, 2:Up, 3:Down
  
  bool emExecucao = (estadoAtual == RODANDO_SOVA || 
                     estadoAtual == RODANDO_CRESCIMENTO || 
                     estadoAtual == RODANDO_ASSAR);
  
  bool todosSoltos = (PINC & (1 << PC2)) && (PINC & (1 << PC3)) && (PINC & (1 << PC4));
  
  if (todosSoltos) {
    botaoProcessado = false;
  }
  
  if (!emExecucao && debounceCounter == 0 && !botaoProcessado) {
    if (!(PINC & (1 << PC2))) {
      btn = 1;
      debounceCounter = 10; // 500ms debounce
      botaoProcessado = true;
    } 
    else if (!(PINC & (1 << PC3))) {
      btn = 2;
      debounceCounter = 10;
      botaoProcessado = true;
    }
    else if (!(PINC & (1 << PC4))) {
      btn = 3;
      debounceCounter = 10;
      botaoProcessado = true;
    }
  }

  switch (estadoAtual) {
    
    case CONFIG_SOVA:
      if (btn == 2 && tempoSova < 99) {
        tempoSova++;
        atualiza_display_config();
      }
      else if (btn == 3 && tempoSova > 1) {
        tempoSova--;
        atualiza_display_config();
      }
      else if (btn == 1) {
        // Aguarda botão ser solto
        while (!(PINC & (1 << PC2))) {
          _delay_ms(10);
        }
        
        estadoAtual = CONFIG_CRESCIMENTO;
        cmd_LCD(0x01, 0);
        atualiza_display_config();
        
        // Reset flags
        botaoProcessado = false;
        debounceCounter = 0;
      }
      break;

    case CONFIG_CRESCIMENTO:
      if (btn == 2 && tempoCrescer < 180) {
        tempoCrescer++;
        atualiza_display_config();
      }
      else if (btn == 3 && tempoCrescer > 1) {
        tempoCrescer--;
        atualiza_display_config();
      }
      else if (btn == 1) {
        // Aguarda botão ser solto
        while (!(PINC & (1 << PC2))) {
          _delay_ms(10);
        }
        
        estadoAtual = CONFIG_ASSAR;
        cmd_LCD(0x01, 0);
        atualiza_display_config();
        
        // Reset flags
        botaoProcessado = false;
        debounceCounter = 0;
      }
      break;

    case CONFIG_ASSAR:
      if (btn == 2 && tempoAssar < 99) {
        tempoAssar++;
        atualiza_display_config();
      }
      else if (btn == 3 && tempoAssar > 1) {
        tempoAssar--;
        atualiza_display_config();
      }
      else if (btn == 1) {
        // Iniciar Ciclo
        while (!(PINC & (1 << PC2))) {
          _delay_ms(10); // Aguarda botão ser solto
        }
        
        estadoAtual = RODANDO_SOVA;
        tempoRestanteSegundos = tempoSova * 60UL;
        contadorSegundos = 0;
        
        // Reset COMPLETO dos contadores
        contadorMs = 0;
        debounceCounter = 0;
        botaoProcessado = false;
        
        cmd_LCD(0x01, 0);
        PORTD |= (1 << PD3); // Liga Motor
      }
      break;

    case RODANDO_SOVA:
      // Ignora botões durante execução
      gerencia_fase("SOVA");
      break;

    case RODANDO_CRESCIMENTO:
      // Ignora botões durante execução
      gerencia_fase("CRESCER");
      break;

    case RODANDO_ASSAR:
      // Ignora botões durante execução
      gerencia_fase("ASSAR");
      break;

    case FINALIZADO:
      if (btn == 1) {
        estadoAtual = CONFIG_SOVA;
        cmd_LCD(0x01, 0);
        atualiza_display_config();
      }
      break;
  }
}

int lerTemperatura() {
  // Seleciona canal A0
  ADMUX = (ADMUX & 0xF0) | 0x00;
  
  // Inicia conversão
  ADCSRA |= (1 << ADSC);
  
  // Aguarda conversão
  while (ADCSRA & (1 << ADSC));
  
  int leitura = ADC;
  
  int temp = map(leitura, 150, 600, 29, 120);
  
  // Limita valores
  if (temp < 0) temp = 0;
  if (temp > 150) temp = 150;
  
  return temp;
}

long map(long x, long in_min, long in_max, long out_min, long out_max) {
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

void gerencia_fase(const char* titulo) {
  static unsigned long ultimoSegundo = 0;
  
  if (contadorSegundos != ultimoSegundo) {
    ultimoSegundo = contadorSegundos;
    
    if (tempoRestanteSegundos > 0) {
      tempoRestanteSegundos--;
      
      // Atualiza LCD com título e tempo
      lcd_set_cursor(0, 0);
      lcd_string(titulo);
      lcd_string(" ");
      exibe_tempo(tempoRestanteSegundos);
      lcd_string("   ");

      // Exibe temperatura somente durante ASSAR
      if (estadoAtual == RODANDO_ASSAR) {
        lcd_set_cursor(0, 1);
        lcd_string("Tmp:");
        char buf[5];
        itoa(temperatura, buf, 10);
        lcd_string(buf);
        lcd_string("C        ");
      } else {
        // limpa/oculta linha inferior nas outras fases
        lcd_set_cursor(0, 1);
        lcd_string("                ");
      }
      
    } else {
      // Transição de fase
      if (estadoAtual == RODANDO_SOVA) {
        PORTD &= ~(1 << PD3); // Desliga Motor
        estadoAtual = RODANDO_CRESCIMENTO;
        tempoRestanteSegundos = tempoCrescer * 60UL;
        contadorSegundos = 0;
        cmd_LCD(0x01, 0);
      }
      else if (estadoAtual == RODANDO_CRESCIMENTO) {
        PORTD |= (1 << PD2); // Liga Resistência (início do ASSAR)
        estadoAtual = RODANDO_ASSAR;
        tempoRestanteSegundos = tempoAssar * 60UL;
        contadorSegundos = 0;
        cmd_LCD(0x01, 0);
      }
      else if (estadoAtual == RODANDO_ASSAR) {
        PORTD &= ~(1 << PD2); // Desliga Resistência
        estadoAtual = FINALIZADO;
        cmd_LCD(0x01, 0);
        lcd_string("PAO PRONTO!");
        
        // Apita Buzzer
        for(int i = 0; i < 100; i++) {
          PORTC |= (1 << PC1);
          _delay_ms(1);
          PORTC &= ~(1 << PC1);
          _delay_ms(1);
        }
      }
    }
  }
}

void pulso_enable() {
  set_bit(CONTR_LCD, E);
  _delay_us(1);
  clr_bit(CONTR_LCD, E);
  _delay_us(50);
}

void cmd_LCD(unsigned char c, char cd) // c é o dado  e cd indica se é instrução(0) ou caractere(1)
{
    if (cd == 0)
        clr_bit(CONTR_LCD, RS);
    else
        set_bit(CONTR_LCD, RS);

    // primeiro nibble de dados - 4 MSB
#if (nibble_dados) // compila código para os pinos de dados do LCD nos 4 MSB do PORT
    DADOS_LCD = (DADOS_LCD & 0b00001111) | (0b11110000 & c);
#else // compila código para os pinos de dados do LCD nos 4 LSB do PORT
    DADOS_LCD = (DADOS_LCD & 0xF0) | (c >> 4);
#endif

    pulso_enable();

    // segundo nibble de dados - 4 LSB
#if (nibble_dados) // compila código para os pinos de dados do LCD nos 4 MSB do PORT
    DADOS_LCD = (DADOS_LCD & 0b00001111) | (0b11110000 & (c << 4));
#else // compila código para os pinos de dados do LCD nos 4 LSB do PORT
    DADOS_LCD = (DADOS_LCD & 0xF0) | (0x0F & c);
#endif

    pulso_enable();

    if ((cd == 0) && (c < 4)) // se for instrução de retorno ou limpeza espera LCD estar pronto
        _delay_ms(2);
}

void inic_LCD_4bits() // sequência ditada pelo fabricante do HD44780
{
    clr_bit(CONTR_LCD, RS); // RS em zero indicando que o dado para o LCD será uma instrução
    clr_bit(CONTR_LCD, E);  // pino de habilitação em zero

    _delay_ms(20); // tempo para estabilizar a tensão do LCD

    cmd_LCD(0x30, 0);

    pulso_enable(); // habilitação respeitando os tempos de resposta do LCD
    _delay_ms(5);
    pulso_enable();
    _delay_us(200);
    pulso_enable(); /*até aqui ainda é uma interface de 8 bits.*/

    // interface de 4 bits, deve ser enviado duas vezes (a outra está abaixo)
    cmd_LCD(0x20, 0);

    pulso_enable();
    cmd_LCD(0x28, 0); // interface de 4 bits 2 linhas (0x2 e 0x8)
    cmd_LCD(0x08, 0); // desliga o display
    cmd_LCD(0x01, 0); // limpa todo o display
    cmd_LCD(0x0F, 0); // mensagem aparente cursor inativo não piscando
    cmd_LCD(0x80, 0); // inicializa cursor na primeira posição a esquerda - 1a linha
    cmd_LCD(0x0C, 0); // Desliga o cursor piscar
}

void lcd_string(const char *str) {
  while (*str) {
    cmd_LCD(*str++, 1);
  }
}

void lcd_set_cursor(int x, int y) {
  unsigned char addr = 0x80 + x + (y * 0x40);
  cmd_LCD(addr, 0);
}

void exibe_tempo(unsigned long segundos) {
  int h = segundos / 60;
  int m = (segundos - h*60);
  char buf[4];
  
  if(h < 10) cmd_LCD('0', 1);
  itoa(h, buf, 10); 
  lcd_string(buf);
  cmd_LCD(':', 1);
  if(m < 10) cmd_LCD('0', 1);
  itoa(m, buf, 10); 
  lcd_string(buf);
}

void atualiza_display_config() {
  lcd_set_cursor(0, 0);
  
  if(estadoAtual == CONFIG_SOVA) 
    lcd_string("CFG: SOVA    ");
  else if(estadoAtual == CONFIG_CRESCIMENTO) 
    lcd_string("CFG: CRESCER ");
  else 
    lcd_string("CFG: ASSAR   ");

  lcd_set_cursor(0, 1);
  lcd_string("Tempo: ");
  
  int t = (estadoAtual == CONFIG_SOVA) ? tempoSova : 
          (estadoAtual == CONFIG_CRESCIMENTO ? tempoCrescer : tempoAssar);
  
  exibe_tempo(t * 60UL);
  lcd_string("  ");
}
