/**
  ******************************************************************************
  * @file           : lcd_i2c.c
  * @brief          : Implementação da Biblioteca LCD I2C
  * @author         : Professor MicroControl
  * @version        : 2.0
  ******************************************************************************
  */

#include "lcd_i2c.h"
#include <stdarg.h>
#include <stdio.h>

// ============================================================================
// COMANDOS INTERNOS DO LCD HD44780
// ============================================================================
#define LCD_CMD_CLEAR           0x01
#define LCD_CMD_HOME            0x02
#define LCD_CMD_ENTRY_MODE      0x04
#define LCD_CMD_DISPLAY_CTRL    0x08
#define LCD_CMD_CURSOR_SHIFT    0x10
#define LCD_CMD_FUNCTION_SET    0x20
#define LCD_CMD_CGRAM_ADDR      0x40
#define LCD_CMD_DDRAM_ADDR      0x80

// Flags para Entry Mode
#define LCD_ENTRY_RIGHT         0x00
#define LCD_ENTRY_LEFT          0x02
#define LCD_ENTRY_SHIFT_INC     0x01
#define LCD_ENTRY_SHIFT_DEC     0x00

// Flags para Display Control
#define LCD_DISPLAY_ON          0x04
#define LCD_DISPLAY_OFF         0x00
#define LCD_CURSOR_ON           0x02
#define LCD_CURSOR_OFF          0x00
#define LCD_BLINK_ON            0x01
#define LCD_BLINK_OFF           0x00

// Flags para Function Set
#define LCD_8BIT_MODE           0x10
#define LCD_4BIT_MODE           0x00
#define LCD_2LINE               0x08
#define LCD_1LINE               0x00
#define LCD_5x10_DOTS           0x04
#define LCD_5x8_DOTS            0x00

// Bits de controle I2C
#define LCD_BIT_EN              0x04  // Enable bit
#define LCD_BIT_RW              0x02  // Read/Write bit
#define LCD_BIT_RS              0x01  // Register select bit
#define LCD_BIT_BACKLIGHT       0x08  // Backlight bit

// ============================================================================
// VARIÁVEIS PRIVADAS
// ============================================================================
static uint8_t display_control = 0;
static uint8_t display_mode = 0;

// ============================================================================
// FUNÇÕES PRIVADAS (LOW-LEVEL)
// ============================================================================

/**
 * @brief  Envia 4 bits para o LCD (modo nibble)
 * @param  hlcd: Ponteiro para handle LCD
 * @param  dados: Byte de dados (usa apenas 4 bits superiores)
 * @param  modo: 0=comando, 1=dados
 * @retval HAL_StatusTypeDef
 */
static HAL_StatusTypeDef LCD_SendNibble(LCD_HandleTypeDef *hlcd, uint8_t dados, uint8_t modo) {
    uint8_t buffer[4];
    uint8_t backlight = hlcd->backlight ? LCD_BIT_BACKLIGHT : 0x00;
    uint8_t rs = modo ? LCD_BIT_RS : 0x00;

    // Prepara o nibble alto com os bits de controle
    uint8_t high_nibble = (dados & 0xF0) | rs | backlight;

    // Sequência de pulso Enable: EN=1 -> EN=0
    buffer[0] = high_nibble | LCD_BIT_EN;   // EN = 1
    buffer[1] = high_nibble;                 // EN = 0

    // Envia via I2C
    return HAL_I2C_Master_Transmit(hlcd->hi2c, hlcd->endereco, buffer, 2, LCD_TIMEOUT);
}

/**
 * @brief  Envia um byte completo para o LCD (2 nibbles)
 * @param  hlcd: Ponteiro para handle LCD
 * @param  dados: Byte completo
 * @param  modo: 0=comando, 1=dados
 * @retval HAL_StatusTypeDef
 */
static HAL_StatusTypeDef LCD_SendByte(LCD_HandleTypeDef *hlcd, uint8_t dados, uint8_t modo) {
    HAL_StatusTypeDef status;

    // Envia nibble alto
    status = LCD_SendNibble(hlcd, dados & 0xF0, modo);
    if(status != HAL_OK) return status;

    // Envia nibble baixo
    status = LCD_SendNibble(hlcd, (dados << 4) & 0xF0, modo);
    if(status != HAL_OK) return status;

    // Delay para processamento
    HAL_Delay(1);

    return HAL_OK;
}

