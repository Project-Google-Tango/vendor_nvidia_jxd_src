/*
 * Copyright (c) 2013-2014, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */
#ifndef NV_ENABLE_DEBUG_PRINTS
#define NV_ENABLE_DEBUG_PRINTS (0)
#endif

#define LOG_NDEBUG 0

#if (BUILD_FOR_AOS == 0)
#include <fcntl.h>
#include <errno.h>

#include "nvos.h"

#include "nvodm_query_camera.h"
#include "imager_det.h"

#if ENABLE_NVIDIA_CAMTRACE
#include "nvcamtrace.h"
#endif

#if NV_ENABLE_DEBUG_PRINTS
#define HAL_DB_PRINTF(...)  NvOsDebugPrintf(__VA_ARGS__)
#else
#define HAL_DB_PRINTF(...)
#endif

#define EAGAIN    11

static NvOsMutexHandle NvOdmImagerDetect_Mutex;
static NvBool DeviceDetect_Done = NV_FALSE;

static char *GetGUIDStr(NvU64 guid, char *buf, NvU32 size)
{
    NvS32 i;

    if (size < 2)
        return NULL;

    for(i = (NvS32)size - 2; i >= 0; i--)
    {
        buf[i] = (char)(0xFFULL & guid);
        guid /= 0x100;
    }
    buf[size - 1] = 0;
    return buf;
}

static NvBool
PCLDevicePower(DeviceProfile *pDev, int fd, NvBool PowerOn)
{
    NvU32 PwrOption = NVC_PWR_COMM;
    NvS32 status;

    HAL_DB_PRINTF("%s %s\n", __func__, PowerOn ? "ON" : "OFF");
    if (!PowerOn)
        PwrOption = NVC_PWR_OFF;

    status = ioctl(fd, PCLLK_IOCTL_PWR_WR, PwrOption);
    if (status < 0) {
        NvOsDebugPrintf("%s: PCLLK_IOCTL_PWR_WR %s fail. %d\n",
            __func__, PwrOption == NVC_PWR_COMM ? "ON" : "OFF", status);
        return NV_FALSE;
    }
    return NV_TRUE;
}

static NvS32
PCLReadLayout(int fd, NvU32 Offset, NvU32 Size, void *buf)
{
    struct nvc_param param;
    NvS32 status;

    HAL_DB_PRINTF("%s\n", __func__);
    param.variant = Offset;
    param.p_value = buf;
    param.sizeofvalue = Size;
    status = ioctl(fd, PCLLK_IOCTL_LAYOUT_RD, &param);
    HAL_DB_PRINTF("status = %d %d\n", status, errno);
    if (status < 0) {
        if (errno == EAGAIN) {
            HAL_DB_PRINTF("%s: PCL not fully read out.\n", __func__);
            return -EAGAIN;
        }
        if (errno == EEXIST) {
            HAL_DB_PRINTF("%s: PCL not available.\n", __func__);
            return -EEXIST;
        }
        NvOsDebugPrintf("%s: PCLLK_IOCTL_LAYOUT_RD %d <%d>\n", __func__, status, errno);
        return status;
    }

    if (param.sizeofvalue != Size) {
        NvOsDebugPrintf("%s: size not match! expected %d, but got %d\n",
            __func__, Size, param.sizeofvalue);
        return -EFAULT;
    }

    return 0;
}

static NvBool
PCLUpdateLayout(struct cam_device_layout *pAvl, NvU32 num, int fd)
{
    struct nvc_param param;
    NvS32 status;

    HAL_DB_PRINTF("%s %d x %d\n", __func__, num, sizeof(*pAvl));
    param.p_value = pAvl;
    param.sizeofvalue = sizeof(*pAvl) * num;
    status = ioctl(fd, PCLLK_IOCTL_LAYOUT_WR, &param);
    if (status < 0) {
        NvOsDebugPrintf("%s: PCLLK_IOCTL_LAYOUT_WR fail. %d\n", __func__, status);
        return NV_FALSE;
    }
    return NV_TRUE;
}

