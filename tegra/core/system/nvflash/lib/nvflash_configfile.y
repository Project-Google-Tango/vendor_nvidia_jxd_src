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

#include "nvcommon.h"
#include "nvos.h"
#include "nvflash_configfile.h"
#include <stdlib.h>
#include <stdio.h>

%}

%union {
    char *string;
}

%token OPEN CLOSE EQUALS
%token <string> STRING
%token <string> STRING2

%%

top:
      object_decl
    | top object_decl
    ;

object_decl: OPEN STRING CLOSE { NvFlashConfigFilePrivDecl( $2 ); } object_attrs
    ;

object_attrs:
      object_attr
    | object_attrs object_attr
    ;

object_attr: object_attr1 | object_attr2 ;
object_attr1:STRING EQUALS STRING { NvFlashConfigFilePrivAttr( $1, $3 ); } ;
object_attr2:STRING EQUALS STRING2 { NvFlashConfigFilePrivAttr( $1, $3 ); };

%%

extern FILE *yyin;

extern char *s_LastError;

NvError
NvFlashConfigFileParse( const char *filename,
    NvFlashConfigFileHandle *hConfig )
{
    NvError retVal;
    NvFlashConfigFilePrivInit( hConfig );
    yyin = fopen( filename, "r" );
    if( !yyin )
    {
        s_LastError = "file not found";
        return NvError_BadParameter;
    }
    retVal = yyparse();
    fclose( yyin );
    return retVal;
}
