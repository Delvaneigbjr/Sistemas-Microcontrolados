#include "LCD_I2C.h"

//--- LCD Hardware Focused Functions

void LCD_Write_Bus(I2C_LCD_Handler *lcd, uint8_t *data, uint16_t size) {HAL_I2C_Master_Transmit(lcd->STM_I2C_Handler, (lcd->AddressAndBl & ~0x01), data, size, 100);}

void LCD_Send(I2C_LCD_Handler *lcd, uint8_t Data_Or_CMD, LCD_Type_Of_Data Type)
{
    uint8_t Data_Buffer[4];

    for(uint8_t Data_Pulse = 0; Data_Pulse < 4 ; Data_Pulse++)
    	Data_Buffer[Data_Pulse] = ( ( (Data_Or_CMD << (0x04 & Data_Pulse<<1) ) & 0xF0 ) | ((lcd->AddressAndBl&0x01)<<3) | (((Data_Pulse&0x01)^0x01)<<2) | (Type&LCD_RS_On) );

    LCD_Write_Bus(lcd, Data_Buffer, sizeof(Data_Buffer));
}

void LCD_Set_Backlight(I2C_LCD_Handler *lcd, uint8_t mode)
{
    lcd->AddressAndBl = (lcd->AddressAndBl & ~0x01) | mode;

    uint8_t BLtUpdater = mode;

    LCD_Write_Bus(lcd, &BLtUpdater, sizeof(BLtUpdater));
}

void LCD_Default_Init(I2C_LCD_Handler *lcd, I2C_HandleTypeDef *STM_H_I2C, uint8_t nColumns, uint8_t nLines)
{
//- Struct Building
	lcd->STM_I2C_Handler = STM_H_I2C;
	for(uint8_t address = 0x08; address <= 0x77; address++) // Valid 7bits I2C Addresses
	    if(HAL_I2C_IsDeviceReady(STM_H_I2C, address << 1, 3, 50) == HAL_OK)
			lcd->AddressAndBl = (address << 1) | 0x01;
	lcd->nColsAndLines = (lcd->nColsAndLines & 0x07) | nColumns<<3;
	lcd->nColsAndLines = (lcd->nColsAndLines & ~0x07) | nLines;

//- Wake Up Sequence
	HAL_Delay(50); // Wait For LCD To Power-Up

	for(uint8_t CPR = 0; CPR < 3; CPR++) // Does "CPR" On The LCD Hoping It Will Begin Receiving Data
	{
		LCD_Send(lcd, 0x30, LCD_Command); //LCD_8bits_Mode, LCD_1Line_Mode, LCD_5x8_Font
		HAL_Delay(1); // "Compression Rhythm"
	}

	LCD_Send(lcd, 0x20, LCD_Command); //LCD_4bits_Mode, LCD_1Line_Mode, LCD_5x8_Font
	HAL_Delay(1);

//- Function Set
	LCD_Send(lcd, 0x28, LCD_Command); // LCD_8bits_Mode, LCD_1Line_Mode, LCD_5x8_Font
	HAL_Delay(1);

//- Display Off
	LCD_Send(lcd, 0x08, LCD_Command); //LCD_Display_Off, LCD_Cursor_Off, LCD_Blink_Off)

//- Clear Display
	LCD_Send(lcd, LCD_Clear_Display, LCD_Command);
	HAL_Delay(2); //Required By DataSheet (max ~2ms)

//- Entry Mode Set
	LCD_Send(lcd, 0x06, LCD_Command); // LCD_Display_On, LCD_Cursor_Off, LCD_Blink_Off)
	HAL_Delay(1);

//- Display ON
	LCD_Send(lcd, 0x0C, LCD_Command);
	HAL_Delay(1);
}

//--- User Focused Functions

//-- LCD Handler Gets

uint8_t LCD_Get_nColumns(I2C_LCD_Handler *lcd) {return ((lcd->nColsAndLines & ~0x07)>>3);}

uint8_t LCD_Get_nLines(I2C_LCD_Handler *lcd) {return (lcd->nColsAndLines & 0x07);}

uint8_t LCD_Get_Column(I2C_LCD_Handler *lcd) {return ((lcd->cColsAndLines & ~0x07)>>3);}

uint8_t LCD_Get_Line(I2C_LCD_Handler *lcd) {return (lcd->cColsAndLines & 0x07);}

//-- LCD Handler Sets

