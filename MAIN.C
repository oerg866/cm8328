/*
 * C-Media CMI8328 DOS Init Driver
 * Main File
 *
 * (C) 2023 Eric Voirin (oerg866@googlemail.com)
 */

#include <stdio.h>

#include "CM8328.H"

int main(int argc, char *argv[])
{
    int i;
    bool ok = true;

    if (!cm8328_prepare()) {
        printf("Error during preparation! Quitting...");
        return -1;
    }

    for (i = 1; (i < argc) && ok; ++i) {
        ok = cm8328_parseArg(argv[i]);
    }

    if (!ok) {
        printf("Command line parsing failed. Use '/?' for help.");
        return 1;
    }

    ok = cm8328_configureCard();

    if (!ok) return 1;

    return 0;

}
