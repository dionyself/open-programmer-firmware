/*
 * Firmware framework for USB I/O on PIC 18F2455 (and siblings)
 * Copyright (C) 2005 Alexander Enzmann
 * adapted to MCC18 by Alberto Maccioni on 1/6/09
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111 USA
 * or see <http://www.gnu.org/licenses/>
 */

#include <p18cxxx.h>
#include <stdio.h>
#include <usart.h>
#include "usb.h"
#include "OPcontrol.h"		// User code

// Define configuration registers (fuses)
// CONFIG1L
#pragma config PLLDIV = 3       // PLL Prescaler Selection bits (Divide by 3 (12 MHz oscillator input))
#pragma config CPUDIV = OSC1_PLL2// System Clock Postscaler Selection bits ([Primary Oscillator Src: /1][96 MHz PLL Src: /2])
#pragma config USBDIV = 2       // USB Clock Selection bit (used in Full-Speed USB mode only; UCFG:FSEN = 1) (USB clock source comes from the 96 MHz PLL divided by 2)
// CONFIG1H
#pragma config FOSC = HSPLL_HS  // Oscillator Selection bits (HS oscillator, PLL enabled (HSPLL))
#pragma config FCMEN = OFF      // Fail-Safe Clock Monitor Enable bit (Fail-Safe Clock Monitor disabled)
#pragma config IESO = OFF       // Internal/External Oscillator Switchover bit (Oscillator Switchover mode disabled)
// CONFIG2L
#pragma config PWRT = OFF       // Power-up Timer Enable bit (PWRT disabled)
#pragma config BOR = ON_ACTIVE  // Brown-out Reset Enable bits (Brown-out Reset enabled in hardware only and disabled in Sleep mode (SBOREN is disabled))
//#pragma config BORV = 3         // Brown-out Reset Voltage bits (Minimum setting)
#pragma config VREGEN = ON      // USB Voltage Regulator Enable bit (USB voltage regulator enabled)
// CONFIG2H
#pragma config WDT = OFF        // Watchdog Timer Enable bit (WDT disabled (control is placed on the SWDTEN bit))
#pragma config WDTPS = 32768    // Watchdog Timer Postscale Select bits (1:32768)
// CONFIG3H
//#pragma config CCP2MX = ON      // CCP2 MUX bit (CCP2 input/output is multiplexed with RC1)
#pragma config PBADEN = OFF     // PORTB A/D Enable bit (PORTB<4:0> pins are configured as digital I/O on Reset)
#pragma config LPT1OSC = OFF    // Low-Power Timer 1 Oscillator Enable bit (Timer1 configured for higher power operation)
#pragma config MCLRE = OFF      // MCLR Pin Enable bit (RE3 input pin enabled; MCLR pin disabled)
// CONFIG4L
#pragma config STVREN = ON      // Stack Full/Underflow Reset Enable bit (Stack full/underflow will cause Reset)
#pragma config LVP = OFF        // Single-Supply ICSP Enable bit (Single-Supply ICSP disabled)
#pragma config XINST = OFF      // Extended Instruction Set Enable bit (Instruction set extension and Indexed Addressing mode disabled (Legacy mode))
#pragma config   DEBUG=OFF
// CONFIG5L (default)
#pragma config CP0 = OFF        // Code Protection bit (Block 0 (000800-001FFFh) is not code-protected)
#pragma config CP1 = OFF        // Code Protection bit (Block 1 (002000-003FFFh) is not code-protected)
#pragma config CP2 = OFF        // Code Protection bit (Block 2 (004000-005FFFh) is not code-protected)
#pragma config CP3 = OFF        // Code Protection bit (Block 3 (006000-007FFFh) is not code-protected)
// CONFIG5H (default)
#pragma config CPB = OFF        // Boot Block Code Protection bit (Boot block (000000-0007FFh) is not code-protected)
#pragma config CPD = OFF        // Data EEPROM Code Protection bit (Data EEPROM is not code-protected)
// CONFIG6L (default)
#pragma config WRT0 = OFF       // Write Protection bit (Block 0 (000800-001FFFh) is not write-protected)
#pragma config WRT1 = OFF       // Write Protection bit (Block 1 (002000-003FFFh) is not write-protected)
#pragma config WRT2 = OFF       // Write Protection bit (Block 2 (004000-005FFFh) is not write-protected)
#pragma config WRT3 = OFF       // Write Protection bit (Block 3 (006000-007FFFh) is not write-protected)
// CONFIG6H (default)
#pragma config WRTC = OFF       // Configuration Register Write Protection bit (Configuration registers (300000-3000FFh) are not write-protected)
#pragma config WRTB = OFF       // Boot Block Write Protection bit (Boot block (000000-0007FFh) is not write-protected)
#pragma config WRTD = OFF       // Data EEPROM Write Protection bit (Data EEPROM is not write-protected)
// CONFIG7L (default)
#pragma config EBTR0 = OFF      // Table Read Protection bit (Block 0 (000800-001FFFh) is not protected from table reads executed in other blocks)
#pragma config EBTR1 = OFF      // Table Read Protection bit (Block 1 (002000-003FFFh) is not protected from table reads executed in other blocks)
#pragma config EBTR2 = OFF      // Table Read Protection bit (Block 2 (004000-005FFFh) is not protected from table reads executed in other blocks)
#pragma config EBTR3 = OFF      // Table Read Protection bit (Block 3 (006000-007FFFh) is not protected from table reads executed in other blocks)
// CONFIG7H (default)
#pragma config EBTRB = OFF      // Boot Block Table Read Protection bit (Boot block (000000-0007FFh) is not protected from table reads executed in other blocks)

