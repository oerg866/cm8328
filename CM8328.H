/*
 * C-Media CMI8328 DOS Init Driver
 * (C) 2023 Eric Voirin (oerg866@googlemail.com)
 *
 */

#ifndef CM8328_H

#define CM8328_H

#include "TYPES.H"

/* Find card and prepare default configuration */
bool cm8328_prepare ();

/* Parse a configuration argument. Forwarded to ARGS */
bool cm8328_parseArg (const char *arg);

/* configure card with previously set configuration */
bool cm8328_configureCard ();

#endif /* CM8328_H */
