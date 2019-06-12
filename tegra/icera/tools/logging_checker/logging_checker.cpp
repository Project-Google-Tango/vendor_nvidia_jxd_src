/*************************************************************************************************
 *
 * Copyright (c) 2013, NVIDIA CORPORATION.  All rights reserved.
 *
 ************************************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "logging_checker.h"
#include "logs.h"

//------------------------------------------------------------------------------------------
#define COM_NUM_DXPS 3

//#define OLD_LOGGING_DO_NOT_ASSERT_ON_SYNC_ERROR
#define OS_LOG_FLOW_CONTROL 0xD

#define uint8_t  unsigned char
#define uint32_t unsigned int
#define uint64 unsigned long long
//------------------------------------------------------------------------------------------
typedef enum
{
   LOG_EXPECT_CHUNK_HEADER_1,
   LOG_EXPECT_CHUNK_HEADER_2,
   LOG_EXPECT_CHUNK_HEADER_3,

   LOG_EXPECT_MESSAGE_HEADER_1,
   LOG_EXPECT_MESSAGE_HEADER_2,
   LOG_EXPECT_MESSAGE_HEADER_3,

   LOG_EXPECT_DATA,

} log_check_step_t;

//------------------------------------------------------------------------------------------
typedef struct
{
   struct
   {
      FILE *             f_out;
      unsigned int       output_only_the_last_x_mbytes;
      unsigned int       logging_data_is_debug;
      long long          total_size_to_proceed;
   }
   init_args;

   log_check_step_t   step;
   long long          global_address;
   long long          address[COM_NUM_DXPS];
   unsigned int       message_lost_error[COM_NUM_DXPS];

   uint32_t           current_proc_id;
   uint32_t           have_last_chunk_info[COM_NUM_DXPS];
   struct
   {
      uint32_t           nb_proc;
      uint32_t           seq_num;
      long long          nb_words;
      long long          address;
   }
   chunk_info[COM_NUM_DXPS];

   uint32_t           have_last_message_info[COM_NUM_DXPS];
   struct
   {
      uint64             timestamp;
      uint32_t           ID;
      long long          nb_words;
      long long          address;
      uint32_t           seq_num;
      log_check_step_t   next_step;
   }
   message_info[COM_NUM_DXPS];

   struct
   {
      int          unaligned_data_index;
      unsigned int unaligned_data;
   }
   data_alignement;

   struct
   {
      unsigned int nb_words[COM_NUM_DXPS];
      unsigned int nb_messages[COM_NUM_DXPS];
      unsigned int nb_chunks[COM_NUM_DXPS];
   }
   stats;

   struct
   {
      unsigned int started;
      unsigned int sync_error;
      unsigned int  message_lost_error;
      struct
      {
         uint32_t           timestamp;
         uint32_t           ID;
         long long          nb_words;
         long long          address;
      } message_info;

   } old_logging;

   unsigned int old_logging_format;

} log_check_ctx_t;


//------------------------------------------------------------------------------------------
#define DEV_FAIL(c)   { \
                           long long optimal_l_value = ((global_checker_ctx.init_args.total_size_to_proceed - ctx->global_address*4)/0x100000) + 1; \
                           logging_checker_out_f_print("%s\n", c); \
                           ALOGI("** ERROR DETECTED **");   \
                           ALOGI("%s", c); \
                           ALOGI("Address:  0x%012llx", ctx->global_address*4); \
                           ALOGI("Size in bytes: %d", size_in_bytes); \
                           ALOGI("data[0]=0x%08x\ndata[1]=0x%08x\ndata[2]=0x%08x\ndata[3]=0x%08x\ndata[4]=0x%08x", *data32, data32[1], data32[2], data32[3], data32[4]); \
                           ALOGI("Relaunch with -o for output details, 'l' option optimal value is %lld", optimal_l_value); \
                           return 1; \
                      }

static const uint32_t sync_words[3]=
{
    0xfb3e98,
    0x30cf29,
    0xc426b9
};

//------------------------------------------------------------------------------------------
static int check_old_log_stream(log_check_ctx_t *ctx, uint32_t * data32, int size_in_bytes);
static int check_log_stream(log_check_ctx_t *ctx, uint32_t * data32, int size_in_bytes);
static log_check_ctx_t global_checker_ctx;

//------------------------------------------------------------------------------------------
#define logging_checker_out_f_print(x, ...)    if ((global_checker_ctx.init_args.f_out)&&((((global_checker_ctx.init_args.total_size_to_proceed/4)-global_checker_ctx.global_address)/0x40000)<=global_checker_ctx.init_args.output_only_the_last_x_mbytes))   \
                                               {  \
                                                   fprintf(global_checker_ctx.init_args.f_out, x, ## __VA_ARGS__);  \
                                               }

//------------------------------------------------------------------------------------------
void logging_checker_init(char * out_filename, int output_only_the_last_x_mbytes, int logging_data_is_debug, long long total_size_to_proceed)
{
   memset(&global_checker_ctx, 0, sizeof(log_check_ctx_t));

   if ((out_filename) && (strlen(out_filename) > 0))
   {
      global_checker_ctx.init_args.f_out = fopen(out_filename, "w+");
   }
   global_checker_ctx.init_args.output_only_the_last_x_mbytes = output_only_the_last_x_mbytes;
   global_checker_ctx.init_args.logging_data_is_debug         = logging_data_is_debug;
   global_checker_ctx.init_args.total_size_to_proceed         = total_size_to_proceed;
}

//------------------------------------------------------------------------------------------
int logging_checker_process(unsigned int * buffer, int size)
{
   if (global_checker_ctx.old_logging_format == 1)
   {
      return check_old_log_stream(&global_checker_ctx, buffer, size);
   }

   return check_log_stream(&global_checker_ctx, buffer, size);
}

//------------------------------------------------------------------------------------------
static void logging_checker_print_stats(void)
{
   unsigned int nb_chunks, nb_chunks_div;
   unsigned int nb_messages, nb_messages_div;
   unsigned int nb_words, nb_words_per100;

   if (global_checker_ctx.old_logging_format == 0)
   {
      nb_chunks   = global_checker_ctx.stats.nb_chunks[0]+global_checker_ctx.stats.nb_chunks[1]+global_checker_ctx.stats.nb_chunks[2];
      nb_messages = global_checker_ctx.stats.nb_messages[0]+global_checker_ctx.stats.nb_messages[1]+global_checker_ctx.stats.nb_messages[2];
      nb_words    = global_checker_ctx.stats.nb_words[0]+global_checker_ctx.stats.nb_words[1]+global_checker_ctx.stats.nb_words[2];

      nb_words_per100    = nb_words/100;

      /* To avoid division by 0 */
      nb_chunks_div   = nb_chunks;
      nb_messages_div = nb_messages;
      if (nb_chunks_div == 0)        nb_chunks_div = 1;
      if (nb_messages_div == 0)      nb_messages_div = 1;
      if (nb_words_per100 == 0)      nb_words_per100 = 1;

      ALOGI(" ");

      if ((global_checker_ctx.have_last_chunk_info[0] == 0) &&
          (global_checker_ctx.have_last_chunk_info[1] == 0) &&
          (global_checker_ctx.have_last_chunk_info[2] == 0))
      {
         ALOGI("This file does not contain any logging data");
         ALOGI("FileSize : %10lldKB", global_checker_ctx.init_args.total_size_to_proceed/1000);
      }
      else
      {
         ALOGI("New logging format");
         ALOGI("STATS:");
         ALOGI("  Chunks");
         ALOGI("     DXP0: %10d %3d%%", global_checker_ctx.stats.nb_chunks[0], global_checker_ctx.stats.nb_chunks[0]*100/nb_chunks_div);
         ALOGI("     DXP1: %10d %3d%%", global_checker_ctx.stats.nb_chunks[1], global_checker_ctx.stats.nb_chunks[1]*100/nb_chunks_div);
         ALOGI("     DXP2: %10d %3d%%", global_checker_ctx.stats.nb_chunks[2], global_checker_ctx.stats.nb_chunks[2]*100/nb_chunks_div);
         ALOGI("     ALL : %10d", nb_chunks);
         ALOGI("  Messages");
         ALOGI("     DXP0: %10d %3d%%", global_checker_ctx.stats.nb_messages[0], global_checker_ctx.stats.nb_messages[0]*100/nb_messages_div);
         ALOGI("     DXP1: %10d %3d%%", global_checker_ctx.stats.nb_messages[1], global_checker_ctx.stats.nb_messages[1]*100/nb_messages_div);
         ALOGI("     DXP2: %10d %3d%%", global_checker_ctx.stats.nb_messages[2], global_checker_ctx.stats.nb_messages[2]*100/nb_messages_div);
         ALOGI("     ALL : %10d", nb_messages);
         ALOGI("  Words proceeded");
         ALOGI("     DXP0: %10d %3d%%", global_checker_ctx.stats.nb_words[0], global_checker_ctx.stats.nb_words[0]/nb_words_per100);
         ALOGI("     DXP1: %10d %3d%%", global_checker_ctx.stats.nb_words[1], global_checker_ctx.stats.nb_words[1]/nb_words_per100);
         ALOGI("     DXP2: %10d %3d%%", global_checker_ctx.stats.nb_words[2], global_checker_ctx.stats.nb_words[2]/nb_words_per100);
         ALOGI("     ALL : %10d", nb_words);
         ALOGI("  Message lost");
         ALOGI("     DXP0: %10d", global_checker_ctx.message_lost_error[0]);
         ALOGI("     DXP1: %10d", global_checker_ctx.message_lost_error[1]);
         ALOGI("     DXP2: %10d", global_checker_ctx.message_lost_error[2]);
         ALOGI("     ALL : %10d", global_checker_ctx.message_lost_error[0]+global_checker_ctx.message_lost_error[1]+global_checker_ctx.message_lost_error[2]);
         ALOGI("FileSize : %10lldKB", global_checker_ctx.init_args.total_size_to_proceed/1000);
      }
   }
   else
   {
      nb_messages = global_checker_ctx.stats.nb_messages[0]+global_checker_ctx.stats.nb_messages[1];
      nb_words    = global_checker_ctx.stats.nb_words[0]+global_checker_ctx.stats.nb_words[1];

      nb_words_per100    = nb_words/100;

      /* To avoid division by 0 */
      nb_messages_div = nb_messages;
      if (nb_messages_div == 0)      nb_messages_div = 1;
      if (nb_words_per100 == 0)      nb_words_per100 = 1;

      ALOGI(" ");
      ALOGI("Old logging format");
      ALOGI("STATS:");
      ALOGI("  Messages");
      ALOGI("     DXP0: %10d %3d%%", global_checker_ctx.stats.nb_messages[0], global_checker_ctx.stats.nb_messages[0]*100/nb_messages_div);
      ALOGI("     DXP1: %10d %3d%%", global_checker_ctx.stats.nb_messages[1], global_checker_ctx.stats.nb_messages[1]*100/nb_messages_div);
      ALOGI("     ALL : %10d", nb_messages);
      ALOGI("  Words proceeded");
      ALOGI("     DXP0: %10d %3d%%", global_checker_ctx.stats.nb_words[0], global_checker_ctx.stats.nb_words[0]/nb_words_per100);
      ALOGI("     DXP1: %10d %3d%%", global_checker_ctx.stats.nb_words[1], global_checker_ctx.stats.nb_words[1]/nb_words_per100);
      ALOGI("     ALL : %10d", nb_words);
      ALOGI("FileSize : %10lldKB", global_checker_ctx.init_args.total_size_to_proceed/1000);
      ALOGI("Sync errors: %d", global_checker_ctx.old_logging.sync_error);
      ALOGI("Message lost errors: %d", global_checker_ctx.old_logging.message_lost_error);
   }

}

