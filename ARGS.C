/*
 *  Args
 *  Some weird args parser I thought made some sense?
 *  (C) 2023 Eric Voirin (oerg866@googlemail.com)
 *
 */

#include "ARGS.H"


#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>


#include "TYPES.H"

typedef struct {
    u8          type;
    const char *name;
    const char *paramDesc;
} arg_type_name;

static const arg_type_name argTypeNames[] = {
    { ARG_STR,  "Text   ", "Text parameter (max. length 255 characters)" },
    { ARG_U8,   "8  Bit ", "Numeric (between 0 and 255)" },
    { ARG_U16,  "16 Bit ", "Numeric (between 0 and 65535)" },
    { ARG_U32,  "32 Bit ", "Numeric (between 0 and 4294967295)" },
    { ARG_BOOL, "Boolean", "Boolean (0 or 1)" },
    { ARG_FLAG, "Flag   ", "None" },
};

#define ARG_TYPE_NAMES_COUNT (sizeof(argTypeNames) / sizeof(arg_type_name))

static const char * getArgTypeName(u8 argType) {
    size_t idx;

    for (idx = 0; idx < ARG_TYPE_NAMES_COUNT; ++idx) {
        if (argTypeNames[idx].type == argType)
           return argTypeNames[idx].name;
    }

    printf ("ARGS: Oops, there's an unknown argument type!\n");
    abort();
    return NULL;
}


void args_printUsage(const args_arg *argList, size_t argCount) {

    size_t idx;
    char   tmp[256];
    size_t printOffset;

    assert(argList != NULL);
    assert(argCount > 0);

    memset(tmp, ' ', sizeof(tmp));
    tmp[255] = '\0';

    /* This MUST be an ARGS_HEADER entry, so we print it at the start */

    printf("%s\n", argList[0].description);

    for (idx = 0; idx < 70; ++idx) putchar(0xcd);

    printf("\n Valid command line parameters are: \n\n");

    for (idx = 1; idx < argCount; ++idx) {

        if (argList[idx].prefix == NULL && argList[idx].description == NULL) {

            /* current entry is a separator so we need to print just a new line */
            printf("\n");

        } else if (argList[idx].prefix == NULL && argList[idx].description != NULL) {

            /* if current entry is "explanation" */

            printf("%*s -> %s\n",
                25, "",
                argList[idx].description);

        } else { 

            /* current entry is an actual parameter we need to print */

            snprintf(tmp, sizeof(tmp), "/%s", argList[idx].prefix);

            printf("%*s%-*s - %s - %s\n",
                    10, tmp,
                    5, (argList[idx].type == ARG_FLAG) ? " ": ":<...>",
                    getArgTypeName(argList[idx].type),
                    argList[idx].description

            );
        }
    }

    /* Print legend */
    printf("\n\n");
    printf("Legend:\n");

    for (idx = 0; idx < ARG_TYPE_NAMES_COUNT; ++idx) {
        printf(" %s - %s\n", argTypeNames[idx].name, argTypeNames[idx].paramDesc); 
    }

}

static bool runCheckerNum(const args_arg *arg, u32 *val) {
    bool ok = (arg->checker) (val);

    if (!ok) {
        printf("ERROR: Value %lu (0x%lx) for parameter '%s' invalid!\n",
            *val,
            *val,
            arg->description);
    }

    return ok;
}

static bool runCheckerStr(const args_arg *arg, const char *val) {
    bool ok = (arg->checker) (&val);

    if (!ok) {
        printf("ERROR: Value '%s' for parameter '%s' invalid!\n",
            val,
            arg->description);
    }

    return ok;
}

static bool parse32(const char *toParse, u32 *dst, u32 limit) {
    u32         parsed;
    char       *parseEnd; 
    int         radix = 0; /* Todo maybe make this allow 'h' at the end for hex in addition to 0x */

    parsed = strtoul(toParse, &parseEnd, 0);


    if ((parseEnd == toParse) || (*parseEnd != '\0')) {
        printf ("ARGS: Input '%s' could not be parsed as a numeric value.\n", toParse); 
        return false;
    } else if (parsed > limit) {
        printf ("ARGS: Input %lu is out of range (limit: %lu)\n", parsed, limit);
        return false;
    } else {
        *dst = parsed;
        return true;
    }
}

static bool parseAndSetNum(const args_arg *arg, const char *toParse, u32 limit, u32 width) {
    u32 val;
    bool ok = parse32(toParse, &val, limit);

    if (ok && arg->checker) ok = runCheckerNum(arg, &val);
    if (ok && arg->dst)     memcpy(arg->dst, &val, width/8);

    return ok;
}

static bool parseAndSetStr(const args_arg *arg, const char *toParse) {
    bool ok = (strlen(toParse) < ARG_MAX);

    if (ok && arg->checker) ok = runCheckerStr(arg, toParse);
    if (ok && arg->dst)     strncpy(arg->dst, toParse, ARG_MAX);

    return ok;
}

static bool setFlag(const args_arg *arg) {
    bool *dstFlag = (bool*) arg->dst;
    bool ok = true;

    assert (dstFlag != NULL);

    if (arg->checker) ok = runCheckerStr(arg, dstFlag); /* dst as cheker parameter */
    if (ok)           *dstFlag = true;

    return ok;
}

static bool doParse(const args_arg *arg, const char *toParse) {
    const char *val = &toParse[strlen(arg->prefix) + 1 + 1];
    bool ok;

    switch (arg->type) {
        case ARG_STR:  return parseAndSetStr(arg, val);
        case ARG_U8:   return parseAndSetNum(arg, val, 0xFF,       8);
        case ARG_U16:  return parseAndSetNum(arg, val, 0xFFFF,     16);
        case ARG_U32:  return parseAndSetNum(arg, val, 0xFFFFFFFF, 32);
        case ARG_BOOL: return parseAndSetNum(arg, val, 0x01,       8);
        case ARG_FLAG: return setFlag(arg);

        default:
            printf("ARGS: OOPS. Unknown argument type!");
            abort();
    }

    return false;
}


static bool isThisArg(const args_arg *arg, const char *str) {

    size_t prefixLen = strlen(arg->prefix);

    /* Flags dont have a value, so we don't need to check for : at the end. */

    size_t minLength = (arg->type == ARG_FLAG) ? prefixLen + 1 : prefixLen + 2; 

    if (  (strlen(str) < minLength) 
       || (str[0] != '/') 
       || (strncmp(&str[1], arg->prefix, prefixLen) != 0) 
       || ((arg->type != ARG_FLAG) && (str[prefixLen + 1] != ':'))
       || ((arg->type == ARG_FLAG) && (str[prefixLen + 1] != '\0')) )
    {

        return false; 
    }

    return true;
}


bool args_parseArg(const args_arg *argList, size_t argCount, const char *toParse) {

    /* Das ja ganz schoen argListig hier :3 */

    size_t    idx;
    const args_arg *checkArg;

    assert (toParse != NULL);

    if (strcmp(toParse, "/?") == 0) {
        args_printUsage(argList, argCount);
        return false;
    } 

    for (idx = 0; idx < argCount; ++idx) {
        checkArg = &argList[idx];

        assert (checkArg != NULL);

        if (checkArg->prefix == NULL) {
           continue;
        }

        if (isThisArg(checkArg, toParse)) {
           return doParse(checkArg, toParse);
        }

    }

    printf("Input Parameter '%s' not recognized.\n");
    printf("Use " ARGS_USAGE " to show possible parameters.\n");

    return false;
}