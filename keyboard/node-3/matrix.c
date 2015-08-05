/* Copyright 2012 Jun Wako <wakojun@gmail.com>
 *
 * This is heavily based on phantom/board.{c|h}.
 * https://github.com/BathroomEpiphanies/AVR-Keyboard
 *
 * Copyright (c) 2012 Fredrik Atmer, Bathroom Epiphanies Inc
 * http://bathroomepiphanies.com
 *
 * As for liscensing consult with the original files or its author.
 */
#include <stdint.h>
#include <stdbool.h>
#include <avr/io.h>
#include <util/delay.h>
#include "print.h"
#include "debug.h"
#include "util.h"
#include "matrix.h"


#ifndef DEBOUNCE
#   define DEBOUNCE	0
#endif
static uint8_t debouncing = DEBOUNCE;

// bit array of key state(1:on, 0:off)
static matrix_row_t matrix[MATRIX_ROWS];
static matrix_row_t matrix_debouncing[MATRIX_ROWS];

static uint8_t read_rows(void);
static void init_rows(void);
static void unselect_cols(void);
static void select_col(uint8_t col);

inline
uint8_t matrix_rows(void)
{
    return MATRIX_ROWS;
}

inline
uint8_t matrix_cols(void)
{
    return MATRIX_COLS;
}

void matrix_init(void)
{
    // To use PORTF disable JTAG with writing JTD bit twice within four cycles.
    MCUCR |= (1<<JTD);
    MCUCR |= (1<<JTD);
	
    // initialize row and col
    unselect_cols();
    init_rows();

    // initialize matrix state: all keys off
    for (uint8_t i = 0; i < MATRIX_ROWS; i++)  {
        matrix[i] = 0;
        matrix_debouncing[i] = 0;
    }
}

uint8_t matrix_scan(void)
{
    for (uint8_t col = 0; col < MATRIX_COLS; col++) {  // 0-16
        select_col(col);
        _delay_us(3);       // without this wait it won't read stable value.
        uint8_t rows = read_rows();
        for (uint8_t row = 0; row < MATRIX_ROWS; row++) {  // 0-5
            bool prev_bit = matrix_debouncing[row] & ((matrix_row_t)1<<col);
            bool curr_bit = rows & (1<<row);
            if (prev_bit != curr_bit) {
                matrix_debouncing[row] ^= ((matrix_row_t)1<<col);
                if (debouncing) {
                    dprint("bounce!: "); dprintf("%02X", debouncing); dprintln();
                }
                debouncing = DEBOUNCE;
            }
        }
        unselect_cols();
    }

    if (debouncing) {
        if (--debouncing) {
            _delay_ms(1);
        } else {
            for (uint8_t i = 0; i < MATRIX_ROWS; i++) {
                matrix[i] = matrix_debouncing[i];
            }
        }
    }

    return 1;
}

bool matrix_is_modified(void)
{
    if (debouncing) return false;
    return true;
}

inline
bool matrix_is_on(uint8_t row, uint8_t col)
{
    return (matrix[row] & ((matrix_row_t)1<<col));
}

inline
matrix_row_t matrix_get_row(uint8_t row)
{
    return matrix[row];
}

void matrix_print(void)
{
    print("\nr/c 0123456789ABCDEF\n");
    for (uint8_t row = 0; row < MATRIX_ROWS; row++) {
        xprintf("%02X: %032lb\n", row, bitrev32(matrix_get_row(row)));
    }
}

uint8_t matrix_key_count(void)
{
    uint8_t count = 0;
    for (uint8_t i = 0; i < MATRIX_ROWS; i++) {
        count += bitpop32(matrix[i]);
    }
    return count;
}

 /* Row pin configuration
 * row: 0   1   2   3   4   5
 * pin: B7  D1  D0  C7  B1  B0
 */
static void init_rows(void)
{
    // Input with pull-up(DDR:0, PORT:1)
    DDRB &= ~0b10000011;
    PORTB |= 0b10000011;

    DDRD &= ~0b00000011;
    PORTD |= 0b00000011;

    DDRC &= ~0b10000000;
    PORTC |= 0b10000000;

}

static uint8_t read_rows(void)
{
    return (PINB&(1<<7) ? 0 : (1<<0)) |
           (PIND&(1<<1) ? 0 : (1<<1)) |
           (PIND&(1<<0) ? 0 : (1<<2)) |
           (PINC&(1<<7) ? 0 : (1<<3)) |
           (PINB&(1<<1) ? 0 : (1<<4)) |
           (PINB&(1<<0) ? 0 : (1<<5));
}

/* Column pin configuration
 * col: 0   1   2   3   4   5   6   7   8   9   10  11  12  13  14  15  16
 * pin: F5  F6  F7  B6  B5  B4  D7  D4  C6  D3  D2  B3  B2  D5  F0  F1  F4
 */
static void unselect_cols(void)
{
    // Hi-Z(DDR:0, PORT:0) to unselect
    DDRB  &= ~0b01111100; // PB: 6 5 4 3 2
    PORTB &= ~0b01111100;

    DDRC  &= ~0b01000000; // PC: 6
    PORTC &= ~0b01000000;

    DDRD  &= ~0b10111100; // PD: 7 5 4 3 2
    PORTD &= ~0b10111100;

    DDRF  &= ~0b11110011; // PF: 7 6 5 4 1 0
    PORTF &= ~0b11110011;
}

static void select_col(uint8_t col)
{
    // Output low(DDR:1, PORT:0) to select
    switch (col) {
        case 0:
            DDRF |=  (1<<5);
            PORTF &= ~(1<<5);
            break;
        case 1:
            DDRF  |=  (1<<6);
            PORTF &= ~(1<<6);
            break;
        case 2:
            DDRF  |=  (1<<7);
            PORTF &= ~(1<<7);
            break;
        case 3:
            DDRB  |=  (1<<6);
            PORTB &= ~(1<<6);
            break;
        case 4:
            DDRB  |=  (1<<5);
            PORTB &= ~(1<<5);
            break;
        case 5:
            DDRB  |=  (1<<4);
            PORTB &= ~(1<<4);
            break;
        case 6:
            DDRD  |=  (1<<7);
            PORTD &= ~(1<<7);
            break;
        case 7:
            DDRD  |=  (1<<4);
            PORTD &= ~(1<<4);
            break;
        case 8:
            DDRC  |=  (1<<6);
            PORTC &= ~(1<<6);
            break;
        case 9:
            DDRD  |=  (1<<3);
            PORTD &= ~(1<<3);
            break;
        case 10:
            DDRD  |=  (1<<2);
            PORTD &= ~(1<<2);
            break;
        case 11:
            DDRB  |=  (1<<3);
            PORTB &= ~(1<<3);
            break;
        case 12:
            DDRB  |=  (1<<2);
            PORTB &= ~(1<<2);
            break;
        case 13:
            DDRD  |=  (1<<5);
            PORTD &= ~(1<<5);
            break;
        case 14:
            DDRF  |=  (1<<0);
            PORTF &= ~(1<<0);
            break;
        case 15:
            DDRF  |=  (1<<1);
            PORTF &= ~(1<<1);
            break;
        case 16:
            DDRF  |=  (1<<4);
            PORTF &= ~(1<<4);
            break;
    }
}
