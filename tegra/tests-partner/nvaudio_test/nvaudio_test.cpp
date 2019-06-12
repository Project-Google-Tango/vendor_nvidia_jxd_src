/*
* Copyright (c) 2012 NVIDIA Corporation.  All Rights Reserved.
*
* NVIDIA Corporation and its licensors retain all intellectual property and
* proprietary rights in and to this software and related documentation.  Any
* use, reproduction, disclosure or distribution of this software and related
* documentation without an express license agreement from NVIDIA Corporation
* is strictly prohibited.
*/

#include "nvaudio_test.h"

int main (int argc, char *argv[])
{
    int ret = 0;

    if (argc != 3) {
        printf("Usage: nvaudio_test [usecase] [platform]\n");
        printf("Currently supported usecase are:\n");
        printf(" voice_call\n");
        printf(" headset_detection\n");
        printf(" loopback\n");
        printf("Currently supported platforms are:\n");
        printf(" pluto\n");
        printf(" enterprise\n");
        return -EINVAL;
    }
    if (strcmp(argv[1], "voice_call") && strcmp(argv[1], "headset_detection")
        && strcmp(argv[1], "loopback"))
    {
        printf("Usecase not supported\n");
        printf("Currently supported usecase are:\n");
        printf(" voice_call\n");
        printf(" headset_detection\n");
        printf(" loopback\n");
        return -EINVAL;
    }
    if (strcmp(argv[2], "pluto") && strcmp(argv[2], "enterprise"))
    {
        printf("Platforms not supported\n");
        printf("Currently supported platforms are:\n");
        printf(" pluto\n");
        printf(" enterprise\n");
        return -EINVAL;
    }

    push_reg_utility();

    if (!(strcmp(argv[1], "voice_call")))
        ret = nvtest_voice_call(argv[2]);

    if (!(strcmp(argv[1], "headset_detection")))
        ret = nvtest_headset_detection(argv[2]);

    if (!(strcmp(argv[1], "loopback")))
        ret = nvtest_loopback();

    remove_reg_utility();

    return ret;
}

int nvtest_loopback()
{
    char process_name[200];
    unsigned long long sad_min;
    int ret = 0;
    struct alsa_card card;
    char cap_cmd[200];

    card = get_card();
    if (card.card_num < 0) {
        ALOGE("Failed to get codec\n");
        printf("Loopback Test Fails\n");
        return -ENODEV;
    }

    sprintf(cap_cmd, TINYCAP_COMMAND);
    sprintf(cap_cmd + strlen(cap_cmd), " -D %d ", card.card_num);
    sprintf(cap_cmd + strlen(cap_cmd), TINYCAP_PARAMS);
    system(cap_cmd);
    usleep(100000);
    system(LOOPBACK_COMMAND);
    system(MUSIC_PLAYER_START_NOTIFICATION);

    sleep(2);
    sprintf(process_name, TINYCAP_PROCESS);
    kill_process(process_name);
    sprintf(process_name, MUSIC_PROCESS);
    kill_process(process_name);

    sad_min = match_audio();
    printf("sad_min=%lld\n", sad_min);
    if (sad_min == 0)
        printf("Loopback Test Pass\n");
    else
        printf("Loopback Test Fails\n");

    ret = sad_min == 0 ? 0 : -EIO;

    return ret;
}