/**
 * @brief  Envia comando para o LCD
 * @param  hlcd: Ponteiro para handle LCD
 * @param  cmd: Comando
 * @retval HAL_StatusTypeDef
 */
static HAL_StatusTypeDef LCD_SendCommand(LCD_HandleTypeDef *hlcd, uint8_t cmd) {
    return LCD_SendByte(hlcd, cmd, 0);
}

/**
 * @brief  Envia dado para o LCD
 * @param  hlcd: Ponteiro para handle LCD
 * @param  data: Dado
 * @retval HAL_StatusTypeDef
 */
static HAL_StatusTypeDef LCD_SendData(LCD_HandleTypeDef *hlcd, uint8_t data) {
    return LCD_SendByte(hlcd, data, 1);
}

// ============================================================================
// FUNÇÕES PÚBLICAS - INICIALIZAÇÃO
// ============================================================================

LCD_Status_t LCD_Init(LCD_HandleTypeDef *hlcd, I2C_HandleTypeDef *hi2c) {
    if(hlcd == NULL || hi2c == NULL) {
        return LCD_ERRO_PARAMETRO_INVALIDO;
    }

    hlcd->hi2c = hi2c;
    hlcd->backlight = true;
    hlcd->inicializado = false;

    // Tenta detectar endereço I2C (0x27 ou 0x3F)
    uint8_t enderecos[] = {0x27 << 1, 0x3F << 1};
    bool encontrado = false;

    for(int i = 0; i < 2; i++) {
        if(HAL_I2C_IsDeviceReady(hi2c, enderecos[i], 3, 100) == HAL_OK) {
            hlcd->endereco = enderecos[i];
            encontrado = true;
            break;
        }
    }

    if(!encontrado) {
        return LCD_ERRO_I2C;
    }

    // Aguarda estabilização (>40ms após VCC=4.5V)
    HAL_Delay(50);

    // Sequência de inicialização do HD44780 (modo 4 bits)
    LCD_SendNibble(hlcd, 0x30, 0);  // Function set (8-bit)
    HAL_Delay(5);
    LCD_SendNibble(hlcd, 0x30, 0);  // Function set (8-bit)
    HAL_Delay(1);
    LCD_SendNibble(hlcd, 0x30, 0);  // Function set (8-bit)
    HAL_Delay(1);
    LCD_SendNibble(hlcd, 0x20, 0);  // Function set (4-bit)
    HAL_Delay(1);

    // Configuração final
    LCD_SendCommand(hlcd, LCD_CMD_FUNCTION_SET | LCD_4BIT_MODE | LCD_2LINE | LCD_5x8_DOTS);
    HAL_Delay(1);

    // Display OFF
    display_control = LCD_CMD_DISPLAY_CTRL | LCD_DISPLAY_OFF;
    LCD_SendCommand(hlcd, display_control);
    HAL_Delay(1);

    // Clear display
    LCD_SendCommand(hlcd, LCD_CMD_CLEAR);
    HAL_Delay(2);

    // Entry mode
    display_mode = LCD_CMD_ENTRY_MODE | LCD_ENTRY_LEFT | LCD_ENTRY_SHIFT_DEC;
    LCD_SendCommand(hlcd, display_mode);
    HAL_Delay(1);

    // Display ON
    display_control = LCD_CMD_DISPLAY_CTRL | LCD_DISPLAY_ON | LCD_CURSOR_OFF | LCD_BLINK_OFF;
    LCD_SendCommand(hlcd, display_control);
    HAL_Delay(1);

    hlcd->inicializado = true;
    return LCD_OK;
}

bool LCD_IsConnected(LCD_HandleTypeDef *hlcd) {
    if(hlcd == NULL || hlcd->hi2c == NULL) return false;
    return (HAL_I2C_IsDeviceReady(hlcd->hi2c, hlcd->endereco, 1, 100) == HAL_OK);
}

// ============================================================================
// FUNÇÕES PÚBLICAS - CONTROLE BÁSICO
// ============================================================================

LCD_Status_t LCD_Clear(LCD_HandleTypeDef *hlcd) {
    if(!hlcd->inicializado) return LCD_ERRO_NAO_INICIALIZADO;

    if(LCD_SendCommand(hlcd, LCD_CMD_CLEAR) != HAL_OK) {
        return LCD_ERRO_I2C;
    }
    HAL_Delay(2);  // Clear precisa de mais tempo
    return LCD_OK;
}

