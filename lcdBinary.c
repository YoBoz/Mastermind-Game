/* ***************************************************************************** */
/* You can use this file to define the low-level hardware control fcts for       */
/* LED, button and LCD devices.                                                  */
/* Note that these need to be implemented in Assembler.                          */
/* You can use inline Assembler code, or use a stand-alone Assembler file.       */
/* Alternatively, you can implement all fcts directly in master-mind.c,          */
/* using inline Assembler code there.                                            */
/* The Makefile assumes you define the functions here.                           */
/* ***************************************************************************** */

#ifndef TRUE
#define TRUE (1 == 1)
#define FALSE (1 == 2)
#endif

#define PAGE_SIZE (4 * 1024)
#define BLOCK_SIZE (4 * 1024)

#define INPUT 0
#define OUTPUT 1

#define LOW 0
#define HIGH 1

// APP constants   ---------------------------------

// Wiring (see call to lcdInit in main, using BCM numbering)
// NB: this needs to match the wiring as defined in master-mind.c

#define STRB_PIN 24
#define RS_PIN 25
#define DATA0_PIN 23
#define DATA1_PIN 10
#define DATA2_PIN 27
#define DATA3_PIN 22

// -----------------------------------------------------------------------------
// includes
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/types.h>
#include <time.h>

// -----------------------------------------------------------------------------
// prototypes

int failure(int fatal, const char *message, ...);

// -----------------------------------------------------------------------------
// Functions to implement here (or directly in master-mind.c)

/* this version needs gpio as argument, because it is in a separate file */
void digitalWrite(uint32_t *gpio, int pin, int value)
{
    asm volatile(
        // Comparing value with 0 (LOW)
        "cmp %[value], #0\n"
        // If value is low then add 0x28 (10 * 4) to gpio
        "addeq %[gpio], #0x28\n"
        // If vaalue is high then add 0x1C (7 * 4) to gpio
        "addne %[gpio], #0x1C\n"
        // Moving 1 into register r3
        "mov r3, #1\n"
        // Logical shift left r3 by pin bits
        "lsl r3, r3, %[pin]\n"
        // Storing r3 to memory location gpio
        "str r3, [%[gpio]]\n"
        :
        //Input Operands
        : [gpio] "r"(gpio), [pin] "r"(pin), [value] "r"(value)
        // Clobbered registers
        : "r3", "cc");
}

void pinMode(uint32_t *gpio, int pin, int mode)
{
    int result;
    // Determining the register for the pin
    int fSelReg = pin / 10;
    // calculating the shift value
    int shiftVal = (pin % 10) * 3;
    asm(
        // Loading the gpio value into r1 register
        "\tldr r1, %[gpio]\n"
        // Adding the fselreg value to r1 and storing the result in r0
        "\tadd r0, r1, %[fSelRegVal]\n"
        // Loading the value from r0 to r1
        "\tldr r1, [r0, #0]\n"
        // Loading 7(0b111) into r2
        "\tmov r2, #0b111\n"
        // Shifting r2 by shiftval number of times to the left
        "\tlsl r2, %[shiftValue]\n"
        // Performing bitwise clear with r1 and r2
        "\tbic r1, r1, r2\n"
        // Loading the mode into r2
        "\tmov r2, %[modeVal]\n"
        // Shiftvalue number of times to the left
        "\tlsl r2, %[shiftValue]\n"
        // Performing bitwise OR with r1 and r2
        "\torr r1, r2\n"
        // Storing the value from r1 to r0
        "\tstr r1, [r0, #0]\n"
        // Storing r1 into the output variable result
        "\tmov %[output], r1\n"
        // Output operands
        : [output] "=r"(result)
        // Input operands
        : [gpio] "m"(gpio), [fSelRegVal] "r"(fSelReg * 4), [shiftValue] "r"(shiftVal), [modeVal] "r"(mode)
        // Clobbered registers
        : "r0", "r1", "r2", "cc");
}

void writeLED(uint32_t *gpio, int led, int value)
{
    int onOff = (value == HIGH) ? 7 : 10;

    asm volatile(
        // Moving value 1 in r2
        "\tmov r2, #1\n"
        // AND the given led value with 31 and store the result in r1
        "\tand r1, %[led], #31\n"
        // Left shifting value of r2 by r1 and storing result in r2
        "\tlsl r2, r2, r1\n"
        // Storing the value in r2 to (gpio + onOff * 4)
        "\tstr r2, [%[gpio], %[onOff], lsl #2]\n"
        :
        // Input operands
        : [led] "r"(led), [gpio] "r"(gpio), [onOff] "r"(onOff)
        // Clobbered registers
        : "r1", "r2", "cc");
}

int readButton(uint32_t *gpio, int button)
{
    // Varible to store pin status
    int pinStatus = LOW;
    // Offset value
    int gplev_offset = 52;
    asm volatile(
        // Loading the value at gpio_base + gplev_offset into r0
        "ldr r0, [%[gpio], %[offset]]\n"
        // Moving the value of buttonPin into r1
        "mov r1, %[pin]\n"
        // Masking the lower 5 bits of r1
        "and r1, #0x1F\n"
        // Loading 1 into r2
        "mov r2, #1\n"
        // Left shifting the value in r2 by r1
        "lsl r2, r1\n"
        // Testing if the corresponding bit is set in r0 (and with r2)
        "tst r0, r2\n"
        // Moveing the result of the test into pinStatus
        "moveq %[status], #0\n"
        "movne %[status], #1\n"
        // Output operands
        : [status] "=r"(pinStatus)
        // Input operands
        : [pin] "r"(button), [gpio] "r"(gpio), [offset] "r"(gplev_offset)
        // Clobbered registers
        : "r0", "r1", "r2", "cc");

    // Returning pinStatus
    return pinStatus;
}

void waitForButton(uint32_t *gpio, int button)
{
    // Initializing variable to LOW
    int buttonStatus = LOW;
    // Button being pressed and updating the value
    while (buttonStatus == LOW) {
        buttonStatus = readButton(gpio, button);
    }
    // Button being released and updating the value
    while (buttonStatus == HIGH) {
        buttonStatus = readButton(gpio, button);
    }
}
