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

#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "icera-ril.h"
#include "icera-ril-utils.h"

#define LOG_TAG rilLogTag
#include <utils/Log.h>
#include <cutils/properties.h>
#include <openssl/bio.h>
#include <openssl/evp.h>
#include <math.h>

/* encode BCD byte to integer */
int bcdByteToInt(char a)
{
    int ret = 0;

    // treat out-of-range BCD values as 0
    if ((a & 0xf0) <= 0x90) {
        ret = (a >> 4) & 0xf;
    }

    if ((a & 0x0f) <= 0x09) {
        ret += (a & 0xf) * 10;
    }

    return ret;
}

/* encode char to hex */
char charToHex(char a)
{
    return (a < 10) ? a + '0' : a - 10 + 'A';
}

/* encode hex to char */
char hexToChar(char a)
{
    if (a >= 'a' && a <= 'f') {
        a -= 'a' - 'A';
    }

    return (a < 'A') ? a - '0' : a - 'A' + 10;
}

/* encode ascii string to hex string */
void asciiToHex(const char *input, char *output, int len)
{
    char ch;

    if (input == NULL ||
        output == NULL ||
        len == 0)
        return;

    while (len--) {
        ch = (*input >> 4) & 0x0f;
        *output++ = charToHex(ch);
        ch = *input & 0x0f;
        *output++ = charToHex(ch);
        input++;
    }

    *output = '\0';
}

void asciiToHexRevByteOrder( const char *input, char *output, int len)
{
    char ch;

    while (len--) {
        ch = *input & 0x0f;
        *output++ = charToHex(ch);
        ch = (*input >> 4) & 0x0f;
        *output++ = charToHex(ch);
        input++;
    }

    *output = '\0';
}

void removeInvalidPhoneNum( char *phoneNumber)
{
    ALOGD("removeInvalidPhoneNum");
    int len = strlen(phoneNumber);
    int i = 0;

    while (i < len) {
        if (phoneNumber[i] < '0' ||
            phoneNumber[i] > '9') {
            ALOGE("removeInvalidPhoneNum - found Invaild char %c", phoneNumber[i]);
            phoneNumber[i] = 0;
            return;
        }
        i++;
    }
}

/* encode hex string to ascii string */
void hexToAscii(const char *input, char *output, int len)
{
    char ch;

    while (len--) {
        ch = hexToChar(*input++) << 4;
        ch |= hexToChar(*input++);
        *output++ = ch;
    }

    *output = '\0';
}

/*
 * Encoding UCS2 string to UTF-8 string
 * input       : UCS2 strings.
 * output      : The output buffer where UTF8 string should be written
 * len         : The length of UCS2 string in symbols (16bits).
 * output_size : Size of the output buffer (to prevent overflows)
 * return      : The length of UTF8 which stored in output buffer.
 */
int UCS2ToUTF8(const char *input, char *output, int ucs2_symbols, int output_size)
{
    int ch;
    int utf8_bytes = 0;

    while (ucs2_symbols--) {
        ch = *input++ << 8;
        ch |= *input++;

        if (!ch)
            break;

        if (ch < 0x80) {
            utf8_bytes += 1;
            if (utf8_bytes > output_size)
               return -1;
            *output++ = (char)(ch & 0xFF);
        }
        else if (ch < 0x800) {
            utf8_bytes += 2;
            if (utf8_bytes > output_size)
               return -1;
            *output++ = (char)(0xC0 + ((ch >> 6) & 0x1F));
            *output++ = (char)(0x80 + (ch & 0x3F));
        }
        else {
            utf8_bytes += 3;
            if (utf8_bytes > output_size)
               return -1;
            *output++ = (char)(0xE0 + ((ch >> 12) & 0x0F));
            *output++ = (char)(0x80 + ((ch >> 6) & 0x3F));
            *output++ = (char)(0x80 + (ch & 0x3F));
        }
    }
    if (utf8_bytes >= output_size)
        return -1;
    *output = '\0';

    return utf8_bytes;
}

