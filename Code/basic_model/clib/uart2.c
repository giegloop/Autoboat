
#include "commProtocol.h"
#include "circBuffer.h"
#include "uart2.h"
#include <p33Fxxxx.h>

CircBuffer uart2RxBuffer;

void __attribute__((__interrupt__, no_auto_psv_)) _U2RXInterrupt(void) {

	while (U2STAbits.URXDA == 1) {
		writeBack(&uart2RxBuffer, (unsigned char)U2RXREG);
	}
	
	// Clear buffer overflow bit if triggered
	if (U2STAbits.OERR == 1) {
		U2STAbits.OERR = 0;
	}

	IFS1bits.U2RXIF = 0;
}

/**
 * This function should be called continously. Each timestep
 * it runs through the most recently received data, parsing
 * it for sensor data. Once a complete message has been parsed
 * the data inside will be returned through the message
 * array.
 */
void parseNewRxData(unsigned char* message) {
	while (getLength(&uart2RxBuffer) > 0) {
		buildAndCheckMessage(readFront(&uart2RxBuffer));
	}
}

// -------------------------------------------------------- UART2 Transmission code

// Circular buffer declaration
CircBuffer uart2TxBuffer;

/**
 * This is the interrupt handler for UART2 transmission.
 * It is called after at least one byte is transmitted (
 * depends on UTXISEL<1:0> as to specifics). This function
 * therefore keeps adding bytes to transmit if there're more
 * in the queue.
 */
void __attribute__((__interrupt__, no_auto_psv_)) _U2TXInterrupt(void) {
	IFS1bits.U2TXIF = 0;
	startUart2Transmission();
}

/**
 * This function actually initiates transmission. It
 * attempts to start transmission with the first element
 * in the queue if transmission isn't already proceeding.
 * Once transmission starts the interrupt handler will
 * keep things moving from there. The buffer is checked
 * for new data and the transmission buffer is checked that
 * it has room for new data before attempting to transmit.
 */
void startUart2Transmission() {
	if (getLength(&uart2TxBuffer) > 0 && !U2STAbits.UTXBF) {
		U2TXREG = readFront(&uart2TxBuffer);
	}
}

/**
 * This function transmits a complete actuator struct
 * through UART2 using a circular buffer.
 * It works by just feeding each byte into the circular
 * buffer. Only reason it's specific to the actuator data
 * is because of the length of the array passed.
 */
void uart2EnqueueActuatorData(unsigned char *data) {
	unsigned char g;
	// Add all 22+6 bytes of the actuator struct to the queue.
	for (g = 0; g < 28;g++) {
		writeBack(&uart2TxBuffer,data[g]);
	}
	startUart2Transmission();
}

/**
 * This function transmits a complete state struct
 * through UART2 using a circular buffer.
 * It works by just feeding each byte into the circular
 * buffer. Only reason it's specific to the actuator data
 * is because of the length of the array passed.
 */
void uart2EnqueueStateData(unsigned char *data) {
	unsigned char g;
	// Add all 23+6 bytes of the state struct to the queue.
	for (g = 0; g < 29;g++) {
		writeBack(&uart2TxBuffer,data[g]);
	}
	startUart2Transmission();
}

/**
 * Initialization function for the circular buffer.
 * Should be called in initialization code for the
 * model.
 */
void initUart2() {
	
	// First initialize the necessary circular buffers.
	newCircBuffer(&uart2RxBuffer);
	newCircBuffer(&uart2TxBuffer);
	
	// Configure and open the port;
	// U2MODE Register
	// ==============
	U2MODEbits.UARTEN	= 0;		// Disable the port		
	U2MODEbits.USIDL 	= 0;		// Stop on idle
	U2MODEbits.IREN		= 0;		// No IR decoder
	U2MODEbits.RTSMD	= 0;		// Ready to send mode (irrelevant)
	U2MODEbits.UEN		= 0;		// Only RX and TX
	U2MODEbits.WAKE		= 1;		// Enable at startup
	U2MODEbits.LPBACK	= 0;		// Disable loopback
	U2MODEbits.ABAUD	= 0;		// Disable autobaud
	U2MODEbits.URXINV	= 0;		// Normal operation (high is idle)
	U2MODEbits.PDSEL	= 0;		// No parity 8 bit
	U2MODEbits.STSEL	= 0;		// 1 stop bit
	U2MODEbits.BRGH 	= 0;		// Low speed mode
	
	// U2STA Register
	// ==============
	U2STAbits.URXISEL	= 2;		// RX interrupt when 3 chars are in
	U2STAbits.OERR		= 0;		// clear overun error
	
	// U2BRG Register
	// ==============
	U2BRG = 21;						// Set the baud rate to 115200 (used Lubin's UART config block to calculate this for me)

  	IPC7bits.U2TXIP = 6;    		// Interrupt priority 6  
  	IPC7bits.U2RXIP = 6;    		// Interrupt priority 6 
	IEC1bits.U2TXIE = 1; 			// Enable transmission interrupt
	IEC1bits.U2RXIE = 1; 			// Enable reception interrupt
	
	// Enable the port;
	U2MODEbits.UARTEN	= 1;		// Enable the port	
	U2STAbits.UTXEN		= 1;		// Enable TX
	
}