static NvBool
PCLDeviceChipReg(DeviceProfile *pDev, int fd)
{
    struct virtual_device vdev;
    NvS32 status, i;
    char *cptr;

    HAL_DB_PRINTF("%s %s\n", __func__, pDev->ChipName);
    NvOsMemset(&vdev, 0, sizeof(vdev));
    NvOsStrncpy((char *)vdev.name, pDev->ChipName, sizeof(vdev.name));
    vdev.bus_type = CAMERA_DEVICE_TYPE_I2C;
    vdev.regmap_cfg.addr_bits = pDev->Device.RegLength * 8;
    vdev.regmap_cfg.val_bits = 8;
    vdev.regmap_cfg.cache_type = 0;
    vdev.gpio_num = pDev->Device.GpioUsed;
    vdev.reg_num = pDev->Device.RegulatorNum;
    cptr = (char *)vdev.reg_names;
    for(i = 0; i < (int)vdev.reg_num; i++) {
        NvOsStrncpy(cptr, pDev->Device.RegNames[i], PCL_MAX_STR_LEN);
        cptr += PCL_MAX_STR_LEN;
    }
    vdev.pwr_on_size = pDev->Device.PowerOnSequenceNum;
    vdev.power_on = (struct camera_reg *)pDev->Device.PowerOnSequence;
    vdev.pwr_off_size = pDev->Device.PowerOffSequenceNum;
    vdev.power_off = (struct camera_reg *)pDev->Device.PowerOffSequence;
    vdev.clk_num = pDev->Device.ClockNum;
    status = ioctl(fd, PCLLK_IOCTL_CHIP_REG, &vdev);
    if (status < 0) {
        NvOsDebugPrintf("%s: PCLLK_IOCTL_CHIP_REG fail. %d\n", __func__, status);
        return NV_FALSE;
    }
    return NV_TRUE;
}

static NvBool
PCLDeviceDevReg(DeviceProfile *pDev, int fd)
{
    struct camera_device_info cdev;
    NvS32 status;

    NvOsStrncpy((char *)cdev.name, pDev->ChipName, sizeof(cdev.name));
    cdev.type = 0;
    cdev.bus = pDev->Device.DevInfo.BusNum;
    cdev.addr = pDev->Device.DevInfo.Slave;
    HAL_DB_PRINTF("%s: %s on bus %d, slave %x\n", __func__, cdev.name, cdev.bus, cdev.addr);
    status = ioctl(fd, PCLLK_IOCTL_DEV_REG, &cdev);
    if (status < 0) {
        NvOsDebugPrintf("%s: PCLLK_IOCTL_DEV_REG fail. %d\n", __func__, status);
        return NV_FALSE;
    }
    return NV_TRUE;
}

static NvBool
PCLDeviceUpdate(DeviceProfile *pDev, int fd)
{
    struct cam_update *upd;
    //struct edp_cfg *p_edp;
    struct nvc_param param;
    NvU32 upd_num, i;
    NvS32 status;

    i = sizeof(*upd) * (pDev->Device.ClockNum + 2);
    /*if (pDev->Device.Edp.Num) {
        i += sizeof(*p_edp) + sizeof(*upd);
    }*/
    upd = NvOsAlloc(i);
    if (!upd) {
        NvOsDebugPrintf("%s %d: couldn't allocate memory for GUIDs!\n", __func__, __LINE__);
        return NV_FALSE;
    }
    memset(upd, 0, i);

    upd_num = 0;
    if (pDev->Device.PinMuxGrp != 0xffff) {
        // update pinmux setting
        upd[0].type = UPDATE_PINMUX;
        upd[0].index = 0;
        upd[0].arg = 0 + pDev->Device.PinMuxGrp * 2;
        upd[1].type = UPDATE_PINMUX;
        upd[1].index = 1;
        upd[1].arg = 1 + pDev->Device.PinMuxGrp * 2;
        upd_num = 2;
    }
    for (i = 0; i < pDev->Device.ClockNum; i++) {
        upd[upd_num].type = UPDATE_CLOCK;
        upd[upd_num].index = i;
        upd[upd_num].arg = (__u32)pDev->Device.ClockNames[i];
        upd[upd_num].size = strlen(pDev->Device.ClockNames[i]);
        upd_num++;
    }

    HAL_DB_PRINTF("%s %d items\n", __func__, i);
    param.sizeofvalue = upd_num;
    param.param = 0;
    param.variant = 0;
    param.p_value = upd;
    status = ioctl(fd, PCLLK_IOCTL_UPDATE, &param);
    NvOsFree(upd);
    if (status < 0) {
        NvOsDebugPrintf("%s: PCLLK_IOCTL_UPDATE fail. %d\n", __func__, status);
        return NV_FALSE;
    }
    return NV_TRUE;
}