/*
 * Encoding UCS2 Hex string to UTF-8 string
 * input       : UCS2-Hex strings.
 * output      : The output buffer where UTF8 string should be written
 * len         : The length of UCS2-hex string in character.
 * output_size : Size of the output buffer (to prevent overflows)
 * return      : The length of UTF8 which stored in output buffer.
 */
int UCS2HexToUTF8(const char *input, char *output, int ucs2hex_bytes, int output_size)
{
    int ucs2_symbols = ucs2hex_bytes/4;
    int utf8_bytes = 0;

    char *ucs2 = calloc(ucs2_symbols * 2 + 1, sizeof(char));

    if (!ucs2) {
        return -1;
    }

    hexToAscii(input, ucs2, ucs2_symbols*2);

    utf8_bytes = UCS2ToUTF8(ucs2, output, ucs2_symbols, output_size);

    free(ucs2);

    return utf8_bytes;
}

int UCS2HexToUTF8_sizeEstimate(int ucs2hex_bytes)
{
    /* 1 UCS2 symbol is 2 bytes thus 4 hex chars.
     * each UCS2 symbol can generate up to 3 UTF8 bytes.
     * + ending char.
     */
    return (ucs2hex_bytes/4)*3 + 1;

}

/*
 * Parameters  : utf8        : should be UTF8 string.
                 ucs2        : output buffer where to write USC2 string
                 input_len   : length of input string
                 output_size : size of the output buffer (to check for overflows)
 * return      : the length of converted UCS2 symbols (16bits), -1 means error.
 */
int UTF8ToUCS2( const char *utf8, char *ucs2, int utf8_bytes, int output_size )
{
    char utf8_char;
    int ucs2_char;
    int ucs2_symbols = 0;

    if (!ucs2 || !utf8)
        return -1;

    while (utf8_bytes > 0) {
        utf8_char = *utf8++;
        utf8_bytes -= 1;
        if ((utf8_char & 0xF8) == 0xF0) {          /* 4 bytes */
            /* Not expected to get such a symbol here but not strictly an UTF8 coding error.
             * Thus only skipping
             */
            utf8 += 3;
            utf8_bytes -= 3;
            continue;
        } else if ((utf8_char & 0xF0) == 0xE0) {   /* 3 bytes */
            /* Ensure we can read the next 2 bytes. Otherwise stop decoding here*/
            if (utf8_bytes < 2)
                break;
            utf8_bytes -= 2;

            ucs2_char  = (utf8_char & 0x0F ) << 12;

            utf8_char = *utf8++;
            if ((utf8_char & 0xC0) != 0x80) {
               /* should be 10xxxxxx */
               ucs2_symbols = -1;
               break;
            }
            ucs2_char |= (utf8_char & 0x3F ) << 6;

            utf8_char = *utf8++;
            if ((utf8_char & 0xC0) != 0x80) {
               /* should be 10xxxxxx */
               ucs2_symbols = -1;
               break;
            }
            ucs2_char |= (utf8_char & 0x3F );

        } else if ((utf8_char & 0xE0) == 0xC0) {   /* 2 bytes */
            /* Ensure we can read the next byte. Otherwise stop decoding here*/
            if (utf8_bytes < 1)
                break;
            utf8_bytes -= 1;

            ucs2_char  = (utf8_char & 0x1F) << 6;

            utf8_char = *utf8++;
            if ((utf8_char & 0xC0) != 0x80) {
               /* should be 10xxxxxx */
               ucs2_symbols = -1;
               break;
            }

            ucs2_char |= (utf8_char & 0x3F );

        } else if (utf8_char < 0x80) {
            ucs2_char = utf8_char;            /* 1 byte */
        } else {
           /* Should not happen - error in coding */
           ucs2_symbols = -1;
           break;
        }

        /* Stop when output buffer is full. */
        if ( 2*(ucs2_symbols + 1) > output_size)
           break;

        *ucs2++ = (char) (ucs2_char >> 8);        /* High byte */
        *ucs2++ = (char) (ucs2_char);             /* Low byte  */
        ucs2_symbols++;

    }

    /* Terminate the UCS2 string. */
    if ( (ucs2_symbols>0) && (2*(ucs2_symbols + 1) <= output_size))
    {
       *ucs2++ = '\0';
       *ucs2++ = '\0';
    }

    return ucs2_symbols;    /* Return the ucs2 length. */
}


