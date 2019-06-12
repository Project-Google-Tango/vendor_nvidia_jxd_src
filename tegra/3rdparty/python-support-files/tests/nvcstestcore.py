# Copyright (c) 2012-2014, NVIDIA Corporation.  All rights reserved.
#
# NVIDIA Corporation and its licensors retain all intellectual property
# and proprietary rights in and to this software, related documentation
# and any modifications thereto.  Any use, reproduction, disclosure or
# distribution of this software and related documentation without an express
# license agreement from NVIDIA Corporation is strictly prohibited.
#

import traceback
import os.path
import nvrawfile
import shutil
import math
import time

import nvcamera
import nvcstestutils
import nvcameraimageutils

class NvCSTestResult(object):
    SUCCESS = 1
    SKIPPED = 2
    ERROR = 3

class GraphState(object):
    STOPPED = 1
    PAUSED = 2
    RUNNING = 3
    CLOSED = 4

class GraphType(object):
    JPEG = 1
    RAW = 2
    HOST_SENSOR = 3

class NvCSTestException(Exception):
    "NvCSTest Exception Class"

    def __init__(self, value):
        self.value = value
    def __str__(self):
        return repr(self.value)

class NvCSTestGraph(object):
    "NvCSTest Graph wrapper class"

    _imagerID = 0
    _state = GraphState.CLOSED
    _stillWidth = 0
    _stillHeight = 0
    _previewWidth = 0
    _previewHeight = 0
    _hostSensorWidth = 0
    _hostSensorHeight = 0
    _graphType = GraphType.JPEG
    _obGraph = None
    logger = None

    def __init__(self, graphType=GraphType.JPEG):
        self.graphType = graphType
        self._obGraph = nvcamera.Graph()
        self._state = GraphState.CLOSED
        self.logger = nvcstestutils.Logger()

    def setPreviewParams(self, width, height):
        self._previewWidth = width
        self._previewHeight = height

    def setStillParams(self, width, height):
        self._stillWidth = width
        self._stillHeight = height

    def setHostSensorParams(self, width, height):
        self._hostSensorWidth = width
        self._hostSensorHeight = height

    def setImager(self, imagerID):
        self._imagerID = imagerID

    def setGraphType(self, graphType):
        self._graphType = graphType

    def createAndRunGraph(self):
        if (self._state == GraphState.CLOSED):
            if(self._graphType == GraphType.HOST_SENSOR):
                self._obGraph.setImager("host")
            else:
                self._obGraph.setImager(self._imagerID)

            if(self._graphType != GraphType.HOST_SENSOR):
                self._obGraph.preview(self._previewWidth, self._previewHeight)

            graphTypeString = self.getGraphTypeString()
            if(graphTypeString ==  None):
                raise ValueError("Invalid graph type")

            self._obGraph.still(self._stillWidth, self._stillHeight, graphTypeString)

            self.logger.debug("Creating the %s graph" % graphTypeString)

            self._obGraph.run()

            self._state = GraphState.RUNNING
        else:
            self.logger.error("Couldn't create and run the graph")
            raise NvCSTestException("Graph is in invalid state: %s" % self.getGraphStateString())

    def stopAndDeleteGraph(self):
        if(self._state == GraphState.RUNNING):
            self.logger.debug("Stopping the graph")
            self._obGraph.stop()
            self._state = GraphState.STOPPED
        if(self._state == GraphState.STOPPED):
            self.logger.debug("Closing the graph")
            self._obGraph.close()
            self._state = GraphState.CLOSED

    def getGraphTypeString(self):
        if(self._graphType == GraphType.JPEG or self._graphType == GraphType.HOST_SENSOR):
            return "Jpeg"
        elif(self._graphType == GraphType.RAW):
            return "Bayer"
        else:
            return None

    def getGraphType(self):
        return self._graphType

    def getGraphStateString(self):
        if(self._state == GraphState.CLOSED):
            return "CLOSED"
        elif(self._state == GraphState.PAUSED):
            return "PAUSED"
        elif(self._state == GraphState.RUNNING):
            return "RUNNING"
        else:
            return "STOPPED"

    def getGraphState(self):
        return self._state

