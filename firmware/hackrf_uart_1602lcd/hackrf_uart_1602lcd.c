/*
 * Copyright 2012 Jared Boone
 * Copyright 2013 Benjamin Vernoux
 *
 * This file is part of HackRF.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; see the file COPYING.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street,
 * Boston, MA 02110-1301, USA.
 */

 /*
 * An example of how to interface with 1602 LCD and UART. By Xianjun Jiao (putaoshu@gmail.com)

 * 1602LCD HACKRF pin connection: *

  HACKRF P20 20 -- 1602LCD pin11
  HACKRF P20 18 -- 1602LCD pin12
  HACKRF P20 16 -- 1602LCD pin13
  HACKRF P20 14 -- 1602LCD pin14

  HACKRF P20 11 -- 1602LCD pin6
  HACKRF P20  9 -- 1602LCD pin5
  HACKRF P20  7 -- 1602LCD pin4

  HACKRF    VCC -- 1602LCD pin2 pin15
  HACKRF    GND -- 1602LCD pin1  pin3 pin16

 * UART HACKRF pin connection: *

  HACKRF P25 1 GND       -- GND of computer or USB-uart dongle
  HACKRF P25 4 UART0 RXD -- TXD of computer or USB-uart dongle
  HACKRF P25 5 UART0 TXD -- RXD of computer or USB-uart dongle
 */

#include <stddef.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>

#include <libopencm3/cm3/vector.h>

#include <libopencm3/lpc43xx/scu.h>
#include <libopencm3/lpc43xx/gpio.h>
#include <libopencm3/lpc43xx/m4/nvic.h>
#include <libopencm3/lpc43xx/uart.h>

#include "hackrf_core.h"

// --------------------UART support---------------------
void uart_str(char *a) // send string *a to uart
{
  int i = 0;
  while(a[i] != 0)
  {
    uart_write(UART0, a[i]);
    i++;
  }
}

bool get_uart_rate_config(int uart_rate, uint16_t *uart_divisor, uint8_t *uart_divaddval, uint8_t *uart_mulval)
{
  if (115200 == uart_rate)
  {
    *uart_divisor = 83;
    *uart_divaddval = 1;
    *uart_mulval = 3;
    return(true);
  }

  if (921600 == uart_rate)
  {
    *uart_divisor = 9;
    *uart_divaddval = 7;
    *uart_mulval = 13;
    return(true);
  }

  if (9600 == uart_rate)
  {
    *uart_divisor = 1033;
    *uart_divaddval = 2;
    *uart_mulval = 7;
    return(true);
  }

  return(false);
}
// --------------------UART support---------------------

//---------------------display stuff--------------------
#define PORT_LCD  (GPIO3)
#define LCD_ADJ   (BIT14)
#define LCD_RS    (BIT12)
#define LCD_RW    (BIT10)
#define LCD_E     (BIT8)
#define LCD_D4    (BIT9)
#define LCD_D5    (BIT11)
#define LCD_D6    (BIT13)
#define LCD_D7    (BIT15)

#define DELAY_20MS 20000
#define DELAY_15MS 15000
#define DELAY_5MS 5000
#define WAIT_CPU_CLOCK_INIT_DELAY   (34)  // 1us

void delay_1us(const uint32_t num_1us)
{
  uint32_t i;
  const uint32_t duration = num_1us*WAIT_CPU_CLOCK_INIT_DELAY;

  for (i = 0; i < duration; i++)
    __asm__("nop");
}

// All following LCD operations may need appropriate delay for your specific LCD module.
// Add delay_1us() or change parameter of delay_1us() if needed.

void lcd_data_set(uint8_t a)
{
  if (a&0x01) gpio_set(PORT_LCD, LCD_D4);
  else gpio_clear(PORT_LCD, LCD_D4);

  if (a&0x02) gpio_set(PORT_LCD, LCD_D5);
  else gpio_clear(PORT_LCD, LCD_D5);

  if (a&0x04) gpio_set(PORT_LCD, LCD_D6);
  else gpio_clear(PORT_LCD, LCD_D6);

  if (a&0x08) gpio_set(PORT_LCD, LCD_D7);
  else gpio_clear(PORT_LCD, LCD_D7);
}

void lcd_write_4bit(uint8_t rs, uint8_t a)
{
  if(rs&0x01) gpio_set(PORT_LCD, (LCD_RS));
  else gpio_clear(PORT_LCD, (LCD_RS));

  gpio_set(PORT_LCD, (LCD_E));
  lcd_data_set(a);
  gpio_clear(PORT_LCD, (LCD_E));
}

