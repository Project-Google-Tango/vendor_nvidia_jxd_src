/*
 * rdy4m.c
 *
 * Copyright (c) 2012, NVIDIA CORPORATION.  All rights reserved.
 */

#include "cdjpeg.h"		/* Common decls for cjpeg/djpeg applications */

#ifdef YUV_SUPPORTED


/* Private version of data source object */
typedef struct {
  struct cjpeg_source_struct pub; /* public fields */
} y4m_source_struct;

typedef y4m_source_struct * y4m_source_ptr;



METHODDEF(JDIMENSION)
get_raw_row (j_compress_ptr cinfo, cjpeg_source_ptr sinfo)
{
    y4m_source_ptr source = (y4m_source_ptr) sinfo;
    unsigned int y;
    unsigned char *pntr;

#ifdef TEGRA_ACCELERATE
  if (cinfo->tegra_acceleration == TRUE)
  {
    /* Write out Y */
    pntr = (unsigned char *)cinfo->jpegTegraMgr->buff[0];
    for(y = 0; y < cinfo->image_height; y++) {
        fread(pntr, 1, cinfo->image_width, source->pub.input_file);
        pntr += cinfo->jpegTegraMgr->pitch[0];
    }

    /* Write out Cb */
    pntr = (unsigned char *)cinfo->jpegTegraMgr->buff[2];
    for(y = 0; y < (cinfo->image_height>>1); y++) {
        fread(pntr, 1, cinfo->image_width>>1, source->pub.input_file);
        pntr += cinfo->jpegTegraMgr->pitch[2];
    }

    /* Write out Cr */
    pntr = (unsigned char *)cinfo->jpegTegraMgr->buff[1];
    for(y = 0; y < (cinfo->image_height>>1); y++) {
        fread(pntr, 1, cinfo->image_width>>1, source->pub.input_file);
        pntr += cinfo->jpegTegraMgr->pitch[1];
    }
  }
#endif
  return cinfo->image_height;
}


LOCAL(unsigned int)
y4m_read_integer (j_compress_ptr cinfo, FILE * infile)
{
  register int ch;
  register unsigned int val;

  ch = getc(infile);
  if (ch < '0' || ch > '9')
    ERREXIT(cinfo, JERR_Y4M_NONNUMERIC);

  val = ch - '0';
  while ((ch = getc(infile)) >= '0' && ch <= '9') {
    val *= 10;
    val += ch - '0';
  }
  ungetc(ch, infile);

  return val;
}


/*
 * Read the file header; return image size and component count.
 */

METHODDEF(void)
start_input_y4m (j_compress_ptr cinfo, cjpeg_source_ptr sinfo)
{
    y4m_source_ptr source = (y4m_source_ptr) sinfo;

    int c;
    char mainHdr[9];
    char frmHdr[5];

    cinfo->data_precision = BITS_IN_JSAMPLE; /* we always rescale data to this */
    cinfo->input_components = 3;
    cinfo->in_color_space = JCS_YCbCr;
    source->pub.get_pixel_rows = get_raw_row;

    /* Main header */
    fread(mainHdr, 1, 9, source->pub.input_file);
    if(strncmp(mainHdr, "YUV4MPEG2", 9))
        ERREXIT(cinfo, JERR_Y4M_INVALIDHDR);

    do {
        c = getc(source->pub.input_file);
        if(c == ' ') {
            c = getc(source->pub.input_file);
            switch(c)
            {
                case 'W':
                    cinfo->image_width = y4m_read_integer (cinfo,
                            source->pub.input_file);
                    break;
                case 'H':
                    cinfo->image_height = y4m_read_integer (cinfo,
                            source->pub.input_file);
                    break;
                default:
                    break;
            }
        }

    } while(c != '\n' && c != EOF);


    /* File header */
    fread(frmHdr, 1, 5, source->pub.input_file);
    if(strncmp(frmHdr, "FRAME", 5))
        ERREXIT(cinfo, JERR_Y4M_INVALIDHDR);

    do {
        c = getc(source->pub.input_file);
        if(c == ' ') {
            c = getc(source->pub.input_file);
            switch(c)
            {
                case 'W':
                    cinfo->image_width = y4m_read_integer (cinfo,
                            source->pub.input_file);
                    break;
                case 'H':
                    cinfo->image_height = y4m_read_integer (cinfo,
                            source->pub.input_file);
                    break;
                default:
                    break;
            }
        }

    } while(c != '\n' && c != EOF);

}

/*
 * Finish up at the end of the file.
 */

METHODDEF(void)
finish_input_y4m (j_compress_ptr cinfo, cjpeg_source_ptr sinfo)
{
  /* no work */
}


/*
 * The module selection routine for Y4M format input.
 */

GLOBAL(cjpeg_source_ptr)
jinit_read_y4m (j_compress_ptr cinfo)
{
  y4m_source_ptr source;

  /* Create module interface object */
  source = (y4m_source_ptr)
      (*cinfo->mem->alloc_small) ((j_common_ptr) cinfo, JPOOL_IMAGE,
				  SIZEOF(y4m_source_struct));
  /* Fill in method ptrs, except get_pixel_rows which start_input sets */
  source->pub.start_input = start_input_y4m;
  source->pub.finish_input = finish_input_y4m;

  return (cjpeg_source_ptr) source;
}

#endif /* YUV_SUPPORTED */