class NvCSTestCamera(object):
    _obGraph = None
    _obCamera = None
    concurrentRawImageDir = "/data/raw"
    _isPreviewRunning = False
    logger = None

    def __init__(self, obGraph):
        if(obGraph == None):
            raise ValueError("Graph is not open")
        self._obGraph = obGraph
        self._obCamera = nvcamera.Camera()
        self.logger = nvcstestutils.Logger()

    # redirect the setAttr call to nvcamera.Camera object
    def setAttr(self, attribute, value):
        self._obCamera.setAttr(attribute, value)

    # redirect the getAttr call to nvcamera.Camera object
    def getAttr(self, attribute):
        return self._obCamera.getAttr(attribute)

    # lock Auto Algs. Preview has to be started by the user.
    def lockAutoAlgs(self):
        self._obCamera.halfpress(10000)
        self._obCamera.waitForEvent(12000, nvcamera.CamConst_ALGS)

    # unlock Auto Algs
    def unlockAutoAlgs(self):
        self._obCamera.hp_release()

    # redirect the setRawImage call to nvcamera Camera object
    def setRawImage(self, header, pixelData, iteration):
        self._obCamera.setRawImage(header, pixelData, iteration)

    def isConcurrentRawCaptureSupported(self):
        retVal = True
        try:
            self._obCamera.setAttr(nvcamera.attr_concurrentrawdumpflag, 7)
        except nvcamera.NvCameraException, err:
            retVal = False
        return retVal

    def isFocuserSupported(self):
        retVal = True
        try:
            physicalRange = self._obCamera.getAttr(nvcamera.attr_focuspositionphysicalrange)
        except nvcamera.NvCameraException, err:
            if (err.value == nvcamera.NvError_NotSupported):
                self.logger.info("focuser is not supported")
                retVal = False
            else:
                print err.value
                raise
        else:
            if (physicalRange[0] == physicalRange[1]):
                self.logger.info("focuser is not present")
                retVal = False
        return retVal

    def captureRAWImage(self, imageName, needHalfPress=True):
        self.logger.debug("Capturing RAW image: %s" % imageName)
        graphType = self._obGraph.getGraphType()
        if(graphType == GraphType.RAW):
            if(needHalfPress):
                self._obCamera.halfpress(10000)
                self._obCamera.waitForEvent(12000, nvcamera.CamConst_ALGS)
            self._obCamera.still(imageName)
            self._obCamera.waitForEvent(12000, nvcamera.CamConst_CAP_READY)
            if(needHalfPress):
                self._obCamera.hp_release()
        else:
            self.logger.error("Can't capture RAW image with %s graph" %
                              self._obGraph.getGraphTypeString())
            raise NvCSTestException("Can't capture RAW image with %s graph" %
                                     self._obGraph.getGraphTypeString())

    def captureConcurrentJpegAndRawImage(self, imageName, needHalfPress=True):
        self.logger.debug("Capturing concurrent RAW and jpeg image %s" % imageName)
        graphType = self._obGraph.getGraphType()
        if(graphType == GraphType.JPEG):
            self.logger.debug("Setting concurrent raw dump flag to 7")
            self._obCamera.setAttr(nvcamera.attr_concurrentrawdumpflag, 7)
            self._obCamera.setAttr(nvcamera.attr_pauseaftercapture, 1)

            # remove existing raw files under /data/raw
            if os.path.exists(self.concurrentRawImageDir):
                fileList = os.listdir(self.concurrentRawImageDir)
                for fileName in fileList:
                    filePath = os.path.join(self.concurrentRawImageDir, fileName)
                    self.logger.debug("removing %s file" % fileName)
                    os.remove(filePath)
            else:
                self.logger.debug("Creating %s directory" % self.concurrentRawImageDir)
                os.mkdir(self.concurrentRawImageDir)

            # capture an image
            self.captureJpegImage(imageName, needHalfPress)

            rawFilesList = os.listdir(self.concurrentRawImageDir)

            # extract the filename
            (fileName, ext) = os.path.splitext(imageName)
            rawFilePath = fileName + ".nvraw"

            # rename the file
            if (rawFilesList):
                os.rename(os.path.join(self.concurrentRawImageDir, rawFilesList[0]),
                          rawFilePath)
            else:
                self.logger.error("Couldn't capture concurrent RAW and jpeg")
                raise NvCSTestException("Couldn't capture concurrent RAW and jpeg")
        else:
            self.logger.error("Can't capture concurrent RAW and jpeg image with %s graph" %
                              self._obGraph.getGraphTypeString())
            raise NvCSTestException("Can't capture concurrent RAW and jpeg image with %s graph" %
                                    self._obGraph.getGraphTypeString())

    def captureJpegImage(self, imageName, needHalfPress=True):
        self.logger.debug("capturing jpeg image")
        graphType = self._obGraph.getGraphType()
        if(graphType == GraphType.JPEG):
            if(needHalfPress):
                self._obCamera.halfpress(10000)
                self._obCamera.waitForEvent(12000, nvcamera.CamConst_ALGS)
            self._obCamera.still(imageName)
            self._obCamera.waitForEvent(12000, nvcamera.CamConst_CAP_READY, nvcamera.CamConst_CAP_FILE_READY)
            if(needHalfPress):
                self._obCamera.hp_release()
        elif(graphType == GraphType.HOST_SENSOR):
            self._obCamera.still(imageName)
            self._obCamera.waitForEvent(12000, nvcamera.CamConst_CAP_READY, nvcamera.CamConst_CAP_FILE_READY)
            #self._obCamera.waitForEvent(12000, nvcamera.CamConst_ALL_CAPTURE_DONE)
        else:
            self.logger.error("Can't capture jpeg image with %s graph" %
                              self._obGraph.getGraphTypeString())
            raise NvCSTestException("Can't capture jpeg image with %s graph" %
                                    self._obGraph.getGraphTypeString())

    def startPreview(self):
        if(not self._isPreviewRunning):
            self._obCamera.startPreview()
            self._obCamera.waitForEvent(12000, nvcamera.CamConst_FIRST_PREVIEW_FRAME)
            self._isPreviewRunning = True

    def stopPreview(self):
        if(self._isPreviewRunning):
            self._obCamera.stopPreview()
            self._obCamera.waitForEvent(12000, nvcamera.CamConst_PREVIEW_EOS)
            self._isPreviewRunning = False


