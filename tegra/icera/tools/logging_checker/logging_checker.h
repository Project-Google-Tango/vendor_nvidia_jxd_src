/*************************************************************************************************
 *
 * Copyright (c) 2013, NVIDIA CORPORATION.  All rights reserved.
 *
 ************************************************************************************************/

void logging_checker_init(char * out_filename, int output_only_the_last_x_mbytes, int logging_data_is_debug, long long total_size_to_proceed);
int  logging_checker_process(unsigned int * buffer, int size);
void logging_checker_finish(void);

