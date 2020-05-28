/*
 * main.c
 *
 *  Created on: 25 мая 2020 г.
 *      Author: spide
 */

#include <stdint.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>
#include <avr/wdt.h>
#include <util/delay.h>
#include "crc7.h"
#include "filter.h"

#define SOFT_TX_PIN (1<<PB0) // PB0 будет работать как TXD
#define SOFT_TX_PORT PORTB
#define SOFT_TX_DDR DDRB

void uart_tx_init ()
{
  TCCR0A = 1 << WGM01;		// compare mode
  TCCR0B = 1 << CS01;		// prescaler 8
  SOFT_TX_PORT |= SOFT_TX_PIN;
  SOFT_TX_DDR |= SOFT_TX_PIN;
  OCR0A = 125;			//9600 baudrate at prescaler 8
}

//bitbanged UART transmit byte
void uart_send_byte (unsigned char data)
{
  uint8_t i;
  TCCR0B = 0;
  TCNT0 = 0;
  TIFR0 |= 1 << OCF0A;
  TCCR0B |= (1 << CS01);
  TIFR0 |= 1 << OCF0A;
  SOFT_TX_PORT &= ~SOFT_TX_PIN;
  while (!(TIFR0 & (1 << OCF0A)));
  TIFR0 |= 1 << OCF0A;
  for (i = 0; i < 8; i++)
  {
    if (data & 1)
      SOFT_TX_PORT |= SOFT_TX_PIN;
    else
      SOFT_TX_PORT &= ~SOFT_TX_PIN;
    data >>= 1;
    while (!(TIFR0 & (1 << OCF0A)));
    TIFR0 |= 1 << OCF0A;
  }
  SOFT_TX_PORT |= SOFT_TX_PIN;
  while (!(TIFR0 & (1 << OCF0A)));
  TIFR0 |= 1 << OCF0A;
}

void uart_print(char *str)
{
  uint8_t i = 0;
  while (str[i]) {
    uart_send_byte(str[i++]);
  }
}

// Функция вывода содержимого переменной
void num_to_str(unsigned int value, unsigned char nDigit)
{
  switch (nDigit)
  {
  case 4:
    uart_send_byte((value / 1000) + '0');
  case 3:
    uart_send_byte(((value / 100) % 10) + '0');
  case 2:
    uart_send_byte(((value / 10) % 10) + '0');
  case 1:
    uart_send_byte((value % 10) + '0');
  }
}

static uint16_t buttons = 0x0000;

int main (void)
{
	_delay_ms(1000);

	wdt_enable(WDTO_2S);

	PORTB |= _BV(PB3);
	PORTB &= ~(_BV(PB2) | _BV(PB4));
	DDRB &= ~(_BV(PB2) | _BV(PB3) | _BV(PB4));
	DIDR0 = _BV(ADC2D) | _BV(ADC1D);
	ADCSRB = 0;
	ADMUX = _BV(MUX0);
	ADCSRA = _BV(ADEN) | _BV(ADPS2) | _BV(ADPS1);
#ifndef NO_NOISE_REDUCTION
	ADCSRA |= _BV(ADIE);
#endif

	uart_tx_init (); // инициализация прог. UARTа
	wdt_reset();
	sei();

	uint16_t cnt = 0, filt1 = 0, filt2 = 0;

	while (1) {
		//Отправлять каждые 300 измерений
		if (cnt++>300) {
			cnt = 0;
			buttons &= ~0x000E;
			if (filt1<100) {
				buttons |= 0x2000;
			} else {
				buttons &= ~0x2000;
				if (filt1<410) {
					buttons |= 0x0002;
				} else
				if (filt1<614) {
					buttons |= 0x0004;
				} else
				if (filt1<819) {
					buttons |= 0x0008;
				}
			}

			buttons &= ~0x01F0;
			if ((filt2<100) || (filt2>1000)) {
				buttons |= 0x1000;
			} else {
				buttons &= ~0x1000;
				if (filt2<245) {
					buttons |= 0x0010;
				} else
				if (filt2<430) {
					buttons |= 0x0020;
				} else
				if (filt2<695) {
					buttons |= 0x0040;
				} else
				if (filt2<818) {
					buttons |= 0x0080;
				} else
				if (filt2<920) {
					buttons |= 0x0100;
				}
			}

//			uart_print("F1: ");
//			num_to_str(filt1, 4);
//			uart_print(" F2: ");
//			num_to_str(filt2, 4);
//			uart_print(" B: ");
//			uint8_t i;
//			for (i=0; i<16; i++) {
//				uart_print((buttons & _BV(15-i))?"1":"0");
//			}
//			uart_print("\r\n");

			uint8_t crc = MMC_CalcCRC7(0x00, 0x80);
			uart_send_byte(0x80);
			crc = MMC_CalcCRC7(crc, (buttons>>7) & 0x7F);
			uart_send_byte((buttons>>7) & 0x7F);
			crc = MMC_CalcCRC7(crc, buttons & 0x7F);
			uart_send_byte(buttons & 0x7F);
			uart_send_byte(crc>>1);

			wdt_reset();
		}
		//Кнопка включения круиза
		if (PINB & _BV(PB3))
			buttons &= ~0x01;
		else
			buttons |= 0x01;

#ifndef NO_NOISE_REDUCTION
		set_sleep_mode(SLEEP_MODE_ADC);
		sleep_enable();
#else
		ADCSRA |= _BV(ADIF);
#endif

		ADCSRA &= ~_BV(ADEN);
		ADMUX = _BV(MUX0);
		ADCSRA |= _BV(ADEN);
#ifndef NO_NOISE_REDUCTION
		// DS Page 31: If the ADC is enabled, a conversion starts automatically when this mode is entered
		sleep_cpu();
#else
		ADCSRA |= _BV(ADSC);
		do {} while (!(ADCSRA & _BV(ADIF)));
#endif
		filt1 = filter(filt1,ADCW);

#ifdef NO_NOISE_REDUCTION
		ADCSRA |= _BV(ADIF);
#endif
		ADCSRA &= ~_BV(ADEN);
		ADMUX = _BV(MUX1);
		ADCSRA |= _BV(ADEN);
#ifndef NO_NOISE_REDUCTION
		sleep_cpu();
#else
		ADCSRA |= _BV(ADSC);
		do {} while (!(ADCSRA & _BV(ADIF)));
#endif
		filt2 = filter(filt2,ADCW);
	}
}

#ifndef NO_NOISE_REDUCTION
ISR(ADC_vect)
{
	//Reset Interrupt flag
	ADCSRA |= _BV(ADIF);
}
#endif
