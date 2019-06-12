/*************************************************************************************************
 *
 * Copyright (c) 2013, NVIDIA CORPORATION.  All rights reserved.
 *
 ************************************************************************************************/
#include "pattern_checker.h"
#include <string.h>

//----------------------------------------------------------------------------------------
void parse_pattern_init(t_parseContext * ctx)
{
   memset(ctx, 0, sizeof(t_parseContext));
   ctx->first_time = 1;
}
//----------------------------------------------------------------------------------------
int parse_pattern(t_parseContext * ctx, unsigned int * buffer, int nbWords)
{
   int ret = 0;
   int i;

   if (!buffer)
      return 0;
   if (nbWords == 0)
      return 0;

   unsigned int word = 0;

   for (i = 0 ; i < nbWords; i++)
   {
      word = buffer[i];
      if (ctx->state == PARSE_STATE_NB_WORDS)
      {
         ctx->remaining_words = (word/4) -1;
         ctx->state = PARSE_STATE_COUNT;
         ctx->expected_count++;
      }
      else if (ctx->state == PARSE_STATE_COUNT)
      {
         if (ctx->first_time)
         {
            ctx->expected_count = word;
            ctx->first_time = 0;
         }
         if (ctx->expected_count != word)
         {
            /* Bad count detected */
            ret = 1;
            if (ctx->errorAt == 0)
            {
               ctx->errorAt = ctx->gFileOffset;
               ctx->errorCount = word;
            }
            return ret;
         }
         ctx->remaining_words--;
         if (ctx->remaining_words == 0)
         {
            ctx->state = PARSE_STATE_NB_WORDS;
         }
      }
      ctx->gFileOffset+= 4;
   }

   return ret;
}