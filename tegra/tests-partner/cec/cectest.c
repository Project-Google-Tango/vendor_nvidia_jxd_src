/*
# Copyright (c) 2013, NVIDIA CORPORATION.  All rights reserved.
#
# NVIDIA CORPORATION and its licensors retain all intellectual property
# and proprietary rights in and to this software, related documentation
# and any modifications thereto.  Any use, reproduction, disclosure or
# distribution of this software and related documentation without an express
# license agreement from NVIDIA CORPORATION is strictly prohibited.
*/

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <linux/input.h>

/* L_KEY is using linux keys instead of androids "input" command.*/
#ifdef USE_L_KEY
#undef USE_L_KEY
#endif
/*#define USE_L_KEY 1*/

#define EOM 8
#define MODE 12
#define START_BIT 16
#define RETRY 17
#define DIRECT 0
#define BROAD 1


#define GIVE_DEVICE_VENDOR_ID 0x8C
#define GIVE_PHYSICAL_ADDRESS 0x83
#define GIVE_OSD_NAME 0x46
#define IMAGE_VIEW_ON 0x04
#define ACTIVE_SOURCE 0x82
#define MENU_STATUS 0x8E
#define USER_CONTROL_PRESSED 0x44
#define GIVE_DEVICE_POWER_STATUS 0x8F
#define STAND_BY 0x36


int file,ev;
unsigned long data;
unsigned char eom=0;
unsigned char mode=DIRECT;
unsigned char start_bit=0;
unsigned char retry=0;
struct input_event event;

void send_data(int data,unsigned char eom, unsigned char mode, unsigned char start_bit, unsigned char retry)
{
  unsigned long mydata=data | eom<<EOM |  mode<<MODE | start_bit<<START_BIT | retry<<RETRY;
  write(file, &mydata, 4);
}

void active_source (void)
{
  printf("%s \n", __func__);
  data=0x4f;eom=0;mode=BROAD;start_bit=1;retry=0;send_data(data,eom,mode,start_bit,retry);
  data=0x82;eom=0;mode=BROAD;start_bit=1;retry=0;send_data(data,eom,mode,start_bit,retry);
  data=0x10;eom=0;mode=BROAD;start_bit=1;retry=0;send_data(data,eom,mode,start_bit,retry);
  data=0x00;eom=1;mode=BROAD;start_bit=1;retry=0;send_data(data,eom,mode,start_bit,retry);
}

void image_view_on(void)
{
  printf("%s \n", __func__);
  data=0x40;eom=0;mode=DIRECT;start_bit=1;retry=0;send_data(data,eom,mode,start_bit,retry);
  data=0x04;eom=1;mode=DIRECT;start_bit=1;retry=0;send_data(data,eom,mode,start_bit,retry);
}

void text_view_on(void)
{
  printf("%s \n", __func__);
  data=0x40;eom=0;mode=DIRECT;start_bit=1;retry=0;send_data(data,eom,mode,start_bit,retry);
  data=0x0D;eom=1;mode=DIRECT;start_bit=1;retry=0;send_data(data,eom,mode,start_bit,retry);
}

void menu_status(void)
{
  printf("%s \n", __func__);
  data=0x40;eom=0;mode=DIRECT;start_bit=1;retry=0;send_data(data,eom,mode,start_bit,retry);
  data=0x8E;eom=0;mode=DIRECT;start_bit=1;retry=0;send_data(data,eom,mode,start_bit,retry);
  data=0x00;eom=1;mode=DIRECT;start_bit=1;retry=0;send_data(data,eom,mode,start_bit,retry);
}

void get_menu_language(void)
{
  printf("%s \n", __func__);
  data=0x40;eom=0;mode=DIRECT;start_bit=1;retry=0;send_data(data,eom,mode,start_bit,retry);
  data=0x91;eom=1;mode=DIRECT;start_bit=1;retry=0;send_data(data,eom,mode,start_bit,retry);
}

