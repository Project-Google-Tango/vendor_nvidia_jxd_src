/*
* Copyright (c) 2008 - 2013 NVIDIA Corporation.  All Rights Reserved.
*
* NVIDIA Corporation and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA Corporation is strictly prohibited.
*/

/**
 * @file
 * <b> Nv3p Status Codes</b>
 *
 * @b Description: Specifies macro expansion of the status codes
 *     defined for Nv3p methods and interfaces.
 */

// Note: if you add a status code, add it both to the long doxy-comment below
// **and** to the definitions below the comment. This is the only way doxygen
// will show us these values in the documentation.

/**
 * @defgroup nv3p_status_group Nv3p Status Codes
 *
 * Provides return status codes for Nv3p functions.
 *
 * Defines macro expansion of the status codes
 * defined for Nv3p methods & interfaces.
 *
 * This header is @b NOT protected from being included multiple times, as it is
 * used for C pre-processor macro expansion of error codes and the descriptions
 * of those error codes.
 *
 * Each status code has a unique name, description, and value to make it easier
 * for developers to identify the source of a failure. Thus there are no
 * generic or unknown error codes.
 *
<pre>
NV3P_STATUS(Ok,                                 0x00000000, "success")
NV3P_STATUS(Unknown,                            0x00000001, "unknown operation")
NV3P_STATUS(DeviceFailure,                      0x00000002, "device failure")
NV3P_STATUS(NotImplemented,                     0x00000003, "operation not implemented")
NV3P_STATUS(InvalidState,                       0x00000004, "module is in invalid state to perform the requested operation")
NV3P_STATUS(BadParameter,                       0x00000005, "bad parameter to method or interface")
NV3P_STATUS(InvalidDevice,                      0x00000006, "specified device is invalid")
NV3P_STATUS(InvalidPartition,                   0x00000007, "specified partition is invalid or does not exist")
NV3P_STATUS(PartitionTableRequired,             0x00000008, "partition table is required for this command")
NV3P_STATUS(MassStorageFailure,                 0x00000009, "fatal failure to read / write to mass storage")
NV3P_STATUS(PartitionCreation,                  0x0000000A, "failed to create the partition")
NV3P_STATUS(BCTRequired,                        0x0000000B, "BCT file required for this command")
NV3P_STATUS(InvalidBCT,                         0x0000000C, "BCT is invalid or corrupted")
NV3P_STATUS(MissingBootloaderInfo,              0x0000000D, "Bootloader load address, entry point, and/or version number missing")
NV3P_STATUS(InvalidPartitionTable,              0x0000000E, "partition table is invalid, missing required information")
NV3P_STATUS(CryptoFailure,                      0x0000000F, "cipher or hash operation failed")
NV3P_STATUS(TooManyBootloaders,                 0x00000010, "BCT is full; no more bootloaders can be added")
NV3P_STATUS(NotBootDevice,                      0x00000011, "specified device is not the boot device")
NV3P_STATUS(NotSupported,                       0x00000012, "operation not supported")
NV3P_STATUS(InvalidPartitionName,               0x00000013, "partition name is too big or unsupported")
NV3P_STATUS(InvalidCmdAfterVerify,              0x00000014, "illegal command after verification enabled")
NV3P_STATUS(BctNotFound,                        0x00000015, "Bct file not found")
NV3P_STATUS(BctWriteFailure,                    0x00000016, "Bct Write Failed")
NV3P_STATUS(BctReadVerifyFailure,               0x00000017, "Bct ReadVerify Failed")
NV3P_STATUS(InvalidBCTSize,                     0x00000018, "BCT size is invalid")
NV3P_STATUS(InvalidBCTPartitionId,              0x00000019, "Bct patition id not valid")
NV3P_STATUS(ErrorBBT,                           0x0000001A, "Error creating babd block table")
NV3P_STATUS(NoBootloader,                       0x0000001B, "Bootloader not found")
NV3P_STATUS(RtcAccessFailure,                   0x0000001C, "Unable to access RTC device")
NV3P_STATUS(UnsparseFailure,                    0x0000001D, "Unsparse file failed")
NV3P_STATUS(BLValidationFailure,                0x0000001E, "bootloader validation failed")
NV3P_STATUS(FuelGaugeFwUpgradeFailure,          0x0000001F, "FuelGauge Firmware Upgrade Failed")
NV3P_STATUS(VerifySdramFailure,                 0x00000020, "Sdram Initialization failed")
NV3P_STATUS(BctInvariant,                       0x00000021, "BctInvariant protection Failed")
NV3P_STATUS(TooManyHashPartition,               0x00000022, "BCT is full, unable to add more partition for which hash is needed.")
NV3P_STATUS(BDKTestRegistrationFailure,         0x00000023, "Failure in creating default NvBDKTest registration. See UART log for specific error")
NV3P_STATUS(BDKTestRunFailure,                  0x00000024, "Failure in NvBDKTest Entry run. See UART log for specific error")
NV3P_STATUS(InvalidCombination,                 0x00000025, "SBK & device key cannot be used with security_mode fuse while burning")
NV3P_STATUS(SecurityModeFuseBurn,               0x00000026, "security mode fuse should be the last fuse to burn")
NV3P_STATUS(FuseBypassSpeedoIddqCheckFailure,   0x00000027, "Chip does not meet the specified SPEEDO/IDDQ limits")
NV3P_STATUS(FuseBypassFailed,                   0x00000028, "Fuse bypassing is failed")
</pre>
 *
 * @ingroup nv3p_modules
 * @{
 */