/*
 * Parameters  : utf8        : should be UTF8 string.
                 ucs2hex     : output buffer where to write USC2 Hex string
                 input_len   : length of input string
                 output_size : size of the output buffer (to check for overflows)
 * return      : the length of converted UCS2Hex string (in bytes), -1 means error.
 */
int UTF8ToUCS2Hex( const char *utf8, char *ucs2hex, int utf8_bytes, int output_size )
{
    int ucs2_symbols = utf8_bytes; /* worst case is 1 UCS2 symbol (2 chars) for each UTF8 char. */
    char *ucs2 = calloc((ucs2_symbols+1) * 2, sizeof(char)); /* Allocate temporary buffer for UCS2 (non hex). */
    if (!ucs2) {
        return -1;
    }

    /* first convert to ucs2 */
    ucs2_symbols = UTF8ToUCS2(utf8, ucs2, utf8_bytes, (ucs2_symbols+1)*2 );

    /* then move to hex if no error. */
    if (ucs2_symbols >= 0) {
        if (ucs2_symbols*4 >= output_size)
           return -1; /* Output buffer too small. */

        asciiToHex(ucs2, ucs2hex, ucs2_symbols*2);
        /*Terminate the string. */
        ucs2hex[ucs2_symbols*4] = '\0';
    }

    /* free temporary buffer */
    free(ucs2);

    /* Returns number of uscHex string bytes. */
    return ucs2_symbols*4;
}


int UTF8ToUCS2Hex_sizeEstimate(int utf8_bytes)
{
    /* Worst case is that each UTF8 byte generates 1 UCS2 symbol
     * 1 UCS2 symbol (16bits) is 4 Hex char
     * Adding 1 ending symbol (0x00)
     */
    return (utf8_bytes+1)*4;

}

