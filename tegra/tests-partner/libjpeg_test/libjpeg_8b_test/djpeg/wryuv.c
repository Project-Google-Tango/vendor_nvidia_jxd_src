/*
 * wryuv.c
 *
 * Copyright (c) 2012, NVIDIA CORPORATION.  All rights reserved.
 */
#include "cdjpeg.h"		/* Common decls for cjpeg/djpeg applications */

#ifdef YUV_SUPPORTED


/* Private version of data destination object */
typedef struct {
  struct djpeg_dest_struct pub;	/* public fields */
} yuv_dest_struct;

typedef yuv_dest_struct * yuv_dest_ptr;


/*
 * Write some pixel data.
 * In this module rows_supplied will always be 1.
 *
 * put_pixel_rows handles the "normal" 8-bit case where the decompressor
 * output buffer is physically the same as the fwrite buffer.
 */

METHODDEF(void)
put_pixel_rows (j_decompress_ptr cinfo, djpeg_dest_ptr dinfo,
		JDIMENSION rows_supplied)
{
    yuv_dest_ptr dest = (yuv_dest_ptr) dinfo;

    unsigned int y;
    unsigned char *pntr;


    /* Write out Y */
    pntr = (unsigned char *)cinfo->jpegTegraMgr->buff[0];
    for(y = 0; y < rows_supplied; y++) {
        fwrite(pntr, 1, cinfo->output_width, dest->pub.output_file);
        pntr += cinfo->jpegTegraMgr->pitch[0];
    }

    unsigned int type = cinfo->jpegTegraMgr->mcu_type;
    switch(type) {
    case 0: /* YUV 420 */
        /* Write out Cb */
        pntr = (unsigned char *)cinfo->jpegTegraMgr->buff[2];
        for(y = 0; y < (rows_supplied>>1); y++) {
            fwrite(pntr, 1, cinfo->output_width>>1, dest->pub.output_file);
            pntr += cinfo->jpegTegraMgr->pitch[2];
        }

        /* Write out Cr */
        pntr = (unsigned char *)cinfo->jpegTegraMgr->buff[1];
        for(y = 0; y < (rows_supplied>>1); y++) {
            fwrite(pntr, 1, cinfo->output_width>>1, dest->pub.output_file);
            pntr += cinfo->jpegTegraMgr->pitch[1];
        }
        break;
    case 1: /* YUV 422 */
        /* Write out Cb */
        pntr = (unsigned char *)cinfo->jpegTegraMgr->buff[2];
        for(y = 0; y < rows_supplied; y++) {
            fwrite(pntr, 1, cinfo->output_width>>1, dest->pub.output_file);
            pntr += cinfo->jpegTegraMgr->pitch[2];
        }

        /* Write out Cr */
        pntr = (unsigned char *)cinfo->jpegTegraMgr->buff[1];
        for(y = 0; y < rows_supplied; y++) {
            fwrite(pntr, 1, cinfo->output_width>>1, dest->pub.output_file);
            pntr += cinfo->jpegTegraMgr->pitch[1];
        }
        break;
    case 2: /* YUV 422 Transposed */
        /* Write out Cb */
        pntr = (unsigned char *)cinfo->jpegTegraMgr->buff[2];
        for(y = 0; y < (rows_supplied>>1); y++) {
            fwrite(pntr, 1, cinfo->output_width, dest->pub.output_file);
            pntr += cinfo->jpegTegraMgr->pitch[2];
        }

        /* Write out Cr */
        pntr = (unsigned char *)cinfo->jpegTegraMgr->buff[1];
        for(y = 0; y < (rows_supplied>>1); y++) {
            fwrite(pntr, 1, cinfo->output_width, dest->pub.output_file);
            pntr += cinfo->jpegTegraMgr->pitch[1];
        }
        break;
    case 3: /* YUV 444 */
        /* Write out Cb */
        pntr = (unsigned char *)cinfo->jpegTegraMgr->buff[2];
        for(y = 0; y < (rows_supplied); y++) {
            fwrite(pntr, 1, cinfo->output_width, dest->pub.output_file);
            pntr += cinfo->jpegTegraMgr->pitch[2];
        }

        /* Write out Cr */
        pntr = (unsigned char *)cinfo->jpegTegraMgr->buff[1];
        for(y = 0; y < (rows_supplied); y++) {
            fwrite(pntr, 1, cinfo->output_width, dest->pub.output_file);
            pntr += cinfo->jpegTegraMgr->pitch[1];
        }
        break;
    case 4: /* YUV 400 */
        // do nothing.
        break;

    default: // Not supported
        ERREXIT(cinfo, JERR_CONVERSION_NOTIMPL);
        return;
    }
}


/*
 * Startup: Dummy function since YUV doesnt have any header
 */
METHODDEF(void)
start_output_yuv (j_decompress_ptr cinfo, djpeg_dest_ptr dinfo)
{
}


/*
 * Finish up at the end of the file.
 */
METHODDEF(void)
finish_output_yuv (j_decompress_ptr cinfo, djpeg_dest_ptr dinfo)
{
  /* Make sure we wrote the output file OK */
  fflush(dinfo->output_file);
  if (ferror(dinfo->output_file))
    ERREXIT(cinfo, JERR_FILE_WRITE);
}


GLOBAL(djpeg_dest_ptr)
jinit_write_yuv (j_decompress_ptr cinfo)
{
  yuv_dest_ptr dest;

  /* Create module interface object, fill in method pointers */
  dest = (yuv_dest_ptr)
      (*cinfo->mem->alloc_small) ((j_common_ptr) cinfo, JPOOL_IMAGE,
				  SIZEOF(yuv_dest_struct));
  dest->pub.start_output = start_output_yuv;
  dest->pub.finish_output = finish_output_yuv;
  dest->pub.put_pixel_rows = put_pixel_rows;

  return (djpeg_dest_ptr) dest;
}

#endif /* YUV_SUPPORTED */