/** Nv3p status codes. */
NV3P_STATUS(Ok,                                 0x00000000, "success")
NV3P_STATUS(Unknown,                            0x00000001, "unknown operation")
NV3P_STATUS(DeviceFailure,                      0x00000002, "device failure")
NV3P_STATUS(NotImplemented,                     0x00000003, "operation not implemented")
NV3P_STATUS(InvalidState,                       0x00000004, "module is in invalid state to perform the requested operation")
NV3P_STATUS(BadParameter,                       0x00000005, "bad parameter to method or interface")
NV3P_STATUS(InvalidDevice,                      0x00000006, "specified device is invalid")
NV3P_STATUS(InvalidPartition,                   0x00000007, "specified partition is invalid or does not exist")
NV3P_STATUS(PartitionTableRequired,             0x00000008, "partition table is required for this command")
NV3P_STATUS(MassStorageFailure,                 0x00000009, "fatal failure to read / write to mass storage")
NV3P_STATUS(PartitionCreation,                  0x0000000A, "failed to create the partition")
NV3P_STATUS(BCTRequired,                        0x0000000B, "BCT file required for this command")
NV3P_STATUS(InvalidBCT,                         0x0000000C, "BCT is invalid or corrupted or SDRAM params may be wrong")
NV3P_STATUS(MissingBootloaderInfo,              0x0000000D, "Bootloader load address, entry point, and/or version number missing")
NV3P_STATUS(InvalidPartitionTable,              0x0000000E, "partition table is invalid, missing required information")
NV3P_STATUS(CryptoFailure,                      0x0000000F, "cipher or hash operation failed")
NV3P_STATUS(TooManyBootloaders,                 0x00000010, "BCT is full; no more bootloaders can be added")
NV3P_STATUS(NotBootDevice,                      0x00000011, "specified device is not the boot device")
NV3P_STATUS(NotSupported,                       0x00000012, "operation not supported")
NV3P_STATUS(InvalidPartitionName,               0x00000013, "partition name is too big or unsupported")
NV3P_STATUS(InvalidCmdAfterVerify,              0x00000014, "illegal command after verification enabled")
NV3P_STATUS(BctNotFound,                        0x00000015, "Bct file not found")
NV3P_STATUS(BctWriteFailure,                    0x00000016, "Bct Write Failed")
NV3P_STATUS(BctReadVerifyFailure,               0x00000017, "Bct ReadVerify Failed")
NV3P_STATUS(InvalidBCTSize,                     0x00000018, "BCT size is invalid")
NV3P_STATUS(InvalidBCTPartitionId,              0x00000019, "Bct patition id not valid")
NV3P_STATUS(ErrorBBT,                           0x0000001A, "Error creating babd block table")
NV3P_STATUS(NoBootloader,                       0x0000001B, "Bootloader not found")
NV3P_STATUS(RtcAccessFailure,                   0x0000001C, "Unable to access RTC device")
NV3P_STATUS(UnsparseFailure,                    0x0000001D, "Unsparse file failed")
NV3P_STATUS(BLValidationFailure,                0x0000001E, "bootloader validation failed")
NV3P_STATUS(FuelGaugeFwUpgradeFailure,          0x0000001F, "FuelGauge Firmware Upgrade Failed")
NV3P_STATUS(VerifySdramFailure,                 0x00000020, "Sdram Initialization failed")
NV3P_STATUS(BctInvariant,                       0x00000021, "BctInvariant protection Failed")
NV3P_STATUS(TooManyHashPartition,               0x00000022, "BCT is full, unable to add more partition for which hash is needed.")
NV3P_STATUS(BDKTestRegistrationFailure,         0x00000023, "Failure in creating default NvBDKTest registration. See UART log for specific error")
NV3P_STATUS(BDKTestRunFailure,                  0x00000024, "Failure in NvBDKTest Entry run. See UART log for specific error")
NV3P_STATUS(InvalidCombination,                 0x00000025, "SBK & device key cannot be used with security_mode fuse while burning")
NV3P_STATUS(SecurityModeFuseBurn,               0x00000026, "security mode fuse should be the last fuse to burn")
NV3P_STATUS(FuseBypassSpeedoIddqCheckFailure,   0x00000027, "Chip does not meet the specified SPEEDO/IDDQ limits")
NV3P_STATUS(FuseBypassFailed,                   0x00000028, "Fuse bypassing is failed")
NV3P_STATUS(UpdateuCFwFailure,                  0x00000029, "Updating the uC Fw has failed")
NV3P_STATUS(BootloaderHashMisMatch,             0x0000002A, "bootloader hash mismatch ")
NV3P_STATUS(CmdCompleteFailure,                 0x0000002B, "Failure during Command Complete")
NV3P_STATUS(DataTransferFailure,                0x0000002C, "Failure during Data Transfer")
NV3P_STATUS(ODMCommandFailure,                  0x0000002D, "custom ODM command generic error")
NV3P_STATUS(BDKTestTimeout,                     0x0000002E, "One of the tests did not complete within the specified time limit. See the UART log for specific error")

