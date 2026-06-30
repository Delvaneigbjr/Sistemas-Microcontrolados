#ifndef LCD_I2C_H
#define LCD_I2C_H

#include <stdint.h>
#include <string.h>

//--- Which STM32 Boards It Is Ready To Function With, Add As Necessary

#if __has_include("stm32f1xx_hal.h")
	#include "stm32f1xx_hal.h"
#elif __has_include("stm32f4xx_hal.h")
	#include "stm32f4xx_hal.h"
#endif

typedef enum
{
    LCD_Command,
    LCD_Text
} LCD_Type_Of_Data;

#define LCD_EN_On 0x04

#define LCD_RS_On 0x01

#define LCD_Clear_Display 0x01

#define LCD_Return_Home 0x02

#define LCD_Set_DDRAM_Address 0x80

#define LCD_Max_Internal_DDRAM_nColumns 0x40

typedef struct
{
    I2C_HandleTypeDef *STM_I2C_Handler;     // I2C Handler For Communication

    uint8_t nColsAndLines,				//Stores (nColumns) and (nLines) in two nybbles
    		cColsAndLines,				//Stores (CurrentColumn) and (CurrentLine) in two nybbles
			AddressAndBl;            	//Stores (I2C Address Of The LCD) and (BackLightState)

}I2C_LCD_Handler;

//--- LCD Hardware Focused Functions

//- STM32 Electrical Translation Of Data To I2C
void LCD_Write_Bus(I2C_LCD_Handler *lcd, uint8_t *data, uint16_t size);

//- Used To Send (Commands ^ Text) To LCD
void LCD_Send(I2C_LCD_Handler *lcd, uint8_t Data_Or_CMD, LCD_Type_Of_Data type);

void LCD_Set_Backlight(I2C_LCD_Handler *lcd, uint8_t mode);

//- Useful For Most Cases, Create More As Needed
void LCD_Default_Init(I2C_LCD_Handler *lcd, I2C_HandleTypeDef *STM_H_I2C, uint8_t nColumns, uint8_t nLines);

//--- User Focused Functions

//-- LCD Handler Gets

//- Althought You Can Use It Directly, The Compiler Often Complains If You Do, So Use These

uint8_t LCD_Get_nColumns(I2C_LCD_Handler *lcd);

uint8_t LCD_Get_nLines(I2C_LCD_Handler *lcd);

uint8_t LCD_Get_Column(I2C_LCD_Handler *lcd);

uint8_t LCD_Get_Line(I2C_LCD_Handler *lcd);

//-- LCD Handler Sets

void LCD_Set_Pos(I2C_LCD_Handler *lcd, uint8_t column, uint8_t line);

//- (LCD_Set_Pos(0,0) & Undoes Any Shifts Made On The Screen)
void LCD_Set_Pos_Home(I2C_LCD_Handler *lcd);

void LCD_Set_Current_Column(I2C_LCD_Handler *lcd, uint8_t column);

void LCD_Set_Current_Line(I2C_LCD_Handler *lcd, uint8_t line);

//-- Writing Functions

void LCD_Write_Char(I2C_LCD_Handler *lcd, char ch);

void LCD_Fill_Line(I2C_LCD_Handler *lcd, uint8_t line, char ch);

void LCD_Fill_Partial_Line(I2C_LCD_Handler *lcd, uint8_t line, uint8_t start, char ch);

void LCD_Fill_All(I2C_LCD_Handler *lcd, char ch);

//- (Clears A Line Starting At A Specific Column & Set_Pos(start,line))
void LCD_Clear_Partial_Line(I2C_LCD_Handler *lcd, uint8_t line, uint8_t start);

//- (Clears A Line & Set_Pos(0,line))
void LCD_Clear_Line(I2C_LCD_Handler *lcd, uint8_t line);

//- (Clears ALL Text & Set_Pos(0,0))
void LCD_Clear_All(I2C_LCD_Handler *lcd);

void LCD_Write_String(I2C_LCD_Handler *lcd, const char *str);

//- Centralize & Writes a String
void LCD_Write_C_String(I2C_LCD_Handler *lcd, const char *str);

//- Converts Numbers Into Text For Exhibition
void LCD_Write_Number(I2C_LCD_Handler *lcd, double num, uint8_t DecimalPrecision);

//-- LCD Advanced Screen Manipulation Functions

//- Any Funtion That Cannot Be Executed SingleHandedly By LCD_Send() (Needs MCU To Happen Properly)

void LCD_Scroll_Shift(I2C_LCD_Handler *lcd, uint8_t Display_Or_Cursor, uint8_t Direction ,  uint32_t ShiftInterval);

void LCD_Scroll_Disappear(I2C_LCD_Handler *lcd, uint8_t Display_Or_Cursor, uint8_t Direction ,  uint32_t ShiftInterval);

//- Writes a string with delay in each char
void LCD_Write_T_String(I2C_LCD_Handler *lcd, const char *str, uint32_t PlacingTime);

//- Centralize & Writes a T_String
void LCD_Write_CT_String(I2C_LCD_Handler *lcd, const char *str, uint32_t PlacingTime);

#endif /* LCD_I2C_H */