void report_power_status(void)
{
  printf("%s \n", __func__);
  data=0x40;eom=0;mode=DIRECT;start_bit=1;retry=0;send_data(data,eom,mode,start_bit,retry);
  data=0x90;eom=0;mode=DIRECT;start_bit=1;retry=0;send_data(data,eom,mode,start_bit,retry);
  data=0x00;eom=1;mode=DIRECT;start_bit=1;retry=0;send_data(data,eom,mode,start_bit,retry);
}

void set_osd_string(void)
{
  printf("%s \n", __func__);
  data=0x4f;eom=0;mode=BROAD;start_bit=1;retry=0;send_data(data,eom,mode,start_bit,retry);
  data=0x64;eom=0;mode=BROAD;start_bit=1;retry=0;send_data(data,eom,mode,start_bit,retry);
  data=0x00;eom=0;mode=BROAD;start_bit=1;retry=0;send_data(data,eom,mode,start_bit,retry);
  data='H';eom=0;mode=BROAD;start_bit=1;retry=0;send_data(data,eom,mode,start_bit,retry);
  data='I';eom=1;mode=BROAD;start_bit=1;retry=0;send_data(data,eom,mode,start_bit,retry);
}

void report_physical_address(void)
{
  printf("%s \n", __func__);
  data=0x4f;eom=0;mode=BROAD;start_bit=1;retry=0;send_data(data,eom,mode,start_bit,retry);
  data=0x84;eom=0;mode=BROAD;start_bit=1;retry=0;send_data(data,eom,mode,start_bit,retry);
  data=0x10;eom=0;mode=BROAD;start_bit=1;retry=0;send_data(data,eom,mode,start_bit,retry);
  data=0x00;eom=0;mode=BROAD;start_bit=1;retry=0;send_data(data,eom,mode,start_bit,retry);
  data=0x04;eom=1;mode=BROAD;start_bit=1;retry=0;send_data(data,eom,mode,start_bit,retry);
}

void device_vendor_id(void)
{
  printf("%s \n", __func__);
  data=0x40;eom=0;mode=DIRECT;start_bit=1;retry=0;send_data(data,eom,mode,start_bit,retry);
  data=0x00;eom=0;mode=DIRECT;start_bit=1;retry=0;send_data(data,eom,mode,start_bit,retry);
  data=0x8C;eom=0;mode=DIRECT;start_bit=1;retry=0;send_data(data,eom,mode,start_bit,retry);
  data=0x00;eom=1;mode=DIRECT;start_bit=1;retry=0;send_data(data,eom,mode,start_bit,retry);
}

void ping_message(void)
{
  printf("%s \n", __func__);
  data=0x40;eom=1;mode=DIRECT;start_bit=1;retry=0;send_data(data,eom,mode,start_bit,retry);
}

void set_osd_name(void)
{
  printf("%s \n", __func__);
  data=0x40;eom=0;mode=DIRECT;start_bit=1;retry=0;send_data(data,eom,mode,start_bit,retry);
  data=0x47;eom=0;mode=DIRECT;start_bit=1;retry=0;send_data(data,eom,mode,start_bit,retry);
  data='T';eom=0;mode=DIRECT;start_bit=1;retry=0;send_data(data,eom,mode,start_bit,retry);
  data='e';eom=0;mode=DIRECT;start_bit=1;retry=0;send_data(data,eom,mode,start_bit,retry);
  data='g';eom=0;mode=DIRECT;start_bit=1;retry=0;send_data(data,eom,mode,start_bit,retry);
  data='r';eom=0;mode=DIRECT;start_bit=1;retry=0;send_data(data,eom,mode,start_bit,retry);
  data='a';eom=0;mode=DIRECT;start_bit=1;retry=0;send_data(data,eom,mode,start_bit,retry);
  data=' ';eom=0;mode=DIRECT;start_bit=1;retry=0;send_data(data,eom,mode,start_bit,retry);
  data='D';eom=0;mode=DIRECT;start_bit=1;retry=0;send_data(data,eom,mode,start_bit,retry);
  data='e';eom=0;mode=DIRECT;start_bit=1;retry=0;send_data(data,eom,mode,start_bit,retry);
  data='v';eom=0;mode=DIRECT;start_bit=1;retry=0;send_data(data,eom,mode,start_bit,retry);
  data='i';eom=0;mode=DIRECT;start_bit=1;retry=0;send_data(data,eom,mode,start_bit,retry);
  data='c';eom=0;mode=DIRECT;start_bit=1;retry=0;send_data(data,eom,mode,start_bit,retry);
  data='e';eom=1;mode=DIRECT;start_bit=1;retry=0;send_data(data,eom,mode,start_bit,retry);
}