class NvCSTestBase(object):
    "NvCSTest Base Class"

    testID = None
    obCamera = None
    obGraph = None
    logger = None
    nvrf = None
    options = None
    testDir = "/data/NVCSTest"

    def __init__(self, logger, testID=None):
        self.testID = testID
        self.obGraph = NvCSTestGraph()
        self.obCamera = NvCSTestCamera(self.obGraph)
        self.testDir = os.path.join(self.testDir, testID)
        self.logger = logger
        self.logger.setClientName(testID)
        self.nvrf = nvcstestutils.NvCSRawFile()

    def run(self, args=None):
        retVal = 0
        try:
            self.logger.info("############################################")
            self.logger.info("Running the %s test..." % self.testID)
            self.logger.info("############################################")

            if(self.needAutoGraphSetup()):
                self.setupGraph()
                self.obGraph.createAndRunGraph()

            retVal = self.setupTest()
            if (retVal != NvCSTestResult.SUCCESS):
                self.logger.error("Test is not setup to run")
                self.cleanupTest()
                return retVal

            # create test specific dir
            if(os.path.exists(self.testDir)):
                shutil.rmtree(self.testDir)
            os.makedirs(self.testDir)

            retVal = self.runPreTest()
            if (retVal != NvCSTestResult.SUCCESS):
                self.cleanupTest()
                return retVal

            retVal = self.runTest(args)
            if (retVal != NvCSTestResult.SUCCESS):
                self.cleanupTest()
                return retVal

            retVal = self.runPostTest()
            if (retVal != NvCSTestResult.SUCCESS):
                self.cleanupTest()
                return retVal

            self.obGraph.stopAndDeleteGraph()
            return retVal

        except Exception, err:
            self.logger.error("TEST FAILURE: %s" % self.testID)
            self.logger.error("Error: %s" % str(err))
            traceback.print_exc()
            self.obGraph.stopAndDeleteGraph()
            raise

        return retVal

    def confirmPrompt(self, promptString):
        if(self.options.force == False):
            if(promptString != ''):
                self.logger.info("%s" % promptString)
            # ask user about the confirmation
            ans = raw_input("Press Enter or enter comments when the test setup is ready:")

            if (ans != ''):
                self.logger.info('User comments: %s' % ans)

        return True

    def needTestSetup(self):
        return False

    def getSetupString(self):
        return ''

    def needAutoGraphSetup(self):
        return True

    def setupTest(self):
        retVal = NvCSTestResult.SUCCESS
        if((self.options.force == False) and self.needTestSetup()):
            self.obCamera.startPreview()
            setupString = self.getSetupString()

            if(not self.confirmPrompt(setupString)):
                retVal = NvCSTestResult.ERROR
            self.obCamera.stopPreview()
        return retVal

    def cleanupTest(self):
        self.obGraph.stopAndDeleteGraph()

    def runTest(self, args=None):
        return NvCSTestResult.SUCCESS

    def runPreTest(self, args=None):
        return NvCSTestResult.SUCCESS

    def runPostTest(self, args=None):
        return NvCSTestResult.SUCCESS

    def setupGraph(self):
        return NvCSTestResult.SUCCESS

    def failIfAohdrEnabled(self):
        retVal = True
        aohdrEnabled = 0
        try:
            aohdrEnabled = self.obCamera.getAttr(nvcamera.attr_enableaohdr)
        except:
            self.logger.info("AOHDRenabled query failed.  Assuming aohdr off and proceeding with test.")
        if (aohdrEnabled > 0):
            raise NvCSTestException("RAW Capture not supported with AOHDR enabled.  Re-run test with AOHDR disabled.")
            retVal = False
        return retVal

    def AEOverride(self, gains=[-1]*4, exposure=-1, short_exposure=-1):
        # Note that the "-1" sentinel is not the greatest way to do this, but
        # we'll use it for now because it limits us to three args instead of 6,
        # which would be pretty unpleasant.  None of these should ever be
        # negative, so it should be fine for now.

        # Build the array of info to pass down.
        aeinfo = []
        # Analog Gains
        if (gains[0] >= 0):
            # AnalogGainsEnable
            aeinfo.append(1)
            # AnalogGains[4]
            aeinfo.append(gains)
        else:
            # AnalogGainsEnable
            aeinfo.append(0)
            # AnalogGains[4]
            aeinfo.append([0.0] * 4)
        # Digital Gains (NOT_SUPPORTED)
        if (False):
            # DigitalGainsEnable
            aeinfo.append(1)
            # DigitalGains[4]
            aeinfo.append([0.0] * 4)
        else:
            # DigitalGainsEnable
            aeinfo.append(0)
            # DigitalGains[4]
            aeinfo.append([0.0] * 4)
        # Exposures
        if (exposure >= 0 or short_exposure >= 0):
            # ExposureEnable
            aeinfo.append(1)
            # NoOfExposures
            if (exposure >= 0 and short_exposure >= 0):
                aeinfo.append(2)
            else:
                aeinfo.append(1)
            # Exposures[]
            exposures = []
            if (exposure >= 0):
                exposures.append(exposure)
            else:
                exposures.append(0.0)
            if (short_exposure >= 0):
                exposures.append(short_exposure)
            else:
                exposures.append(0.0)
            aeinfo.append(exposures)
        else:
            # ExposureEnable
            aeinfo.append(0)
            # NoOfExposures
            aeinfo.append(0)
            # Exposures
            aeinfo.append([0.0] * 2)

        try:
            self.obCamera.setAttr(nvcamera.attr_aeoverride, aeinfo)
        except:
            self.logger.info("\"attr_aecontrol\" not supported")
            return NvCSTestResult.ERROR

        return NvCSTestResult.SUCCESS