//------------------------------------------------------------------------------------------
void logging_checker_finish(void)
{
   if (global_checker_ctx.init_args.f_out)
   {
      fflush(global_checker_ctx.init_args.f_out);
      fclose(global_checker_ctx.init_args.f_out);
      global_checker_ctx.init_args.f_out = NULL;
   }
   logging_checker_print_stats();
}

//------------------------------------------------------------------------------------------
static int getAlignedWord(log_check_ctx_t *ctx, uint32_t & word, int & size_in_bytes, uint8_t * &data, uint32_t * &data32)
{
   unsigned char *   unaligned_data = ((unsigned char *) ctx->data_alignement.unaligned_data);

   /*** Complete non aligned data to make a 4 bytes word ***/
   if ((ctx->data_alignement.unaligned_data_index != 0) && (size_in_bytes >= (4-ctx->data_alignement.unaligned_data_index)))
   {
      while(ctx->data_alignement.unaligned_data_index < 4)
      {
         unaligned_data[ctx->data_alignement.unaligned_data_index] = *data;
         data++;
         size_in_bytes--;
         ctx->data_alignement.unaligned_data_index++;
      }
      ctx->data_alignement.unaligned_data_index = 0;
      word = *((uint32_t *)ctx->data_alignement.unaligned_data);
   }
   /*** Remaining bytes but can not complete a word of 4 bytes ***/
   else if (size_in_bytes < 4)
   {
      /* Not enough remaining data to make 4 bytes word, we'll just store it */
      while(size_in_bytes > 0)
      {
         unaligned_data[ctx->data_alignement.unaligned_data_index] = *data;
         data++;
         size_in_bytes--;
         ctx->data_alignement.unaligned_data_index++;
      }

      return 1; /* no more data, exit the main while loop */
   }
   /* Is the data buffer aligned on 4 bytes ? */
   else if ((((unsigned int)data) & 3) == 0)
   {
      word = *data32;
      data += 4;
      size_in_bytes -= 4;
   }
   else
   {
      word = data[0]|(data[1]<<8)|(data[2]<<16)|(data[3]<<24);
      data += 4;
      size_in_bytes -= 4;
   }
   data32+=(((uint32_t)data - (uint32_t)data32)/4);

   return 0;
}

