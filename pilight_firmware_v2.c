/* attiny45 - 433 MHz prefilter - V 2.0 - written by mercuri0 & CurlyMo */
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/power.h>
#include <avr/wdt.h>

#define bool  					_Bool
#define true  					1
#define false  					0

#define CAT(a, ...) 			PRIMITIVE_CAT(a, __VA_ARGS__)
#define PRIMITIVE_CAT(a, ...) 	a ## __VA_ARGS__

#define SET(a,b)				a |=  _BV(b)
#define CLEAR(a,b)				a &= ~_BV(b)
#define HIGH(a,b)				SET(a,b)
#define LOW(a,b)				CLEAR(a,b)
#define GET(a,b)				a & _BV(b)
#define TOGGLE(a,b)				a ^= _BV(b)

#define SET_OUTPUT(a,b)			a |= _BV(b)
#define SET_INPUT(a,b)			a &= ~_BV(b)

#define D_PORT 					DDRB
#define D_PIN(a) 				PRIMITIVE_CAT(DDB,a)

#define V_PORT 					PORTB
#define V_PIN(a) 				PRIMITIVE_CAT(PORTB,a)

#define REC_OUT 				4
#define PI_IN 					3

#define MIN_PULSELENGTH 		8			//tested to work down to 30us pulsewidth (=2)
#define MAX_PULSELENGTH 		1600
#define PLSLEN 					160
#define REPEATS					2
#define VERSION					2

volatile uint16_t 				ten_us_counter = 0;
volatile unsigned long 			ten_us_counter1 = 0;
volatile uint8_t 				valid_buffer = 0x00;

volatile uint16_t 				version = VERSION;
volatile uint16_t 				bit = 0;
volatile uint8_t 				state = 0;
volatile uint8_t 				lsb = 0;
volatile uint8_t 				nrrepeat = 0;

volatile uint16_t 				_version = 0;
volatile uint16_t 				_minplslen = MIN_PULSELENGTH;
volatile uint16_t 				_maxplslen = MAX_PULSELENGTH;

void get_mcusr(void) __attribute__((naked)) __attribute__((section(".init3")));
void get_mcusr(void) {
  MCUSR = 0;
  wdt_disable();
}

void init_system(void){
	cli();

	SET(TCCR1, CS12);
	SET(TCCR1, CTC1);
	OCR1A = OCR1C = 0x14;
	SET(TIMSK, OCIE1A);

	SET(PCMSK, PCINT4);
	SET(MCUCR, ISC00);
	SET(GIMSK, PCIE);

	SET_OUTPUT(D_PORT, D_PIN(PI_IN));
	CLEAR(V_PORT, V_PIN(PI_IN));

	sei();

	power_adc_disable();
	power_usi_disable();
	power_timer0_disable();

	wdt_enable(WDTO_4S);
}

int main (void)
{
	init_system();

	while(1) {
		asm volatile("NOP");
	}
}

void send() {
	TOGGLE(V_PORT, V_PIN(PI_IN));
	ten_us_counter1 = 0;
	state ^= 1;
}

void send1(int len) {
	if(ten_us_counter1 > len) {
		send();
		bit++;
	}
}

void send2(int len) {
	if(ten_us_counter1 > len) {
		send();
		lsb++;
	}
}

void shift1() {
	lsb = 0;
	bit++;
	_version >>= 1;
}

void shift2() {
	lsb = 0;
	bit++;
	_minplslen >>= 1;
}

void shift3() {
	lsb = 0;
	bit++;
	_maxplslen >>= 1;
}

void reset() {
	_version = version;
	_minplslen = MIN_PULSELENGTH;
	_maxplslen = MAX_PULSELENGTH;
	bit = 0;
	ten_us_counter1 = 0;
	lsb = 0;
}

void sendHigh() {
	if(lsb == 1) {
		send2((PLSLEN*3)/10);
	} else {
		send2(PLSLEN/10);
	}
}

void sendLow() {
	if(lsb == 3) {
		send2((PLSLEN*3)/10);
	} else {
		send2(PLSLEN/10);
	}
}

ISR(TIMER1_COMPA_vect){
	wdt_reset();
	ten_us_counter++;
	ten_us_counter1++;
	if(nrrepeat >= REPEATS && ten_us_counter1 >= 6000000) {
		reset();
		if(state == 1) {
			send();
		}
		nrrepeat = 0;
	}
	if(nrrepeat < REPEATS) {
		cli();
		if(bit < 52) {
			if(bit < 2) {
				if(state == 0) {
					send1(PLSLEN/10);
				} else {
					send1((PLSLEN*6)/10);
				}
			} else if(bit < 18) {
				if((_version&0x0000000000000001) == 1) {
					sendHigh();
				} else {
					sendLow();
				}
				if(lsb == 4) {
					shift1();
				}
			} else if(bit < 34) {
				if((_minplslen&0x0000000000000001) == 1) {
					sendHigh();
				} else {
					sendLow();
				}
				if(lsb == 4) {
					shift2();
				}
			} else if(bit < 50) {
				if((_maxplslen&0x0000000000000001) == 1) {
					sendHigh();
				} else {
					sendLow();
				}
				if(lsb == 4) {
					shift3();
				}
			} else {
				if(state == 0) {
					send1(PLSLEN/10);
				} else {
					send1((PLSLEN*34)/10);
				}
			}
		} else {
			nrrepeat++;
			reset();
			if(state == 1) {
				send();
			}
		}
	} else {
		sei();
	}
}

ISR(PCINT0_vect){
	cli();
	valid_buffer <<= 1;
	if(ten_us_counter > MIN_PULSELENGTH)
	{
		if(ten_us_counter < MAX_PULSELENGTH)
		{
			valid_buffer |= 0x01;
			if (valid_buffer == 0xFF)
			{
				state ^= 1;
				TOGGLE(V_PORT, V_PIN(PI_IN));
			}
		}
	}
	ten_us_counter = 0;
	TCNT1 = 0;
	sei();
}