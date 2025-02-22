#include "traffic_light/light_ws2812.h"

#define F_CPU 16000000

#include <avr/io.h>        // SFR definitions, bit positions etc.
#include <util/delay.h>   // include delay functions 

#include "ultrasonicsensor/ultrasonicsensor.h"
#include <avr/interrupt.h>
#include <string.h>


#define PORT_DIRECTION DDRB
#define PORT_VALUE PORTB

// Pin definitions (on Port B)
#define SCK  5
#define MISO 4
#define MOSI 3
#define SS   2

volatile uint8_t frequenz=0;

void SPI_SlaveInit(void) {
    // Set MISO output, all others input
    PORT_DIRECTION |= (1 << MISO);
    // Enable SPI
   // SPCR |= (1 << SPE) | (1 << CPOL) | (1 << CPHA);
    SPCR |= (1 << SPE) | (1<< SPIE);
}

void uart_transmit_slave(char character) {
    while (!(UCSR0A & (1 << 5)));
    UDR0 = (uint8_t) character;
}

char uart_receive_slave() {
    while (!(UCSR0A & (1 << RXC0)));
    return (char) UDR0;
}

void uart_transmit_string(char *string) {
    for (int i = 0; string[i] != 0; i++) {
        uart_transmit(string[i]);
    }
}


//non blocking
char SPI_SlaveReceive(char toMaster) {
	 SPDR = toMaster;
    // Wait for reception complete
    // SPI Status Reg & 1<<SPI Interrupt Flag
    if (!(SPSR & (1 << SPIF))){
    // Return data register
		return 0;	
	}else{
		return SPDR;	
	}	
}


void uart_init(uint32_t baudrate) {
    // UBRR formula from datasheet
    // 8 as we use doublespeed
    uint16_t ubrr = (F_CPU / 8 / baudrate) - 1;
    // Set baud rate, use double speed
    UBRR0H = (uint8_t) (ubrr >> 8); // get higher 8 bits
    UBRR0L = (uint8_t) (ubrr & 0xff); // get lower 8 bits
    UCSR0A |= (1 << U2X0);
    // Enable receiver and transmitter
    UCSR0B = (1 << TXEN0 | 1 << RXEN0);
    // select character size, stop and parity bits: 8N1
    // UCSR0C = (1<<UCSZ01 | 1<<UCSZ00);
    // not necessary, because UCSR0C is initialised corretly !
    // for interrupts: enable receive complete interrupt
    // UCSR0B |= (1<<RXCIE0); we do not use interrupts by now..
}
ISR(TIMER0_OVF_vect) //timer 0 overflow interrupt service routine
{
	static uint16_t count1=0,count2=0; //static: global lifetime, local visibility		
	TCNT0 =0;				//25 ticks bis zum overflow
	if(count2<=1000){
		if(frequenz>count1) PORTD |= (1<<4); //Pin high setzen
		else PORTD &= ~(1<<4); 				//Pin low setzen
		count1++;
		if(count1==10) count1=0;	// counter zurücksetzten
	}
	count2++;
	if(count2>=2000){
		count2=0 ;
	}
}

int main() {
    uart_init(115200);
    DDRD = (1<<4);  		//PD4 is Buzzer
    DDRB |= (1<<PB4);  	 	// Set MISO output //SPI_SlaveInit();
    PORTD |= (1<<PD2);
	SPCR |= (1<<SPE);       // enable SPI in slave mode
	TCCR0B =(1<<CS00)|(1<<CS01); 	//b prescaler 64 -> 4µs tick time, 250 ticks == 1ms
	TCNT0= 0; 	//25 ticks bis zum overflow
	TIMSK0=1; 	// enable timer 0 overflow interrupts
	sei();    	//	enable interrupts (globally)
    uart_transmit_string("I bims der Slave2_walker\n\r");
    uint8_t button=0;
	char c = 'X';
	char status='0';
	uint16_t distance;
	//char s = 'a';

    while (1) {
		char s=SPI_SlaveReceive(status);
        if(s!=0){
			uart_sendstring("received:");
			uart_transmit_slave(s);
			uart_transmit_string("\n\r");
           c = s;
		}

        //logik for traffic lights - the intepretation of the cmds of the master
     
        if(c=='1'){ //blink green befor switching to red
			BlinkGreenPL();
			frequenz=6;
			status= '0';
		}
        if(c=='4'){// Switch to red pedestrian traffic light
            SwitchRedPL();
            frequenz=0;
			button=0;
       		distance = ultrasonicsensor();
			if(distance <= 5){
				uart_sendstring("dist 1\n\r"); 
				status= '1';
				//c= SPI_SlaveReceive(distance+48); //send a char to master to know that a person is waiting
			}else{
				status= '0';
				uart_sendstring("dist 0\n\r"); 
			}
			
        }
        if(c=='2'){ //Switch to green pedestrian traffic light
			SwitchGreenPL();
			
			if((PIND & (1<<PD2))==0){//Button to turn on the Buzzer 
				button = 1; 
			}
			if(button==1){
				status='1';
				frequenz=7;
				
			}
			else{
				frequenz=10; //no sound
				distance = ultrasonicsensor();
				if(distance <= 5){
					uart_sendstring("dist 1\n\r"); 
					status= '1';
					//c= SPI_SlaveReceive(distance+48); //send a char to master to know that a person is waiting
				}else{
					status= '0';
					uart_sendstring("dist 0\n\r"); 
				}
			}	
        }
       
		if(c=='7'){ // night mode
			NoLights(); // Alle LEDs ausschalten
		} 
    }
}