//------------------------------------------------------------------------------------------
static int check_old_log_stream(log_check_ctx_t *ctx, uint32_t * data32, int size_in_bytes)
{
     uint32_t word;
     uint8_t * data = (uint8_t *) data32;

     while(size_in_bytes > 0)
     {
         if (getAlignedWord(ctx, word, size_in_bytes, data, data32))
         {
            break;
         }

         logging_checker_out_f_print("%d 0x%012llx G[0x%012llx] 0x%08x - ", ctx->current_proc_id, ctx->old_logging.message_info.address*4, ctx->global_address*4, word);
         /*
            HEADER, 32bit words
            [0] 0x807f807f   <---- Sync word
            [1] timestamp
            [2] ProcId(1) | spare(2) | ID(13) | messageLen(16)
         */

         switch(ctx->step)
         {
            case LOG_EXPECT_MESSAGE_HEADER_1:
            {
               logging_checker_out_f_print("LOG_EXPECT_MESSAGE_HEADER_1\n");
               if (ctx->old_logging.started)
               {
                  if (word != 0x807f807f) /* OSL_SYNC_CHUNK_WORD */
                  {
#ifdef OLD_LOGGING_DO_NOT_ASSERT_ON_SYNC_ERROR
                     ctx->old_logging.started = 0;
                     ctx->old_logging.sync_error++;
#else
                     DEV_FAIL("LOG_EXPECT_MESSAGE_HEADER_1 expected sync word");
#endif
                  }
               }
               if (word == 0x807f807f)
               {
                  ctx->old_logging.started = 1;
                  ctx->step = LOG_EXPECT_MESSAGE_HEADER_2;
               }
            }
            break;
            case LOG_EXPECT_MESSAGE_HEADER_2:
            {
               logging_checker_out_f_print("LOG_EXPECT_MESSAGE_HEADER_2 time32=0x%08x\n", word);
               /* timestamp */
               ctx->old_logging.message_info.timestamp = word;
               ctx->step = LOG_EXPECT_MESSAGE_HEADER_3;
            }
            break;
            case LOG_EXPECT_MESSAGE_HEADER_3:
            {
               /* ProcId(1) | spare(2) | ID(13) | messageLen(16) */
               uint32_t ID, nb_words, proc;

               ID = (word >> 16) & 0x1FFF;
               nb_words = word & 0xFFFF;
               proc = ((word & (1 << 31)) != 0 );

               ctx->current_proc_id = proc;
               ctx->old_logging.message_info.ID = ID;
               ctx->old_logging.message_info.nb_words = nb_words;
               ctx->old_logging.message_info.address = ctx->global_address;

               logging_checker_out_f_print("LOG_EXPECT_MESSAGE_HEADER_3 proc=%d ID=%d nb_words=%d\n", proc, ID, nb_words);
               if (nb_words > 3)
               {
                  ctx->step = LOG_EXPECT_DATA;
               }
               else
               {
                  ctx->step = LOG_EXPECT_MESSAGE_HEADER_1;
               }
               ctx->stats.nb_messages[ctx->current_proc_id]++;

               if (ID == OS_LOG_FLOW_CONTROL)
               {
                  ctx->old_logging.message_lost_error++;
               }
            }
            break;
            case LOG_EXPECT_DATA:
            {
               long long remaining_message;
               long long message_data_index;

               remaining_message = ctx->old_logging.message_info.address + ctx->old_logging.message_info.nb_words - 3 - ctx->global_address;
               message_data_index = ctx->global_address - ctx->old_logging.message_info.address - 1;
               if (remaining_message == 0)
               {
                  ctx->step = LOG_EXPECT_MESSAGE_HEADER_1;
               }
               else
               {
                  ctx->step = LOG_EXPECT_DATA;
               }

               logging_checker_out_f_print("LOG_EXPECT_DATA remainMessage=%lld\n", remaining_message);
               if (remaining_message < 0)
               {
                  DEV_FAIL("negative remaining bytes");
               }
            }
            break;

            default:
               ctx->step = LOG_EXPECT_MESSAGE_HEADER_1;
               break;
         }

         ctx->global_address++;
         ctx->stats.nb_words[ctx->current_proc_id]++;
     }

     return 0;
}

