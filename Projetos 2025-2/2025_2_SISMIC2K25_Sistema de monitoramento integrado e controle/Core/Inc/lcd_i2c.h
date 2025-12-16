/******************************************************************************
  * @file           : lcd_i2c.h
  * @brief          : Biblioteca Profissional para LCD 16x2 via I2C
  * @author         : Professor MicroControl
  * @version        : 2.0
  * @date           : 2025
  ******************************************************************************
  * CARACTERÍSTICAS:
  * - Detecção automática de endereço I2C (0x27 ou 0x3F)
  * - Tratamento completo de erros
  * - Funções de controle de cursor, blink, backlight
  * - Suporte a caracteres customizados (até 8)
  * - Buffer interno para otimização
  * - Código limpo e bem documentado
  ******************************************************************************
  */

#ifndef LCD_I2C_H
#define LCD_I2C_H

#include "stm32f4xx_hal.h"  // Ajuste para seu STM32 (f1xx, f4xx, etc)
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

// ============================================================================
// CONFIGURAÇÕES (Ajuste conforme necessário)
// ============================================================================
#define LCD_LINHAS      2       // Número de linhas do LCD (2 ou 4)
#define LCD_COLUNAS     16      // Número de colunas (16 ou 20)
#define LCD_TIMEOUT     100     // Timeout I2C em ms

// ============================================================================
// ESTRUTURA DE CONFIGURAÇÃO
// ============================================================================
typedef struct {
    I2C_HandleTypeDef *hi2c;    // Ponteiro para handle I2C
    uint8_t endereco;           // Endereço I2C (0x27 ou 0x3F)
    bool backlight;             // Estado do backlight
    bool inicializado;          // Flag de inicialização
} LCD_HandleTypeDef;

// ============================================================================
// ENUMERAÇÕES
// ============================================================================
typedef enum {
    LCD_OK = 0,
    LCD_ERRO_I2C,
    LCD_ERRO_TIMEOUT,
    LCD_ERRO_NAO_INICIALIZADO,
    LCD_ERRO_PARAMETRO_INVALIDO
} LCD_Status_t;

// Posições de caracteres customizados
typedef enum {
    LCD_CHAR_0 = 0,
    LCD_CHAR_1,
    LCD_CHAR_2,
    LCD_CHAR_3,
    LCD_CHAR_4,
    LCD_CHAR_5,
    LCD_CHAR_6,
    LCD_CHAR_7
} LCD_CustomChar_t;

// ============================================================================
// FUNÇÕES PÚBLICAS - INICIALIZAÇÃO
// ============================================================================

/**
 * @brief  Inicializa o LCD (detecta endereço automaticamente)
 * @param  hlcd: Ponteiro para estrutura LCD_HandleTypeDef
 * @param  hi2c: Ponteiro para handle I2C (ex: &hi2c1)
 * @retval LCD_Status_t
 *
 * Exemplo de uso:
 *   LCD_HandleTypeDef lcd;
 *   if(LCD_Init(&lcd, &hi2c1) == LCD_OK) {
 *       // LCD inicializado com sucesso
 *   }
 */
LCD_Status_t LCD_Init(LCD_HandleTypeDef *hlcd, I2C_HandleTypeDef *hi2c);

/**
 * @brief  Verifica se o LCD está conectado
 * @param  hlcd: Ponteiro para estrutura LCD_HandleTypeDef
 * @retval true se conectado, false se não
 */
bool LCD_IsConnected(LCD_HandleTypeDef *hlcd);

// ============================================================================
// FUNÇÕES PÚBLICAS - CONTROLE BÁSICO
// ============================================================================

/**
 * @brief  Limpa a tela do LCD
 * @param  hlcd: Ponteiro para estrutura LCD_HandleTypeDef
 * @retval LCD_Status_t
 */
LCD_Status_t LCD_Clear(LCD_HandleTypeDef *hlcd);

/**
 * @brief  Retorna o cursor para posição inicial (0,0)
 * @param  hlcd: Ponteiro para estrutura LCD_HandleTypeDef
 * @retval LCD_Status_t
 */
