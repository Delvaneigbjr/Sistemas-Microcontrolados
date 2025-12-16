#ifndef I2C_LCD_H
#define I2C_LCD_H

#include <stdint.h>
#include "stm32f4xx_hal.h"
#include "stm32f4xx_hal_i2c.h"

// --- Adicionado para resolver o erro 'no member named columns/rows' ---
#define LCD_COLUMNS 16
#define LCD_ROWS    2
// ---------------------------------------------------------------------

/**
 * @brief Includes the HAL driver present in the project
 */
#if __has_include("stm32f4xx_hal.h")
	#include "stm32f4xx_hal.h"
#elif __has_include("stm32c0xx_hal.h")
	#include "stm32c0xx_hal.h"
#elif __has_include("stm32g4xx_hal.h")
	#include "stm32g4xx_hal.h"
#endif

/**
 * @brief Structure to hold LCD instance information
 * * CORREÇÃO: Adicionados os membros 'columns' e 'rows' que o main.c espera.
 */
typedef struct {
    I2C_HandleTypeDef *hi2c;     // I2C handler for communication
    uint8_t address;            // I2C address of the LCD

    // Membros adicionados para resolver os erros de compilação em main.c
    uint8_t columns;            // Número de colunas do display (e.g., 16)
    uint8_t rows;               // Número de linhas do display (e.g., 2)

} I2C_LCD_HandleTypeDef;

/**
 * @brief Initializes the LCD.
 * @param lcd: Pointer to the LCD handle
 */
void lcd_init(I2C_LCD_HandleTypeDef *lcd);

/**
 * @brief Sends a command to the LCD.
 * @param lcd: Pointer to the LCD handle
 * @param cmd: Command byte to send
 */
void lcd_send_cmd(I2C_LCD_HandleTypeDef *lcd, char cmd);

/**
 * @brief Sends data (character) to the LCD.
 * @param lcd: Pointer to the LCD handle
 * @param data: Data byte to send
 */
void lcd_send_data(I2C_LCD_HandleTypeDef *lcd, char data);

/**
 * @brief Sends a single character to the LCD.
 * @param lcd: Pointer to the LCD handle
 * @param ch: Character to send
 */
void lcd_putchar(I2C_LCD_HandleTypeDef *lcd, char ch);

/**
 * @brief Sends a string to the LCD.
 * @param lcd: Pointer to the LCD handle
 * @param str: Null-terminated string to send
 */
void lcd_puts(I2C_LCD_HandleTypeDef *lcd, char *str);

/**
 * @brief Moves the cursor to a specific position on the LCD.
 *
 * NOTA: O seu protótipo original usava `int col, int row`. Mudei para `uint8_t` no código
 * da etapa 1 para ser mais consistente com microcontroladores. Se o seu
 * 'i2c_lcd.c' ainda usar `int`, você deve ajustá-lo para `uint8_t`.
 * Deixei como `int` aqui para não gerar novos conflitos de tipo com seu `.c`.
 *
 * @param lcd: Pointer to the LCD handle
 * @param col: Column number (0-15)
 * @param row: Row number (0 or 1)
 */
void lcd_gotoxy(I2C_LCD_HandleTypeDef *lcd, int col, int row);

/**
 * @brief Clears the LCD display.
 * @param lcd: Pointer to the LCD handle
 */
void lcd_clear(I2C_LCD_HandleTypeDef *lcd);

#endif /* I2C_LCD_H */