static NvBool
PCLDeviceRead(DeviceProfile *pDev, int fd, const PCLDeviceDetect *detect, NvU32 *data)
{
    struct nvc_param param;
    NvU32 offset, len, i;
    struct camera_reg reg_read[sizeof(NvU32) + 1];
    NvS32 status;

    offset = detect->Offset;
    len = detect->BytesRead;
    if (len > sizeof(reg_read) - 1) {
        NvOsDebugPrintf("%s: bytes read(%d) exceeds limit(%d).\n", __func__, len, sizeof(reg_read));
        return NV_FALSE;
    }

    /* read from device */
    NvOsMemset(reg_read, 0, sizeof(reg_read));
    for (i = 0; i < len; i++)
        reg_read[i].addr = offset + i;
    reg_read[i].addr = CAMERA_TABLE_END;
    param.param = 0;
    param.variant = 0;
    param.sizeofvalue = sizeof(reg_read);
    param.p_value = reg_read;
    status = ioctl(fd, PCLLK_IOCTL_SEQ_RD, &param);
    if (status < 0) {
        NvOsDebugPrintf("%s: %s %d - %d\n", __func__, pDev->ChipName, pDev->Index, status);
        return NV_FALSE;
    }

    for (offset = 0, i = 0; i < len; i++)
        offset = offset * 256 + reg_read[i].val;
    *data = offset;

    HAL_DB_PRINTF("%s: read reg %x: %x\n", __func__, detect->Offset, *data);
    return NV_TRUE;
}

static NvBool
PCLDeviceCleanUp(DeviceProfile *pDev, int fd)
{
    NvS32 status;

    HAL_DB_PRINTF("%s\n", __func__);
    /* release use of device */
    status = ioctl(fd, PCLLK_IOCTL_DEV_FREE, 0);
    if (status < 0) {
        NvOsDebugPrintf("%s: PCLLK_IOCTL_DEV_FREE fail. %d\n", __func__, status);
        return NV_FALSE;
    }

    /* remove device from pcl */
    status = ioctl(fd, PCLLK_IOCTL_DEV_DEL, 0);
    if (status < 0) {
        NvOsDebugPrintf("%s: PCLLK_IOCTL_DEV_DEL fail. %d\n", __func__, status);
        return NV_FALSE;
    }
    return NV_TRUE;
}

static NvBool
PCLDeviceDriverInstall(DeviceProfile *pDev, int fd)
{
    struct nvc_param param;
    NvS32 status;

    HAL_DB_PRINTF("%s %s\n", __func__, pDev->Device.AlterName);
    param.param = 0;
    param.variant = pDev->Device.DevInfo.BusNum;
    param.sizeofvalue = NvOsStrlen(pDev->Device.AlterName);
    param.p_value = (void *)pDev->Device.AlterName;
    status = ioctl(fd, PCLLK_IOCTL_DRV_ADD, &param);
    if (status < 0) {
        NvOsDebugPrintf("%s: PCLLK_IOCTL_DRV_ADD fail. %d\n", __func__, status);
        return NV_FALSE;
    }
    return NV_TRUE;
}