LCD_Status_t LCD_Home(LCD_HandleTypeDef *hlcd);

/**
 * @brief  Posiciona o cursor em linha e coluna específica
 * @param  hlcd: Ponteiro para estrutura LCD_HandleTypeDef
 * @param  linha: Linha (0 ou 1 para LCD 16x2)
 * @param  coluna: Coluna (0 a 15 para LCD 16x2)
 * @retval LCD_Status_t
 *
 * Exemplo:
 *   LCD_SetCursor(&lcd, 1, 5);  // Linha 2, coluna 6
 */
LCD_Status_t LCD_SetCursor(LCD_HandleTypeDef *hlcd, uint8_t linha, uint8_t coluna);

// ============================================================================
// FUNÇÕES PÚBLICAS - ESCRITA DE TEXTO
// ============================================================================

/**
 * @brief  Escreve uma string no LCD
 * @param  hlcd: Ponteiro para estrutura LCD_HandleTypeDef
 * @param  str: String a ser escrita
 * @retval LCD_Status_t
 *
 * Exemplo:
 *   LCD_Print(&lcd, "SISMIC 2K25");
 */
LCD_Status_t LCD_Print(LCD_HandleTypeDef *hlcd, const char *str);

/**
 * @brief  Escreve um caractere no LCD
 * @param  hlcd: Ponteiro para estrutura LCD_HandleTypeDef
 * @param  c: Caractere a ser escrito
 * @retval LCD_Status_t
 */
LCD_Status_t LCD_PutChar(LCD_HandleTypeDef *hlcd, char c);

/**
 * @brief  Escreve string formatada (tipo printf)
 * @param  hlcd: Ponteiro para estrutura LCD_HandleTypeDef
 * @param  format: String de formato (ex: "V:%d.%dV")
 * @param  ...: Argumentos variáveis
 * @retval LCD_Status_t
 *
 * Exemplo:
 *   LCD_Printf(&lcd, "Temp: %dC", temperatura);
 */
LCD_Status_t LCD_Printf(LCD_HandleTypeDef *hlcd, const char *format, ...);

/**
 * @brief  Escreve texto em posição específica
 * @param  hlcd: Ponteiro para estrutura LCD_HandleTypeDef
 * @param  linha: Linha (0 ou 1)
 * @param  coluna: Coluna (0 a 15)
 * @param  str: String a ser escrita
 * @retval LCD_Status_t
 *
 * Exemplo:
 *   LCD_PrintAt(&lcd, 0, 0, "SISMIC 2K25");
 */
LCD_Status_t LCD_PrintAt(LCD_HandleTypeDef *hlcd, uint8_t linha, uint8_t coluna, const char *str);

// ============================================================================
// FUNÇÕES PÚBLICAS - CONTROLE DE DISPLAY
// ============================================================================

/**
 * @brief  Liga/Desliga o backlight do LCD
 * @param  hlcd: Ponteiro para estrutura LCD_HandleTypeDef
 * @param  estado: true = liga, false = desliga
 * @retval LCD_Status_t
 */
LCD_Status_t LCD_Backlight(LCD_HandleTypeDef *hlcd, bool estado);

/**
 * @brief  Mostra/Esconde o cursor
 * @param  hlcd: Ponteiro para estrutura LCD_HandleTypeDef
 * @param  estado: true = mostra, false = esconde
 * @retval LCD_Status_t
 */
LCD_Status_t LCD_Cursor(LCD_HandleTypeDef *hlcd, bool estado);

/**
 * @brief  Liga/Desliga o blink do cursor
 * @param  hlcd: Ponteiro para estrutura LCD_HandleTypeDef
 * @param  estado: true = pisca, false = fixo
 * @retval LCD_Status_t
 */
LCD_Status_t LCD_Blink(LCD_HandleTypeDef *hlcd, bool estado);

/**
 * @brief  Liga/Desliga o display (mantém dados na memória)
 * @param  hlcd: Ponteiro para estrutura LCD_HandleTypeDef
 * @param  estado: true = liga, false = desliga
 * @retval LCD_Status_t
 */