int nvtest_headset_detection (char *pltfm)
{
    struct mixer *mixer;
    struct mixer_ctl *ctl;
    struct mixer_ctl *hs_ctl;
    int hs_status;
    int ret = 0;
    unsigned int j;
    int codec_id;
    char process_name[200];
    struct alsa_card card;

    card = get_card();
    codec_id = card.codec_id;
    if (codec_id < 0) {
        ALOGE("Failed to get codec\n");
        printf("Headset Detection Test Fails\n");
        return -ENODEV;
    }

    mixer =  mixer_open(
        snd_card_get_card_id_from_sub_string(
        hs_alsa_ctl[codec_id].card_name));

    if (!mixer) {
        ALOGE("Failed to open mixer\n");
        printf("Headset Detection Test Fails\n");
        return -ENODEV;
    }

    ctl = mixer_get_ctl_by_name(mixer, "Headset Plug State");
    hs_ctl = mixer_get_ctl_by_name(mixer,
        hs_alsa_ctl[codec_id].headset_ctl_name);

    /*Set Headset Plug State*/
    for (j = 0; j < mixer_ctl_get_num_values(ctl); j++) {
        ret = mixer_ctl_set_value(ctl, j, 1);
        if (ret != 0) {
            ALOGE("Failed to set 'Headset Plug State'.%d to 1\n", j);
        } else {
            ALOGV("Set 'Headset Plug State'.%d to 1\n", j);
        }
    }

     /*Start Music Player and wait for a couple of seconds*/
     system(MUSIC_PLAYER_START_COMMAND);
     printf("Turning ON Playback for 5 sec\n");
     sleep(2);

     /*Read Headset Status*/
     hs_status = mixer_ctl_get_value(hs_ctl, 0);

     /*after sleeping for remainder of time, kill music app*/
     sleep(3);
     printf("Turning OFF Playback\n");
     sprintf(process_name, MUSIC_PROCESS);
     kill_process(process_name);

    /*Reset Headset Plug State*/
    for (j = 0; j < mixer_ctl_get_num_values(ctl); j++) {
        ret = mixer_ctl_set_value(ctl, j, 0);
        if (ret != 0) {
            ALOGE("Failed to set 'Headset Plug State'.%d to 0\n", j);
        } else {
            ALOGV("Set 'Headset Plug State'.%d to 0\n", j);
        }
    }

    if (hs_status)
        printf("Headset Detection Test Pass\n");
    else
        printf("Headset Detection Test Fails\n");

    ret = hs_status == 1 ? 0 :  -EIO;

    return ret;
}

