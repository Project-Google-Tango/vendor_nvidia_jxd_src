/*************************************************************************************************
 *
 * Copyright (c) 2013, NVIDIA CORPORATION.  All rights reserved.
 *
 ************************************************************************************************/

/* To use the pattern mode, modify the modem by pasting/replacing the following code in
   //software/main.br/modem/ps_devices/private/os_logmux.c
   osLogDiagChGetDataByChunk(...)

static DXP_CACHED_UNI1 uint32 osLogDiagCount = 0;

static void osLogDiagChGetDataByChunk(Int8       *data_p,
                                      Int16      maxLength,
                                      Int16      *returnedLength_p,
                                      Boolean    interrupt)
{
   PARAMETER_NOT_USED(interrupt);
   int length = 0;

   uint32 sr;
   uint32 * data32 = (uint32 *)data_p;
   sr = os_AtomicSectionEnterLight();

   if (maxLength > 4)
   {
      int i = 1;
      data32[0] = maxLength;
      length =  (maxLength - 4) / 4;
      while(length > 0)
      {
            data32[i] = osLogDiagCount;
            i++;
            length--;
      }
      osLogDiagCount++;
   }

   *returnedLength_p = maxLength;
   os_AtomicSectionExitLight(sr);
}

*/


typedef enum
{
   PARSE_STATE_NB_WORDS,
   PARSE_STATE_COUNT,

   PARSE_STATE_INVALID
} e_parseState;

typedef struct
{
   unsigned int first_time;
   e_parseState state;
   unsigned int expected_count;
   unsigned int remaining_words;
   unsigned int gFileOffset;
   unsigned int errorAt;
   unsigned int errorCount;
} t_parseContext;

void parse_pattern_init(t_parseContext * ctx);
int parse_pattern(t_parseContext * ctx, unsigned int * buffer, int nbWords);