/* encode GSM string to UTF-8 string */
void GSMToUTF8(const char *input, char *output, int len)
{
    /* brute force Look up table */
    uint16_t Utf8vsGsmLookup[]=
    {
        /*00*/  0x40,       //@
        /*01*/  0xC2A3,     //£
        /*02*/  0x24,       //$
        /*03*/  0xC2A5,     //¥
        /*04*/  0xC3A8,     //è
        /*05*/  0xC3A9,     //é
        /*06*/  0xC3B9,     //ù
        /*07*/  0xC3AC,     //ì
        /*08*/  0xC3B2,     //ò
        /*09*/  0xC387,     //Ç
        /*0A*/  0x0A,       //<LF>
        /*0B*/  0xC398,     //Ø
        /*0C*/  0xC3B8,     //ø
        /*0D*/  0x0D,       //<CR>
        /*0E*/  0xC385,     //Å
        /*0F*/  0xC3A5,     //å
        /*10*/  0xCE94,
        /*11*/  0x5F,       //_
        /*12*/  0xCEA6,     //Φ
        /*13*/  0xCE93,     //Γ
        /*14*/  0xCE9B,     //Λ
        /*15*/  0xCEA9,     //Ω
        /*16*/  0xCEA0,     //Π
        /*17*/  0xCEA8,     //Ψ
        /*18*/  0xCEA3,     //Σ
        /*19*/  0xCE98,     //Θ
        /*1A*/  0xCE9E,     //Ξ
        /*1B*/  0x1B,       //<ESC>
        /*1C*/  0xC386,     //Æ
        /*1D*/  0xC3A6,     //æ
        /*1E*/  0xC39F,     //ß
        /*1F*/  0xC389,     //É
        /*20*/  0x20,       //<SP>
        /*21*/  0x21,       //!
        /*22*/  0x22,
        /*23*/  0x23,       //#
        /*24*/  0xC2A4,     //¤
        /*25*/  0x25,       //%
        /*26*/  0x26,       //&
        /*27*/  0x27,
        /*28*/  0x28,       //(
        /*29*/  0x29,       //)
        /*2A*/  0x2A,       //*
        /*2B*/  0x2B,       //+
        /*2C*/  0x2C,       //,
        /*2D*/  0x2D,       //-
        /*2E*/  0x2E,       //.
        /*2F*/  0x2F,       ///
        /*30*/  0x30,       //0
        /*31*/  0x31,       //1
        /*32*/  0x32,       //2
        /*33*/  0x33,       //3
        /*34*/  0x34,       //4
        /*35*/  0x35,       //5
        /*36*/  0x36,       //6
        /*37*/  0x37,       //7
        /*38*/  0x38,       //8
        /*39*/  0x39,       //9
        /*3A*/  0x3A,       //:
        /*3B*/  0x3B,       //;
        /*3C*/  0x3C,       //<
        /*3D*/  0x3D,       //=
        /*3E*/  0x3E,       //>
        /*3F*/  0x3F,       //?
        /*40*/  0xC2A1,     //¡
        /*41*/  0x41,       //A
        /*42*/  0x42,       //B
        /*43*/  0x43,       //C
        /*44*/  0x44,       //D
        /*45*/  0x45,       //E
        /*46*/  0x46,       //F
        /*47*/  0x47,       //G
        /*48*/  0x48,       //H
        /*49*/  0x49,       //I
        /*4A*/  0x4A,       //J
        /*4B*/  0x4B,       //K
        /*4C*/  0x4C,       //L
        /*4D*/  0x4D,       //M
        /*4E*/  0x4E,       //N
        /*4F*/  0x4F,       //O
        /*50*/  0x50,       //P
        /*51*/  0x51,       //Q
        /*52*/  0x52,       //R
        /*53*/  0x53,       //S
        /*54*/  0x54,       //T
        /*55*/  0x55,       //U
        /*56*/  0x56,       //V
        /*57*/  0x57,       //W
        /*58*/  0x58,       //X
        /*59*/  0x59,       //Y
        /*5A*/  0x5A,       //Z
        /*5B*/  0xC384,     //Ä
        /*5C*/  0xC396,     //Ö
        /*5D*/  0xC391,     //Ñ
        /*5E*/  0xC39C,     //Ü
        /*5F*/  0xC2A7,     //§
        /*60*/  0xC2BF,     //¿
        /*61*/  0x61,       //a
        /*62*/  0x62,       //b
        /*63*/  0x63,       //c
        /*64*/  0x64,       //d
        /*65*/  0x65,       //e
        /*66*/  0x66,       //f
        /*67*/  0x67,       //g
        /*68*/  0x68,       //h
        /*69*/  0x69,       //i
        /*6A*/  0x6A,       //j
        /*6B*/  0x6B,       //k
        /*6C*/  0x6C,       //l
        /*6D*/  0x6D,       //m
        /*6E*/  0x6E,       //n
        /*6F*/  0x6F,       //o
        /*70*/  0x70,       //p
        /*71*/  0x71,       //q
        /*72*/  0x72,       //r
        /*73*/  0x73,       //s
        /*74*/  0x74,       //t
        /*75*/  0x75,       //u
        /*76*/  0x76,       //v
        /*77*/  0x77,       //w
        /*78*/  0x78,       //x
        /*79*/  0x79,       //y
        /*7A*/  0x7A,       //z
        /*7B*/  0xC3A4,     //ä
        /*7C*/  0xC3B6,     //ö
        /*7D*/  0xC3B1,     //ñ
        /*7E*/  0xC3BC,     //ü
        /*7F*/  0xC3A0,     //à
    };

    uint16_t ch;
    while (len--) {
        switch (*input)
        {
            /* Escape, special treatment */
            case 0x1B:
                input++;
                len--;
                switch(*input)
                {
                    case 0x10:
                        *output = 0x0C;//<FF>
                        break;
                    case 0x14:
                        *output = 0x5E;//^
                        break;
                    case 0x28:
                        *output = 0x7B;//{
                        break;
                    case 0x29:
                        *output = 0x7D;//}
                        break;
                    case 0x2F:
                        *output = 0x5C;//\'
                        break;
                    case 0x3C:
                        *output = 0x5B;//[
                        break;
                    case 0x3D:
                        *output = 0x7E;//~
                        break;
                    case 0x3E:
                        *output = 0x5D;//]
                        break;
                    case 0x40:
                        *output = 0x7C;//|
                        break;
                    case 0x65:
                        *output++ = 0xE2;//€
                        *output++ = 0x82;
                        *output = 0xAC;
                        break;
                    default:
                        /*
                         * false alert, this wasn't an escape after all
                         * Pass the esc char and rewind.
                         */ 
                        len++;
                        input--;
                        *output = 0x1B;
                        break;
                }
            break;
            default:
                ch = Utf8vsGsmLookup[(int)*input];
                /* Double char */
                if(ch >= 0x80)
                {
                    *output = (ch>>8);
                    output++;
                    ch &=0xFF;
                }
                *output = ch;
            break;
        }
        output++;
        input++;
    }
    *output = '\0';
}