static NvBool
ImagerDeviceDetect(DeviceProfile *pDev)
{
    const PCLDeviceDetect *detect;
    NvU32 i2c_bus;
    NvU64 TimeOfDay;
    int fd = -1;
    NvBool ret = NV_FALSE, found = NV_FALSE;

    HAL_DB_PRINTF("%s +++ %s\n", __func__, pDev->ChipName);

    i2c_bus = pDev->Device.DevInfo.BusNum;
    TimeOfDay = NvOsGetTimeUS();

    // open a new PCL instance
    fd = open(PCL_KERNEL_NODE, O_RDWR);
    if (fd < 0) {
        NvOsDebugPrintf("%s: PCL driver open failed - %d\n", __func__, errno);
        return NV_FALSE;
    }

    // register the chip type
    ret = PCLDeviceChipReg(pDev, fd);
    if (!ret) {
        NvOsDebugPrintf("%s: Failed to register new device\n", __func__);
        goto dev_det_end;
    }

    // create a new virtual device install on that chip
    ret = PCLDeviceDevReg(pDev, fd);
    if (!ret) {
        NvOsDebugPrintf("%s: Failed to create device instance\n", __func__);
        goto dev_det_end;
    }

    // update device with necessary resources
    ret = PCLDeviceUpdate(pDev, fd);
    if (!ret) {
        NvOsDebugPrintf("%s: Failed to update device resources\n", __func__);
        goto dev_det_end;
    }

    detect = pDev->Device.Detect;
    if (!detect || !detect->BytesRead) {
        found = NV_TRUE;
        ret = NV_TRUE;
        goto dev_det_clean;
    }

    /* power on device */
    ret = PCLDevicePower(pDev, fd, NV_TRUE);
    if (!ret) {
        NvOsDebugPrintf("%s: Failed to power on device\n", __func__);
        goto dev_det_clean;
    }

/* enable this to manually debug using i2cwr */
    while (detect->BytesRead) {
        NvU32 comp = 0;

        // read a sequence of bytes from device
        ret = PCLDeviceRead(pDev, fd, detect, &comp);
        if (!ret) {
            HAL_DB_PRINTF("%s: Failed to read from device\n", __func__);
            found = NV_FALSE;
            break;
        }

        // compare with reference after bit mask
        if ((comp & detect->Mask) == detect->TrueValue) {
            HAL_DB_PRINTF("%s: MATCH %x & %x vs %x\n", __func__, comp, detect->Mask, detect->TrueValue);
            found = NV_TRUE;
            ret = NV_TRUE;
        } else {
            HAL_DB_PRINTF("%s: not match %x & %x vs %x\n", __func__, comp, detect->Mask, detect->TrueValue);
            ret = NV_FALSE;
            break;
        }

        detect++;
    }

    /* power off device */
    ret = PCLDevicePower(pDev, fd, NV_FALSE);
    if (!ret) {
        NvOsDebugPrintf("%s: Failed to power off device\n", __func__);
    }

dev_det_clean:
    // release and remove device instance
    ret = PCLDeviceCleanUp(pDev, fd);
    if (!ret) {
        NvOsDebugPrintf("%s: Failed to remove device instance\n", __func__);
    }

dev_det_end:
    close(fd);
    TimeOfDay = NvOsGetTimeUS() - TimeOfDay;

    HAL_DB_PRINTF("%s --- %llu\n", __func__, TimeOfDay);
    return found;
}

/**
 * Initialization for device detection session.
 */
NvBool NvOdmImagerDetInit(void)
{
    if (!NvOdmImagerDetect_Mutex) {
        NvOsMutexCreate(&NvOdmImagerDetect_Mutex);
    }
    return NV_TRUE;
}

/**
 * Destroy device detection session.
 */
NvBool NvOdmImagerDetExit(void)
{
    if (NvOdmImagerDetect_Mutex) {
        NvOsMutexDestroy(NvOdmImagerDetect_Mutex);
        NvOdmImagerDetect_Mutex = NULL;
    }
    return NV_TRUE;
}