class NvCSTestConfiguration:
    filename = "undefined"
    testing = "undefined"
    attr_gain = -1
    attr_expo = -1
    attr_focuspos = -1
    attr_order = 0
    attr_sensorhdrratio = -1
    Avg = {"Color":[0.0, 0.0, 0.0],
            "Region":[0.0, 0.0, 0.0]}
    AvgNames = {"Color":["Red", "Green", "Blue"],
            "Region":["Top", "Middle", "Bottom"]}
    AvgPhaseR = 0.0
    AvgPhaseGR = 0.0
    AvgPhaseGB = 0.0
    AvgPhaseB = 0.0

    MaxPixValR = 0.0
    MaxPixValGR = 0.0
    MaxPixValGB = 0.0
    MaxPixValB = 0.0

    ratioR = 0.0
    ratioGR = 0.0
    ratioGB = 0.0
    ratioB = 0.0

    HDRIndex = []

    AvgPhaseRShort = 0.0
    AvgPhaseGRShort = 0.0
    AvgPhaseGBShort = 0.0
    AvgPhaseBShort = 0.0
    AvgShort = {"Color":[0.0, 0.0, 0.0],
            "Region":[0.0, 0.0, 0.0]}

    BitsPerPixel = 0
    errMargin = 0.001
    BlackLevelPadding = 10.0

    def __init__(self, subtest, filepath, exposure, gain, focus=450, short_exposure = -1, hdrindex = [[0, 0], [1, 1]]):
        self.attr_expo = exposure
        self.attr_gain = gain
        self.attr_focuspos = focus
        self.short_exposure = short_exposure
        self.testing = subtest
        self.filename = filepath
        self.Avg = {"Color":[0.0, 0.0, 0.0],
                    "Region":[0.0, 0.0, 0.0]}
        self.AvgPhaseR = 0.0
        self.AvgPhaseGR = 0.0
        self.AvgPhaseGB = 0.0
        self.AvgPhaseB = 0.0
        self.HDRIndex = hdrindex

    def processRawHDRAvgs(self, nvrf):

        #
        # Calculate image average per row based on center region
        #                    START
        #

        # get the readout scheme
        if (type(nvrf._hdrReadoutScheme) is str):
            if (len(nvrf._hdrReadoutScheme) == 4):
                print("HDR Readout Scheme: %s" % nvrf._hdrReadoutScheme)
                # decode the string
                for i in range(len(nvrf._hdrReadoutScheme)):
                    if (nvrf._hdrReadoutScheme[i] == 'L' or nvrf._hdrReadoutScheme[i] == 'l'):
                        self.HDRIndex[(i-i%2)/2][i%2] = 0
                    elif (nvrf._hdrReadoutScheme[i] == 'S' or nvrf._hdrReadoutScheme[i] == 's'):
                        self.HDRIndex[(i-i%2)/2][i%2] = 1
                    else:
                        print("\nSpecified Exposure Pattern \"%s\" contains invalid character \'%c\'" % (self.options.exposurePattern, nvrf._hdrReadoutScheme[i]))
                        print("Using default Exposure Pattern: \"LLSS\"\n")
                        self.HDRIndex = [[0, 0],[1, 1]]
                        break
            else:
                print("readout scheme does not have length of 4")
                print("Using default Exposure Pattern: \"LLSS\"\n")
                self.HDRIndex = [[0, 0],[1, 1]]
        else:
            print("Raw header has no HDR info")
            print("Using default Exposure Pattern: \"LLSS\"\n")
            self.HDRIndex = [[0, 0],[1, 1]]


        CenterRegionSize_W = 640
        CenterRegionSize_H = 480

        if (nvrf._width < CenterRegionSize_W):
            CenterRegionSize_W = nvrf._width
        if (nvrf._height < CenterRegionSize_H):
            CenterRegionSize_H = nvrf._height

        # round to even coordinates
        CenterRegionSize_W = (CenterRegionSize_W & (~1))
        CenterRegionSize_H = (CenterRegionSize_H & (~1))

        # Compute the starting point of the interesting region (even coordinates)
        CenterRegion_left = (((nvrf._width -
                            CenterRegionSize_W) / 2) & (~1))
        CenterRegion_top = (((nvrf._height -
                            CenterRegionSize_H) / 2) & (~1))
        CenterRegion_right = CenterRegion_left + CenterRegionSize_W
        CenterRegion_bottom = CenterRegion_top + CenterRegionSize_H

        w = CenterRegion_right - CenterRegion_left
        h = CenterRegion_bottom - CenterRegion_top

        bayerIndex = [0,0,0,0]

        # Long Exposure (default) Pixels
        AvgR = 0.0
        AvgG = 0.0
        AvgB = 0.0
        AvgPhaseR = 0.0
        AvgPhaseGR = 0.0
        AvgPhaseGB = 0.0
        AvgPhaseB = 0.0
        numPixels = 0.0
        # Short Exposure Pixels
        AvgRS = 0.0
        AvgGS = 0.0
        AvgBS = 0.0
        AvgPhaseRS = 0.0
        AvgPhaseGRS = 0.0
        AvgPhaseGBS = 0.0
        AvgPhaseBS = 0.0
        numPixelsS = 0.0

        bayerPhaseStr = nvrf._bayerPhase

        if (bayerPhaseStr == "GBRG"):
            # case NvColorFormat_X6Bayer10GBRG:
            # self.logger.debug("BayerPhase GBRG")
            IndexGb = 0;
            IndexB = 1;
            IndexR = 2;
            IndexGr = 3;
        elif (bayerPhaseStr == "RGGB"):
            # case NvColorFormat_X6Bayer10RGGB:
            # self.logger.debug("BayerPhase RGGB")
            IndexR = 0;
            IndexGr = 1;
            IndexGb = 2;
            IndexB = 3;
        elif (bayerPhaseStr == "BGGR"):
            # case NvColorFormat_X6Bayer10BGGR:
            # self.logger.debug("BayerPhase BGGR")
            IndexB = 0;
            IndexGb = 1;
            IndexGr = 2;
            IndexR = 3;
        elif (bayerPhaseStr == "GRBG"):
            # case NvColorFormat_X6Bayer10GRBG:
            # self.logger.debug("BayerPhase GRBG")
            IndexGr = 0;
            IndexR = 1;
            IndexB = 2;
            IndexGb = 3;
        else:
            # self.logger.error("BayerPhase Not Recognized '%s'", bayerPhaseStr)
            print "BayerPhase Not Recognized '%s'" % bayerPhaseStr

        for y in range(0, h, 2):
            for x in range(0, w, 2):
                bayerIndex[0] = nvrf._pixelData[(CenterRegion_top+y) * nvrf._width + (CenterRegion_left+x)]
                bayerIndex[1] = nvrf._pixelData[(CenterRegion_top+y) * nvrf._width + (CenterRegion_left+x)+1]
                bayerIndex[2] = nvrf._pixelData[((CenterRegion_top+y)+1) * nvrf._width + (CenterRegion_left+x)]
                bayerIndex[3] = nvrf._pixelData[((CenterRegion_top+y)+1) * nvrf._width + (CenterRegion_left+x)+1]

                if (self.HDRIndex[((CenterRegion_top + y)/2)%2][((CenterRegion_left + x)/2)%2] == 0):
                    AvgR +=  bayerIndex[IndexR]
                    AvgG +=  (bayerIndex[IndexGr] + bayerIndex[IndexGb])/2
                    AvgB +=  bayerIndex[IndexB]
                    AvgPhaseR +=  bayerIndex[IndexR]
                    AvgPhaseGR +=  bayerIndex[IndexGr]
                    AvgPhaseGB +=  bayerIndex[IndexGb]
                    AvgPhaseB +=  bayerIndex[IndexB]
                    numPixels += 1.0
                else:
                    AvgRS +=  bayerIndex[IndexR]
                    AvgGS +=  (bayerIndex[IndexGr] + bayerIndex[IndexGb])/2
                    AvgBS +=  bayerIndex[IndexB]
                    AvgPhaseRS +=  bayerIndex[IndexR]
                    AvgPhaseGRS +=  bayerIndex[IndexGr]
                    AvgPhaseGBS +=  bayerIndex[IndexGb]
                    AvgPhaseBS +=  bayerIndex[IndexB]
                    numPixelsS += 1.0

        AvgR /= numPixels
        AvgG /= numPixels
        AvgB /= numPixels
        AvgPhaseR /= numPixels
        AvgPhaseGR /= numPixels
        AvgPhaseGB /= numPixels
        AvgPhaseB /= numPixels

        AvgRS /= numPixelsS
        AvgGS /= numPixelsS
        AvgBS /= numPixelsS
        AvgPhaseRS /= numPixelsS
        AvgPhaseGRS /= numPixelsS
        AvgPhaseGBS /= numPixelsS
        AvgPhaseBS /= numPixelsS

        self.Avg["Color"][0] = AvgR
        self.Avg["Color"][1] = AvgG
        self.Avg["Color"][2] = AvgB
        self.AvgPhaseR = AvgPhaseR
        self.AvgPhaseGR = AvgPhaseGR
        self.AvgPhaseGB = AvgPhaseGB
        self.AvgPhaseB = AvgPhaseB

        self.AvgShort["Color"][0] = AvgRS
        self.AvgShort["Color"][1] = AvgGS
        self.AvgShort["Color"][2] = AvgBS
        self.AvgPhaseRShort = AvgPhaseRS
        self.AvgPhaseGRShort = AvgPhaseGRS
        self.AvgPhaseGBShort = AvgPhaseGBS
        self.AvgPhaseBShort = AvgPhaseBS

    def processRawAvgs(self, nvrf):

        #
        # Calculate image average based on center region
        #                    START
        #

        CenterRegionSize_W = 640
        CenterRegionSize_H = 480

        if (nvrf._width < CenterRegionSize_W):
            CenterRegionSize_W = nvrf._width
        if (nvrf._height < CenterRegionSize_H):
            CenterRegionSize_H = nvrf._height

        # round to even coordinates
        CenterRegionSize_W = (CenterRegionSize_W & (~1))
        CenterRegionSize_H = (CenterRegionSize_H & (~1))

        # Compute the starting point of the interesting region (even coordinates)
        CenterRegion_left = (((nvrf._width -
                            CenterRegionSize_W) / 2) & (~1))
        CenterRegion_top = (((nvrf._height -
                            CenterRegionSize_H) / 2) & (~1))
        CenterRegion_right = CenterRegion_left + CenterRegionSize_W
        CenterRegion_bottom = CenterRegion_top + CenterRegionSize_H

        w = CenterRegion_right - CenterRegion_left
        h = CenterRegion_bottom - CenterRegion_top

        bayerIndex = [0,0,0,0]
        AvgR = 0.0
        AvgG = 0.0
        AvgB = 0.0
        AvgPhaseR = 0.0
        AvgPhaseGR = 0.0
        AvgPhaseGB = 0.0
        AvgPhaseB = 0.0
        numPixels = 0.0

        bayerPhaseStr = nvrf._bayerPhase

        if (bayerPhaseStr == "GBRG"):
            # case NvColorFormat_X6Bayer10GBRG:
            # self.logger.debug("BayerPhase GBRG")
            IndexGb = 0;
            IndexB = 1;
            IndexR = 2;
            IndexGr = 3;
        elif (bayerPhaseStr == "RGGB"):
            # case NvColorFormat_X6Bayer10RGGB:
            # self.logger.debug("BayerPhase RGGB")
            IndexR = 0;
            IndexGr = 1;
            IndexGb = 2;
            IndexB = 3;
        elif (bayerPhaseStr == "BGGR"):
            # case NvColorFormat_X6Bayer10BGGR:
            # self.logger.debug("BayerPhase BGGR")
            IndexB = 0;
            IndexGb = 1;
            IndexGr = 2;
            IndexR = 3;
        elif (bayerPhaseStr == "GRBG"):
            # case NvColorFormat_X6Bayer10GRBG:
            # self.logger.debug("BayerPhase GRBG")
            IndexGr = 0;
            IndexR = 1;
            IndexB = 2;
            IndexGb = 3;
        else:
            # self.logger.error("BayerPhase Not Recognized '%s'", bayerPhaseStr)
            print "BayerPhase Not Recognized '%s'" % bayerPhaseStr

        for y in range(0, h, 2):
            for x in range(0, w, 2):
                bayerIndex[0] = nvrf.getPixelValue(CenterRegion_left+x,   CenterRegion_top+y)
                bayerIndex[1] = nvrf.getPixelValue(CenterRegion_left+x+1, CenterRegion_top+y)
                bayerIndex[2] = nvrf.getPixelValue(CenterRegion_left+x,   CenterRegion_top+y+1)
                bayerIndex[3] = nvrf.getPixelValue(CenterRegion_left+x+1, CenterRegion_top+y+1)

                AvgR +=  bayerIndex[IndexR]
                AvgG +=  (bayerIndex[IndexGr] + bayerIndex[IndexGb])/2
                AvgB +=  bayerIndex[IndexB]
                AvgPhaseR +=  bayerIndex[IndexR]
                AvgPhaseGR +=  bayerIndex[IndexGr]
                AvgPhaseGB +=  bayerIndex[IndexGb]
                AvgPhaseB +=  bayerIndex[IndexB]
                numPixels += 1.0

        AvgR /= numPixels
        AvgG /= numPixels
        AvgB /= numPixels
        AvgPhaseR /= numPixels
        AvgPhaseGR /= numPixels
        AvgPhaseGB /= numPixels
        AvgPhaseB /= numPixels

        self.Avg["Color"][0] = AvgR
        self.Avg["Color"][1] = AvgG
        self.Avg["Color"][2] = AvgB
        self.AvgPhaseR = AvgPhaseR
        self.AvgPhaseGR = AvgPhaseGR
        self.AvgPhaseGB = AvgPhaseGB
        self.AvgPhaseB = AvgPhaseB

        #
        # Calculate image average based on center region
        #                     END
        #

        #
        # Calculate image black levels (based on top, middle bottom)
        #                          START
        #
        BLCenterRegionSize_W  = 640
        BLCenterRegionSize_H  = 480
        if (nvrf._width < CenterRegionSize_W):
            BLCenterRegionSize_W = nvrf._width
        # Calculate row region of interest
        CenterRegion_left = (((nvrf._width -
                            CenterRegionSize_W) / 2) & (~1))
        CenterRegion_right = CenterRegion_left + CenterRegionSize_W

        w = CenterRegion_right - CenterRegion_left

        # First get top image average
        CenterRegion_top = 0
        BLtop = 0.0
        for x in range(0, w):
            BLtop += nvrf.getPixelValue(CenterRegion_left+x, CenterRegion_top)
        BLtop /= w
        self.Avg["Region"][0] = BLtop

        # Next get middle image average
        CenterRegion_top = nvrf._height/2
        BLmiddle = 0.0
        for x in range(0, w):
            BLmiddle += nvrf.getPixelValue(CenterRegion_left+x, CenterRegion_top)
        BLmiddle /= w
        self.Avg["Region"][1] = BLmiddle

        # Finally get bottom image average
        CenterRegion_top = nvrf._height*.9
        CenterRegion_top = int(CenterRegion_top)
        BLbottom = 0.0
        for x in range(0, w):
            BLbottom += nvrf.getPixelValue(CenterRegion_left+x, CenterRegion_top)
        BLbottom /= w
        self.Avg["Region"][2] = BLbottom
        #
        # Calculate image black levels (based on top, middle bottom)
        #                           END
        #

    @classmethod
    def processLinearityStats(self, testName, testsList, bias=[0.0, 0.0, 0.0]):
        TestSensorRangeMax = 0.95

        # Variables that will be returned
        OverExposed = False
        UnderExposed = False
        rSquared = [0.0,0.0,0.0]
        a = [0.0,0.0,0.0] # Slope
        b = [0.0,0.0,0.0] # Y intercept
        logStr = ""
        csvStr = ""

        # Additional statistics, but not used
        MaxPointError = [0.0, 0.0, 0.0]
        TotalError = [0.0, 0.0, 0.0]

        # Variables used for calculations
        ImageCount = 0
        SumC = [0.0,0.0,0.0]
        SumCSquared = [0.0,0.0,0.0]
        SumCxEv = [0.0,0.0,0.0]
        Ev = 0.0
        SumEv = 0.0
        SumEvSquared = 0.0

        #
        # Given N sets of (ExposureValue, Red), we are trying to solve a and b so
        # that sum of (Red - a*ExposureValue + b)^2 is minimal
        # So, we are fitting the data into a line: f(x) = ax + b where x is
        # ExposureValue and f(x) is Red.
        # We can solve this by the least linear square system: XB = Y =>
        # |x0 1|       |f(x0)|
        # |x1 1| |a|   |f(x1)|
        # | : 1| |b| = |  :  |
        # |xn 1|       |f(xn)|
        #
        # Multiply X^T to both side and we have
        # |x0^2 + ... + xn^2   x0 + ... + xn| |a|   |x0*f(x0) + ... + xn*f(xn)|
        # |x0 + ... + xn               n    | |b| = |f(x0) + ... + f(xn)      |
        #
        # Let SumXSquared = x0^2 + ... + xn^2
        #     SumX = x0 + ... + xn
        #     SumY = f(x0) + ... + f(xn)
        #     SumXY = x0*f(x0) + ... + xn*f(xn)
        # Then, we have two equations
        # SumXSquared * a + SumX * b = SumXY
        # SumX * a + n * b = SumY
        #
        # Therefore,
        # a = (SumX * SumY - n * SumXY) / (SumX * SumX - n * SumXSquared)
        # b = (SumY - SumX * a) / n
        #

        logStr += ("\nUsing a Bias:  R:%.1f, G:%.1f, B:%.1f\n" %
            (bias[0], bias[1], bias[2]))
        logStr += ("-----------------------\n")
        logStr += ("%s\n" % testName)
        logStr += ("-----------------------\n")
        logStr += ("%8s %8s %8s %7s %7s %7s %7s %7s %7s %5s\n" %
            ("Exposure", "Gain", "EV", "R", "G", "B", "R-BL", "G-BL", "B-BL", "Order"))
        csvStr += ("%8s, %8s, %8s, %7s, %7s, %7s, %7s, %7s, %7s, %5s\n" %
            ("Exposure", "Gain", "EV", "R", "G", "B", "R-BL", "G-BL", "B-BL", "Order"))

        MaxPixelVal = math.pow(2, testsList[0].BitsPerPixel) - 1.0
        # Iterate through to accumulate color information from each image
        for imageStat in testsList:
            if (imageStat.testing  == "BlackLevel"):
                logStr += ("\nI SHOULD NOT HAVE REACHED THIS CODE, NO BLACKLEVEL CONFIGS\n")
            if (imageStat.Avg["Color"][0] < (bias[0]+self.BlackLevelPadding)):
                UnderExposed = True
            if (imageStat.Avg["Color"][1] < (bias[1]+self.BlackLevelPadding)):
                UnderExposed = True
            if (imageStat.Avg["Color"][2] < (bias[2]+self.BlackLevelPadding)):
                UnderExposed = True

            AvgColor = [0.0, 0.0, 0.0]
            if (imageStat.Avg["Color"][0] > (TestSensorRangeMax*MaxPixelVal)):
                OverExposed = True
            if (imageStat.Avg["Color"][1] > (TestSensorRangeMax*MaxPixelVal)):
                OverExposed = True
            if (imageStat.Avg["Color"][2] > (TestSensorRangeMax*MaxPixelVal)):
                OverExposed = True

            AvgColor[0] = imageStat.Avg["Color"][0] - bias[0];
            AvgColor[1] = imageStat.Avg["Color"][1] - bias[1];
            AvgColor[2] = imageStat.Avg["Color"][2] - bias[2];

            # This saftey check would only be reached in the case of extreme underexposed images
            if (AvgColor[0] < 0.0): AvgColor[0] = 0.0
            if (AvgColor[1] < 0.0): AvgColor[1] = 0.0
            if (AvgColor[2] < 0.0): AvgColor[2] = 0.0

            Ev = imageStat.attr_expo * imageStat.attr_gain
            SumEv += Ev;
            SumEvSquared += (Ev * Ev);
            SumC[0] += AvgColor[0];
            SumC[1] += AvgColor[1];
            SumC[2] += AvgColor[2];
            SumCSquared[0] += AvgColor[0] * AvgColor[0];
            SumCSquared[1] += AvgColor[1] * AvgColor[1];
            SumCSquared[2] += AvgColor[2] * AvgColor[2];
            SumCxEv[0] += (AvgColor[0] * Ev);
            SumCxEv[1] += (AvgColor[1] * Ev);
            SumCxEv[2] += (AvgColor[2] * Ev);

            ImageCount+=1;

            logStr += ("%8.3f %8.3f %8.3f %7.1f %7.1f %7.1f %7.1f %7.1f %7.1f %5d\n" %
                (imageStat.attr_expo, imageStat.attr_gain, imageStat.attr_expo * imageStat.attr_gain,
                imageStat.Avg["Color"][0], imageStat.Avg["Color"][1], imageStat.Avg["Color"][2],
                    AvgColor[0], AvgColor[1], AvgColor[2], imageStat.attr_order))
            csvStr += ("%8.3f, %8.3f, %8.3f, %7.1f, %7.1f, %7.1f, %7.1f, %7.1f, %7.1f, %5d\n" %
                (imageStat.attr_expo, imageStat.attr_gain, imageStat.attr_expo * imageStat.attr_gain,
                imageStat.Avg["Color"][0], imageStat.Avg["Color"][1], imageStat.Avg["Color"][2],
                    AvgColor[0], AvgColor[1], AvgColor[2], imageStat.attr_order))

        # This is a legacy check. Leaving in, but not sure what is failure case
        # other than only running 1 capture
        if (SumEv * SumEv == SumEvSquared):
            logStr += ("\n\nInvalid State: SumEv * Sum Ev == SumEvSquared\n")
            logStr += ("\n\nInvalid State: This may be caused due to improper configurations used in the test\n")
            return logStr, rSquared, a, b, OverExposed, UnderExposed

        #This set of formulas apply linear regression
        for j in range(0, 3):
            t1 = 0
            t2 = 0
            tx = 0
            t3 = round((ImageCount * SumEvSquared - SumEv * SumEv), 3)
            a[j] = 0.0 if(t3 == 0.0) else (ImageCount * SumCxEv[j] - SumEv * SumC[j]) / t3
            b[j] = (SumC[j] - a[j] * SumEv) / ImageCount;

            t1 = ImageCount * SumEvSquared - SumEv * SumEv;
            t2 = ImageCount * SumCSquared[j] - SumC[j] * SumC[j];

            tx = (ImageCount * SumCxEv[j] - SumEv * SumC[j]);

            if ((t1 == 0) or (t2 == 0)):
                rSquared[j] = 0.0
            else:
                rSquared[j] = (tx * tx) / abs(t1 * t2);

        # Calculate the mean squared error
        # Not used, but just additional statistical information
        for imageStat in testsList:
            if (imageStat.testing  == "BlackLevel"):
                logStr += ("\nIT SHOULD NOT HAVE REACHED THIS CODE, NO BLACKLEVEL CONFIGS\n")
            AvgColor = [0.0, 0.0, 0.0]
            AvgColor[0] = imageStat.Avg["Color"][0] - bias[0];
            AvgColor[1] = imageStat.Avg["Color"][1] - bias[1];
            AvgColor[2] = imageStat.Avg["Color"][2] - bias[2];
            if (AvgColor[0] < 0.0): AvgColor[0] = 0.0
            if (AvgColor[1] < 0.0): AvgColor[1] = 0.0
            if (AvgColor[2] < 0.0): AvgColor[2] = 0.0

            for j in range(0, 3):
                e = (AvgColor[j] - a[j] * imageStat.attr_expo * imageStat.attr_gain - b[j]);
                e *= e;
                if (e > MaxPointError[j]):
                    MaxPointError[j] = e;
                    TotalError[j] += e;

        # Printing statisal results on image linearity
        for j in range(0, 3):
            colorName = ["Red", "Green", "Blue"]
            TotalError[j] /= ImageCount;

            if (j == 0):
                logStr += ("Color:      R^2   Line\n")

            logStr += ("%5s:  %7.5f   Y=%.2fX + %.2f\n" %
                (colorName[j], rSquared[j], a[j], b[j]))

        return logStr, rSquared, a, b, OverExposed, UnderExposed, csvStr

    @classmethod
    def calculateRSquared(self, testsList, xAttr, yAttrList, listSize):

        rSquared = []

        sumX = [0.0] * listSize
        sumY = [0.0] * listSize
        sumXSquared = [0.0] * listSize
        sumYSquared = [0.0] * listSize
        sumXY = [0.0] * listSize

        numItems = len(testsList)

        for listIndex in range(0, listSize):
            for testIndex in range(0, len(testsList)):
                x = getattr(testsList[testIndex], xAttr)
                y = getattr(testsList[testIndex], yAttrList)[listIndex]

                sumX[listIndex] += x
                sumY[listIndex] += y
                sumXSquared[listIndex] += x * x
                sumYSquared[listIndex] += y * y
                sumXY[listIndex] += x * y

            # Numerator of R
            numerator = numItems * sumXY[listIndex] - sumX[listIndex] * sumY[listIndex]

            # Denominator of R^2
            denominator = ((numItems * sumXSquared[listIndex] - sumX[listIndex] * sumX[listIndex]) *
                           (numItems * sumYSquared[listIndex] - sumY[listIndex] * sumY[listIndex]))

            # Calculate R^2
            rSqrd = float(numerator * numerator) / denominator
            rSquared.append(rSqrd)

        return rSquared

    @classmethod
    def processVariationStats(self, testName, testsList, attrStr, bias=[0.0, 0.0, 0.0]):
        attrVar = [0.0,0.0,0.0]
        Variation = [float("inf"), float("inf"), float("inf")]
        attrName = NvCSTestConfiguration.AvgNames[attrStr]
        ImageCount = 0
        Variance = [0.0, 0.0, 0.0]
        logStr = ""
        csvStr = ""
        MaxTracker = 0
        MinTracker = 1023

        logStr += ("\nBias used for calculating SNR:  R:%.1f, G:%.1f, B:%.1f\n" % (bias[0], bias[1], bias[2]))
        logStr += ("-----------------------\n")
        logStr += ("%s\n" % testName)
        logStr += ("-----------------------\n")
        logStr += ("%8s %8s %8s %7s %7s %7s %5s\n" % ("Exposure", "Gain", "EV", attrName[0],
                      attrName[1], attrName[2], "Order"))
        csvStr += ("%8s, %8s, %8s, %7s, %7s, %7s, %5s\n" % ("Exposure", "Gain", "EV", attrName[0],
                      attrName[1], attrName[2], "Order"))

        # attrVar is tracking the average of all frames
        for imageStat in testsList:
            attrVar[0] += imageStat.Avg[attrStr][0] - bias[0]
            attrVar[1] += imageStat.Avg[attrStr][1] - bias[1]
            attrVar[2] += imageStat.Avg[attrStr][2] - bias[2]

            logStr += ("%8.5f %8.3f %8.3f %7.1f %7.1f %7.1f %5d\n" %
                (imageStat.attr_expo, imageStat.attr_gain, imageStat.attr_expo * imageStat.attr_gain,
                imageStat.Avg[attrStr][0], imageStat.Avg[attrStr][1], imageStat.Avg[attrStr][2], imageStat.attr_order))
            csvStr += ("%8.5f, %8.3f, %8.3f, %7.1f, %7.1f, %7.1f, %5d\n" %
                (imageStat.attr_expo, imageStat.attr_gain, imageStat.attr_expo * imageStat.attr_gain,
                imageStat.Avg[attrStr][0], imageStat.Avg[attrStr][1], imageStat.Avg[attrStr][2], imageStat.attr_order))

            ImageCount +=1;

        attrVar[0] /= ImageCount;
        attrVar[1] /= ImageCount;
        attrVar[2] /= ImageCount;


        # Calculate the variance across all frames
        for imageStat in testsList:
            e =  attrVar[0] - (imageStat.Avg[attrStr][0] - bias[0]);
            Variance[0] += e * e;
            e =  attrVar[1] - (imageStat.Avg[attrStr][1] - bias[1]);
            Variance[1] += e * e;
            e =  attrVar[2] - (imageStat.Avg[attrStr][2] - bias[2]);
            Variance[2] += e * e;

        Variance[0] /= 1.0*ImageCount
        Variance[1] /= 1.0*ImageCount
        Variance[2] /= 1.0*ImageCount

        for j in range(0, 3):
            stdDev = math.sqrt(Variance[j])
            # SNR (units of dB). Typical imaging definition. Take raw S:N ratio
            # of this test's images, then apply the log-20 rule to convert to dB.
            if (attrVar[j] != 0):
                Variation[j] = 100 * stdDev / attrVar[j]
            else:
                Variation[j] = float("inf")

            if (j == 0):
                logStr += ("%5s  %7s  %7s\n" %
                    (attrStr, "Average", "Variation(%)"))

            logStr += ("%5s  %8.2f  %7.3f\n" %
                (attrName[j], attrVar[j], Variation[j]))

            # Also track the maximum and minimum of averages across images

            # Assuming 10 bits max...
            # MaxTracker was initialized to 0 to make this logic work
            # MinTracker was initialized to 1023 to make this logic work
            if(attrVar[j] > MaxTracker):
                MaxTracker = attrVar[j]
            if(attrVar[j] < MinTracker):
                MinTracker = attrVar[j]

        return logStr, Variation, MaxTracker, MinTracker, csvStr



    @classmethod
    def runConfig(self, test, testConfig):
        test.obCamera.startPreview()
        if(test.options.ignoreFocuser == False):
            if(test.obCamera.isFocuserSupported()):
                test.obCamera.setAttr(nvcamera.attr_continuousautofocus, 0)
                test.obCamera.setAttr(nvcamera.attr_autofocus, 0)
                test.obCamera.setAttr(nvcamera.attr_focuspos, testConfig.attr_focuspos)
        # set gain/exposures for HDR case
        if (testConfig.short_exposure >= 0):
            if(NvCSTestResult.SUCCESS != test.AEOverride([testConfig.attr_gain] * 4, testConfig.attr_expo, testConfig.short_exposure)):
                test.logger.error("unable to set AE parameters!")
                return NvCSTestResult.ERROR
        # set gain/exposure for NON-HDR case
        elif(NvCSTestResult.SUCCESS != test.AEOverride([testConfig.attr_gain] * 4, testConfig.attr_expo)):
            test.logger.info("Falling back to attr_exposuretime and attr_bayergains.")
            test.obCamera.setAttr(nvcamera.attr_exposuretime, testConfig.attr_expo)
            test.obCamera.setAttr(nvcamera.attr_bayergains, [testConfig.attr_gain] * 4)

        # take an image
        test.logger.info("capturing image with exposure time set to %.5f..." % testConfig.attr_expo)
        if (testConfig.short_exposure >= 0):
            test.logger.info("capturing image with short exposure time set to %.2f..." % testConfig.short_exposure)
        test.logger.info("capturing image with gains set to %.2f..." % testConfig.attr_gain)

        # take an image with specified ISO
        fileName = testConfig.filename
        rawFilePath = os.path.join(test.testDir, fileName + ".nvraw")
        try:
            test.obCamera.captureRAWImage(rawFilePath)
        except nvcamera.NvCameraException, err:
            if (err.value == nvcamera.NvError_NotSupported):
                test.logger.info("raw capture is not supported")
                return NvCSTestResult.SKIPPED
            else:
                raise

        test.obCamera.stopPreview()

        if not test.nvrf.readFile(rawFilePath):
            test.logger.error("couldn't open the file: %s" % rawFilePath)
            return NvCSTestResult.ERROR

        # Check no values over 2**bitsPerSample
        if (test.nvrf.getMaxPixelValue() >= (2**test.nvrf._bitsPerSample)):
            test.logger.error("pixel value is over %d." % 2**test.nvrf._bitsPerSample)
            return NvCSTestResult.ERROR

        # check SensorGain value
        if (abs(test.nvrf._sensorGains[0] - float(testConfig.attr_gain)) > 0.001):
            test.logger.error("SensorGains value is not correct in the raw header: %f" % test.nvrf._sensorGains[0])
            return NvCSTestResult.ERROR
        # check SensorExposure value
        if (abs(test.nvrf._exposureTime - testConfig.attr_expo) > 0.001):
            test.logger.error( "exposure value is not correct in the raw header...")
            return NvCSTestResult.ERROR
        expTime = test.obCamera.getAttr(nvcamera.attr_exposuretime)
        if (not ((expTime > testConfig.attr_expo - self.errMargin) and (expTime < testConfig.attr_expo + self.errMargin))):
            test.logger.error("exposuretime is not set in the driver...")
            return NvCSTestResult.ERROR

        self.BitsPerPixel = test.nvrf._bitsPerSample
        if (testConfig.testing == "HDRRatioTest" or testConfig.testing == "BlackLevelHDR"):
            testConfig.processRawHDRAvgs(test.nvrf)
        else:
            testConfig.processRawAvgs(test.nvrf)

        return NvCSTestResult.SUCCESS

    @classmethod
    def runConfigList(self, test, configList):
        for testConfig in configList:
            result = testConfig.runConfig(test, testConfig)
            # return ERROR or SKIPPED
            if (result != NvCSTestResult.SUCCESS):
                return result
        return NvCSTestResult.SUCCESS