LCD_Status_t LCD_Home(LCD_HandleTypeDef *hlcd) {
    if(!hlcd->inicializado) return LCD_ERRO_NAO_INICIALIZADO;

    if(LCD_SendCommand(hlcd, LCD_CMD_HOME) != HAL_OK) {
        return LCD_ERRO_I2C;
    }
    HAL_Delay(2);
    return LCD_OK;
}

LCD_Status_t LCD_SetCursor(LCD_HandleTypeDef *hlcd, uint8_t linha, uint8_t coluna) {
    if(!hlcd->inicializado) return LCD_ERRO_NAO_INICIALIZADO;
    if(linha >= LCD_LINHAS || coluna >= LCD_COLUNAS) return LCD_ERRO_PARAMETRO_INVALIDO;

    // Endereços DDRAM: Linha 0 = 0x00, Linha 1 = 0x40
    uint8_t offset[] = {0x00, 0x40, 0x14, 0x54};  // Para LCD 16x2 e 20x4
    uint8_t endereco = LCD_CMD_DDRAM_ADDR | (coluna + offset[linha]);

    if(LCD_SendCommand(hlcd, endereco) != HAL_OK) {
        return LCD_ERRO_I2C;
    }
    return LCD_OK;
}

// ============================================================================
// FUNÇÕES PÚBLICAS - ESCRITA DE TEXTO
// ============================================================================

LCD_Status_t LCD_Print(LCD_HandleTypeDef *hlcd, const char *str) {
    if(!hlcd->inicializado) return LCD_ERRO_NAO_INICIALIZADO;
    if(str == NULL) return LCD_ERRO_PARAMETRO_INVALIDO;

    while(*str) {
        if(LCD_SendData(hlcd, *str++) != HAL_OK) {
            return LCD_ERRO_I2C;
        }
    }
    return LCD_OK;
}

LCD_Status_t LCD_PutChar(LCD_HandleTypeDef *hlcd, char c) {
    if(!hlcd->inicializado) return LCD_ERRO_NAO_INICIALIZADO;

    if(LCD_SendData(hlcd, c) != HAL_OK) {
        return LCD_ERRO_I2C;
    }
    return LCD_OK;
}

LCD_Status_t LCD_Printf(LCD_HandleTypeDef *hlcd, const char *format, ...) {
    if(!hlcd->inicializado) return LCD_ERRO_NAO_INICIALIZADO;

    char buffer[LCD_COLUNAS + 1];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    return LCD_Print(hlcd, buffer);
}

LCD_Status_t LCD_PrintAt(LCD_HandleTypeDef *hlcd, uint8_t linha, uint8_t coluna, const char *str) {
    LCD_Status_t status = LCD_SetCursor(hlcd, linha, coluna);
    if(status != LCD_OK) return status;
    return LCD_Print(hlcd, str);
}

// ============================================================================
// FUNÇÕES PÚBLICAS - CONTROLE DE DISPLAY
// ============================================================================

LCD_Status_t LCD_Backlight(LCD_HandleTypeDef *hlcd, bool estado) {
    if(!hlcd->inicializado) return LCD_ERRO_NAO_INICIALIZADO;

    hlcd->backlight = estado;

    // Envia comando vazio apenas para atualizar backlight
    uint8_t data = estado ? LCD_BIT_BACKLIGHT : 0x00;
    if(HAL_I2C_Master_Transmit(hlcd->hi2c, hlcd->endereco, &data, 1, LCD_TIMEOUT) != HAL_OK) {
        return LCD_ERRO_I2C;
    }
    return LCD_OK;
}

LCD_Status_t LCD_Cursor(LCD_HandleTypeDef *hlcd, bool estado) {
    if(!hlcd->inicializado) return LCD_ERRO_NAO_INICIALIZADO;

    if(estado) {
        display_control |= LCD_CURSOR_ON;
    } else {
        display_control &= ~LCD_CURSOR_ON;
    }

    if(LCD_SendCommand(hlcd, display_control) != HAL_OK) {
        return LCD_ERRO_I2C;
    }
    return LCD_OK;
}

LCD_Status_t LCD_Blink(LCD_HandleTypeDef *hlcd, bool estado) {
    if(!hlcd->inicializado) return LCD_ERRO_NAO_INICIALIZADO;

    if(estado) {
        display_control |= LCD_BLINK_ON;
    } else {
        display_control &= ~LCD_BLINK_ON;
    }

    if(LCD_SendCommand(hlcd, display_control) != HAL_OK) {
        return LCD_ERRO_I2C;
    }
    return LCD_OK;
}