//------------------------------------------------------------------------------------------
static int check_log_stream(log_check_ctx_t *ctx, uint32_t * data32, int size_in_bytes)
{
     uint32_t word;
     uint64   current_timestamp = 0;
     uint8_t * data = (uint8_t *) data32;

     while(size_in_bytes > 0)
     {
         if (getAlignedWord(ctx, word, size_in_bytes, data, data32))
         {
            break;
         }

         logging_checker_out_f_print("%d 0x%012llx G[0x%012llx] 0x%08x - ", ctx->current_proc_id, ctx->address[ctx->current_proc_id]*4, ctx->global_address*4, word);

         switch(ctx->step)
         {
            case LOG_EXPECT_CHUNK_HEADER_1:
            {
               logging_checker_out_f_print("LOG_EXPECT_CHUNK_HEADER_1\n");

               if (ctx->have_last_chunk_info[ctx->current_proc_id])
               {
                  if (word != 0x28c63fd7) /* OSL_SYNC_CHUNK_WORD */
                  {
                     DEV_FAIL("LOG_EXPECT_CHUNK_HEADER_1 expected sync word");
                  }
               }
               else
               {
                  ctx->stats.nb_words[ctx->current_proc_id] = 0;
               }

               if (word == 0x28c63fd7) /* OSL_SYNC_CHUNK_WORD */
               {
                  ctx->step = LOG_EXPECT_CHUNK_HEADER_2;
               }
               if (word == 0x807f807f)
               {
                  //DEV_FAIL("Old logging format is not supported");
                  logging_checker_out_f_print("*** Old logging format detected ***\n");
                  ctx->old_logging_format = 1;
                  ctx->step = LOG_EXPECT_MESSAGE_HEADER_1;
                  ctx->old_logging.started = 0;
                  ctx->stats.nb_words[0] = 0;
                  return check_old_log_stream(ctx, (uint32_t*)((uint32_t)data32-4), size_in_bytes+4);  // because we want to reparse this word again in the old checker
               }
            }
            break;
            case LOG_EXPECT_CHUNK_HEADER_2:
            {
               logging_checker_out_f_print("LOG_EXPECT_CHUNK_HEADER_2\n");

               if (word != 0) /* reserved */
               {
                  DEV_FAIL("LOG_EXPECT_CHUNK_HEADER_2 expected 0");
               }
               else
               {
                  ctx->step = LOG_EXPECT_CHUNK_HEADER_3;
               }
            }
            break;
            case LOG_EXPECT_CHUNK_HEADER_3:
            {
               uint32_t proc_id, nb_proc, seq_num, nb_words;

               proc_id = (word >> 30) & 3;
               nb_proc = (word >> 28) & 3;
               seq_num = (word >> 16) & 0x0FFF;
               nb_words = word & 0xFFFF;

               logging_checker_out_f_print("LOG_EXPECT_CHUNK_HEADER_3 procId=%d nb_proc=%d seq_num=%d nb_words=%d\n", proc_id, nb_proc, seq_num, nb_words);

               if (ctx->have_last_chunk_info[proc_id])
               {
                  if (ctx->chunk_info[proc_id].nb_proc != nb_proc)
                  {
                     DEV_FAIL("LOG_EXPECT_CHUNK_HEADER_3 nb proc changed");
                  }

                  if (ctx->chunk_info[ctx->current_proc_id].seq_num == 4095)
                  {
                     if (seq_num != 0)
                     {
                        DEV_FAIL("LOG_EXPECT_CHUNK_HEADER_3 seq num should wrap");
                     }
                  }
                  else if ((ctx->chunk_info[ctx->current_proc_id].seq_num+1) != seq_num)
                  {
                     DEV_FAIL("LOG_EXPECT_CHUNK_HEADER_3 seq num does not follow");
                  }
               }

               if (nb_words == 0)
               {
                  DEV_FAIL("LOG_EXPECT_CHUNK_HEADER_3 no data, is it normal?");
               }

               if (ctx->have_last_message_info[ctx->current_proc_id])
               {
                  if (ctx->message_info[ctx->current_proc_id].next_step == LOG_EXPECT_DATA)
                  {
                     logging_checker_out_f_print("Message increment address=0x%12llx procId=%d\n", ctx->message_info[ctx->current_proc_id].address, ctx->current_proc_id);
                     ctx->message_info[ctx->current_proc_id].address += 3;
                  }
               }

               ctx->address[ctx->current_proc_id] -= 2;
               ctx->stats.nb_words[ctx->current_proc_id] -= 2;
               ctx->current_proc_id = proc_id;
               ctx->address[ctx->current_proc_id] += 2;
               ctx->stats.nb_words[ctx->current_proc_id] += 2;

               ctx->chunk_info[ctx->current_proc_id].nb_proc  = nb_proc;
               ctx->chunk_info[ctx->current_proc_id].seq_num  = seq_num;
               ctx->chunk_info[ctx->current_proc_id].nb_words = nb_words;
               ctx->chunk_info[ctx->current_proc_id].address  = ctx->address[ctx->current_proc_id];
               ctx->have_last_chunk_info[ctx->current_proc_id] = 1;

               ctx->stats.nb_chunks[ctx->current_proc_id]++;

               if (nb_words == 3)
               {
                  ctx->step = LOG_EXPECT_CHUNK_HEADER_1;
               }
               else if ((ctx->have_last_message_info[ctx->current_proc_id]) && (ctx->message_info[ctx->current_proc_id].next_step != LOG_EXPECT_CHUNK_HEADER_1))
               {
                  ctx->step = ctx->message_info[ctx->current_proc_id].next_step;
               }
               else
               {
                  ctx->step = LOG_EXPECT_MESSAGE_HEADER_1;
               }
            }
            break;

            case LOG_EXPECT_MESSAGE_HEADER_1:
            {
               logging_checker_out_f_print("LOG_EXPECT_MESSAGE_HEADER_1 remainChunk=%lld\n", ctx->chunk_info[ctx->current_proc_id].address + ctx->chunk_info[ctx->current_proc_id].nb_words - 3 - ctx->address[ctx->current_proc_id]);

               if ((word&0x00FFFFFF) != sync_words[ctx->current_proc_id])
               {
                  if (ctx->have_last_message_info[ctx->current_proc_id])
                  {
                     DEV_FAIL("LOG_EXPECT_MESSAGE_HEADER_1 expected sync word");
                  }
                  else if ((ctx->chunk_info[ctx->current_proc_id].address + ctx->chunk_info[ctx->current_proc_id].nb_words - 3)  == ctx->address[ctx->current_proc_id])
                  {
                     ctx->step = LOG_EXPECT_CHUNK_HEADER_1;
                  }
               }
               else
               {
                  current_timestamp = (word & 0x00000000FF000000ULL)<<8;

                  ctx->step = LOG_EXPECT_MESSAGE_HEADER_2;
                  if ((ctx->chunk_info[ctx->current_proc_id].address + ctx->chunk_info[ctx->current_proc_id].nb_words - 3)  == ctx->address[ctx->current_proc_id])
                  {
                     ctx->message_info[ctx->current_proc_id].next_step = LOG_EXPECT_MESSAGE_HEADER_2;
                     ctx->step = LOG_EXPECT_CHUNK_HEADER_1;
                  }

                  ctx->message_info[ctx->current_proc_id].seq_num = 0; /* Reset the sequence number. 0 means not set */

                  ctx->stats.nb_messages[ctx->current_proc_id]++;
               }
            }
            break;
            case LOG_EXPECT_MESSAGE_HEADER_2:
            {
               current_timestamp |= word;
               logging_checker_out_f_print("LOG_EXPECT_MESSAGE_HEADER_2 time32=0x%08x remainChunk=%lld\n", word, ctx->chunk_info[ctx->current_proc_id].address + ctx->chunk_info[ctx->current_proc_id].nb_words - 3 - ctx->address[ctx->current_proc_id]);
               ctx->step = LOG_EXPECT_MESSAGE_HEADER_3;
               if ((ctx->chunk_info[ctx->current_proc_id].address + ctx->chunk_info[ctx->current_proc_id].nb_words - 3)  == ctx->address[ctx->current_proc_id])
               {
                  ctx->message_info[ctx->current_proc_id].next_step = LOG_EXPECT_MESSAGE_HEADER_3;
                  ctx->step = LOG_EXPECT_CHUNK_HEADER_1;
               }
            }
            break;
            case LOG_EXPECT_MESSAGE_HEADER_3:
            {
               uint32_t ID, nb_words;

               ID = (word >> 16) & 0x3FFF;
               nb_words = word & 0xFFFF;

               if (ctx->have_last_message_info[ctx->current_proc_id])
               {
                  /* TO DO : Add checks */
               }

               logging_checker_out_f_print("LOG_EXPECT_MESSAGE_HEADER_3 ID=%d nb_words=%d remainChunk=%lld\n", ID, nb_words, ctx->chunk_info[ctx->current_proc_id].address + ctx->chunk_info[ctx->current_proc_id].nb_words - 3 - ctx->address[ctx->current_proc_id]);

               ctx->message_info[ctx->current_proc_id].timestamp = current_timestamp;
               ctx->message_info[ctx->current_proc_id].ID = ID;
               ctx->message_info[ctx->current_proc_id].nb_words = nb_words;
               ctx->message_info[ctx->current_proc_id].address = ctx->address[ctx->current_proc_id];
               ctx->have_last_message_info[ctx->current_proc_id] = 1;

               if (ID == OS_LOG_FLOW_CONTROL)
               {
                  ctx->message_lost_error[ctx->current_proc_id]++;
               }

               if (nb_words > 3)
               {
                  ctx->step = LOG_EXPECT_DATA;
               }
               else
               {
                  ctx->step = LOG_EXPECT_MESSAGE_HEADER_1;
               }
               if ((ctx->chunk_info[ctx->current_proc_id].address + ctx->chunk_info[ctx->current_proc_id].nb_words - 3)  == ctx->address[ctx->current_proc_id])
               {
                  if (nb_words == 3)
                  {
                     ctx->message_info[ctx->current_proc_id].next_step = LOG_EXPECT_MESSAGE_HEADER_1;
                  }
                  else
                  {
                     ctx->message_info[ctx->current_proc_id].next_step = LOG_EXPECT_DATA;
                  }
                  ctx->step = LOG_EXPECT_CHUNK_HEADER_1;
               }
            }
            break;

            case LOG_EXPECT_DATA:
            {
               long long remaining_chunk;
               long long remaining_message;
               long long message_data_index;

               remaining_chunk   = ctx->chunk_info[ctx->current_proc_id].address + ctx->chunk_info[ctx->current_proc_id].nb_words - 3 - ctx->address[ctx->current_proc_id];
               remaining_message = ctx->message_info[ctx->current_proc_id].address + ctx->message_info[ctx->current_proc_id].nb_words - 3 - ctx->address[ctx->current_proc_id];
               message_data_index = ctx->address[ctx->current_proc_id] - ctx->message_info[ctx->current_proc_id].address - 1;

               if (remaining_chunk == 0)
               {
                  ctx->step = LOG_EXPECT_CHUNK_HEADER_1;
                  ctx->message_info[ctx->current_proc_id].next_step = LOG_EXPECT_CHUNK_HEADER_1;
                  if (remaining_message > 0)
                  {
                     ctx->message_info[ctx->current_proc_id].next_step = LOG_EXPECT_DATA;
                  }
               }
               else if (remaining_message == 0)
               {
                  ctx->step = LOG_EXPECT_MESSAGE_HEADER_1;
               }
               else
               {
                  ctx->step = LOG_EXPECT_DATA;
               }

               /* Checks message content consistency */
               if (ctx->init_args.logging_data_is_debug == 1)
               {
                  if (message_data_index == 0)
                  {
                     /* write_idx */
                  }
                  else if (((message_data_index/2)*2) == message_data_index)
                  {
                     /* infos: dxp and byte number */
                  }
                  else
                  {
                     /* Sequence number */
                     if (ctx->message_info[ctx->current_proc_id].seq_num == 0)
                     {
                        ctx->message_info[ctx->current_proc_id].seq_num = word;
                     }
                     else
                     {
                        if (ctx->message_info[ctx->current_proc_id].seq_num != word)
                        {
                           DEV_FAIL("MESSAGE_DATA_SEQ_NUM bad seq num in data");
                        }
                     }
                  }
               }

               logging_checker_out_f_print("LOG_EXPECT_DATA remainChunk=%lld remainMessage=%lld\n", remaining_chunk, remaining_message);

               if ((remaining_chunk < 0) || (remaining_message < 0))
               {
                  DEV_FAIL("negative remaining bytes");
               }
            }
            break;

            default:
               ctx->step = LOG_EXPECT_CHUNK_HEADER_1;
               break;
         }

         ctx->address[ctx->current_proc_id]++;
         ctx->global_address++;
         ctx->stats.nb_words[ctx->current_proc_id]++;
     }
    return 0;
}