#if 0
// FIXME -- additional status codes from the POR (which aren't used yet)
NV3P_STATUS(FailureToConnect,                   0x01000000, "failed to connect to target device")
NV3P_STATUS(CreateAlreadyInitialized,           0x01000001, "full init failed because device is already initialized")
NV3P_STATUS(MiniLoaderVersionMismatch,          0x01000003, "version mismatch between NvFlash and the mini loader")
NV3P_STATUS(BootLoaderVersionMismatch,          0x01000004, "version mismatch between NvFlash and the boot loader")
NV3P_STATUS(TargetDisconnected,                 0x01000005, "connection to target was dropped")
NV3P_STATUS(ConfigFileRequired,                 0x01000006, "configuration file required for this command")
NV3P_STATUS(ConfigFileInvalid,                  0x01000008, "configuration file is invalid")
NV3P_STATUS(BCTInvalid,                         0x01000009, "BCT file is invalid")
NV3P_STATUS(ExistingBCTInvalid,                 0x0100000A, "existing BCT on the target is invalid")
NV3P_STATUS(ConfigFileVersionMismatch,          0x0100000B, "version mistmatch between NvFlash and the configuration file")
NV3P_STATUS(ConfigFileError,                    0x0100000C, "error within the syntax of the configuration file")
NV3P_STATUS(PartitionNumberRequired,            0x0100000D, "partition number is required for this command")
NV3P_STATUS(PartitionFileRequired,              0x0100000E, "partition file is required for this command")
NV3P_STATUS(PartitionFileMissing,               0x0100000F, "partition file is missing from the configuration file")
NV3P_STATUS(SBKMissing,                         0x01000010, "target is secure and requires the secure boot key")
NV3P_STATUS(SBKIncorrect,                       0x01000011, "SBK does not match the secure target")
NV3P_STATUS(DigitalSignatureIncorrect,          0x01000012, "digital signature of the partition file is incorrect")
NV3P_STATUS(PartitionInvalid,                   0x01000013, "specified partiton does not exist")
NV3P_STATUS(NoExistingBCT,                      0x01000014, "no BCT exists on the target")
NV3P_STATUS(BootROMTimeout,                     0x01000015, "boot rom on the target device failed to respond")
NV3P_STATUS(MiniloaderTimeout,                  0x01000016, "mini loader on the target device failed to respond")
NV3P_STATUS(BootloaderTimeout,                  0x01000017, "boot loader on the target device failed to respond")
NV3P_STATUS(PartitionSize,                      0x01000019, "partition size in the configuration file is invalid")
NV3P_STATUS(PartitionStart,                     0x0100001A, "start location of the parition is invalid")
NV3P_STATUS(NotABootPartition,                  0x0100001B, "specified partition is not a boot partition")
NV3P_STATUS(ODMCommandUnknown,                  0x0100001C, "custom ODM command unknown on target")
NV3P_STATUS(ODMCommandError,                    0x0100001D, "custom ODM command generic error")
#endif


/** @} */
/* ^^^ ADD ALL NEW STATUS CODES RIGHT ABOVE HERE ^^^ */