/* encode numeric string to BCD string */
void numericToBCD(const char *input, char *output, int len)
{
    char ch1, ch2;

    while (len--) {
        ch1 = *input++;
        ch2 = *input++;

        if (ch1) {
            ch1 -= '0';
        }
        else {
            ch1 = 0x0F;
        }

        if (ch2) {
            ch2 -= '0';
        }
        else {
            ch2 = 0x0F;
        }

        *output++ = (ch1 | (ch2 << 4));
    }

    *output = '\0';
}

/**
 * Dump byte array in hex mode
 */
void DumpByteArray(char *in, int len)
{
    int i=0;
    for (i=0;i<len;i++) {
        ALOGD("%02x", in[i]);
    }
}

//Calculates the length of a decoded base64 string
static int calcDecodeLength(char** b64input)
{
    int len = strlen(*b64input);
    char *ptrB64Str = *b64input;
    int padding = ((len % 4) == 0)?0:4-(len%4);

    if (padding == 0) {
        if (ptrB64Str[len-1] == '=' && ptrB64Str[len-2] == '=') //last two chars are =
            padding = 2;
        else if (ptrB64Str[len-1] == '=') //last char is =
            padding = 1;
    } else if (padding == 1 || padding == 2) {
        // Filling the padding characters
        *b64input = (char *)realloc(*b64input, sizeof(char) * (len + padding));
        ptrB64Str = *b64input;
        len += padding;
        ptrB64Str[len-1] = '=';
        if (padding == 2)
            ptrB64Str[len-2] = '=';
    } else {
        ALOGE("padding = %d", padding);
        return -1;  // if padding == 4, it means the b64 is not legal.
    }

    return (int)len*0.75 - padding;
}

/*
 * input : base64 string
 * output : ascii string in binary
 * return : length of the output, or -1 mean wrong B64 input.
 */
int decode_Base64(const char *input, uint8_t **output)
{
    BIO *bio = NULL, *b64 = NULL;
    int ret = -1;
    char *paddingInput = strdup(input);
    int decodeLen = calcDecodeLength(&paddingInput);
    if (decodeLen == -1) {
        *output = NULL;
        ALOGE("input is not a legal Base64 string.");
        goto error;
    }

    *output = (uint8_t *)malloc(decodeLen);

    if (*output == NULL) {
        ALOGE("Alloc buffer failed");
        goto error;
    }

    b64 = BIO_new(BIO_f_base64());
    bio = BIO_new_mem_buf((void *)paddingInput, strlen(paddingInput));
    BIO_set_close(bio, BIO_NOCLOSE);
    bio = BIO_push(b64, bio);
    BIO_set_flags(bio, BIO_FLAGS_BASE64_NO_NL); //Do not use newlines to flush buffer
    ret = BIO_read(bio, *output, strlen(paddingInput));

error:
    BIO_free_all(bio);
    free(paddingInput);
    return ret;
}

