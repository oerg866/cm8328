/*
 *  Args
 *  Some weird args parser I thought made some sense?
 *  (C) 2023 Eric Voirin (oerg866@googlemail.com)
 *
 */

#ifndef ARGS_H

#define ARGS_H

#include "TYPES.H"

#include <stddef.h>

#define ARG_STR  0
#define ARG_U8   1
#define ARG_U16  2
#define ARG_U32  3
#define ARG_BOOL 4
#define ARG_FLAG 5
#define ARG_NONE 255

/* ARG_MAX is the minimum length a target string buffer for a string argument must have */
#define ARG_MAX  255

typedef u8 arg_type;

typedef bool (*arg_check_fn) (const void *); /* The void* argument is a pointer to the value to check */

typedef struct {
    const char     *prefix;         /* Prefix of the argument's value */
    const char     *description;    /* Description of the argument */
    arg_type        type;           /* Argument type (see defines above) */
    void           *dst;            /* Pointer to where the parsed value will go */
    arg_check_fn    checker;        /* Pointer to function that checks validity */
} args_arg;


/* ARGS_HEADER indicates a header that is displayed at the start of the program.
   This must ALWAYS be the first entry of an args_arg array!

   ARGS_BLANK indicates a blank entry. The usage will display a blank line here.
   This can be used to logically group parameters together.

   ARGS_EXPLAIN indicates that the previous argument requires further
   explanation. It will be printed separately.

   Example:


      ARGS_HEADER("Cool Program with cool arguments, version 1"), 
     ...
      { "asd", "A cool argument", ARG_STR, &sArgument, NULL },
      ARGS_EXPLAIN("This argument is so cool that you can't even imagine it!"),
     ... 

*/

#define ARGS_HEADER(x)  { NULL, x,    ARG_NONE, NULL, NULL }
#define ARGS_BLANK      { NULL, NULL, ARG_NONE, NULL, NULL }
#define ARGS_EXPLAIN(x) { NULL, x,    ARG_NONE, NULL, NULL }

#define ARGS_USAGE "/?"

/* Parse an argument and set the destinations value if found.
   the 'checker' function gets called afterwards. It may check
   the argument for validity. (Can be NULL, too.) */
bool args_parseArg(const args_arg * argList, size_t argsCount, const char *toParse);

#endif /* ARGS_H */