NvBool
NvOdmImagerDeviceDetect(void)
{
    const DeviceProfile *pDevices = NULL;
    DeviceProfile **pDevLst = NULL;
    struct cam_device_layout *pAvl = NULL;
    NvU32 Count = 0, idx, Avl_Num = 0;
    NvBool ret = NV_TRUE;
    NvS32 err = 0;
    int fd_pcl = -1;
    NvU64 TimeOfDay = NvOsGetTimeUS();

    HAL_DB_PRINTF("%s +++\n", __func__);

    if (!NvOdmImagerDetect_Mutex) {
        return NV_FALSE;
    }
    NvOsMutexLock(NvOdmImagerDetect_Mutex);

    if (DeviceDetect_Done == NV_TRUE) {
        HAL_DB_PRINTF("%s already done!\n", __func__);
        NvOsMutexUnlock(NvOdmImagerDetect_Mutex);
        return NV_TRUE;
    }

    fd_pcl = open(PCL_KERNEL_NODE, O_RDWR);
    if (fd_pcl < 0) {
        NvOsDebugPrintf("%s: Camera autodetect disabled - %d\n", __func__, errno);
        ret = NV_FALSE;
        goto NvOdmImagerDetect_Done;
    }

    err = PCLReadLayout(fd_pcl, 0, sizeof(Count), &Count);
    if (!err || err == -EAGAIN) {
        HAL_DB_PRINTF("%s: AVL available.\n", __func__);
        goto NvOdmImagerDetect_Done;
    }

    if (err != -EEXIST) {
        NvOsDebugPrintf("%s: PCL layout not empty.\n", __func__);
        ret = NV_FALSE;
        goto NvOdmImagerDetect_Done;
    }

    pDevices = NvOdmCameraQuery(&Count);
    if (Count == 0)
    {
        // if no sensor in database, fall back to the default
        // virtual sensor
        HAL_DB_PRINTF("%s %d: no device listed. %p\n", __func__, __LINE__, pDevices);
        ret = NV_FALSE;
        goto NvOdmImagerDetect_Done;
    }

    pAvl = NvOsAlloc(sizeof(*pAvl) * Count + sizeof(*pDevLst) * Count);
    pDevLst = (void *)(pAvl + Count);
    if (!pAvl) {
        NvOsDebugPrintf("%s %d: couldn't allocate memory for GUIDs!\n", __func__, __LINE__);
        ret = NV_FALSE;
        goto NvOdmImagerDetect_Done;
    }
    HAL_DB_PRINTF("%s %d devices listed.\n", __func__, Count);
    NvOsMemset(pAvl, 0, sizeof(*pAvl) * Count);

    for (idx = 0; idx < Count; idx++) {
        DeviceProfile *pDev = (DeviceProfile *)&pDevices[idx];
    #if NV_ENABLE_DEBUG_PRINTS
        char buf[9];
    #endif
        HAL_DB_PRINTF("\n#%d - %s\n", idx, GetGUIDStr(pDev->Device.GUID, buf, sizeof(buf)));

        if (ImagerDeviceDetect(pDev)) {
            pDevLst[Avl_Num] = pDev;
            Avl_Num++;
            HAL_DB_PRINTF("%s found %s, %d found.\n",
                __func__, GetGUIDStr(pDev->Device.GUID, buf, sizeof(buf)), Avl_Num);
        }
    }

    for (idx = 0; idx < Avl_Num; idx++) {
        HAL_DB_PRINTF("%s - %llx %d %d %x %d %x %s\n",
            pDevLst[idx]->ChipName, pDevLst[idx]->Device.GUID, pDevLst[idx]->Device.Position,
            pDevLst[idx]->Device.DevInfo.BusNum, pDevLst[idx]->Device.DevInfo.Slave,
            pDevLst[idx]->Device.RegLength, pDevLst[idx]->Device.DevId, pDevLst[idx]->Device.AlterName);
    }

    if (Avl_Num) {
        NvU32 i;

        ret = NV_TRUE;
        for (idx = 0, i = 0; i < Avl_Num; i++) {
            /* install device kernel drivers */
            ret = PCLDeviceDriverInstall(pDevLst[i], fd_pcl);
            if (!ret) {
                NvOsDebugPrintf("%s: Failed to install device driver\n", __func__);
            } else {
                pAvl[idx].guid = pDevLst[i]->Device.GUID;
                NvOsStrncpy((char *)pAvl[idx].name, pDevLst[i]->ChipName, sizeof(pAvl->name));
                NvOsStrncpy((char *)pAvl[idx].alt_name, pDevLst[i]->Device.AlterName, sizeof(pAvl->alt_name));
                pAvl[idx].pos = pDevLst[i]->Device.Position;
                pAvl[idx].bus = pDevLst[i]->Device.DevInfo.BusNum;
                pAvl[idx].addr = pDevLst[i]->Device.DevInfo.Slave;
                pAvl[idx].addr_byte = pDevLst[i]->Device.RegLength;
                pAvl[idx].dev_id = pDevLst[i]->Device.DevId;
                pAvl[idx].index = pDevLst[i]->Index;
                switch (pDevLst[i]->Type) {
                    case     PCLDeviceType_Sensor:
                        pAvl[idx].type = DEVICE_SENSOR;
                        break;
                    case     PCLDeviceType_Focuser:
                        pAvl[idx].type = DEVICE_FOCUSER;
                        break;
                    case     PCLDeviceType_Flash:
                        pAvl[idx].type = DEVICE_FLASH;
                        break;
                    case     PCLDeviceType_ROM:
                        pAvl[idx].type = DEVICE_ROM;
                        break;
                    default:
                        pAvl[idx].type = DEVICE_OTHER;
                }
                HAL_DB_PRINTF("%d: %.20s %016llx %d %.20s %1x %1x %.2x %1x %.8x %x\n",
                        idx, pAvl[idx].name, pAvl[idx].guid, pAvl[idx].type, pAvl[idx].alt_name, pAvl[idx].pos,
                        pAvl[idx].bus, pAvl[idx].addr, pAvl[idx].addr_byte, pAvl[idx].dev_id, pAvl[idx].index);
                idx++;
            }
        }

        ret = PCLUpdateLayout(pAvl, idx, fd_pcl);
        if (!ret) {
            NvOsDebugPrintf("%s: Failed to update AVL\n", __func__);
            ret = NV_FALSE;
        }
        else {
            DeviceDetect_Done = NV_TRUE;
        }
    }
    NvOsFree(pAvl);

NvOdmImagerDetect_Done:
    if (fd_pcl >= 0)
        close(fd_pcl);
    NvOsMutexUnlock(NvOdmImagerDetect_Mutex);
    TimeOfDay = NvOsGetTimeUS() - TimeOfDay;
    HAL_DB_PRINTF("%s - %d devices found - %llu uS\n", __func__, Avl_Num, TimeOfDay);
    return ret;
}