void LCD_Set_Pos(I2C_LCD_Handler *lcd, uint8_t Column, uint8_t Line)
{
	if(Column >= ((lcd->nColsAndLines & ~0x07)>>3))
	    return; //Invalid Column

    uint8_t address;

    switch (Line) // Address Values May Differ, But This Ought To Work With Most LCDs
    {
        case 0: address = 0x00 + Column; break;  // First line
        case 1: address = 0x40 + Column; break;  // Second line
        case 2: address = 0x14 + Column; break;  // Third line
        case 3: address = 0x54 + Column; break;  // Fourth line
        default: return; // Invalid Line
    }


    lcd->cColsAndLines = (lcd->cColsAndLines & 0x07) | Column << 3;
    lcd->cColsAndLines = (lcd->cColsAndLines & ~0x07) | Line;

    LCD_Send(lcd, (LCD_Set_DDRAM_Address | address), LCD_Command);
}

//- (LCD_Set_Pos(0,0) & Undoes Any Shifts Made On The Screen)
void LCD_Set_Pos_Home(I2C_LCD_Handler *lcd)
{
    LCD_Send(lcd, LCD_Return_Home, LCD_Command);

    HAL_Delay(2);

    lcd->cColsAndLines = 0x00;
}

void LCD_Set_Current_Column(I2C_LCD_Handler *lcd, uint8_t Column) {LCD_Set_Pos(lcd, Column, (lcd->cColsAndLines & 0x07));}

void LCD_Set_Current_Line(I2C_LCD_Handler *lcd, uint8_t Line) {LCD_Set_Pos(lcd, (lcd->cColsAndLines & ~0x07) >> 3, Line);}

//-- Writing Functions

void LCD_Write_Char(I2C_LCD_Handler *lcd, char ch)
{
    LCD_Send(lcd, ch, LCD_Text);

    if ((1 + ((lcd->cColsAndLines & ~0x07) >> 3)) >= ((lcd->nColsAndLines & ~0x07) >> 3))
    {
        lcd->cColsAndLines = (1 + (lcd->cColsAndLines & 0x07) >= (lcd->nColsAndLines & 0x07)) ?
                             0x00
                             :
                             (1 + (lcd->cColsAndLines & 0x07));

        LCD_Set_Pos(lcd, (lcd->cColsAndLines & ~0x07) >> 3, (lcd->cColsAndLines & 0x07));
    }
    else
        lcd->cColsAndLines = (lcd->cColsAndLines & 0x07) | ((1 + ((lcd->cColsAndLines & ~0x07) >> 3)) << 3);
}

void LCD_Fill_Partial_Line(I2C_LCD_Handler *lcd, uint8_t line, uint8_t start, char ch)
{
    LCD_Set_Pos(lcd, start, line);
    while ((lcd->cColsAndLines & 0x07) == line) LCD_Write_Char(lcd, ch);
}

void LCD_Fill_Line(I2C_LCD_Handler *lcd, uint8_t line, char ch) {LCD_Fill_Partial_Line(lcd, line, 0 , ch);}

void LCD_Fill_All(I2C_LCD_Handler *lcd, char ch) {for (uint8_t line = 0; line < (lcd->nColsAndLines & 0x07); line++)LCD_Fill_Line(lcd, line, ch);}

//- (Clears A Line Starting At A Specific Column & Set_Pos(start,line))
void LCD_Clear_Partial_Line(I2C_LCD_Handler *lcd, uint8_t line, uint8_t start)
{
    LCD_Fill_Partial_Line(lcd, line, start, ' ');

    LCD_Set_Pos(lcd, start, line);
}

//- (Clears A Line & Set_Pos(0,line))
void LCD_Clear_Line(I2C_LCD_Handler *lcd, uint8_t line)
{
    LCD_Fill_Line(lcd, line , ' ');

    LCD_Set_Pos(lcd, 0, line);
}

//- (Clears ALL Text & Set_Pos(0,0))
void LCD_Clear_All(I2C_LCD_Handler *lcd)
{
    LCD_Send(lcd, LCD_Clear_Display, LCD_Command);

    HAL_Delay(2); //Required By DataSheet (max ~2ms)

    lcd->cColsAndLines = 0x00;
}

void LCD_Write_String(I2C_LCD_Handler *lcd, const char *str) {while (*str) LCD_Write_Char(lcd, *str++);}

