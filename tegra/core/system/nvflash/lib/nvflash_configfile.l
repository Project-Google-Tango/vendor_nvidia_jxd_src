%{
/*
 * Copyright (c) 2008-2012, NVIDIA CORPORATION. All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */

#include "nvos.h"
#include "nvflash_configfile.tab.h"
#include <stdio.h>

// suppress echoing to stdout
#define ECHO

int linenum = 1;

%}

%x COMMENT

%%

"\""[\40-\176]+"\"" {
        NvU32 len;
        len = NvOsStrlen( yytext );
        len++;
        yylval.string = NvOsAlloc( len );
        if( !yylval.string )
        {
            // FIXME: how to handle?
        }
        NvOsStrncpy( yylval.string, yytext, len - 1 );
        yylval.string[len-1] = 0;

        return STRING2;
    }

"["     { return OPEN; }
"]"     { return CLOSE; }
"="     { return EQUALS; }

[a-zA-Z0-9_:".""\\"/\-]+ {
        NvU32 len;
        len = NvOsStrlen( yytext );
        len++;
        yylval.string = NvOsAlloc( len );
        if( !yylval.string )
        {
            // FIXME: how to handle?
        }
        NvOsStrncpy( yylval.string, yytext, len - 1 );
        yylval.string[len-1] = 0;

        return STRING;
    }

[ \t]+          { ; }

"#"             { BEGIN COMMENT; }
<COMMENT>.      { ; }
<COMMENT>\n     { linenum++; BEGIN INITIAL; }

\n      { linenum++; }

%%

int yywrap( void )
{
    return 1;
}

void yyerror( char *msg )
{
    printf( "%s (%d): %s\n", msg, linenum, yytext );
}