void dump_packet(unsigned short *buffer, int length)
{
  int i;
  for(i=0;i<length;i++)
  {
    if(i%4==0)printf("\n");
    printf("data=0x%02x \t",buffer[i]&0x0ff);
  }
  printf("\n");
}

int main()
{
  int count = 1,i=0, ret_count,state=0;
  unsigned short *my_rx;

  file = open("/dev/tegra_cec",O_RDWR);
/* event0 is GPIO key input, it can chagne from platform to platform. It is only useful if you are using L_KEY. */
  ev = open("/dev/input/event0",O_RDWR);
  my_rx=(unsigned short *)malloc(count*sizeof(my_rx));

  if (my_rx == NULL) {
    printf("memory allocation failed!!!. Exiting");
    exit(0);
  }

  while(1)
  {
    ret_count=read(file, my_rx, count*2);
    for (i=0;i<count;i++)
    {
      if((my_rx[i]&0xff) == GIVE_DEVICE_VENDOR_ID)
        device_vendor_id();
      else if((my_rx[i]&0xff) == GIVE_DEVICE_POWER_STATUS)
      {
        report_power_status();
        state=1;
      }
      else if((my_rx[i]&0xff) == GIVE_PHYSICAL_ADDRESS)
      {
        report_physical_address();
        image_view_on();
        active_source();
        get_menu_language();
      }
      else if((my_rx[i]&0xff) == USER_CONTROL_PRESSED)
      {
        ret_count=read(file, my_rx, count*2);

        printf("user_control_pressed = 0x%x\n",(my_rx[i]&0xff)) ;

        if((my_rx[i]&0xff) == 0x00)
        {
#ifdef USE_L_KEY
          memset(&event, 0, sizeof(event));
          event.type = 1;event.value = 1;event.code = 232;
          write(ev, &event, sizeof(event));

          memset(&event, 0, sizeof(event));
          event.type = 1;event.value = 0;event.code = 232;
          write(ev, &event, sizeof(event));
#else
          system("input keyevent 23");
#endif
        }
        else if((my_rx[i]&0xff) == 0x01)
        {
#ifdef USE_L_KEY
          memset(&event, 0, sizeof(event));
          event.type = 1;event.value = 1;event.code = 103;
          write(ev, &event, sizeof(event));

          memset(&event, 0, sizeof(event));
          event.type = 1;event.value = 0;event.code = 103;
          write(ev, &event, sizeof(event));
#else
          system("input keyevent 19");
#endif
        }
        else if((my_rx[i]&0xff) == 0x02)
        {
#ifdef USE_L_KEY
          memset(&event, 0, sizeof(event));
          event.type = 1;event.value = 1;event.code = 108;
          write(ev, &event, sizeof(event));

          memset(&event, 0, sizeof(event));
          event.type = 1;event.value = 0;event.code = 108;
          write(ev, &event, sizeof(event));
#else
          system("input keyevent 20");
#endif
        }
        else if((my_rx[i]&0xff) == 0x03)
        {
#ifdef USE_L_KEY
          memset(&event, 0, sizeof(event));
          event.type = 1;event.value = 1;event.code = 105;
          write(ev, &event, sizeof(event));

          memset(&event, 0, sizeof(event));
          event.type = 1;event.value = 0;event.code = 105;
          write(ev, &event, sizeof(event));
#else
          system("input keyevent 21");
#endif
        }
        else if((my_rx[i]&0xff) == 0x04)
        {
#ifdef USE_L_KEY
          memset(&event, 0, sizeof(event));
          event.type = 1;event.value = 1;event.code = 106;
          write(ev, &event, sizeof(event));

          memset(&event, 0, sizeof(event));
          event.type = 1;event.value = 0;event.code = 106;
          write(ev, &event, sizeof(event));
#else
          system("input keyevent 22");
#endif
        }
        else if((my_rx[i]&0xff) == 0x44)
        {
#ifdef USE_L_KEY
          memset(&event, 0, sizeof(event));
          event.type = 1;event.value = 1;event.code = 164;
          write(ev, &event, sizeof(event));

          memset(&event, 0, sizeof(event));
          event.type = 1;event.value = 0;event.code = 164;
          write(ev, &event, sizeof(event));
#else
          system("input keyevent 126");
#endif
        }
        else if((my_rx[i]&0xff) == 0x46)
        {
#ifdef USE_L_KEY
          memset(&event, 0, sizeof(event));
          event.type = 1;event.value = 1;event.code = 165;
          write(ev, &event, sizeof(event));

          memset(&event, 0, sizeof(event));
          event.type = 1;event.value = 0;event.code = 165;
          write(ev, &event, sizeof(event));
#else
          system("input keyevent 127");
#endif
        }
        else if((my_rx[i]&0xff) == 0x0d)
        {
#ifdef USE_L_KEY
          memset(&event, 0, sizeof(event));
          event.type = 1;event.value = 1;event.code = 115;
          write(ev, &event, sizeof(event));

          memset(&event, 0, sizeof(event));
          event.type = 1;event.value = 0;event.code = 115;
          write(ev, &event, sizeof(event));
#else
          system("input keyevent 4");
#endif
        }
        else if((my_rx[i]&0xff) == 0x35)
          set_osd_string();
        else if((my_rx[i]&0xff) == 0x09)
        {
#ifdef USE_L_KEY
          memset(&event, 0, sizeof(event));
          event.type = 1;event.value = 1;event.code = 102;
          write(ev, &event, sizeof(event));

          memset(&event, 0, sizeof(event));
          event.type = 1;event.value = 0;event.code = 102;
          write(ev, &event, sizeof(event));
#else
          system("input keyevent 3");
#endif
        }
        else if((my_rx[i]&0xff) == 0x74)
        {
#ifdef USE_L_KEY
          memset(&event, 0, sizeof(event));
          event.type = 1;event.value = 1;event.code = 114;
          write(ev, &event, sizeof(event));

          memset(&event, 0, sizeof(event));
          event.type = 1;event.value = 0;event.code = 114;
          write(ev, &event, sizeof(event));
#else
          system("input keyevent 82");
#endif
        }
        else if((my_rx[i]&0xff) == 0x71)
        {
          text_view_on();
          set_osd_string();
        }
      }
      else if((my_rx[i]&0xff) == GIVE_OSD_NAME)
        set_osd_name();
      else if((my_rx[i]&0xff) == STAND_BY)
      {
        printf("stand_by\n");
        memset(&event, 0, sizeof(event));
        event.type = 1;event.value = 1;event.code = 26;
        write(ev, &event, sizeof(event));

        memset(&event, 1, sizeof(event));
        event.type = 1;event.value = 0;event.code = 26;
        write(ev, &event, sizeof(event));

        system("input keyevent 26");
      }
      if(state==1)
      {
        image_view_on();
        active_source();
        menu_status();
        get_menu_language();
        state=2;
      }
    }
  }
  free (my_rx);
  printf("flushing stream, press enter\n");
  close(file); /*done!*/
  return 0;
}
