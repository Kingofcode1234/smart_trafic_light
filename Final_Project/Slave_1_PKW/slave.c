#include "traffic_light/light_ws2812.h"
#include "ultrasonicsensor/ultrasonicsensor.h"
#define F_CPU 16000000 
#include<avr/io.h>
#include<util/delay.h>
#include <avr/interrupt.h>
#include <string.h>

#define PORT_DIRECTION DDRB
#define PORT_VALUE PORTB

// Pin definitions (on Port B)
#define SCK  5
#define MISO 4
#define MOSI 3
#define SS   2

void SPI_SlaveInit(void) {
    // Set MISO output, all others input
    PORT_DIRECTION |= (1 << MISO);
    // Enable SPI
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
    for (int i = 0; strcmp(string[i], '\0') != 0; i++) {
        uart_transmit(string[i]);
    }
}
void SPI_SlaveReceive(char toMaster) {
	SPDR = toMaster;
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

volatile char s = 0;
uint8_t sending = 0;
ISR(SPI_STC_vect){
	sending = 0;
	s = SPDR;
}

int main() {
	uint8_t status='0';// Status 0 no one is infront of the Traffic Light
    uart_init(115200);
    uart_transmit_string("I bims der Slave1_traffic\n\r");
	uint16_t distance=6;
	sei();
	
    SPI_SlaveInit();
	char c = 'X';
    while (1) {
		if(!sending) {
			sending = 1;
			SPI_SlaveReceive(status); // Send
		}
		
		//When the Master send somthing transmitt it on uart
		if(s!=0){
			uart_sendstring("reveived");
			uart_transmit_slave(c);
			uart_transmit_string("\n\r");
			c=s;
		}
        
        //logik for traffic lights - the intepretation of the cmds of the master
        if(c=='1'){// blink green
            BlinkGreenTL();
			status= '0';        
        }
        if(c=='2'){// Switch to green
            SwitchGreenTL();
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
        if(c=='3'){ // Switch to yellow
            SwitchYellowTL();
        }
        if(c=='4'){ // switch to red
		SwitchRedTL();
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
        if(c=='5'){ // ckeck if someone is near the trfficlight
          
        }
        if(c=='7'){ //Nightmode
            BlinkYellowTL();
            status=0;
      }
    }
}