int nvtest_voice_call (char *pltfm)
{
    sp<IAudioPolicyService> audioPolicySrvc;
    FILE *fp;
    char command_out[200];
    char command[300];
    unsigned int i, reg_addr, reg_value_int;
    int ret = 0;

    sp<IServiceManager> sm = defaultServiceManager();
    sp<IBinder> binder = NULL;
    binder = sm->getService(String16("media.audio_policy"));
    if (binder < 0) {
        ALOGE("Failed to get media.audio_policy");
        printf("Voice Call Test Fails\n");
        return -ENODEV;
    }
    audioPolicySrvc = interface_cast<IAudioPolicyService>(binder);

    audioPolicySrvc->setPhoneState(AUDIO_MODE_IN_CALL);

    sleep(2);

    /***** Read Tegra Audio Cluster Register *****/
    /*Read AHUB registers*/
    ahub_read_reg(ahub_dut_reg, TEGRA_AHUB_BASE, TEGRA_AHUB_OFFSET,
        TEGRA_AHUB_NUM_REG);
    /*Read I2S registers*/
    i2s_read_reg(i2s0_dut_reg, TEGRA_I2S0_BASE, TEGRA_I2S0_OFFSET,
        TEGRA_I2S0_NUM_REG);
    i2s_read_reg(i2s1_dut_reg, TEGRA_I2S1_BASE, TEGRA_I2S1_OFFSET,
        TEGRA_I2S1_NUM_REG);
    i2s_read_reg(i2s2_dut_reg, TEGRA_I2S2_BASE, TEGRA_I2S2_OFFSET,
        TEGRA_I2S2_NUM_REG);
    i2s_read_reg(i2s3_dut_reg, TEGRA_I2S3_BASE, TEGRA_I2S3_OFFSET,
        TEGRA_I2S3_NUM_REG);
    i2s_read_reg(i2s4_dut_reg, TEGRA_I2S4_BASE, TEGRA_I2S4_OFFSET,
        TEGRA_I2S4_NUM_REG);
    /*Read DAM registers*/
    dam_read_reg(dam0_dut_reg, TEGRA_DAM0_BASE, TEGRA_DAM0_OFFSET,
        TEGRA_DAM0_NUM_REG);
    dam_read_reg(dam1_dut_reg, TEGRA_DAM1_BASE, TEGRA_DAM1_OFFSET,
        TEGRA_DAM1_NUM_REG);
    dam_read_reg(dam2_dut_reg, TEGRA_DAM2_BASE, TEGRA_DAM2_OFFSET,
         TEGRA_DAM2_NUM_REG);
    /*Read APBIF registers*/
    apbif_read_reg(apbif0_dut_reg, TEGRA_APBIF0_BASE, TEGRA_APBIF0_OFFSET,
        TEGRA_APBIF0_NUM_REG);
    apbif_read_reg(apbif1_dut_reg, TEGRA_APBIF1_BASE, TEGRA_APBIF1_OFFSET,
        TEGRA_APBIF1_NUM_REG);
    apbif_read_reg(apbif2_dut_reg, TEGRA_APBIF2_BASE, TEGRA_APBIF2_OFFSET,
        TEGRA_APBIF2_NUM_REG);
    apbif_read_reg(apbif3_dut_reg, TEGRA_APBIF3_BASE, TEGRA_APBIF3_OFFSET,
        TEGRA_APBIF3_NUM_REG);

    //print_audio_cluster_regs();

    sleep(2);

    audioPolicySrvc->setPhoneState(AUDIO_MODE_NORMAL);

    if (!strcmp(pltfm, "pluto"))
        ret = compare_pluto();
    else if (!strcmp(pltfm, "enterprise"))
        ret = compare_enterprise();

    if (ret < 0)
       printf("Voice Call Test Fails\n");
    else
       printf("Voice Call Test Pass\n");

    return ret;
}

int file_size (const char *file_name)
{
    FILE * f = fopen(file_name, "rb");

    fseek(f, 0, SEEK_END);
    int len = (unsigned long)ftell(f);
    fclose(f);

    return len;
}

unsigned long long compute_sad (short *samples, short *ref,
    unsigned int num_of_samples)
{
    unsigned long long sad;
    unsigned int i;

    sad = 0;
    for (i=0; i < num_of_samples; i++)
        sad += abs(samples[i]- ref[i]);

    return sad;
}

unsigned long long match_audio()
{
    FILE * f;
    int file_sz, i;
    short *samples;
    int num_of_frames, frames_to_iterate;
    unsigned long long sad, sad_min;
    int num_of_golden_frm;
#ifdef DUMP_MATCHED_AUDIO
    FILE * f_cut;
    int pos = 0;
#endif

    file_sz = file_size(CAPTURED_FILE);
    samples = (short *)malloc(file_sz);
    f = fopen(CAPTURED_FILE, "rb");
#ifdef DUMP_MATCHED_AUDIO
    f_cut = fopen(DUMP_AUDIO_FILE, "wb");
#endif
    fread(samples, sizeof(short), file_sz/2, f);
    num_of_frames = file_sz/4;
    num_of_golden_frm = (sizeof(ref_samples))/4;
    frames_to_iterate = num_of_frames - num_of_golden_frm + 1;
    sad_min = 0;

    for (i=0; i < frames_to_iterate; i++ )
    {
        sad = compute_sad(&(samples[i*2]), ref_samples, num_of_golden_frm*2);
        if ((i==0) || (sad < sad_min)) {
#ifdef DUMP_MATCHED_AUDIO
            pos = i;
#endif
            sad_min = sad;
        }

        if (sad_min == 0)
            break;
    }

#ifdef DUMP_MATCHED_AUDIO
    fwrite(&(samples[pos*2]), sizeof(short), num_of_golden_frm*2, f_cut);
    fclose(f_cut);
#endif
    fclose(f);

    return sad_min;
}