NvBool
NvOdmImagerHasSensorInPos(struct cam_device_layout *pDev, NvU8 Pos)
{
    if (!pDev)
        return NV_FALSE;

    while (pDev->guid) {
        if (pDev->type == DEVICE_SENSOR)
            return NV_TRUE;
        pDev++;
    }

    return NV_FALSE;
}

NvBool
NvOdmImagerGetLayout(struct cam_device_layout **pDev, NvU32 *Count)
{
    struct cam_device_layout *pLayOut = NULL;
    NvS32 status = -EAGAIN;
    NvU32 cnt = 0;
    NvU32 offset = 0;
    int fd;
#if NV_ENABLE_DEBUG_PRINTS
    char buf[9];
#endif

    HAL_DB_PRINTF("%s\n", __func__);
    fd = open(PCL_KERNEL_NODE, O_RDWR);
    if (fd < 0) {
        NvOsDebugPrintf("%s: PCL layout not detected - %d\n", __func__, errno);
        return NV_FALSE;
    }

    while (status == -EAGAIN) {
        cnt++;
        pLayOut = (struct cam_device_layout *)NvOsRealloc(pLayOut, sizeof(*pLayOut) * (cnt + 1));
        if (!pLayOut){
            NvOsDebugPrintf("%s %d: couldn't allocate memory! %d\n", __func__, __LINE__, cnt);
            status = -ENOMEM;
            break;
        }

        NvOsMemset((void *)pLayOut + offset, 0, sizeof(*pLayOut) * 2);
        status = PCLReadLayout(fd, offset, sizeof(*pLayOut), (void *)pLayOut + offset);
        offset += sizeof(*pLayOut);
        HAL_DB_PRINTF("%s %d %d\n", __func__, cnt, status);
    }

    close(fd);
    if (status < 0) {
        NvOsDebugPrintf("%s: Failed to read AVL %d\n", __func__, status);
        NvOsFree(pLayOut);
        if (status == -EEXIST) {
            if (pDev)
                *pDev = NULL;
            if (Count)
                *Count = 0;
            return NV_TRUE;
        }
        return NV_FALSE;
    }

    if (pDev)
        *pDev = pLayOut;
    if (Count)
        *Count = cnt;
    for (offset = 0; offset < cnt; offset++) {
        HAL_DB_PRINTF("%s %d: %s %s %s - %d %d %d %d\n", __func__, offset,
            pLayOut[offset].name, GetGUIDStr(pLayOut[offset].guid, buf, sizeof(buf)), pLayOut[offset].alt_name,
            pLayOut[offset].bus, pLayOut[offset].addr, pLayOut[offset].addr_byte, pLayOut[offset].pos);
    }
    return NV_TRUE;
}
#else
NvBool
NvOdmImagerDeviceDetect(void)
{
    return NV_TRUE;
}
#endif