int encode_Base64(const char *input, char **output, int input_len)
{
    BIO *b64, *mem;
    int len = 0;
    char *ptr_b64_mem = NULL;

    //DumpByteArray((char *)input, input_len);

    b64 = BIO_new(BIO_f_base64());
    BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL); //Ignore newlines - write everything in one line
    mem = BIO_new(BIO_s_mem());
    b64 = BIO_push(b64, mem);
    BIO_write(b64, input, input_len);
    BIO_flush(b64);
    len = BIO_get_mem_data(b64, &ptr_b64_mem);
    if (len != 0 && ptr_b64_mem != NULL) {
        *output = (char *)calloc(sizeof(char), len + 1);
        memcpy(*output, ptr_b64_mem, len);
        (*output)[len] = '\0';
    }
    BIO_free_all(b64);
    return len;
}

/**
 * Find the emergency call number is in the list or not
 * Return: -1 means error, 0 means can't find, 1 means in the list.
 */
int findNumInList(const char *list, char *num)
{
    char *numInList = NULL;
    char *dupList = NULL;
    if (list == NULL || num == NULL)
        return -1;

    dupList = strdup(list);
    numInList = strtok(dupList, ",");
    while (numInList) {
        if(strcmp(numInList, num) == 0) {
            free(dupList);
            return 1;
        }
        numInList = strtok(NULL, ",");
    }
    free(dupList);
    return 0;
}

#ifdef ECC_VERIFICATION
const const char *DTT_hexcodeDefault="030708090B0C0E0F24405B5C5D5E5F601C1D1E1F1012131415161718191A";
const const char *DTT_digitDefault=  "897023415618307564290729486153";

Dtt_table* initDttTable(void)
{
    char Hexcode[PROPERTY_VALUE_MAX];
    char Digit[PROPERTY_VALUE_MAX];
    unsigned int NumEntries = 0, index = 0;
    Dtt_table *TmoDtt;

    /* Get OTA'ed DTT if present, otherwise use default */
    property_get("ril.dtt_hexcode", Hexcode, DTT_hexcodeDefault);
    property_get("ril.dtt_digit", Digit, DTT_digitDefault);

    NumEntries = strlen(Digit);

    if(strlen(Hexcode)!=NumEntries*2)
    {
        /*There is a problem */
        return NULL;
    }

    TmoDtt = malloc(sizeof(Dtt_table));
    TmoDtt->table = malloc(NumEntries * sizeof(Dtt_tableEntry));
    TmoDtt->NumEntries= NumEntries;

    while(index < NumEntries)
    {
        TmoDtt->table[index].hexChar=hexToChar(Hexcode[2*index])*0x10+hexToChar(Hexcode[(2*index)+1]);
        TmoDtt->table[index].digit = hexToChar(Digit[index]);
        index ++;
    }
    return TmoDtt;
}

static char DttTranslate(char value, Dtt_table* table)
{
    int i=0;

    while(i < table->NumEntries)
    {
        if(table->table[i].hexChar == value)
        {
            return table->table[i].digit;
        }
        i++;
    }
    return -1;
}

int ProcessUNR(char* UNR, char* UVR, Dtt_table* table)
{
    int i, j;

    /* Compute A B C */
    for(j=0;j<3;j++)
    {
        i = DttTranslate(UNR[j],table);
        if(i<0)goto error;
        /*
         * Spec says UNR i+4 but they seem to start their string at
         * index 1 rather than 0
         */
        i = DttTranslate(UNR[i+3],table);
        if(i<0) goto error;

        /* Store and convert value to digit char */
        UVR[j] = i+'0';
    }
    ALOGD("UVR: %s",UVR);
    return 0;

    error:
    return -1;
}

#endif