void lcd_reset()
{ /* set LCD pin as output. */
  GPIO3_DIR |= (LCD_ADJ | LCD_RS | LCD_RW | LCD_E | LCD_D4 | LCD_D5  | LCD_D6  | LCD_D7);
  delay_1us(DELAY_20MS);
  gpio_clear(PORT_LCD, (LCD_ADJ | LCD_RW)); // highest Contrast & RW=WRITE

  // reset sequence for 4bit mode
  gpio_set(PORT_LCD, (LCD_RS | LCD_E | LCD_D4 | LCD_D5  | LCD_D6  | LCD_D7) );
  delay_1us(DELAY_20MS);

  lcd_write_4bit(0, 3);
  delay_1us(DELAY_15MS);

  lcd_write_4bit(0, 3);
  delay_1us(DELAY_5MS);

  lcd_write_4bit(0, 3);
  delay_1us(DELAY_5MS);

  lcd_write_4bit(0, 2);
  delay_1us(DELAY_5MS);
}

void lcd_cmd(uint8_t a)
{
  lcd_write_4bit(0, a>>4);
  lcd_write_4bit(0, a);
  delay_1us(50);
}

void lcd_data(uint8_t a)
{
  lcd_write_4bit(1, a>>4);
  lcd_write_4bit(1, a);
  delay_1us(50);
}

void lcd_init(void)
{
  lcd_reset();

  lcd_cmd(0x28); // 4-bit mode - 2 line - 5x7 font.
  lcd_cmd(0x0C); // Display ON. no cursor - no blink.
  lcd_cmd(0x06); // Automatic Increment - No Display shift.
}

void lcd_clear(void)
{
  lcd_cmd(0x01); // fill with blank
  delay_1us(800);
  lcd_cmd(0x02); // reset cursor
  delay_1us(800);
}

void lcd_str(uint8_t line_start, uint8_t start_offset, char *a)
{
  lcd_cmd(line_start + start_offset);
  int i = 0;
  while(a[i] != 0)
  {
    lcd_data(a[i]);
    i++;
  }
}

#define CMD_LINE0_START 0x80
#define CMD_LINE1_START 0xC0
void lcd_li0_str(uint8_t start_offset, char *a) //put string to LCD row 0 from offset
{
  lcd_str(CMD_LINE0_START, start_offset, a);
}

void lcd_li1_str(uint8_t start_offset, char *a) //put string to LCD row 1 from offset
{
  lcd_str(CMD_LINE1_START, start_offset, a);
}

void led_set(int a)
{
    if (a&0x01) gpio_set(PORT_LED1_3, (PIN_LED1));
    else gpio_clear(PORT_LED1_3, (PIN_LED1));

    if (a&0x02) gpio_set(PORT_LED1_3, (PIN_LED2));
    else gpio_clear(PORT_LED1_3, (PIN_LED2));

    if (a&0x04) gpio_set(PORT_LED1_3, (PIN_LED3));
    else gpio_clear(PORT_LED1_3, (PIN_LED3));
}
//---------------------display stuff--------------------

char STR_TMP_BUFFER[128]; //display buffer

int main(void) {
  uint16_t uart_divisor;
  uint8_t uart_divaddval, uart_mulval, i;

  pin_setup();
  /* Configure UART0 PIN*/
  scu_pinmux(SCU_PINMUX_U0_TXD, SCU_UART_RX_TX | SCU_CONF_FUNCTION1);
  scu_pinmux(SCU_PINMUX_U0_RXD, SCU_UART_RX_TX | SCU_CONF_FUNCTION1);

  enable_1v8_power();

  cpu_clock_init();
  cpu_clock_pll1_max_speed();

  lcd_init();
  lcd_clear();

  led_set(0x00); // turn off all three led
  if ( !get_uart_rate_config(921600, &uart_divisor, &uart_divaddval, &uart_mulval) )
  // you may change baud rate 921600 to 9600 or 115200
  {
    led_set(0xff); // if uart set error, turn on all three led
    return(1);
  }
  uart_init(UART0, UART_DATABIT_8,
      UART_STOPBIT_1, UART_PARITY_NONE,
      uart_divisor, uart_divaddval, uart_mulval);

  i = 0;
  while(1)
  {
    sprintf(STR_TMP_BUFFER, "Hello HACKRF. %d", i);
    lcd_li0_str(0, STR_TMP_BUFFER);

    sprintf(STR_TMP_BUFFER, "Hello HACKRF. %d", 7-i);
    lcd_li1_str(0, STR_TMP_BUFFER);

    sprintf(STR_TMP_BUFFER, "Hello HACKRF. %d\n", 8+i);
    uart_str( STR_TMP_BUFFER );

    led_set(i);

    i = (i+1)&0x07;

    delay_1us(500000); //delay 500ms roughly
  }
  return 0;
}
