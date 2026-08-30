#ifndef DISPLAY_H
#define DISPLAY_H

#include <stdint.h>
#include <stdbool.h>

void display_init(void);
void display_power(bool turn_on);
void display_clear(void);
void display_show_text(const char *text);

#endif