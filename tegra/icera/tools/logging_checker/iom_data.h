/*************************************************************************************************
 *
 * Copyright (c) 2013, NVIDIA CORPORATION.  All rights reserved.
 *
 ************************************************************************************************/
#include "socket.h"

class IomData
{
private:
   unsigned int * buffer;
   unsigned int   buffer_size;
   long long      total_nb_read;
   int            percent;
   long long      file_size;

   FILE   *       file_handle;
   Socket *       socket_obj;

public:
   IomData(unsigned int buffer_size);
   ~IomData();

   int SetSourceIsSocket(int port_number);
   int SetSourceIsFile(char * filename);
   long long GetFileSize();

   bool HasData();
   int Read();
   int getProgressPercent();
   unsigned int * GetBuffer();
   long long GetTotalNbRead() { return total_nb_read; }

};