struct alsa_card get_card()
{
    int j, card_num;
    struct alsa_card card;

    card.card_num = -ENODEV;
    card.codec_id = -ENODEV;
    for (j=0; j < NUM_OF_CODECS; j++) {
        card_num = snd_card_get_card_id_from_sub_string(
            hs_alsa_ctl[j].card_name);
        if (card_num < 0) {
            continue;
        }
        else {
            card.card_num = card_num;
            card.codec_id = j;
            return card;
        }
    }

    return card;
}

void kill_process(char *process_name)
{
    FILE *fp;
    char command_out[1000];
    char command[200];
    char *pid;
    char kill_command[200];

    sprintf(command, "ps | grep -w ");
    sprintf(command + strlen(command), "%s", process_name);
    fp = popen(command, "r");
    fgets(command_out, sizeof(command_out)-1, fp);
    pclose(fp);
    pid = strtok (command_out," ");
    pid = strtok (NULL," ");
    sprintf(kill_command, "kill ");
    sprintf(kill_command + strlen(kill_command), "%s", pid);
    system(kill_command);
}

void push_reg_utility()
{
    int fd;

    system("mount -o rw,remount /system");
    fd = open(REG_UTILITY, O_CREAT | O_WRONLY | O_TRUNC,
        S_IRWXU | S_IRWXG | S_IRWXO);
    write(fd, reg_utility, sizeof(reg_utility));
    close(fd);
}

void remove_reg_utility()
{
    char command[200];

    sprintf(command, "rm ");
    sprintf(command + strlen(command), REG_UTILITY);
    system(command);
}

int compare_pluto()
{
    int ret;

    ret = compare_array(ahub_golden_reg_set[PLUTO], ahub_dut_reg,
        TEGRA_AHUB_NUM_REG);
    if (ret < 0)
        return ret;
    ret = compare_array(i2s0_golden_reg_set[PLUTO], i2s0_dut_reg,
        TEGRA_I2S0_NUM_REG);
    if (ret < 0)
        return ret;
    ret = compare_array(i2s2_golden_reg_set[PLUTO], i2s2_dut_reg,
        TEGRA_I2S2_NUM_REG);

    return ret;
}

int compare_enterprise()
{
    return -EIO;
}

int compare_array(unsigned int *ref, unsigned int *dut, unsigned int length)
{
    unsigned int i;
    int ret;

    for (i=0; i < length; i++) {
        if (ref[i] != dut[i]) {
            break;
        }
    }

    ret = i != length ? -EIO : 0;

    return ret;
}

unsigned int  read_reg(unsigned int reg_base, unsigned int reg_off)
{
    FILE *fp;
    char command_out[200];
    char command[300];
    unsigned int reg_addr, reg_value_int;

    reg_addr = reg_base + reg_off;

    sprintf(command, REG_READ_COMMAND);
    sprintf(command + strlen(command), "%08x", reg_addr);
    fp = popen(command, "r");
    fgets(command_out, sizeof(command_out)-1, fp);
    pclose(fp);
    reg_value_int = strtoul(command_out + (strlen(command_out) -1) - 8,
        NULL, 16);

    return(reg_value_int);
}

void read_regs(unsigned int *reg, unsigned int reg_base, unsigned int reg_off,
    unsigned int num_of_reg)
{
    unsigned int i, reg_offset;

    reg_offset = 0;
    for (i=0; i < num_of_reg; i++) {
        *reg = read_reg(reg_base, reg_offset);
        reg_offset += reg_off;
        reg++;
    }
}

void ahub_read_reg(unsigned int *reg, unsigned int reg_base,
    unsigned int reg_off, unsigned int num_of_reg)
{
    read_regs(reg, reg_base, reg_off, num_of_reg);
}

