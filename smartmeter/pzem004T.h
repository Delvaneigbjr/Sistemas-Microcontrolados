/*
 * pzem004T.h
 *
 *  Created on: 29 Eki 2022
 *      Author: theen
 */

#ifndef SRC_PZEM004T_H_
#define SRC_PZEM004T_H_

#include "stm32f4xx_hal.h"
#include "stm32f4xx.h"
#include "stdio.h"
#include "string.h"
#include "stdlib.h"
#include <stdarg.h>

#define UART_PZEM USART1
#define RXBUFFER_MAX_SIZE 30

#define PZEM_DEFAULT_ADDR   0xF8
#define PZEM_BAUD_RATE      9600

#define REG_VOLTAGE     0x0000
#define REG_CURRENT_L   0x0001
#define REG_CURRENT_H   0X0002
#define REG_POWER_L     0x0003
#define REG_POWER_H     0x0004
#define REG_ENERGY_L    0x0005
#define REG_ENERGY_H    0x0006
#define REG_FREQUENCY   0x0007
#define REG_PF          0x0008
#define REG_ALARM       0x0009

#define CMD_RHR         0x03
#define CMD_RIR         0X04
#define CMD_WSR         0x06
#define CMD_CAL         0x41
#define CMD_REST        0x42

#define WREG_ALARM_THR   0x0001
#define WREG_ADDR        0x0002

#define UPDATE_TIME     200

#define RESPONSE_SIZE 32
#define READ_TIMEOUT 100

#define INVALID_ADDRESS 0x00

typedef struct {

	float voltage;
	float current;
	float power;
	float energy;
	float frequency;
	float pf;
	uint16_t alarms;
} _values;

typedef struct {
	UART_HandleTypeDef *huart;
	volatile uint8_t rxbufferIndex;
	volatile uint8_t rxBuffer[RXBUFFER_MAX_SIZE];
	uint8_t byteRx;
	uint8_t _addr;
	_values values;

} pzem;

void init_pzem(pzem *pzem, UART_HandleTypeDef *huart, uint8_t addr);
uint8_t readAddress(pzem *pzem);
void sendCmd8(pzem *pzem, uint8_t cmd, uint16_t rAddr, uint16_t val,
		uint16_t slave_addr);
void setCRC(uint8_t *buf, uint16_t len);
uint16_t CRC16(const volatile uint8_t *data, uint16_t len);
uint8_t CRC_CHECK(pzem *pzem1, uint8_t len);
uint8_t updateValues(pzem *pzem1);
float getVolt(pzem *pzem);
float getCurrent(pzem *pzem);
float getPower(pzem *pzem);
float getEnergy(pzem *pzem);
float getFrequency(pzem *pzem);
float getpF(pzem *pzem);

#endif /* SRC_PZEM004T_H_ */