LCD_Status_t LCD_Display(LCD_HandleTypeDef *hlcd, bool estado);

// ============================================================================
// FUNÇÕES PÚBLICAS - CARACTERES CUSTOMIZADOS
// ============================================================================

/**
 * @brief  Cria um caractere customizado
 * @param  hlcd: Ponteiro para estrutura LCD_HandleTypeDef
 * @param  local: Posição (0-7) onde o caractere será armazenado
 * @param  bitmap: Array de 8 bytes representando o caractere (5x8 pixels)
 * @retval LCD_Status_t
 *
 * Exemplo - Criar coração:
 *   uint8_t coracao[8] = {
 *       0b00000,
 *       0b01010,
 *       0b11111,
 *       0b11111,
 *       0b01110,
 *       0b00100,
 *       0b00000,
 *       0b00000
 *   };
 *   LCD_CreateChar(&lcd, LCD_CHAR_0, coracao);
 *   LCD_PutChar(&lcd, 0);  // Exibe o coração
 */
LCD_Status_t LCD_CreateChar(LCD_HandleTypeDef *hlcd, LCD_CustomChar_t local, uint8_t bitmap[8]);

// ============================================================================
// FUNÇÕES PÚBLICAS - UTILITÁRIOS
// ============================================================================

/**
 * @brief  Rola o texto para a esquerda
 * @param  hlcd: Ponteiro para estrutura LCD_HandleTypeDef
 * @retval LCD_Status_t
 */
LCD_Status_t LCD_ScrollLeft(LCD_HandleTypeDef *hlcd);

/**
 * @brief  Rola o texto para a direita
 * @param  hlcd: Ponteiro para estrutura LCD_HandleTypeDef
 * @retval LCD_Status_t
 */
LCD_Status_t LCD_ScrollRight(LCD_HandleTypeDef *hlcd);

/**
 * @brief  Desenha uma barra de progresso
 * @param  hlcd: Ponteiro para estrutura LCD_HandleTypeDef
 * @param  linha: Linha onde desenhar (0 ou 1)
 * @param  percentual: Valor de 0 a 100
 * @retval LCD_Status_t
 *
 * Exemplo:
 *   LCD_DrawProgressBar(&lcd, 1, 75);  // Barra 75% na linha 2
 */
LCD_Status_t LCD_DrawProgressBar(LCD_HandleTypeDef *hlcd, uint8_t linha, uint8_t percentual);

/**
 * @brief  Converte código de erro para string
 * @param  status: Código de erro LCD_Status_t
 * @retval Ponteiro para string de erro
 */
const char* LCD_GetErrorString(LCD_Status_t status);

#endif /* LCD_I2C_H */

/**
 * ============================================================================
 * EXEMPLO DE USO COMPLETO:
 * ============================================================================
 *
 * #include "lcd_i2c.h"
 *
 * LCD_HandleTypeDef lcd;
 *
 * int main(void) {
 *     HAL_Init();
 *     SystemClock_Config();
 *     MX_I2C1_Init();
 *
 *     // Inicializa LCD (detecta endereço automaticamente)
 *     if(LCD_Init(&lcd, &hi2c1) == LCD_OK) {
 *         LCD_Clear(&lcd);
 *         LCD_PrintAt(&lcd, 0, 0, "SISMIC 2K25 PRO");
 *         LCD_PrintAt(&lcd, 1, 0, "Inicializando...");
 *         HAL_Delay(2000);
 *
 *         while(1) {
 *             LCD_Clear(&lcd);
 *             LCD_Printf(&lcd, "Tensao: %dV", tensao);
 *             LCD_SetCursor(&lcd, 1, 0);
 *             LCD_Printf(&lcd, "Corrente: %.2fA", corrente);
 *             HAL_Delay(500);
 *         }
 *     } else {
 *         // Erro ao inicializar LCD
 *         Error_Handler();
 *     }
 * }
 *
 * ============================================================================
 */