//- Centralize & Writes a String
void LCD_Write_C_String(I2C_LCD_Handler *lcd, const char *str)
{
	uint8_t Centrifying = (((lcd->nColsAndLines & ~0x07)>>3)-((uint8_t)strlen(str)));
	LCD_Set_Current_Column(lcd, Centrifying==0? 0 : (Centrifying>>1));
	LCD_Write_String(lcd, str);
}

//- Converts Numbers Into Text For Exhibition
void LCD_Write_Number(I2C_LCD_Handler *lcd, double Num, uint8_t DecimalPrecision)
{
	if(DecimalPrecision > 9)
	    return; //Decimal Precision is too big for uint32_t conversion

	uint8_t TxtSize = 0;

    uint32_t Uint32Num = Num<0?
    		(TxtSize++,(uint32_t)(-Num))
    		:
			(uint32_t)Num;

    uint8_t Uint32NumDigits = 1;

    for(uint32_t CountingDigits = Uint32Num ; CountingDigits >= 10; CountingDigits /= 10)
        Uint32NumDigits++;

     TxtSize += DecimalPrecision?
		Uint32NumDigits + 1 + DecimalPrecision + 1
		:
		Uint32NumDigits + 1;

    char Txt[TxtSize];

    uint8_t index = sizeof(Txt) - 1;

    if(index>(((lcd->nColsAndLines & ~0x07)>>3)-((lcd->cColsAndLines & ~0x07)>>3)))
    	return; //num too big to exhibit

    Txt[index] = '\0';

    if(DecimalPrecision)
    {
        uint32_t Pow10[] =
        {
            1, 			// 0
            10, 		// 1
            100,		// 2
            1000,		// 3
            10000,		// 4
            100000,		// 5
            1000000,	// 6
            10000000,	// 7
            100000000,	// 8
            1000000000 	// 9
        }; // uint32_t has max value of 10 digits

        double decimal = Num<0?
        		((-Num) - Uint32Num)
				:
				(Num - Uint32Num);

        uint32_t dNum = (uint32_t)(decimal * Pow10[DecimalPrecision]);

        for(uint8_t DecimalDigits = 0; DecimalDigits < DecimalPrecision; DecimalDigits++)
        {
            Txt[--index] = '0' + (dNum % 10);
            dNum /= 10;
        }

        Txt[--index] = ',';
    }

    do
    {
        Txt[--index] = '0' + (Uint32Num % 10);
        Uint32Num /= 10;
    }
    while(Uint32Num);

    if (Num<0)
    	Txt[--index] = '-';

    LCD_Write_String(lcd, &Txt[index]);
}

//-- LCD Advanced Screen Manipulation Functions

//- Any Funtion That Cannot Be Executed SingleHandedly By LCD_Send() (Needs MCU To Happen Properly)

void LCD_Scroll_Shift(I2C_LCD_Handler *lcd, uint8_t Display_Or_Cursor, uint8_t Direction ,  uint32_t ShiftInterval)
{
	for(uint8_t column = 0; column < LCD_Max_Internal_DDRAM_nColumns; column++)
	{
		LCD_Send(lcd, (0x10 | Display_Or_Cursor | Direction), LCD_Command);
		HAL_Delay(ShiftInterval);
	}
}

void LCD_Scroll_Disappear(I2C_LCD_Handler *lcd, uint8_t Display_Or_Cursor, uint8_t Direction ,  uint32_t ShiftInterval)
{
	for(uint8_t column = 0; column < (LCD_Max_Internal_DDRAM_nColumns>>1); column++)
	{
		LCD_Send(lcd, (0x10 | Display_Or_Cursor | Direction), LCD_Command);
		HAL_Delay(ShiftInterval);
	}
	LCD_Clear_All(lcd);
}

//- Writes a string with delay in each char
void LCD_Write_T_String(I2C_LCD_Handler *lcd, const char *str, uint32_t PlacingTime) {while (*str){LCD_Write_Char(lcd, *str++);HAL_Delay(PlacingTime);}}

//- Centralize & Writes a T_String
void LCD_Write_CT_String(I2C_LCD_Handler *lcd, const char *str, uint32_t PlacingTime)
{
	uint8_t Centrifying = (((lcd->nColsAndLines & ~0x07)>>3)-((uint8_t)strlen(str)));
	LCD_Set_Current_Column(lcd, Centrifying==0? 0 : (Centrifying>>1));
	while (*str)
	{
		LCD_Write_Char(lcd, *str++);
		HAL_Delay(PlacingTime);
	}
}
