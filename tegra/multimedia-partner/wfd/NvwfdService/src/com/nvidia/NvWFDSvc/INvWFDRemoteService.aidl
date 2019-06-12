/*
 * Copyright (c) 2012, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */

package com.nvidia.NvWFDSvc;

import com.nvidia.NvWFDSvc.INvWFDServiceListener;

/**
 * @hide
 */
interface INvWFDRemoteService {
    boolean isInitialized();
    void discoverSinks();
    void stopSinkDiscovery();
    List<String> getSinkList();
    List<String> getRememberedSinkList();
    boolean getPbcModeSupport(String sinkSSID);
    boolean getPinModeSupport(String sinkSSID);
    int createConnection(INvWFDServiceListener callback);
    void closeConnection(int connId);

    boolean connect(String sinkSSID, String mAuthentication);
    void cancelConnect();
    void disconnect();
    boolean isConnected();
    boolean isConnectionOngoing();
    String getConnectedSinkId();
    List<String> getSupportedAudioFormats();
    List<String> getSupportedVideoFormats();
    String getActiveAudioFormat();
    String getActiveVideoFormat();
    void configurePolicyManager();
    boolean updateConnectionParameters(int resoultionId);
    void forceResolution(boolean bValue);
    void updatePolicy(int value);
    int getResolutionCheckId();
    boolean getForceResolutionValue();
    int getPolicySeekBarValue();
    boolean getSinkAvailabilityStatus(String sinkSSID);
    boolean getSinkBusyStatus(String sinkSSID);
    boolean modifySink(String sinkSSID, boolean modify, String nickName);
    String getSinkNickname(String sinkSSID);
    int getSinkFrequency(String sinkSSID);
    String getSinkSSID(String sinkNickName);
    String getSinkGroupNetwork(String sinkSSID);
    List<String> getSinkNetworkGroupList();
    boolean removeNetworkGroupSink(String networkGroupName);
    boolean setGameModeOption(boolean enable);
    boolean getGameModeOption();
    void pause();    //asynchronous call
    void resume();    //asynchronous call
}