void dam_read_reg(unsigned int *reg, unsigned int reg_base,
    unsigned int reg_off, unsigned int num_of_reg)
{
    read_regs(reg, reg_base, reg_off, num_of_reg);
}

void i2s_read_reg(unsigned int *reg, unsigned int reg_base,
    unsigned int reg_off, unsigned int num_of_reg)
{
    read_regs(reg, reg_base, reg_off, num_of_reg-1);

    /* Read the Slot Ctrl2 reg at offset 0x64*/
    *(reg + num_of_reg -1) =  read_reg(reg_base, 0x64);
}

void apbif_read_reg(unsigned int *reg, unsigned int reg_base,
    unsigned int reg_off, unsigned int num_of_reg)
{
    /* Read reg at offset 0x0, 0x14 and 0x18*/
    reg[0] =  read_reg(reg_base, 0x0);
    reg[1] =  read_reg(reg_base, 0x14);
    reg[2] =  read_reg(reg_base, 0x18);
}

void print_audio_cluster_regs()
{
    unsigned int i;

    ALOGE("/*** AHUB Regs ***/\n");
    for (i=0; i< TEGRA_AHUB_NUM_REG; i++)
        ALOGE("%d: %08x\n", i, ahub_dut_reg[i]);

    ALOGE("/*** I2S0 Regs ***/\n");
    for (i=0; i< TEGRA_I2S0_NUM_REG; i++)
        ALOGE("%d: %08x\n", i, i2s0_dut_reg[i]);

    ALOGE("/*** I2S1 Regs ***/\n");
    for (i=0; i< TEGRA_I2S1_NUM_REG; i++)
        ALOGE("%d: %08x\n", i, i2s1_dut_reg[i]);

    ALOGE("/*** I2S2 Regs ***/\n");
    for (i=0; i< TEGRA_I2S2_NUM_REG; i++)
        ALOGE("%d: %08x\n", i, i2s2_dut_reg[i]);

    ALOGE("/*** I2S3 Regs ***/\n");
    for (i=0; i< TEGRA_I2S3_NUM_REG; i++)
        ALOGE("%d: %08x\n", i, i2s3_dut_reg[i]);

    ALOGE("/*** I2S4 Regs ***/\n");
    for (i=0; i< TEGRA_I2S4_NUM_REG; i++)
        ALOGE("%d: %08x\n", i, i2s4_dut_reg[i]);

    ALOGE("/*** DAM0 Regs ***/\n");
    for (i=0; i< TEGRA_DAM0_NUM_REG; i++)
        ALOGE("%d: %08x\n", i, dam0_dut_reg[i]);

    ALOGE("/*** DAM1 Regs ***/\n");
    for (i=0; i< TEGRA_DAM1_NUM_REG; i++)
        ALOGE("%d: %08x\n", i, dam1_dut_reg[i]);

    ALOGE("/*** DAM2 Regs ***/\n");
    for (i=0; i< TEGRA_DAM2_NUM_REG; i++)
        ALOGE("%d: %08x\n", i, dam2_dut_reg[i]);

    ALOGE("/*** APBIF0 Regs ***/\n");
    for (i=0; i< TEGRA_APBIF0_NUM_REG; i++)
        ALOGE("%d: %08x\n", i, apbif0_dut_reg[i]);

    ALOGE("/*** APBIF1 Regs ***/\n");
    for (i=0; i< TEGRA_APBIF1_NUM_REG; i++)
        ALOGE("%d: %08x\n", i, apbif1_dut_reg[i]);

    ALOGE("/*** APBIF2 Regs ***/\n");
    for (i=0; i< TEGRA_APBIF2_NUM_REG; i++)
        ALOGE("%d: %08x\n", i, apbif2_dut_reg[i]);

    ALOGE("/*** APBIF3 Regs ***/\n");
    for (i=0; i< TEGRA_APBIF3_NUM_REG; i++)
        ALOGE("%d: %08x\n", i, apbif3_dut_reg[i]);
}
