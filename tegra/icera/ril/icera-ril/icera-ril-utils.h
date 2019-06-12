/* Copyright (c) 2010-2014, NVIDIA CORPORATION.  All rights reserved. */
/* 
** Copyright 2010, Icera Inc.
**
** Licensed under the Apache License, Version 2.0 (the "License");
** You may not use this file except in compliance with the License.
** You may obtain a copy of the License at:
**
** http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** imitations under the License.
*/
/*
**
** Copyright 2006, The Android Open Source Project
**
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
**
**     http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
*/

#ifndef ICERA_RIL_UTILS_H
#define ICERA_RIL_UTILS_H 1

int bcdByteToInt(char a);
char charToHex(char a);
char hexToChar(char a);
void asciiToHex(const char *input, char *output, int len);
void asciiToHexRevByteOrder( const char *input, char *output, int len);
void removeInvalidPhoneNum( char *phoneNumber);
void hexToAscii(const char *input, char *output, int len);
int UCS2ToUTF8(const char *input, char *output, int ucs2_symbols, int output_size);
int UCS2HexToUTF8(const char *input, char *output, int ucs2hex_bytes, int output_size);
int UCS2HexToUTF8_sizeEstimate(int ucs2hex_bytes);
void GSMToUTF8(const char *input, char *output, int len);
void numericToBCD(const char *input, char *output, int len);
int UTF8ToUCS2( const char *utf8, char *ucs2, int utf8_bytes, int output_size );
int UTF8ToUCS2Hex( const char *utf8, char *ucs2hex, int utf8_bytes, int output_size );
int UTF8ToUCS2Hex_sizeEstimate(int utf8_bytes);
int decode_Base64(const char *input, uint8_t **output);
int encode_Base64(const char *input, char **output, int input_len);
/**
 * Dump byte array in hex mode
 */
void DumpByteArray(char *in, int length);
int findNumInList(const char *list, char *num);

#ifdef ECC_VERIFICATION
typedef struct  _Dtt_tableEntry{
    int hexChar;
    int digit;
}Dtt_tableEntry;

typedef struct  _Dtt_table{
    int NumEntries;
    Dtt_tableEntry *table;
}Dtt_table;

Dtt_table* initDttTable(void);
int ProcessUNR(char* UNR, char* UVR, Dtt_table* table);

#endif
#endif