LCD_Status_t LCD_Display(LCD_HandleTypeDef *hlcd, bool estado) {
    if(!hlcd->inicializado) return LCD_ERRO_NAO_INICIALIZADO;

    if(estado) {
        display_control |= LCD_DISPLAY_ON;
    } else {
        display_control &= ~LCD_DISPLAY_ON;
    }

    if(LCD_SendCommand(hlcd, display_control) != HAL_OK) {
        return LCD_ERRO_I2C;
    }
    return LCD_OK;
}

// ============================================================================
// FUNÇÕES PÚBLICAS - CARACTERES CUSTOMIZADOS
// ============================================================================

LCD_Status_t LCD_CreateChar(LCD_HandleTypeDef *hlcd, LCD_CustomChar_t local, uint8_t bitmap[8]) {
    if(!hlcd->inicializado) return LCD_ERRO_NAO_INICIALIZADO;
    if(local > LCD_CHAR_7) return LCD_ERRO_PARAMETRO_INVALIDO;

    // Define endereço CGRAM
    if(LCD_SendCommand(hlcd, LCD_CMD_CGRAM_ADDR | (local << 3)) != HAL_OK) {
        return LCD_ERRO_I2C;
    }

    // Escreve 8 bytes do bitmap
    for(int i = 0; i < 8; i++) {
        if(LCD_SendData(hlcd, bitmap[i]) != HAL_OK) {
            return LCD_ERRO_I2C;
        }
    }

    // Retorna para DDRAM
    LCD_SetCursor(hlcd, 0, 0);
    return LCD_OK;
}

// ============================================================================
// FUNÇÕES PÚBLICAS - UTILITÁRIOS
// ============================================================================

LCD_Status_t LCD_ScrollLeft(LCD_HandleTypeDef *hlcd) {
    if(!hlcd->inicializado) return LCD_ERRO_NAO_INICIALIZADO;

    if(LCD_SendCommand(hlcd, LCD_CMD_CURSOR_SHIFT | 0x08) != HAL_OK) {
        return LCD_ERRO_I2C;
    }
    return LCD_OK;
}

LCD_Status_t LCD_ScrollRight(LCD_HandleTypeDef *hlcd) {
    if(!hlcd->inicializado) return LCD_ERRO_NAO_INICIALIZADO;

    if(LCD_SendCommand(hlcd, LCD_CMD_CURSOR_SHIFT | 0x0C) != HAL_OK) {
        return LCD_ERRO_I2C;
    }
    return LCD_OK;
}

LCD_Status_t LCD_DrawProgressBar(LCD_HandleTypeDef *hlcd, uint8_t linha, uint8_t percentual) {
    if(!hlcd->inicializado) return LCD_ERRO_NAO_INICIALIZADO;
    if(linha >= LCD_LINHAS) return LCD_ERRO_PARAMETRO_INVALIDO;
    if(percentual > 100) percentual = 100;

    // Cria caracteres customizados para barra
    uint8_t empty[8] =  {0b10001, 0b10001, 0b10001, 0b10001, 0b10001, 0b10001, 0b10001, 0b11111};
    uint8_t full[8] =   {0b11111, 0b11111, 0b11111, 0b11111, 0b11111, 0b11111, 0b11111, 0b11111};

    LCD_CreateChar(hlcd, LCD_CHAR_0, empty);
    LCD_CreateChar(hlcd, LCD_CHAR_1, full);

    // Desenha barra
    LCD_SetCursor(hlcd, linha, 0);
    uint8_t blocos_cheios = (percentual * LCD_COLUNAS) / 100;

    for(int i = 0; i < LCD_COLUNAS; i++) {
        if(i < blocos_cheios) {
            LCD_PutChar(hlcd, 1);  // Bloco cheio
        } else {
            LCD_PutChar(hlcd, 0);  // Bloco vazio
        }
    }

    return LCD_OK;
}

const char* LCD_GetErrorString(LCD_Status_t status) {
    switch(status) {
        case LCD_OK: return "OK";
        case LCD_ERRO_I2C: return "Erro I2C";
        case LCD_ERRO_TIMEOUT: return "Timeout";
        case LCD_ERRO_NAO_INICIALIZADO: return "Nao inicializado";
        case LCD_ERRO_PARAMETRO_INVALIDO: return "Parametro invalido";
        default: return "Erro desconhecido";
    }
}
