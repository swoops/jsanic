#!/usr/bin/python3
import sys

ONAME = "tokens"
TAB = "\t"

extra_nameids = [
    "VARIABLE",
]
types = [
    "WHITESPACE",
    "IDENTIFIER",
    "KEYWORD",
    "NUMERIC",
    "PUNCTUATOR",
    "REGULAREXPRESSION",
    "STRING",
    "INVALID", # is not kept anyways
]

tokens = {
    # whitespace
    "'": {
        "nameid" : "QUOTE",
        "type" : "STRING",
    },
    "\"": {
        "nameid" : "SINGEQUOTE",
        "type" : "STRING",
    },
    " ": {
        "nameid" : "SPACE",
        "type" : "WHITESPACE",
    },
    "\n": {
        "nameid" : "NEWLINE",
        "type" : "WHITESPACE",
    },
    "\t": {
        "nameid" : "TAB",
        "type" : "WHITESPACE",
    },
    "0": {
        "nameid" : "NUMBER",
        "type" : "NUMERIC",
    },
    "\x7B": {
        "nameid" : "CURLYOPEN",
        "type" : "PUNCTUATOR",
    },
    "\x7D" :{
        "nameid" : "CURLYCLOSE",
        "type" : "PUNCTUATOR",
    },
    "[": {
        "nameid" : "SQUAREOPEN",
        "type" : "PUNCTUATOR",
    },
    "]" :{
        "nameid" : "SQUARECLOSE",
        "type" : "PUNCTUATOR",
    },
    "(" : {
        "nameid" :"OPENPAREN",
        "type" : "PUNCTUATOR",
    },
    ")" : {
        "nameid" :"CLOSEPAREN",
        "type" : "PUNCTUATOR",
    },
    "==" : {
        "nameid" : "EQUAL",
        "type" : "PUNCTUATOR",
    },
    "<=" : {
        "nameid" :"LESSTHANEQUAL",
        "type" : "PUNCTUATOR",
    },
    ">=" : {
        "nameid" : "GREATERTHANEQUAL",
        "type" : "PUNCTUATOR",
    },
    "===" : {
        "nameid" : "EXACTLYEQUAL",
        "type" : "PUNCTUATOR",
    },
    "=" : {
        "nameid" : "ASSIGNMENT",
        "type" : "PUNCTUATOR",
    },
    "!=" : {
        "nameid" : "NOTEQUAL",
        "type" : "PUNCTUATOR",
    },
    "!" : {
        "nameid" : "NOT",
        "type" : "PUNCTUATOR",
    },
    ":" : {
        "nameid" : "COLON",
        "type" : "PUNCTUATOR",
    },
    ";" : {
        "nameid" : "SEMICOLON",
        "type" : "PUNCTUATOR",
    },
    "," : {
        "nameid" : "COMMA",
        "type" : "PUNCTUATOR",
    },
    "switch" : {
        "nameid" : "SWITCH",
        "type" : "KEYWORD",
    },
    "return" : {
        "nameid" : "RETURN",
        "type" : "KEYWORD",
    },
    "if" : {
        "nameid" : "IF",
        "type" : "KEYWORD",
    },
    "else" : {
        "nameid" : "ELSE",
        "type" : "KEYWORD",
    },
    "do" : {
        "nameid" : "DO",
        "type" : "KEYWORD",
    },
    "while" : {
        "nameid" : "WHILE",
        "type" : "KEYWORD",
    },
    "try" : {
        "nameid" : "TRY",
        "type" : "KEYWORD",
    },
    "catch" : {
        "nameid" : "CATCH",
        "type" : "KEYWORD",
    },
    "throw" : {
        "nameid" : "THROW",
        "type" : "KEYWORD",
    },
    "function" : {
        "nameid" : "FUNCTION",
        "type" : "KEYWORD",
    },
    "var" : {
        "nameid" : "VAR",
        "type" : "KEYWORD",
    },
    "let" : {
        "nameid" : "LET",
        "type" : "KEYWORD",
    },
    "const" : {
        "nameid" : "CONST",
        "type" : "KEYWORD",
    },
}

tokens_sorted_list = list( tokens.keys() )
tokens_sorted_list.sort()



def get_warning():
    return """/*
 * DO NOT MODIFY THIS FILE!
 *
 * this was generated by the %s script to update how this works, update
 * that script otherwise your changes will not be kept for the next compile
*/

""" % sys.argv[0]

def get_tokendata_struct():
    return """
typedef struct tokendata {
    char *name;
    size_t length;
    int type;
    size_t nameid;
} tokendata;
"""

def get_types():
    ret  = "enum tokentypes {\n"
    for i in types:
        ret += TAB
        ret += "%s,\n" % i
    ret += "};\n"
    return ret

def get_nameids():
    ret  = "enum nameids {\n"
    for i,c in enumerate( tokens_sorted_list ):
        ret += TAB
        ret += "%s = %d,\n" % (tokens[c]["nameid"], i)
    for c in extra_nameids:
        i += 1
        ret += TAB
        ret += "%s = %d,\n" % (c, i)

    ret += "};\n"
    return ret

def get_header():
    return """
#include <stdio.h>
"""

def format_token(token):
    # should not be a '\"' in a token
    token = token
    token = token.replace('\\', '\\\\')
    token = token.replace('"', '\\"')
    token = token.replace('\n', '\\n')
    token = token.replace('\t', '\\t')
    token = '"%s"' % token
    return token

def get_tokenstrs():
    ## NOTICE:
    ## the order of the tokens is very important  "==" must be before "="
    ## otherwise you would get two tokens of "=" instead of one "=="
    ret = "tokendata tokenstrs[] = {\n"
    for name in tokens_sorted_list:
        info = tokens[name]
        ret += "{\n"
        ret += TAB*2 + ".name = %s,\n" % (format_token(name))
        ret += TAB*2 + ".length = %d,\n" % (len(name))
        for i in info:
            ret += TAB*2 + ".%s = %s,\n" % (i, info[i])
        ret += TAB + "},"
    ret += "\n};\n"
    return ret


def build_h_file():
    name = "%s.h" % ONAME
    print("buildling %s" % name)
    with open(name, "w") as fp:
        fp.write( get_warning() )
        fp.write( get_header() )
        fp.write( get_types() )
        fp.write( get_nameids() )
        fp.write( get_tokendata_struct() )
        fp.write( get_tokenstrs() )

build_h_file()
