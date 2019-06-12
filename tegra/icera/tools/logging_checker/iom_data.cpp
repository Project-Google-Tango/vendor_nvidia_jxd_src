/*************************************************************************************************
 *
 * Copyright (c) 2013, NVIDIA CORPORATION.  All rights reserved.
 *
 ************************************************************************************************/

#include "iom_data.h"
#include "logs.h"
#include <sys/stat.h>
#include <string.h>

//----------------------------------------------------------------------
IomData::IomData(unsigned int buffer_size): buffer(0),
                                            buffer_size(0),
                                            total_nb_read(0),
                                            percent(0),
                                            file_size(0),
                                            file_handle(0),
                                            socket_obj(0)
{
   this->buffer_size = buffer_size;
   buffer = new unsigned int[(buffer_size+3)/4]; /* assuming sizeof(unsigned int) == 4 */
   if (!buffer)
   {
      ALOGE("Could not allocate buffer");
      return;
   }
   memset(buffer, 0, buffer_size);
}

//----------------------------------------------------------------------
IomData::~IomData()
{
   if (buffer)
   {
      delete [] buffer;
      buffer = NULL;
   }
   if (socket_obj)
   {
      delete socket_obj;
      socket_obj = NULL;
   }
   if (file_handle)
   {
      fclose(file_handle);
      file_handle = NULL;
   }
}
//----------------------------------------------------------------------
int IomData::SetSourceIsSocket(int port_number)
{
   if (file_handle)
      return 0;

   socket_obj = new Socket;
   if (!socket_obj)
      return 0;

   socket_obj->OpenClient(port_number, buffer_size, buffer_size);
   return socket_obj->IsOpen();
}

//----------------------------------------------------------------------
int IomData::SetSourceIsFile(char * filename)
{
   if (socket_obj)
      return 0;

   file_handle = fopen(filename, "rb");
   if (!file_handle)
      return 0;

   struct stat in_st;
   if (stat(filename, &in_st) != -1)
   {
   file_size = (long long)in_st.st_size & 0xFFFFFFFF;
   }
   else
   {
      file_size = 0;
   }

   return (file_size > 0);
}

//----------------------------------------------------------------------
long long IomData::GetFileSize()
{
   return file_size;
}

//----------------------------------------------------------------------
bool IomData::HasData()
{
   if (!buffer)
      return 0;

   if (socket_obj)   /* SOCKET */
   {
      return socket_obj->IsOpen();
   }
   else if (file_handle)       /* FILE */
   {
      if (!feof(file_handle))
      {
         return true;
      }
   }
   return false;
}

//----------------------------------------------------------------------
int IomData::Read()
{
   if (!buffer)
      return 0;

   int nb_read = 0;
   if (socket_obj && socket_obj->IsOpen())  /* SOCKET */
   {
      nb_read = socket_obj->Read((char*)buffer, buffer_size);
   }
   else if (file_handle)      /* FILE */
   {
      nb_read = fread(buffer, 1, buffer_size, file_handle);
   }
   total_nb_read += nb_read;
   return nb_read;
}

//----------------------------------------------------------------------
int IomData::getProgressPercent()
{
   if (socket_obj && socket_obj->IsOpen())  /* SOCKET */
   {
      percent = 0; /* there is no percentage as it is a continuous stream */
   }
   else if (file_handle)      /* FILE */
   {
      if (file_size > 100)
      {
         percent = (int)((total_nb_read)/(file_size/100));
      }
      else
      {
         percent = (int)((total_nb_read)*100/file_size);
      }
   }
   return percent;
}

//----------------------------------------------------------------------
unsigned int * IomData::GetBuffer()
{
   return buffer;
}

//----------------------------------------------------------------------