#if !defined(__18F2450)
#pragma config CCP2MX=OFF, BORV=3
#else
#pragma config BORV=21
#endif
#pragma code low_vector=0x8
void low_interrupt (void)
{
	_asm GOTO timer_isr _endasm
}


#pragma code

// Allocate buffers in RAM for storage of bytes that have either just
// come in from the SIE or are waiting to go out to the SIE.
//char txBuffer[HID_INPUT_REPORT_BYTES];
//char rxBuffer[HID_OUTPUT_REPORT_BYTES];

#if DEBUG_PRINT
static void InitializeUSART()
{
    TRISC &= 0xBF; // Set RC6 as an output
    TRISC |= 0x80; // Set RC7 as an input
    RCSTA   = 0x90; // Enable serial port, enable receiver
    TXSTA   = 0x24; // Asynch, TSR empty, BRGH=1

    // Baud rate formula for BRG16=1, BRGH=1: Baud Rate = Fosc/(4 (n + 1)),
    // or n = (Fosc / (4 * BaudRate)) - 1
    // At 48 MHz, for 115.2K Baud:
    //     SPBRGH:SPBRG = n = Fosc / (4 * 115200) - 1 = 103.17
    BAUDCON = 0x18; // BRG16=1 txd inverted
//    SPBRGH  = 0x00; // At 48MHz, SPBRGH=0, SPBRG=103 gives 115.2K Baud
    SPBRGH  = 0x02; // 0x0270 gives 19200 Baud
#if CLK_48MHZ
//    SPBRG   = 103;  // For 48 MHz clock
    SPBRG   = 0x70;  // For 48 MHz clock
#else
    SPBRG   = 52;   // For 24 MHz clock
#endif
    printf("USB Test Startup\r\n");
}
#endif


// Entry point of the firmware
void main(void)
{
	// Set all I/O pins to digital
    ADCON1 |= 0x0F;
    
	// Initialize USB
    UCFG = 0x14; // Enable pullup resistors; full speed mode

    deviceState = DETACHED;
    remoteWakeup = 0x00;
    currentConfiguration = 0x00;

	// Call user initialization function
	UserInit();
		
	while(1)
    {
        // Ensure USB module is available
		EnableUSBModule();
		
		// As long as we aren't in test mode (UTEYE), process
		// USB transactions.
		if(UCFGbits.UTEYE != 1)
			ProcessUSBTransactions();

        // Application specific tasks
        ProcessIO();
        
    }
}
