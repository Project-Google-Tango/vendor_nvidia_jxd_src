# Copyright (c) 2012-2013 NVIDIA Corporation.  All rights reserved.
#
# NVIDIA Corporation and its licensors retain all intellectual property
# and proprietary rights in and to this software, related documentation
# and any modifications thereto.  Any use, reproduction, disclosure or
# distribution of this software and related documentation without an express
# license agreement from NVIDIA Corporation is strictly prohibited.
#

from nvcstestcore import *
import nvcstestutils

class NvCSGainTest(NvCSTestBase):
    "Gain test"

    startGainVal = 0.0
    stopGainVal = 0.0
    step = 0.0
    errMargin = 10.0/100.0
    iterations = 11

    def __init__(self, options, logger):
        NvCSTestBase.__init__(self, logger, "Gain")
        self.options = options

    def setupGraph(self):
        self.obGraph.setImager(self.options.imager_id)
        self.obGraph.setGraphType(GraphType.RAW)

        return NvCSTestResult.SUCCESS

    def runTest(self, args):
        self.failIfAohdrEnabled()
        # get the gain range
        self.obCamera.startPreview()
        gainRange = self.obCamera.getAttr(nvcamera.attr_gainrange)
        # TODO: remove this check when Bug 1480305 is fixed
        if (gainRange[1] > 16):
            self.logger.warning("Max Sensor Gain Higher than 16 [%d] - waiting for next preview frame" % (gainRange[1]))
            time.sleep(0.150)
            gainRange = self.obCamera.getAttr(nvcamera.attr_gainrange)
            if (gainRange[1] > 16):
                self.logger.warning("Max Sensor Gain STILL Higher than 16 [%d] after wait. Continuing with test anyway." % (gainRange[1]))
        self.obCamera.stopPreview()
        self.logger.info("Gain test: camera queried gain range (%f, %f)" % (gainRange[0], gainRange[1]))
        self.startGainVal = gainRange[0]
        self.stopGainVal = gainRange[1]
        if(self.options.numTimes > 1):
            self.iterations = self.options.numTimes
        elif(self.options.numTimes == 1):
            self.logger.warning("User entered invalid number of samples (1).  Using default (%d)" % self.iterations)
        self.step = (gainRange[1] - gainRange[0])/(self.iterations - 1.0)
        if(gainRange[1] > 16):
            self.logger.warning("Max Sensor Gain Higher than 16 [%d]" % (gainRange[1]))

        while(self.startGainVal <= self.stopGainVal):
            self.obCamera.startPreview()
            self.obCamera.setAttr(nvcamera.attr_bayergains, [self.startGainVal] * 4)

            # take an image
            fileName = "%s_%s_test" % (self.testID, str(self.startGainVal).replace('.', '_'))
            rawFilePath = os.path.join(self.testDir, fileName + ".nvraw")
            self.logger.info("capturing raw image with gains set to %.2f..." % self.startGainVal)

            # capture raw image
            try:
                self.obCamera.captureRAWImage(rawFilePath, False)
            except nvcamera.NvCameraException, err:
                if (err.value == nvcamera.NvError_NotSupported):
                    self.logger.info("raw capture is not supported")
                    return NvCSTestResult.SKIPPED
                else:
                    raise

            self.obCamera.stopPreview()

            if not self.nvrf.readFile(rawFilePath):
                self.logger.error("couldn't open the file: %s" % rawFilePath)
                return NvCSTestResult.ERROR

            if (abs(self.nvrf._sensorGains[0] -  self.startGainVal) > self.errMargin):
                self.logger.error("SensorGains value is not correct in the raw header: %f" % self.nvrf._sensorGains[0])
                return NvCSTestResult.ERROR
            self.startGainVal = self.startGainVal + self.step

            # Check no values over 2**bitsPerSample
            if (self.nvrf.getMaxPixelValue() >= (2**self.nvrf._bitsPerSample)):
                self.logger.error("pixel value is over %d." % 2**self.nvrf._bitsPerSample)
                return NvCSTestResult.ERROR

        return NvCSTestResult.SUCCESS

class NvCSExposureTimeTest(NvCSTestBase):
    "Exposure Time Test"

    errMargin = 10.0/100.0
    iterations = 5

    def __init__(self, options, logger):
        NvCSTestBase.__init__(self, logger, "Exposure_Time")
        self.options = options

    def setupGraph(self):
        self.obGraph.setImager(self.options.imager_id)
        self.obGraph.setGraphType(GraphType.RAW)

        return NvCSTestResult.SUCCESS

    def runTest(self, args):
        self.failIfAohdrEnabled()
        if (args != None):
            self.exposureTimeValues = args

        # query and print exposuretime range
        # we need to start preview to get correct exposure time range
        self.obCamera.startPreview()
        exposureTimeRange = self.obCamera.getAttr(nvcamera.attr_exposuretimerange)
        self.obCamera.stopPreview()
        self.logger.info("exposuretime range: [%fsec, %fsec]" %  (exposureTimeRange[0], exposureTimeRange[1]))

        if(exposureTimeRange[0] <= 0 or exposureTimeRange[1] <= 0):
            self.logger.error("exposuretime range is invalid")
            return NvCSTestResult.ERROR

        retVal = NvCSTestResult.SUCCESS

        # clamp minimum exposure to 100us
        if (exposureTimeRange[0] >= 0.0001):
            startExpTimeValue = exposureTimeRange[0]
        else:
            startExpTimeValue = 0.0001
        stopExpTimeValue = exposureTimeRange[1]
        if(self.options.numTimes > 1):
            self.iterations = self.options.numTimes
        elif(self.options.numTimes == 1):
            self.logger.warning("User entered invalid number of samples (1).  Using default (%d)" % self.iterations)
        step = (stopExpTimeValue - startExpTimeValue)/(self.iterations - 1.0)

        while startExpTimeValue <= stopExpTimeValue:
            # take an image specified exposure time
            self.logger.info("capturing raw image with exposure time set to %fs..." % startExpTimeValue)
            self.obCamera.startPreview()

            self.obCamera.setAttr(nvcamera.attr_exposuretime, startExpTimeValue)

            fileName = ("%s_%.5f_test" % (self.testID, startExpTimeValue)).replace('.','_') \
                                                                          .replace('-','_')
            rawFilePath = os.path.join(self.testDir, fileName + ".nvraw")

            # capture raw image
            try:
                self.obCamera.captureRAWImage(rawFilePath, False)
            except nvcamera.NvCameraException, err:
                if (err.value == nvcamera.NvError_NotSupported):
                    self.logger.info("raw capture is not supported")
                    return NvCSTestResult.SKIPPED
                else:
                    raise

            self.obCamera.stopPreview()

            if not self.nvrf.readFile(rawFilePath):
                self.logger.error("couldn't open the file: %s" % rawFilePath)
                retVal = NvCSTestResult.ERROR
                break

            # Check no values over 2**bitsPerSample
            if (self.nvrf.getMaxPixelValue() >= (2**self.nvrf._bitsPerSample)):
                self.logger.error("pixel value is over %d." % 2**self.nvrf._bitsPerSample)
                retVal = NvCSTestResult.ERROR
                break

            minExpectedExpTime = startExpTimeValue - (startExpTimeValue * self.errMargin)
            maxExpectedExpTime = startExpTimeValue + (startExpTimeValue * self.errMargin)

            # check SensorExposure value
            if ((self.nvrf._exposureTime) < minExpectedExpTime or
                (self.nvrf._exposureTime) > maxExpectedExpTime):
                self.logger.error( "exposure time value is not correct in the raw header: %.6f" %
                                    self.nvrf._exposureTime)
                self.logger.error( "exposure time value should be between %.6f and %.6f" %
                                    (minExpectedExpTime, maxExpectedExpTime))
                retVal = NvCSTestResult.ERROR
                break

            expTime = self.obCamera.getAttr(nvcamera.attr_exposuretime)
            if (expTime < minExpectedExpTime or expTime > maxExpectedExpTime):
                self.logger.error("exposuretime is not set in the driver...")
                self.logger.error( "exposure value %.6f should be between %.6f and %.6f" %
                                    (expTime, minExpectedExpTime, maxExpectedExpTime))
                retVal = NvCSTestResult.ERROR
                break

            startExpTimeValue = startExpTimeValue + step

        return retVal

class NvCSLinearityRawTest(NvCSTestBase):
    "Linearity Raw Image Test"

    RunUsingClassicRanges = False

    GainStartVal = 2.0
    GainStopVal = 6.0
    GainExpo = 0.050
    GainStep = 1.0

    ExpTimeStart = 0.050
    ExpTimeStop = 0.250 # This is referenced for both query and non query runs
    ExpTimeGain = 2.0
    ExpTimeStep = 0.333

    qMaxExpTime = 0.0
    qMinExpTime = 0.0
    qMaxGain = 0.0
    qMinGain = 0.0

    defFocusPos = 450
    qMinFocusPos = 0
    qMaxFocusPos = 0
    NumImages = 6.0
    noShuffle = False
    TestSensorRangeMax = 0.95
    TestSensorGainRange = 0.60
    BayerPhaseConfirmed = True
    MaxPixelVal = 0

    def __init__(self, options, logger):
        NvCSTestBase.__init__(self, logger, "Linearity")
        self.options = options

    def needTestSetup(self):
        return True

    def getSetupString(self):
        return "\n\nThis test must be run with a controlled, uniformly lit scene.  Cover the sensor with the light source on its lowest setting.\n\n"

    def setupGraph(self):
        self.obGraph.setImager(self.options.imager_id)
        self.obGraph.setGraphType(GraphType.RAW)

        return NvCSTestResult.SUCCESS

    def getBlackLevel(self):

        # Test BlackLevel using minimum settings reported
        # Also test a smaller exposure as the driver should be limiting the config
        BLList = []
        BLList.append(NvCSTestConfiguration(
                "BlackLevel", "BL_%.5f_%.2f" % (self.qMinExpTime, self.qMinGain),
                self.qMinExpTime, self.qMinGain))
        BLList.append(NvCSTestConfiguration(
                "BlackLevel", "BL_%.5f_%.2f" % (self.qMinExpTime*0.1, self.qMinGain),
                self.qMinExpTime*0.1, self.qMinGain))

        self.logger.info("\n\nThe test will now take captures for black levels.\n\n")

        NvCSTestConfiguration.runConfigList(self, BLList)

        self.MaxPixelVal = math.pow(2, BLList[0].BitsPerPixel) - 1.0

        # Setting initial black levels to be overwritten
        BlackLevel = [self.MaxPixelVal, self.MaxPixelVal, self.MaxPixelVal]

        for imageStat in BLList:
            # Leaving in a sanity check of the blacklevel configuration
            if (imageStat.testing == "BlackLevel"):
                if(BlackLevel[0] > imageStat.Avg["Color"][0]):
                    BlackLevel[0] = imageStat.Avg["Color"][0]
                if(BlackLevel[1] > imageStat.Avg["Color"][1]):
                    BlackLevel[1] = imageStat.Avg["Color"][1]
                if(BlackLevel[2] > imageStat.Avg["Color"][2]):
                    BlackLevel[2] = imageStat.Avg["Color"][2]
            else:
                self.logger.info("\nIT SHOULD NOT HAVE REACHED THIS CODE. IT RAN INTO NON BLACKLEVEL CONFIG\n")

        self.logger.info("\nPerceived Black Level:  R:%.1f, G:%.1f, B:%.1f\tMax Possible Pixel Value: %d\n" %
            (BlackLevel[0], BlackLevel[1], BlackLevel[2], self.MaxPixelVal))

        return BlackLevel

    def runLinearity(self, configList, testName, bias):
        MIN_RSQUARED_ERROR = 0.970
        MIN_SLOPE = 10.0
        Linearity = True
        OverExposed = True
        UnderExposed = True
        logStr = ""
        rSquared = [0.0, 0.0, 0.0] # Correlation Coefficient
        a = [0.0, 0.0, 0.0] # Slope
        b = [0.0, 0.0, 0.0] # Y-intercept

        NvCSTestConfiguration.runConfigList(self, configList)
        # Assign Capture Order & Sort
        captureCounter = 1
        for imageStat in configList:
            imageStat.attr_order = captureCounter
            captureCounter += 1
        configList = sorted(configList, key=lambda entry: entry.attr_gain)
        configList = sorted(configList, key=lambda entry: entry.attr_expo)
        logStr, rSquared, a, b, OverExposed, UnderExposed, csvStr = NvCSTestConfiguration.processLinearityStats(testName, configList, bias)

        # Dump CSV String to File
        filePath = os.path.join(self.testDir, testName + ".csv")
        f = open(filePath, 'w')
        f.write(csvStr)
        f.close()

        # These two conditions should not be reached because of earlier environment check
        if (OverExposed):
            logStr += ("\n\nPlease make sure you are using the light panel to keep a controlled uniform lighting\n")
            logStr += ("The sample set may have been overexposed.  Re-Run the test with lower lights or darker colors.\n")
            Linearity = False
        if (UnderExposed):
            logStr += ("\n\nPlease make sure you are using the light panel to keep a controlled uniform lighting\n")
            logStr += ("The sample set may have been underexposed.  Re-Run the test with more light or brighter colors.\n")
            Linearity = False

        for j in range(0, 3):
            if (rSquared[j] < MIN_RSQUARED_ERROR):
                Linearity = False
                logStr += ("\n\n%s FAILED, minimum R^2 value is %f, minimum slope value is %f\n" % (testName, MIN_RSQUARED_ERROR, MIN_SLOPE))
                logStr += ("The minimum R^2 (correlation coefficient) value has not been met. (%f vs %f)\n" % (rSquared[j], MIN_RSQUARED_ERROR))
            if (a[j] < MIN_SLOPE):
                Linearity = False
                logStr += ("%s FAILED, minimum R^2 value is %f, minimum slope value is %f\n" % (testName, MIN_RSQUARED_ERROR, MIN_SLOPE))
                logStr += ("The minimum slope value has not been met. (%f vs %f)\n" % (a[j], MIN_SLOPE))

        return Linearity, logStr

    def runSNR(self, configList, testName, bias=[0.0, 0.0, 0.0]):
        # 3.16% coefficient of variation is equivalent to the old 30dB SNR figure.
        MIN_VARIATION_PERCENT = 3.16
        Linearity = True
        Variation = [float("inf"), float("inf"), float("inf")]
        MaxTracker = -1
        MinTracker = -1
        logStr = ""

        NvCSTestConfiguration.runConfigList(self, configList)
        # Assign Capture Order & Sort
        captureCounter = 1
        for imageState in configList:
            imageState.attr_order = captureCounter
            captureCounter += 1
        configList = sorted(configList, key=lambda entry: entry.attr_gain)
        configList = sorted(configList, key=lambda entry: entry.attr_expo)
        logStr, Variation, MaxTracker, MinTracker, csvStr = NvCSTestConfiguration.processVariationStats(testName, configList, "Color", bias)

        # Dump CSV String to File
        filePath = os.path.join(self.testDir, testName + ".csv")
        f = open(filePath, 'w')
        f.write(csvStr)
        f.close()

        for j in range(0, 3):
            if(Variation[j] > MIN_VARIATION_PERCENT):
                Linearity = False

        return Linearity, logStr

    def runEnvCheck(self, BlackLevel):
        if(self.options.useClassicRanges == True):
            self.logger.info("A command line option flag was used to run the classic hard coded ranges for the test.")
            self.logger.warning("Classic hard coded ranges are no longer supported.  Environment check will continue as usual.")

        # Find appropriate exposure value to use in testing the gain linearity
        GainStartVal = self.qMinGain
        GainStopVal = self.qMaxGain * self.TestSensorGainRange
        GainExpo = self.qMinExpTime

        UnderExposed = False
        OverExposed = False
        self.BayerPhaseConfirmed = True
        BlackLevelPadding = NvCSTestConfiguration.BlackLevelPadding

        # First determine the lowest exposure to run with minimum gain
        # We check that the lowest exposure configuration should be at
        # least BlackLevelPadding (10) pixel values higher than reference black level
        self.logger.info("\n\nDetermining exposure to run in gain linearity to avoid dark level noise.\n\n")

        # check upper limit first so we don't waste time churning on this
        # if the light is off.
        config = NvCSTestConfiguration("TestHalfMaxExposure",
            "envCheck_%.5f_%.2f_test" % (self.qMaxExpTime/2.0, GainStartVal),
            self.qMaxExpTime/2.0, GainStartVal)
        NvCSTestConfiguration.runConfig(self, config)

        self.logger.info("Determining exposure to run in gain linearity.  Checking Gain %f [%.2f, %.2f, %.2f]" %
            (GainStopVal, config.Avg["Color"][0], config.Avg["Color"][1], config.Avg["Color"][2]))

        if (config.Avg["Color"][0] < (BlackLevel[0]+BlackLevelPadding)):
            UnderExposed = True
        if (config.Avg["Color"][1] < (BlackLevel[1]+BlackLevelPadding)):
            UnderExposed = True
        if (config.Avg["Color"][2] < (BlackLevel[2]+BlackLevelPadding)):
            UnderExposed = True

        if(UnderExposed == True):
            # Add a print stating the reason for using dummy values
            self.logger.info("\n\nImage is underexposed with exposure of %f (half the queried maximum (%f))\n\n" %
                (self.qMaxExpTime/2.0, self.qMaxExpTime))
            self.logger.error("\n\nEnvironment Check Failed.  Check that test setup is correct.  Is the light turned on?\n\n")
            return NvCSTestResult.ERROR

        UnderExposed = True
        self.logger.info("Beginning search for best exposure.")
        while(UnderExposed == True):
            config = NvCSTestConfiguration("TestMinGain",
                    "envCheck_%.5f_%.2f_test" % (GainExpo, GainStartVal), GainExpo, GainStartVal)
            NvCSTestConfiguration.runConfig(self, config)
            UnderExposed = False
            self.logger.info("Determining exposure to run in gain linearity.  Checking %fs [%.2f, %.2f, %.2f]" %
                (GainExpo, config.Avg["Color"][0], config.Avg["Color"][1], config.Avg["Color"][2]))

            if (config.Avg["Color"][0] < (BlackLevel[0]+BlackLevelPadding)):
                UnderExposed = True
            if (config.Avg["Color"][1] < (BlackLevel[1]+BlackLevelPadding)):
                UnderExposed = True
            if (config.Avg["Color"][2] < (BlackLevel[2]+BlackLevelPadding)):
                UnderExposed = True

            if (UnderExposed == True):
                GainExpo += 0.010

            # If we have trouble grabbing a valid exposure setting, fail.
            # Arbitrarily picking half the range
            if (GainExpo > self.qMaxExpTime/2.0):
                self.logger.info("\n\nExposure has reached %f, which is greater than half the queried maximum (%f)\n\n" % (GainExpo, self.qMaxExpTime))
                self.logger.error("\n\nEnvironment Check Failed.  Check that test setup is correct.\n\n")
                return NvCSTestResult.ERROR

        # Check if highest desired gain will overexpose with
        # previously determined exposure setting
        config = NvCSTestConfiguration("TestMaxGain",
            "envCheck_%.5f_%.2f_test" % (GainExpo, GainStopVal), GainExpo, GainStopVal)
        NvCSTestConfiguration.runConfig(self, config)

        self.logger.info("Determining exposure to run in gain linearity.  Checking Gain %f [%.2f, %.2f, %.2f]" %
            (GainStopVal, config.Avg["Color"][0], config.Avg["Color"][1], config.Avg["Color"][2]))

        if (config.Avg["Color"][0] > (self.TestSensorRangeMax*self.MaxPixelVal)):
            OverExposed = True
        if (config.Avg["Color"][1] > (self.TestSensorRangeMax*self.MaxPixelVal)):
            OverExposed = True
        if (config.Avg["Color"][2] > (self.TestSensorRangeMax*self.MaxPixelVal)):
            OverExposed = True

        if(OverExposed == True):
            # Add a print stating the reason for using dummy values
            self.logger.info("\n\nValues captured with highest desired gain (%f) will overexpose the sensor.\n\n")
            self.logger.error("\n\nEnvironment Check Failed.  Check that test setup is correct.\n\n")
            return NvCSTestResult.ERROR
        self.GainStartVal = GainStartVal
        self.GainStopVal = GainStopVal
        self.GainStep = (GainStopVal-GainStartVal)/self.NumImages
        self.GainExpo = GainExpo

        # Gain test typically has shown greater intensity results, checking bayer order here
        percErr = (abs(config.AvgPhaseGR - config.AvgPhaseGB)/max(config.AvgPhaseGR, config.AvgPhaseGB))
        if(percErr > 1.0):
            self.logger.error("Average GR (%f) & Average GB (%f) have a %f%% error (1%% is Passing)" %
                (config.AvgPhaseGR, config.AvgPhaseGB, percErr))
            self.confirmPrompt("Average GR (%f) & Average GB (%f) should be approximately equal" %
                (config.AvgPhaseGR, config.AvgPhaseGB))
            self.BayerPhaseConfirmed = False
        if(config.AvgPhaseB < config.AvgPhaseR):
            self.logger.error("Average B (%f) should be GREATER than R (%f)." %
                (config.AvgPhaseB, config.AvgPhaseR))
            self.BayerPhaseConfirmed = False
        if(self.BayerPhaseConfirmed == False):
            self.confirmPrompt("The bayer phase could not automatically be validated.\n\
            Average B (%f) should be GREATER than R (%f)" %
                (config.AvgPhaseB, config.AvgPhaseR))

        # Find appropriate staring exposure value to use in testing exposure linearity

        ExpTimeStart = self.qMinExpTime
        ExpTimeStop = self.ExpTimeStop
        if(ExpTimeStop > self.qMaxExpTime):
            ExpTimeStop = self.qMaxExpTime
        ExpTimeGain = self.ExpTimeGain

        UnderExposed = True
        OverExposed = False

        # First determine the lowest exposure to run with desired gain
        # We check that the lowest exposure configuration should be at
        # least BlackLevelPadding (10) pixel values higher than reference black level
        self.logger.info("\n\nDetermining minimum exposure to run in exposure linearity to avoid dark level noise.\n\n")
        while(UnderExposed == True):
            config = NvCSTestConfiguration("TestMinExp",
                    "envCheck_%.5f_%.2f_test" % (ExpTimeStart, ExpTimeGain), ExpTimeStart, ExpTimeGain)
            NvCSTestConfiguration.runConfig(self, config)

            UnderExposed = False
            self.logger.info("Determining mininum exposure to run in exposure linearity.  Checking %fs [%.2f, %.2f, %.2f]" %
                (ExpTimeStart, config.Avg["Color"][0], config.Avg["Color"][1], config.Avg["Color"][2]))

            if (config.Avg["Color"][0] < (BlackLevel[0]+BlackLevelPadding)):
                UnderExposed = True
            if (config.Avg["Color"][1] < (BlackLevel[1]+BlackLevelPadding)):
                UnderExposed = True
            if (config.Avg["Color"][2] < (BlackLevel[2]+BlackLevelPadding)):
                UnderExposed = True

            if (UnderExposed == True):
                ExpTimeStart += 0.010

            # If we have trouble grabbing a valid exposure setting, fail.
            # Arbitrarily picking half the range
            if (ExpTimeStart > self.qMaxExpTime/2.0):
                self.logger.info("\n\nExposure has reached %f, which is greater than half the queried maximum (%f)\n\n" % (ExpTimeStart, self.qMaxExpTime))
                self.logger.error("\n\nEnvironment Check Failed.  Check that test setup is correct.\n\n")
                return NvCSTestResult.ERROR

        # Check if highest desired exposure will overexpose with
        # previously determined exposure setting
        config = NvCSTestConfiguration("TestMaxExp",
            "envCheck_%.5f_%.2f_test" % (ExpTimeStop, ExpTimeGain), ExpTimeStop, ExpTimeGain)
        NvCSTestConfiguration.runConfig(self, config)

        self.logger.info("Determining exposure to run in exposure linearity.  Checking with stop Exposure %fs [%.2f, %.2f, %.2f]" %
            (ExpTimeStop, config.Avg["Color"][0], config.Avg["Color"][1], config.Avg["Color"][2]))
        if (config.Avg["Color"][0] > (self.TestSensorRangeMax*self.MaxPixelVal)):
            OverExposed = True
        if (config.Avg["Color"][1] > (self.TestSensorRangeMax*self.MaxPixelVal)):
            OverExposed = True
        if (config.Avg["Color"][2] > (self.TestSensorRangeMax*self.MaxPixelVal)):
            OverExposed = True

        if(OverExposed == True):
            self.logger.info("\n\nValues captured with highest desired gain (%f) will overexpose the sensor.\n\n")
            self.logger.error("\n\nEnvironment Check Failed.  Check that test setup is correct.\n\n")
            return NvCSTestResult.ERROR

        self.ExpTimeStart = ExpTimeStart
        self.ExpTimeStop = ExpTimeStop
        self.ExpTimeStep = (ExpTimeStop-ExpTimeStart)/self.NumImages
        self.ExpTimeGain = ExpTimeGain

        return NvCSTestResult.SUCCESS

    def shuffleCmp(self, x, y):
        return x.filename > y.filename

    def retShuffleList(self, origList):
        shuffledList = []
        length = len(origList)
        origList.sort(reverse=True, cmp=self.shuffleCmp)

        for i in range(0, int(length/2)):
            if(i%2 == 0):
                shuffledList.append(origList[i])
                shuffledList.insert(0, origList[length-1-i])
            else:
                shuffledList.insert(0, origList[i])
                shuffledList.append(origList[length-1-i])
        if(length%2 != 0):
            shuffledList.append(origList[(length/2)])

        return shuffledList

    def runGainLinearity(self, BlackLevel=[0.0, 0.0, 0.0]):
        testGainLinearityList = []

        # Create the list of capture configurations
        # Using  a *1000 trick to utilize range()
        for gainVal in range(int(self.GainStartVal*1000), int(self.GainStopVal*1000), int(self.GainStep*1000)):
            testGainLinearityList.append(NvCSTestConfiguration(
                "GainLinearity",
                "G_%.3f_%.2f" % (self.GainExpo, gainVal/1000.0),
                self.GainExpo,
                gainVal/1000.0,
                self.qMinFocusPos))

        # Shuffle the configuration list to catch any buffered configuration cases (Bug 752686)
        if(self.noShuffle == False):
            testGainLinearityList = self.retShuffleList(testGainLinearityList)

        GainIsLinear, GainLog = self.runLinearity(testGainLinearityList, "Gain Linearity", BlackLevel)

        return GainIsLinear, GainLog

    def runExposureLinearity(self, BlackLevel=[0.0, 0.0, 0.0]):
        testExposureLinearityList = []

        # Create the list of capture configurations
        # Using  a *1000 trick to utilize range()
        for expoVal in range(int(self.ExpTimeStart*1000), int(self.ExpTimeStop*1000), int(self.ExpTimeStep*1000)):
            testExposureLinearityList.append(NvCSTestConfiguration(
                "ExposureLinearity",
                "E_%.3f_%.2f" % (expoVal/1000.0, self.ExpTimeGain),
                expoVal/1000.0,
                self.ExpTimeGain,
                self.qMinFocusPos))

        # Shuffle the configuration list to catch any buffered configuration cases (Bug 752686)
        if(self.noShuffle == False):
            testExposureLinearityList = self.retShuffleList(testExposureLinearityList)

        ExpoIsLinear, ExpoLog = self.runLinearity(testExposureLinearityList, "ExposureTime Linearity", BlackLevel)

        return ExpoIsLinear, ExpoLog

    def runEVLinearity(self, BlackLevel):
        testEVSnrList = []

        # Arbitrary limits for now
        maxEV = 0.8
        maxExp = 0.250

        EV = 0.55 * maxEV
        exp = EV / self.GainStopVal
        if (exp <  self.qMinExpTime):
            exp = self.qMinExpTime
        expStop = EV / self.GainStartVal
        if (expStop > maxExp):
            expStop = maxExp
        expStep = (expStop - exp)/self.NumImages

        for expoVal in range(int(exp*1000), int(expStop*1000), int(expStep*1000)):
            gainVal = EV/(expoVal/1000.0)
            testEVSnrList.append(NvCSTestConfiguration(
                "ExposureValueLinearity",
                "EV_%.3f_%.2f" % (expoVal/1000.0, gainVal),
                expoVal/1000.0,
                gainVal,
                self.qMinFocusPos))

        # Shuffle the configuration list to catch any buffered configuration cases (Bug 752686)
        if(self.noShuffle == False):
            testEVSnrList = self.retShuffleList(testEVSnrList)

        EVIsLinear, EVLog = self.runSNR(testEVSnrList, "Exposure Value Linearity", BlackLevel)

        return EVIsLinear, EVLog

    def runTest(self, args):
        self.logger.info("NVCS Linearity Test v2.1")
        self.failIfAohdrEnabled()

        if(self.options.numTimes > 1):
            self.NumImages = self.options.numTimes
        elif(self.options.numTimes == 1):
            self.logger.warning("User entered invalid number of samples (1).  Using default (%d)" % self.NumImages)

        self.noShuffle = self.options.noShuffle

        # Query exposure, gain, and focuser range
        self.obCamera.startPreview()
        gainRange = self.obCamera.getAttr(nvcamera.attr_gainrange)
        # TODO: remove this check when Bug 1480305 is fixed
        if (gainRange[1] > 16):
            self.logger.warning("Max Sensor Gain Higher than 16 [%d] - waiting for next preview frame" % (gainRange[1]))
            time.sleep(0.150)
            gainRange = self.obCamera.getAttr(nvcamera.attr_gainrange)
            if (gainRange[1] > 16):
                self.logger.warning("Max Sensor Gain STILL Higher than 16 [%d] after wait. Continuing with test anyway." % (gainRange[1]))
        exposureTimeRange = self.obCamera.getAttr(nvcamera.attr_exposuretimerange)
        if (self.obCamera.isFocuserSupported()):
            focusRange = self.obCamera.getAttr(nvcamera.attr_focuspositionphysicalrange)
        self.obCamera.stopPreview()

        if((gainRange[1] == 0) or (exposureTimeRange[1] == 0)):
            self.logger.info("")
            self.logger.info("")
            self.logger.error("TEST FAILED TO QUERY GAIN AND EXPOSURETIME RANGE!")
            self.logger.error("Make sure your driver supports range/limit queries like your reference driver")
            self.logger.info("")
            self.logger.info("")
            return NvCSTestResult.ERROR

        self.logger.info("Gain Range: [%f, %f]" % (gainRange[0], gainRange[1]))
        self.logger.info("ExposureTime Range: [%f sec, %f sec]" % (exposureTimeRange[0], exposureTimeRange[1]))
        if(gainRange[1] > 16):
            self.logger.warning("Max Sensor Gain Higher than 16 [%d]" % (gainRange[1]))

        self.qMinGain = gainRange[0]
        self.qMaxGain = gainRange[1]
        self.qMinExpTime = exposureTimeRange[0]
        self.qMaxExpTime = exposureTimeRange[1]
        if (self.obCamera.isFocuserSupported()):
            self.qMinFocusPos = focusRange[0]
            self.qMaxFocusPos = focusRange[1]

        # Take captures to reference as black level
        BlackLevel = self.getBlackLevel()

        # Process the test conditions
        goodEnv = self.runEnvCheck(BlackLevel)
        if(goodEnv == NvCSTestResult.ERROR):
            self.logger.info("")
            self.logger.info("")
            self.logger.info("It is possible that an auto feature may also be on, preventing the test from calibrating correctly.")
            self.logger.info("All auto features (ie AGC, AWB, AEC) should be turned off.")
            self.logger.info("")
            self.logger.info("It is possible that the driver is incorrect and unusable to take valid measurements.")
            self.logger.info("Look back at the log and see if the [R:#, G:#, B:#] values make sense relative to the other capture settings.")
            self.logger.info("There was a line specifying the BlackLevel reference being used, is it a valid estimate?")
            self.logger.info("Are the captures with the lowest exposure and gain settings the smallest of the 3 captures?")
            self.logger.info("If not, validate the requested camera settings from NVCS are reaching your driver correctly.")
            self.logger.info("")
            self.logger.info("To help the debugging efforts, try removing the capture shuffling order by adding the flags --nv --noshuffle")
            self.logger.info("To help the debugging efforts, try a different number of captures by adding the flags --nv -n <# of captures>")
            self.logger.info("")
            return NvCSTestResult.ERROR

        # Run the Gain Linearity Experiment

        self.logger.info("")
        self.logger.info("")
        self.logger.info("Starting Gain Linearity Test.")
        self.logger.info("")
        self.logger.info("")

        GainIsLinear, GainLog = self.runGainLinearity(BlackLevel)

        if (not GainIsLinear):
            self.logger.info("%s" % GainLog)
            self.logger.info("")
            self.logger.info("")
            self.logger.error("Gain Linearity Test failed.")
            self.logger.info("--Check if values are being written to the proper camera registers")
            self.logger.info("--Check if values are being translated correctly from floating-point values to register values")
            self.logger.info("--Ensure all exposure value auto features are disabled, enable manual modes (ie. AGC, AEC, AWB)")
            self.logger.info("--Check if test configuration gain values are reaching ODM correctly:")
            self.logger.info("\t-Add a print to confirm gain floating point values that are translated in the ODM driver (F32_TO_GAIN)")
            self.logger.info("\t-Compare printed floating point values from the ODM driver to the test configurations listed in the table")
            self.logger.info("\t\tIf the values are different, there is an issue outside of your driver.")
            self.logger.info("\t\tCheck if OMX or blocks-camera (camera/core) are missing logic from a patch")
            self.logger.info("")
            self.logger.info("Still not fixed?  Look at the data results to see what values don't make sense")
            self.logger.info("\t-A higher gain value should result in higher intensity values")
            self.logger.info("\t-Ensure you have not hit any boundary limitations of the sensor or another dependent register")
            self.logger.info("")
            self.logger.info("To help the debugging efforts, try removing the capture shuffling order by adding the flags --nv --noshuffle")
            self.logger.info("To help the debugging efforts, try a different number of captures by adding the flags --nv -n <# of captures>")
            self.logger.info("")
            self.logger.info("")
            return NvCSTestResult.ERROR

        self.logger.info("")
        self.logger.info("")
        self.logger.info("Gain Linearity Test Passed.")
        self.logger.info("")
        self.logger.info("")

        # Run the Exposure Linearity Experiment

        self.logger.info("")
        self.logger.info("")
        self.logger.info("Starting Exposure Linearity Test.")
        self.logger.info("")
        self.logger.info("")

        ExpoIsLinear, ExpoLog = self.runExposureLinearity(BlackLevel)

        if (not ExpoIsLinear):
            self.logger.info("%s" % GainLog)
            self.logger.info("%s" % ExpoLog)
            self.logger.info("")
            self.logger.info("")
            self.logger.error("Exposure Linearity Test failed.")
            self.logger.info("--Check if values are being written to the proper camera registers")
            self.logger.info("--Ensure all exposure value auto features are turned off, we want manual modes (ie. AGC, AEC, AWB)")
            self.logger.info("--Check if test configuration exposure values are reaching ODM correctly:")
            self.logger.info("\t-Add a print to confirm exposure time floating point values that are translated in the ODM driver (WriteExposure, GroupHold, SetMode)")
            self.logger.info("\t-Compare printed floating point values from the ODM driver to the test configurations listed in the table")
            self.logger.info("\t\tIf the values are different, there is an issue outside of your driver.")
            self.logger.info("\t\tCheck if OMX or blocks-camera (camera/core) are missing logic from a patch")
            self.logger.info("")
            self.logger.info("Still not fixed?  Look at the data results to see what values don't make sense")
            self.logger.info("\t-A higher exposure value should result in higher intensity values")
            self.logger.info("\t-Ensure you have not hit any boundary limitations of the sensor or another dependent register")
            self.logger.info("\t\t-A common boundary is a margin of padding between the framelength (VTS) and coarse time (exposure)")
            self.logger.info("")
            self.logger.info("To help the debugging efforts, try removing the capture shuffling order by adding the flags --nv --noshuffle")
            self.logger.info("To help the debugging efforts, try a different number of captures by adding the flags --nv -n <# of captures>")
            self.logger.info("")
            self.logger.info("")
            return NvCSTestResult.ERROR

        self.logger.info("")
        self.logger.info("")
        self.logger.info("Exposure Linearity Test Passed.")
        self.logger.info("")
        self.logger.info("")

        # Run the ExposureValue Linearity Experiment

        self.logger.info("")
        self.logger.info("")
        self.logger.info("Starting Exposure Value Linearity Test.")
        self.logger.info("")
        self.logger.info("")

        EVIsLinear, EVLog = self.runEVLinearity(BlackLevel)

        if (not EVIsLinear):
            self.logger.info("%s" % GainLog)
            self.logger.info("%s" % ExpoLog)
            self.logger.info("%s" % EVLog)
            self.logger.info("")
            self.logger.info("")
            self.logger.error("Exposure Value Linearity Test failed.")
            self.logger.info("The pixel channel values (R,G,B) values here should have remained constant throughout the test")
            self.logger.info("--Check if test configuration exposure and gain values are reaching ODM correctly:")
            self.logger.info("--Double Check gain values are translated correctly")
            self.logger.info("--Do you think the test is requesting too high of a resolution of your sensor?")
            self.logger.info("\t\tThe sensor does not have to match the exact resolution of the test to pass, but")
            self.logger.info("\t\tYou can use the '--classic' option flag to run  an 'easier' range.")
            self.logger.info("--If it passed the previous Gain and Exposure Linearity tests, try increasing brightness")
            self.logger.info("  to reduce possible low level noises")
            self.logger.info("")
            self.logger.info("To help the debugging efforts, try removing the capture shuffling order by adding the flags --nv --noshuffle")
            self.logger.info("To help the debugging efforts, try a different number of captures by adding the flags --nv -n <# of captures>")
            self.logger.info("")
            self.logger.info("")
            return NvCSTestResult.ERROR

        self.logger.info("")
        self.logger.info("")
        self.logger.info("Exposure Value Linearity Test Passed.")
        self.logger.info("")
        self.logger.info("")

        self.logger.info("Test Data Results:")
        self.logger.info("%s" % GainLog)
        self.logger.info("%s" % ExpoLog)
        self.logger.info("%s" % EVLog)

        if(self.RunUsingClassicRanges == True):
            self.logger.info("")
            self.logger.info("")
            self.logger.info("**The test was completed using the classic hard coded ranges.")
            if(self.options.useClassicRanges == True):
                self.logger.info("**This was done in accordance to the option parameter flag --classic being set")
            else:
                self.logger.info("**This was done because it ran into underexposure OR overexposure discrepancies")
                self.logger.info("**This does not mean the driver is bad, but it should be noted")
                # TODO
                # Depending on how frequent we run into the underexposure and overexposure discrepancy, will
                # determine how critical it is to implement this test to be more dynamic at varying
                # light conditions
            self.logger.info("")
            self.logger.info("")

        if(self.BayerPhaseConfirmed == False):
            self.logger.info("")
            self.logger.info("")
            self.logger.info("**The Bayer Phase could not automatically be validated in this test.")
            self.logger.info("**Please confirm the bayer phase is correct in the ODM driver.")
            self.logger.info("**You have passed linearity, but there is a phase discrepancy.")
            self.logger.info("")
            self.logger.info("")

        return NvCSTestResult.SUCCESS

class NvCSLinearityV3Test(NvCSTestBase):
    "LinearityV3 Raw Image Test"

    CalibExpoStart = 0.0

    ExpTimeLow = 0.050
    ExpTimeHigh = 0.250 # This is referenced for both query and non query runs
    GainLow = 0.0
    GainHigh = 0.0

    qMaxExpTime = 0.0
    qMinExpTime = 0.0
    qMaxGain = 0.0
    qMinGain = 0.0

    defFocusPos = 450
    NumImages = 6.0
    TestSensorRangeMax = 0.95
    TestSensorGainRange = 0.60
    BayerPhaseConfirmed = True
    MaxPixelVal = 0

    def __init__(self, options, logger):
        NvCSTestBase.__init__(self, logger, "Linearity3")
        self.options = options

    def needTestSetup(self):
        return True

    def getSetupString(self):
        return "\n\nThis test is best ran with a controlled uniform neutral lighting scene. Please cover the sensor with the provided light panel TURNED OFF. A second prompt will ask you to turn on after black level captures.\n\n"

    def setupGraph(self):
        self.obGraph.setImager(self.options.imager_id)
        self.obGraph.setGraphType(GraphType.RAW)

        return NvCSTestResult.SUCCESS

    def testBasicExposure(self):
        exposurePass = True
        logStr = ""

        self.logger.info("\n\nRunning basic sensor exposure configuration test first...\n\n")

        ###########################################################
        #### Capture reference images to check sensor response ####
        ###########################################################

        # Config 0 is base minimum gain and minimum exposuretime
        GainVal = self.qMinGain
        ExpoVal = self.qMinExpTime
        config0 = NvCSTestConfiguration("Basic",
            "basic_%.5f_%.2f_test" % (ExpoVal, GainVal), ExpoVal, GainVal)
        NvCSTestConfiguration.runConfig(self, config0)
        # Config 1 is minimum gain and high exposuretime
        GainVal = self.qMinGain
        ExpoVal = 0.200 #self.qMaxExpTime
        if (ExpoVal > self.qMaxExpTime):
            ExpoVal = self.qMaxExpTime
        config1 = NvCSTestConfiguration("Basic",
            "basic_%.5f_%.2f_test" % (ExpoVal, GainVal), ExpoVal, GainVal)
        NvCSTestConfiguration.runConfig(self, config1)

        ###########################################################
        ### Checking EXPOSURE TIME response values will change ####
        ###########################################################

        # Checking all 3 color channels independently
        for idx in range(0,3):
            percent_d = ((config1.Avg["Color"][idx] - config0.Avg["Color"][idx])/config0.Avg["Color"][idx])
            # Let's require at least a 20% change
            if(math.fabs(percent_d) < 0.20):
                logStr += ("\n")
                logStr += ("Pixel intensity didn't change as much as expected\n")
                logStr += ("Old Exposure time (%fs) gave a pixel intensity of %f\n" % (config0.attr_expo, config0.Avg["Color"][idx]))
                logStr += ("New Exposure time (%fs) gave a pixel intensity of %f\n" % (config1.attr_expo, config1.Avg["Color"][idx]))
                logStr += ("\n")
                exposurePass = False
                break
            elif(percent_d < 0):
                logStr += ("\n")
                logStr += ("Pixel intensity value didn't INCREASE as expected.\n")
                logStr += ("Old Exposure time (%fs) gave a pixel intensity of %f\n" % (config0.attr_expo, config0.Avg["Color"][idx]))
                logStr += ("New Exposure time (%fs) gave a pixel intensity of %f\n" % (config1.attr_expo, config1.Avg["Color"][idx]))
                logStr += ("\n")
                exposurePass = False
                break

        if(exposurePass):
            self.logger.info("\n\nBasic Exposure time test passed, it is changing when communicating a new exposure time configuration\n\n")
        else:
            logStr += ("\n")
            logStr += ("Basic Exposure time test failed.\n")
            logStr += ("-Check that all auto algorithm features are turned off (AWB, AE, AGC)\n")
            logStr += ("-Check that the exposure times are actually being sent to the camera device in your kernel driver.\n")
            logStr += ("-Check that the exposure times are being translated appropriately to the form the camera device expects\n")
            logStr += ("The driver should be able to support exposure times outside of its minimum framelength (or VTS), we acknowledge that the framerate may slower than nominal values for those modes in these situations.\n")
            logStr += ("\n")


        return exposurePass, logStr

    def testBasicGain(self):
        gainPass = True
        logStr = ""

        self.logger.info("\n\nRunning basic sensor gain configuration test first...\n\n")

        ###########################################################
        #### Capture reference images to check sensor response ####
        ###########################################################

        # Config 0 is base minimum gain and minimum exposuretime
        GainVal = self.qMinGain
        ExpoVal = self.CalibExpoStart
        config0 = NvCSTestConfiguration("Basic",
            "basic_%.5f_%.2f_test" % (ExpoVal, GainVal), ExpoVal, GainVal)
        NvCSTestConfiguration.runConfig(self, config0)
        # Config 1 is high gain and minimum exposuretime
        GainVal = self.qMaxGain
        ExpoVal = self.CalibExpoStart
        config1 = NvCSTestConfiguration("Basic",
            "basic_%.5f_%.2f_test" % (ExpoVal, GainVal), ExpoVal, GainVal)
        NvCSTestConfiguration.runConfig(self, config1)


        #######################################################
        ###### Checking GAIN response values will change ######
        #######################################################

        # Checking all 3 color channels independently
        for idx in range(0,3):
            percent_d = ((config1.Avg["Color"][idx] - config0.Avg["Color"][idx])/config0.Avg["Color"][idx])
            # Let's require at least a 20% change
            if(math.fabs(percent_d) < 0.20):
                logStr += ("\n")
                logStr += ("Pixel intensity didn't change as much as expected\n")
                logStr += ("Old gain setting of (%fs) gave a pixel intensity of %f\n" % (config0.attr_gain, config0.Avg["Color"][idx]))
                logStr += ("New gain setting of(%fs) gave a pixel intensity of %f\n" % (config1.attr_gain, config1.Avg["Color"][idx]))
                logStr += ("\n")
                gainPass = False
                break
            elif(percent_d < 0):
                logStr += ("\n")
                logStr += ("Pixel intensity value didn't INCREASE as expected.\n")
                logStr += ("Old gain setting of (%fs) gave a pixel intensity of %f\n" % (config0.attr_gain, config0.Avg["Color"][idx]))
                logStr += ("New gain setting of (%fs) gave a pixel intensity of %f\n" % (config1.attr_gain, config1.Avg["Color"][idx]))
                logStr += ("\n")
                gainPass = False
                break

        if(gainPass):
            self.logger.info("")
            self.logger.info("Basic Gain test passed, it is changing when communicating a new gain configuration\n")
            self.logger.info("")
        else:
            logStr += ("\n")
            logStr += ("Gain test failed.\n")
            logStr += ("-Check that all auto algorithm features are turned off (AWB, AE, AGC)\n")
            logStr += ("-Check that the gain values are actually being sent to the camera device in your kernel driver.\n")
            logStr += ("-Check that the gain values are being translated appropriately to the form the camera device expects\n")
            logStr += ("\n")


        return gainPass, logStr

    def getBlackLevel(self):

        # Test BlackLevel using minimum settings reported
        # Also test a smaller exposure as the driver should be limiting the config
        BLList = []
        BLList.append(NvCSTestConfiguration(
                "BlackLevel", "BL_%.5f_%.2f" % (self.qMinExpTime, self.qMinGain),
                self.qMinExpTime, self.qMinGain))
        BLList.append(NvCSTestConfiguration(
                "BlackLevel", "BL_%.5f_%.2f" % (self.qMinExpTime*0.1, self.qMinGain),
                self.qMinExpTime*0.1, self.qMinGain))

        self.logger.info("\n\nThe test will now take captures for black levels.\n\n")

        NvCSTestConfiguration.runConfigList(self, BLList)

        self.MaxPixelVal = math.pow(2, BLList[0].BitsPerPixel) - 1.0

        # Setting initial black levels to be overwritten
        BlackLevel = [self.MaxPixelVal, self.MaxPixelVal, self.MaxPixelVal]

        for imageStat in BLList:
            # Leaving in a sanity check of the blacklevel configuration
            if (imageStat.testing == "BlackLevel"):
                if(BlackLevel[0] > imageStat.Avg["Color"][0]):
                    BlackLevel[0] = imageStat.Avg["Color"][0]
                if(BlackLevel[1] > imageStat.Avg["Color"][1]):
                    BlackLevel[1] = imageStat.Avg["Color"][1]
                if(BlackLevel[2] > imageStat.Avg["Color"][2]):
                    BlackLevel[2] = imageStat.Avg["Color"][2]
            else:
                self.logger.info("\nIT SHOULD NOT HAVE REACHED THIS CODE. IT RAN INTO NON BLACKLEVEL CONFIG\n")

        self.logger.info("\nPerceived Black Level:  %.1f, %.1f, %.1f\tMax Pixel Intensity: %d\n" %
            (BlackLevel[0], BlackLevel[1], BlackLevel[2], self.MaxPixelVal))

        return BlackLevel

    def checkLinearity(self, testName, configList, bias, minRSquared, minSlope):
        Linearity = True
        OverExposed = True
        UnderExposed = True
        logStr = ""
        rSquared = [0.0, 0.0, 0.0] # Correlation Coefficient
        a = [0.0, 0.0, 0.0] # Slope
        b = [0.0, 0.0, 0.0] # Y-intercept

        logStr, rSquared, a, b, OverExposed, UnderExposed, _ = NvCSTestConfiguration.processLinearityStats(testName, configList, bias)

        # Checking each color channel independently
        for j in range(0, 3):
            if (rSquared[j] < minRSquared):
                Linearity = False
                logStr += ("")
                logStr += ("")
                logStr += ("\n\n%s FAILED, minimum R^2 value is %f, minimum slope value is %f\n" % (testName, minRSquared, minSlope))
                logStr += ("The minimum R^2 (correlation coefficient) value has not been met. (%f vs %f)\n" % (rSquared[j], minRSquared))
            if (a[j] < minSlope):
                Linearity = False
                logStr += ("")
                logStr += ("")
                logStr += ("%s FAILED, minimum R^2 value is %f, minimum slope value is %f\n" % (testName, minRSquared, minSlope))
                logStr += ("The minimum slope value has not been met. (%f vs %f)\n" % (a[j], minSlope))

        return Linearity, logStr

    def runSNR(self, configList, testName, bias=[0.0, 0.0, 0.0]):
        # 3.16% coefficient of variation is equivalent to the old 30dB SNR figure.
        MAX_VARIATION_PERCENT = 3.16
        Linearity = True
        Variation = [float("inf"), float("inf"), float("inf")]
        MaxTracker = -1
        MinTracker = -1
        logStr = ""

        NvCSTestConfiguration.runConfigList(self, configList)
        logStr, Variation, MaxTracker, MinTracker = NvCSTestConfiguration.processVariationStats(testName, configList, "Color", bias)

        for j in range(0, 3):
            if(Variation[j] > MAX_VARIATION_PERCENT):
                Linearity = False

        return Linearity, logStr

    def runExpCalibration(self, BlackLevel):
        # Find appropriate exposure value to use in testing the gain linearity
        MinGain = self.qMinGain
        CurExpo= self.qMinExpTime

        UnderExposed = True
        ForceContinue = False
        BlackLevelPadding = NvCSTestConfiguration.BlackLevelPadding

        # First determine the lowest exposure to run with minimum gain
        # We check that the lowest exposure configuration should be at
        # least BlackLevelPadding (10) pixel values higher than reference black level
        self.logger.info("\n\nDetermining starting exposure to run in linearity tests avoid dark level noise.\n\n")
        while(UnderExposed == True):
            config = NvCSTestConfiguration("TestMinGain",
                    "calib_%.5f_%.2f_test" % (CurExpo, MinGain), CurExpo, MinGain)
            NvCSTestConfiguration.runConfig(self, config)

            UnderExposed = False
            self.logger.info("Determining exposure to run in gain linearity.  Checking %fs [%.2f, %.2f, %.2f]" %
                (CurExpo, config.Avg["Color"][0], config.Avg["Color"][1], config.Avg["Color"][2]))

            if (config.Avg["Color"][0] < (BlackLevel[0]+BlackLevelPadding)):
                UnderExposed = True
            if (config.Avg["Color"][1] < (BlackLevel[1]+BlackLevelPadding)):
                UnderExposed = True
            if (config.Avg["Color"][2] < (BlackLevel[2]+BlackLevelPadding)):
                UnderExposed = True

            if (UnderExposed == True):
                CurExpo += 0.010

            # If we have trouble grabbing a valid exposure setting, try
            # Dummy settings.  Arbitrarily picking half the range
            if ((CurExpo > self.qMaxExpTime/2.0) and (ForceContinue == False)):
                self.logger.info("\n\nExposure has reached %f, which is greater than half the queried maximum (%f)\n\n" % (CurExpo, self.qMaxExpTime))
                cont = self.confirmPrompt("Does the log with the respective color intensity values make sense with increasing exposure? Enter 'no' without the quotes to stop the test.  Entering anything else will continue to increase the exposure.")
                if (cont == True):
                    ForceContinue = True
                else: #cont == False
                    return NvCSTestResult.ERROR


        self.CalibExpoStart = CurExpo

        return NvCSTestResult.SUCCESS

    def shuffleCmp(self, x, y):
        return x.filename > y.filename

    def retShuffleList(self, origList):
        shuffledList = []
        length = len(origList)
        origList.sort(reverse=True, cmp=self.shuffleCmp)

        for i in range(0, int(length/2)):
            if(i%2 == 0):
                shuffledList.append(origList[i])
                shuffledList.insert(0, origList[length-1-i])
            else:
                shuffledList.insert(0, origList[i])
                shuffledList.append(origList[length-1-i])
        if(length%2 != 0):
            shuffledList.append(origList[(length/2)])

        return shuffledList

    def testGainLinearityRange(self, configList, BlackLevel, curIndex, minRSquared, minSlope):
        isLinear = False
        wasOverExposed = False
        analyzeList = []
        logStr = ("")

        for testConfig in configList:
            testConfig.runConfig(self, testConfig)

            OverExposedThreshold = (math.pow(2, testConfig.BitsPerPixel) - 1.0) * self.TestSensorRangeMax

            wasOverExposed = False
            for idx in range(0,3):
                if (testConfig.Avg["Color"][idx] > OverExposedThreshold):
                    wasOverExposed = True

            # if not overexposed, append and move on to next
            if(not wasOverExposed):
                analyzeList.append(testConfig)
                curIndex += 1

            # if overexposed, leave index alone, and continue to the test
            else:
                break

        # If loop finished without being overexposed,
        # it wasn't tested according to previous logic, so test now.
        if(len(analyzeList) > 2):
            isLinear, logStr = self.checkLinearity(analyzeList[0].testing, analyzeList, BlackLevel, minRSquared, minSlope)
        #else:
            # Boundary conditions... overexposed early... already handled outside this function

        return logStr, isLinear, wasOverExposed, curIndex

    def runGainLinearity(self, BlackLevel=[0.0, 0.0, 0.0]):
        MIN_RSQUARED_ERROR = 0.970
        MIN_SLOPE = 10.0
        testName = "GainLinearity"
        testGainLinearityList = []
        logStr = ("")

        CurGainLow = self.qMinGain
        CurGainHigh = self.qMaxGain

        CurGainStep = (CurGainHigh - CurGainLow)/self.NumImages
        CurExpTime = self.CalibExpoStart

        # Will fail when first config in the upper range fails
        HalfGainRangePassed = False

        # Create the list of capture configurations
        # Using  a *1000 trick to utilize range()
        gainVal = CurGainLow
        while gainVal <= CurGainHigh:
            #for gainVal in range(int(CurGainLow*1000), int(CurGainHigh*1000), int(CurGainStep*1000)):
                testGainLinearityList.append(NvCSTestConfiguration(
                    testName,
                    "G_%.3f_%.2f" % (CurExpTime, gainVal),
                    CurExpTime,
                    gainVal))
                gainVal += CurGainStep

        analyzeList = []
        isLinear = True
        isOverExposed = True
        newTestList = testGainLinearityList
        curIndex = 0
        lastIndex = curIndex
        expoRetries = 0
        oldLogStr = logStr

        while((isLinear) and (isOverExposed)):
            logStr, isLinear, isOverExposed, curIndex = self.testGainLinearityRange(newTestList, BlackLevel, curIndex, MIN_RSQUARED_ERROR, MIN_SLOPE)
            logStr = oldLogStr + logStr
            if(len(newTestList) == curIndex):
                # By logic of testGainLinearityRange(), it is not overexposed
                # Tested full range
                # All that matters at this point is if it is linear
                logStr += ("Tested complete gain range.\n")

            elif(isLinear and isOverExposed):
                # reset retries
                expoRetries = 0

                # check to see if reached upper half of range
                if(curIndex > ((len(testGainLinearityList)-1)/2) + 1):
                    HalfGainRangePassed = True
                # setup new range
                newExpTime = (newTestList[curIndex].attr_expo)/(newTestList[curIndex-2].attr_gain)
                newTestList = []
                # Create new list starting from 2 previous known linear & not overexposed
                for testConfig in testGainLinearityList[(curIndex-2):]:
                    newTestList.append(NvCSTestConfiguration(
                    testName,
                    "G_%.3f_%.2f" % (newExpTime, testConfig.attr_gain),
                    newExpTime,
                    testConfig.attr_gain))
                curIndex -= 2

            elif((not isLinear) and (isOverExposed)):
                # Edge case, retry with smaller exposure
                if ((curIndex - lastIndex) < 4):

                    if(expoRetries > 3):
                        logStr += ("Capture is overexposed at an exposuretime of %f, and gain %f\n" % (newTestList[curIndex].attr_expo, newTestList[curIndex].attr_gain))
                        break #fail
                    expoRetries += 1

                    # setup new range
                    newExpTime = (newTestList[curIndex].attr_expo)*(0.666)
                    newTestList = []
                    # Create new list starting from 2 previous known linear & not overexposed
                    for testConfig in testGainLinearityList[(lastIndex):]:
                        newTestList.append(NvCSTestConfiguration(
                        testName,
                        "G_%.3f_%.2f" % (newExpTime, testConfig.attr_gain),
                        newExpTime,
                        testConfig.attr_gain))

                    # Reset index
                    curIndex = lastIndex

                    # set Linear to true to make loop happen again? : \
                    isLinear = True
                # either fail, or just beginning and needs calibration
            elif((not isLinear) and HalfGainRangePassed):
                # Will fail for now, but should be noted for why this happened
                break
            else: # HalfGainRangePassed is not true, and it is not linear
                break

            oldLogStr = logStr

        if (not isLinear):
            logStr += ("\n")
            logStr += ("\n")
            logStr += ("The images captured were not linear.  As the exposure time configuration increases, the color intensity values should be increasing as well.\n")
            logStr += ("Examine the pixel intensity values listed in tables compared to their gain and exposure values.  Where did it go wrong?  How would you explain the configurations that were not linear?\n")
            logStr += ("-Check that all auto algorithm features are turned off (AWB, AE, AGC)\n")
            logStr += ("-Check that the gain values are actually being sent to the camera device in your kernel driver.\n")
            logStr += ("-Check that the gain values are being translated appropriately to the form the camera device expects\n")
            logStr += ("-Look at the values and see if they seem to be over saturated. The sensor may not be using entire pixel depth.  If so, dim the light.\n")
            logStr += ("\n")
            logStr += ("\n")

        if(isOverExposed):
            logStr += ("\n")
            logStr += ("\n")
            logStr += ("It appears the data we ran so far were linear, but we cannot proceed without overexposing the camera.  Please re-run test with a dimmer light setting or lower the higher light setting.\n")
            logStr += ("\n")
            logStr += ("\n")
            # Fail phase here because it was linear, but ran into overexposure
            # The test needs to be ran again under a different configuration

        return isLinear, logStr

    def runExpLinearity(self, BlackLevel=[0.0, 0.0, 0.0]):
        PhasePass = True
        MIN_RSQUARED_ERROR = 0.970
        MIN_SLOPE = 10.0
        testName = "ExposureTimeLinearity"
        testExposureLinearityList = []
        logStr = ("")

        CurExpTimeLow = self.ExpTimeLow
        CurExpTimeHigh = self.ExpTimeHigh
        if (CurExpTimeLow < self.CalibExpoStart):
            CurExpTimeLow = self.CalibExpoStart

        CurExpTimeStep = (CurExpTimeHigh - CurExpTimeLow)/self.NumImages
        CurGain = self.qMinGain

        # Create the list of capture configurations
        # Using  a *1000 trick to utilize range()
        for expoVal in range(int(CurExpTimeLow*1000), int(CurExpTimeHigh*1000), int(CurExpTimeStep*1000)):
            testExposureLinearityList.append(NvCSTestConfiguration(
                testName,
                "E_%.3f_%.2f" % (expoVal/1000.0, CurGain),
                expoVal/1000.0,
                CurGain))
            logStr += ("appending exposure linearity run config %f, %f\n" % ((expoVal/1000.0), CurGain))

        analyzeList = []
        configIdx = 0
        for testConfig in testExposureLinearityList:
            testConfig.runConfig(self, testConfig)

            OverExposedThreshold = (math.pow(2, testConfig.BitsPerPixel) - 1.0) * self.TestSensorRangeMax

            idxOverExposed = False
            for idx in range(0,3):
                if (testConfig.Avg["Color"][idx] > OverExposedThreshold):
                    idxOverExposed = True
                    logStr += ("%s is overexposed at exposure time %f, and gain %f\n" % (testConfig.AvgNames["Color"][idx], testConfig.attr_expo, testConfig.attr_gain))
                    logStr += ("Threshold for exposure is %f, but reading an average pixel value of %f\n" % (OverExposedThreshold, testConfig.Avg["Color"][idx]))
            if(not idxOverExposed):
                analyzeList.append(testConfig)
                configIdx += 1
            else:
                break

        if(len(analyzeList) > 2):
            isLinear, logStr = self.checkLinearity(testName, analyzeList, BlackLevel, MIN_RSQUARED_ERROR, MIN_SLOPE)

            if (not isLinear):
                logStr += ("\n")
                logStr += ("\n")
                logStr += ("The images captured were not linear.  As the exposure time configuration increases, the color intensity values should be increasing as well.\n")
                logStr += ("-Check that all auto algorithm features are turned off (AWB, AE, AGC)\n")
                logStr += ("-Check that the exposure times are actually being sent to the camera device in your kernel driver.\n")
                logStr += ("-Check that the exposure times are being translated appropriately to the form the camera device expects\n")
                logStr += ("-Look at the values and see if they seem to be over saturated. The sensor may not be using entire pixel depth.  If so, dim the light.\n")
                logStr += ("The driver should be able to support exposure times outside of its minimum framelength (or VTS), we acknowledge that the framerate may slower than nominal values for those modes in these situations.\n")
                logStr += ("\n")
                logStr += ("\n")
                PhasePass = False

        if(idxOverExposed):
            logStr += ("\n")
            logStr += ("\n")
            # If it overexposed on the first capture, it is too bright
            if(configIdx > 1):
                logStr += ("It appears the data we ran so far were linear, but we cannot proceed without overexposing the camera.  Please re-run test with a DIMMER light setting or lower the higher light setting.\n")
                logStr += ("The current exposure settings are (%f, %f).  Try re-running with the following option parameters to the linearity test without the quotes '--nv --eL %f --nv --eH %f'\n" % (CurExpTimeLow, CurExpTimeHigh, CurExpTimeLow, CurExpTimeHigh*0.9))
            else:
                logStr += ("It appears the scene is too bright, we cannot proceed without overexposing the camera.  Please re-run test with a dimmer light setting\n")
            logStr += ("\n")
            logStr += ("\n")
            # Fail phase here because it was linear, but ran into overexposure
            # The test needs to be ran again under a different configuration
            PhasePass = False

        return PhasePass, logStr

    def runTest(self, args):
        self.logger.info("NVCS Linearity Test v3.0")
        self.failIfAohdrEnabled()

        if(self.options.numTimes > 1):
            self.NumImages = self.options.numTimes
        elif(self.options.numTimes == 1):
            self.logger.warning("User entered invalid number of samples (1).  Using default (%d)" % self.NumImages)

        if(self.options.exposureLow != 0):
            self.ExpTimeLow = self.options.exposureLow
        if(self.options.exposureHigh != 0):
            self.ExpTimeHigh= self.options.exposureHigh
        self.logger.info("Exposure Time limits for this run -- %f Min, %f Max\n" % (self.ExpTimeLow, self.ExpTimeHigh))

        self.noShuffle = self.options.noShuffle

        # Query exposure and gain range
        self.obCamera.startPreview()
        gainRange = self.obCamera.getAttr(nvcamera.attr_gainrange)
        # TODO: remove this check when Bug 1480305 is fixed
        if (gainRange[1] > 16):
            self.logger.warning("Max Sensor Gain Higher than 16 [%d] - waiting for next preview frame" % (gainRange[1]))
            time.sleep(0.150)
            gainRange = self.obCamera.getAttr(nvcamera.attr_gainrange)
            if (gainRange[1] > 16):
                self.logger.warning("Max Sensor Gain STILL Higher than 16 [%d] after wait. Continuing with test anyway." % (gainRange[1]))
        exposureTimeRange = self.obCamera.getAttr(nvcamera.attr_exposuretimerange)
        if (self.obCamera.isFocuserSupported()):
            focusRange = self.obCamera.getAttr(nvcamera.attr_focuspositionphysicalrange)
        self.obCamera.stopPreview()

        if((gainRange[1] == 0) or (exposureTimeRange[1] == 0)):
            self.logger.info("")
            self.logger.info("")
            self.logger.error("TEST FAILED TO QUERY GAIN AND EXPOSURETIME RANGE!")
            self.logger.error("Make sure your driver supports range/limit queries like your reference driver")
            self.logger.info("")
            self.logger.info("")
            return NvCSTestResult.ERROR

        if(gainRange[1] > 16):
            self.logger.warning("Max Sensor Gain Higher than 16 [%d]" % (gainRange[1]))

        self.qMinGain = gainRange[0]
        self.qMaxGain = gainRange[1]
        self.qMinExpTime = exposureTimeRange[0]
        self.qMaxExpTime = exposureTimeRange[1]
        if (self.obCamera.isFocuserSupported()):
            self.qMinFocusPos = focusRange[0]
            self.qMaxFocusPos = focusRange[1]

        BasicExpPass, BasicExpLog = self.testBasicExposure()
        self.logger.info("%s" % BasicExpLog)

        if(not BasicExpPass):
            return NvCSTestResult.ERROR

        # Take captures to reference as black level
        BlackLevel = self.getBlackLevel()

        self.runExpCalibration(BlackLevel)

        BasicGainPass, BasicGainLog = self.testBasicGain()
        self.logger.info("%s" % BasicGainLog)

        if(not BasicGainPass):
            return NvCSTestResult.ERROR

        ExpIsLinear, ExpLog = self.runExpLinearity(BlackLevel)

        if (not ExpIsLinear):
            self.logger.info("")
            self.logger.info("%s" % ExpLog)
            self.logger.info("")
            return NvCSTestResult.ERROR

        GainIsLinear, GainLog = self.runGainLinearity(BlackLevel)

        if (not GainIsLinear):
            self.logger.info("")
            self.logger.info("%s" % ExpLog)
            self.logger.info("%s" % GainLog)
            self.logger.info("")
            return NvCSTestResult.ERROR

        self.logger.info("%s" % ExpLog)
        self.logger.info("%s" % GainLog)

        return NvCSTestResult.SUCCESS

class NvCSBlackLevelRawTest(NvCSTestBase):
    "Black Level Raw Image Test"

    defNumImages = 15
    defMaxTemporalDiff = 1
    MaxTemporalDiff = 0.0
    MIN_SNR_dB = 30.0

    def __init__(self, options, logger):
        NvCSTestBase.__init__(self, logger, "BlackLevel")
        self.options = options

    def needTestSetup(self):
        return True

    def getSetupString(self):
        return "This test requires the camera to be covered for darkest capture possible"

    def setupGraph(self):
        self.obGraph.setImager(self.options.imager_id)
        self.obGraph.setGraphType(GraphType.RAW)

        return NvCSTestResult.SUCCESS

    def runSNR(self, configList, testName):
        # 3.16% coefficient of variation is equivalent to the old 30dB SNR figure.
        MAX_VARIATION_PERCENT = 3.16
        Linearity = True
        Variation = [float("inf"), float("inf"), float("inf")]
        MaxTracker = -1
        MinTracker = -1
        logStr = ""
        csvStr = ""

        NvCSTestConfiguration.runConfigList(self, configList)

        logStr, Variation, MaxTracker, MinTracker, csvStr = NvCSTestConfiguration.processVariationStats(testName, configList, "Region")

        for j in range(0, 3):
            if(Variation[j] > MAX_VARIATION_PERCENT):
                logStr += ("\n\nBlack Levels are fluctuating too greatly!\n\n")
                logStr += ("\n\nMaximum Coefficient of Variation has been exceeded! Found: %f%%, Maximum: %f%%\n\n" %
                    (Variation[j], MAX_VARIATION_PERCENT))
                Linearity = False

        if((MaxTracker - MinTracker) > self.MaxTemporalDiff):
            logStr += ("\n\nBlack Levels are fluctuating too greatly!\n\n")
            logStr += ("\n\nSpread of averages is greater than %.2f (%.2f to %.2f)!!!!!\n\n" %
                (self.MaxTemporalDiff, MinTracker, MaxTracker))
            Linearity = False
        else:
            logStr += ("\n\nSpread of averages is within the specification of %.2f (%.2f to %.2f)\n\n" %
                (self.MaxTemporalDiff, MinTracker, MaxTracker))

        return Linearity, logStr

    def runTest(self, args):
        self.logger.info("Black Level Test v2.0")
        self.failIfAohdrEnabled()

        numImages = self.defNumImages
        if(self.options.numTimes > 1):
            numImages = self.options.numTimes
        elif(self.options.numTimes == 1):
            self.logger.warning("User entered invalid number of samples (1).  Using default (%d)" % numImages)

        if(self.options.threshold != 0):
            self.MaxTemporalDiff = self.options.threshold
        else:
            self.MaxTemporalDiff = self.defMaxTemporalDiff

        # Query ranges to use minimum configuration settings
        self.obCamera.startPreview()
        gainRange = self.obCamera.getAttr(nvcamera.attr_gainrange)
        # TODO: remove this check when Bug 1480305 is fixed
        if (gainRange[1] > 16):
            self.logger.warning("Max Sensor Gain Higher than 16 [%d] - waiting for next preview frame" % (gainRange[1]))
            time.sleep(0.150)
            gainRange = self.obCamera.getAttr(nvcamera.attr_gainrange)
            if (gainRange[1] > 16):
                self.logger.warning("Max Sensor Gain STILL Higher than 16 [%d] after wait. Continuing with test anyway." % (gainRange[1]))
        exposureTimeRange = self.obCamera.getAttr(nvcamera.attr_exposuretimerange)
        self.obCamera.stopPreview()

        qMinGain = gainRange[0]
        qMinExpTime = exposureTimeRange[0]

        gain = qMinGain
        expTime = 0.100
        if(expTime < qMinExpTime):
            expTime = qMinExpTime

        # Create the configuration list
        testBlackLevelList = []
        for index in range(0, numImages):
            testBlackLevelList.append(NvCSTestConfiguration("Testing_BlackLevel",
                "BL_%.5f_%.2f_test" % (expTime, gain), expTime, gain))

        # Run the experiment
        TestPASS, BLLog = self.runSNR(testBlackLevelList, "Black Level Test")

        self.logger.info("%s" % BLLog)

        if(TestPASS == True):
            return NvCSTestResult.SUCCESS

        return NvCSTestResult.ERROR

class NvCSBayerPhaseTest(NvCSTestBase):
    "Bayer Phase Test"

    def __init__(self, options, logger):
        NvCSTestBase.__init__(self, logger, "BayerPhase")
        self.options = options

    def needTestSetup(self):
        return True

    def getSetupString(self):
        return "This test requires camera to be covered with the LED light panel turned on. Be careful not to overexpose or underexpose the scene."

    def setupGraph(self):
        self.obGraph.setImager(self.options.imager_id)
        self.obGraph.setGraphType(GraphType.RAW)

        return NvCSTestResult.SUCCESS

    def runTest(self, args):
        self.logger.info("Bayer Phase Test v1.1")

        # Exposue and Gain value acquired through general testing
        # These values may need to be changed
        Expo = 0.050
        Gain = 2.00
        config = NvCSTestConfiguration("Testing_BayerPhase",
                "BP_%.5f_%.2f_test" % (Expo, Gain), Expo, Gain)


        NvCSTestConfiguration.runConfig(self, config)

        # Calculating conditions of failure
        percErr = (abs(config.AvgPhaseGR - config.AvgPhaseGB)/max(config.AvgPhaseGR, config.AvgPhaseGB))
        if(percErr > 1.0):
            self.logger.error("Average GR (%f) & Average GB (%f) should be approximately equal." %
                (config.AvgPhaseGR, config.AvgPhaseGB))
            self.logger.error("Average GR (%f) & Average GB (%f) have a %f%% error (1%% is Passing)." %
                (config.AvgPhaseGR, config.AvgPhaseGB, percErr))
            return NvCSTestResult.ERROR
        if(config.AvgPhaseB < config.AvgPhaseR):
            self.logger.error("Average B (%f) should be GREATER than R (%f)." %
                (config.AvgPhaseB, config.AvgPhaseR))
            return NvCSTestResult.ERROR

        self.logger.info("\n\nConditions PASS for R(%.2f) GR(%.2f) GB(%.2f) B(%.2f)\n\n" % (config.AvgPhaseR, config.AvgPhaseGR, config.AvgPhaseGB, config.AvgPhaseB))
        return NvCSTestResult.SUCCESS

class NvCSFuseIDTest(NvCSTestBase):
    "Fuse ID test"

    FuseIDData = 0
    FuseID = []
    FuseIDSize = 0

    # Maximum length in bytes permitted for a fuse ID.  This should hold the same value as the maximum space allowed in the
    #   raw header.
    FuseIDMaxLength = 16

    # Table of values the Fuse ID should NOT hold.  A warning will be issued if the fuse ID matches any of these values.
    # Each entry is composed of the following fields:
    #   The first field per entry is the fuse ID byte value. If the length of this array is less than the maximum allowable size
    #       (the third field per entry), then the test will continue checking the last byte in the array.  This functionality
    #       is intended to serve two purposes:
    #           1. Checking for fuse IDs consisting of single repeated bytes.
    #           2. Accounting for the possibility of zero-padding after specific undesired fuse IDs.
    #   The second field per entry is the minimum size in bytes the fuse ID must be to be considered a match for this "bad value".
    #   The third field per entry is the maximum size in bytes the fuse ID must be to be considered a match for this "bad value".
    #   The fourth field per entry is a flag indicating if the fuse ID may still be equivalent to this "bad value"
    #       Set to "False" to disable checking for the corresponding "bad value".  Else set to "True".
    #   The final field per entry is a warning string to print if the fuse ID matches the bad value.
    BadValues = [
                  # Bits are all 0.  permitted size is 0-FuseIDMaxLength, so any size fuse id is valid.
                  [ [0x00],
                    0, FuseIDMaxLength, True,
                    "All Fuse ID bits are 0.  There is likely a problem with the driver, as it is very unlikely that a sensor will not have any data programmed to its OTP memory."],
                  # Bits are all 1.  permitted size is 0-FuseIDMaxLength, so any size fuse id is valid.
                  [ [0xff],
                    0, FuseIDMaxLength, True,
                    "All Fuse ID bits are 1.  There is likely a problem with the driver, as it is very unlikely that a sensor will have all of its OTP data written to 1."],
                  # Bytes spell out "DEADBEEF" in hex.  This is a common default value and usually will indicate an error.
                  # Must be at least 4 bytes long to be considered a match, but may be longer if following bytes are 0 (zero-padding).
                  [ [0xde, 0xad, 0xbe, 0xef, 0x00],
                    4, FuseIDMaxLength, True,
                    "The Fuse ID spells out \"DEADBEEF\" in hexadecimal.  This is a common default value, and hence means there is likely a problem with the driver."],
                  # This is a known default value.  A fuse ID return of this value will indicate there is probably a problem.
                  # Must be exactly 16 bytes long to be considered a match.
                  [ [0xff, 0xff, 0xff, 0xff, 0x67, 0x45, 0x23, 0x01, 0xf0, 0xde, 0xbc, 0x8a, 0xaa, 0xaa, 0xaa, 0xaa],
                    16, 16, True,
                    "The fuse ID has the value 0xffffffff67452301f0debc8aaaaaaaaa, which is a known default value.  Hence, there is likely a problem with the driver."]
                ]

    def __init__(self, options, logger):
        NvCSTestBase.__init__(self, logger, "FuseID")
        self.options = options

    def setupGraph(self):
        self.obGraph.setImager(self.options.imager_id)
        self.obGraph.setGraphType(GraphType.RAW)

        return NvCSTestResult.SUCCESS

    def runTest(self, args):
        self.failIfAohdrEnabled()
        self.obCamera.startPreview()
        # get the fuse id
        try:
            self.FuseIDData = self.obCamera.getAttr(nvcamera.attr_fuseid)
        except nvcamera.NvCameraException, err:
            self.logger.error("Fuse ID read failed.\nIs fuse ID read supported in the sensor driver?\n")
            self.obCamera.stopPreview()
            return NvCSTestResult.ERROR

        self.FuseIDSize = self.FuseIDData[0]

        rawFilePath = os.path.join(self.testDir, "header_test.nvraw")

        # capture image to check against header
        try:
            self.obCamera.captureRAWImage(rawFilePath, False)
        except nvcamera.NvCameraException, err:
            if (err.value == nvcamera.NvError_NotSupported):
                self.logger.info("raw capture is not supported")
                return NvCSTestResult.SKIPPED
            else:
                raise

        self.obCamera.stopPreview()

        if not self.nvrf.readFile(rawFilePath):
            self.logger.error("couldn't open the file: %s" % rawFilePath)
            return NvCSTestResult.ERROR

        match = True
        # compare header fuse id with driver fuse id
        for index in range(self.FuseIDSize):
            val = int(self.nvrf._fuseId[(index * 2):(index * 2 + 2)], 16)
            if(val != self.FuseIDData[1][index]):
                match = False
                break

        if(match == False):
            fusestring = ""
            for index in range(self.FuseIDSize):
                bytehex = str(hex(self.FuseIDData[1][index]))[2:]
                if(len(bytehex) < 2):
                    bytehex = "0" + bytehex
                fusestring += bytehex
            self.logger.error("Header FuseID did not match Driver FuseID")
            self.logger.error("Header FuseID = 0x%s" % (self.nvrf._fuseId[:(self.FuseIDSize*2)]))
            self.logger.error("Driver FuseID = 0x%s" % (fusestring[:(self.FuseIDSize*2)]))
            return NvCSTestResult.ERROR

        self.logger.info("")
        self.logger.info("Fuse ID Size:\t%d" % self.FuseIDSize)
        if(self.FuseIDSize < 1):
            self.logger.error("Fuse ID Size (%d) should be greater than 0.\n" % self.FuseIDSize)
            return NvCSTestResult.ERROR
        if(self.FuseIDSize > self.FuseIDMaxLength):
            self.logger.error("Fuse ID Size (%d) should be less than %d.\n" % (self.FuseIDSize, self.FuseIDMaxLength))
            self.logger.error("This larger size will not fit in the raw header.")
            return NvCSTestResult.ERROR
        # eliminate BadValue possible matches that have the wrong size range.
        for j in range(len(self.BadValues)):
            if(self.FuseIDSize < self.BadValues[j][1] or
               self.FuseIDSize > self.BadValues[j][2]):
                self.BadValues[j][3] = False;

        self.logger.info("")
        self.logger.info(" __Fuse ID:_______________ ")
        self.logger.info("| Byte #\t| Byte value\t|")
        self.logger.info("|_________|_______________|")
        for i in range(self.FuseIDSize):
            self.FuseID.append(self.FuseIDData[1][i])
            self.logger.info("| %d\t| 0x%x\t\t|" % (i, self.FuseID[i]))
            if(self.FuseID[i] > 0xff or self.FuseID[i] < 0):
                # this should never really happen, as the pipeline stores the values as single bytes throughout
                self.logger.error("Fuse ID byte is %d, which is not a valid single-byte value.  Values should fall between 0x00 and 0xff.")
                return NvCSTestResult.ERROR
            # check for undesirable values
            for j in range(len(self.BadValues)):
                if(self.BadValues[j][3] == True):
                    # repeat last array entry if needed
                    arrayindex = i if i < len(self.BadValues[j][0]) else len(self.BadValues[j][0])-1
                    if(self.BadValues[j][0][arrayindex] != self.FuseID[i]):
                        self.BadValues[j][3] = False
        self.logger.info("|_________|_______________|")
        self.logger.info("")

        for j in range(len(self.BadValues)):
            if(self.BadValues[j][3] == True):
                self.logger.info("*************************************************")
                self.logger.info("WARNING: %s" %(self.BadValues[j][4]))
                self.logger.info("*************************************************")

        return NvCSTestResult.SUCCESS

class NvCSAutoExposureTest(NvCSTestBase):
    "Auto Exposure Stability Test"

    settingsBase = """ap15Function.lensShading = FALSE;
ae.MeanAlg.SmartTarget = FALSE;
ae.MeanAlg.ConvergeSpeed = 0.9000;
ae.MaxFstopDeltaNeg = 0.9000;
ae.MaxFstopDeltaPos = 0.9000;
ae.MaxSearchFrameCount = 50;\n"""

    qMinGain = 0.0
    qMinExpTime = 0.0
    rSquaredMin = 0.97

    def __init__(self, options, logger):
        NvCSTestBase.__init__(self, logger, "AE_Stability")
        self.options = options

    def needTestSetup(self):
        return True

    def getSetupString(self):
        return ("\n\nThis test must be run with a controlled, uniformly lit scene.  Cover the sensor with the light source on its middle setting.\n\n")

    def setupGraph(self):
        self.obGraph.setImager(self.options.imager_id)
        self.obGraph.setGraphType(GraphType.RAW)

        return NvCSTestResult.SUCCESS

    def aeCapture(self, config):
        rawFilePath = os.path.join(self.testDir, config.filename + ".nvraw")
        try:
            self.obCamera.captureRAWImage(rawFilePath)
        except nvcamera.NvCameraException, err:
            if (err.value == nvcamera.NvError_NotSupported):
                self.logger.info("raw capture is not supported")
                return NvCSTestResult.SKIPPED
            else:
                raise
        if not self.nvrf.readFile(rawFilePath):
            self.logger.error("couldn't open the file: %s" % rawFilePath)
            return NvCSTestResult.ERROR
        config.processRawAvgs(self.nvrf)

    def applySettings(self, settings):
        # Create Directory if DNE
        try:
            if not os.path.exists(self.options.overridesDir):
                os.makedirs(self.options.overridesDir)
        except:
            self.logger.error("Cannot access Directory: [" + self.options.overridesDir + "]")
            return NvCSTestResult.ERROR

        # Calculate Overrides Target
        suffix = ""
        if (self.options.imager_id == 1):
            suffix = "_front"
        overridesPath = os.path.join(self.options.overridesDir, self.options.overridesName + suffix + ".isp")

        # Set Overrides
        try:
            f = open(overridesPath, 'w')
            f.write(settings)
            f.close()
        except IOError:
            self.logger.error("Unable to set Overrides: [" + overridesPath + "]")
            NvCSTestResult.ERROR

    def wipeSettings(self):
        # Calculate Overrides Target
        suffix = ""
        if (self.options.imager_id == 1):
            suffix = "_front"
        overridesPath = os.path.join(self.options.overridesDir, self.options.overridesName + suffix + ".isp")
        # Delete Overrides
        os.remove(overridesPath)

    def runTest(self, args):
        self.failIfAohdrEnabled()
        testSettings = [[200, "High"],[100, "Med"],[50, "Low"]]
        finishedTestData = []
        invGammaList = []
        avgBrightnessList = []
        imageCount = 0

        suffix = ""
        if (self.options.imager_id == 1):
            suffix = "_front"
        splitPath = os.path.splitext(self.options.overridesName)
        if(splitPath[-1] == ".isp"):
            self.options.overridesName = splitPath[0]
        overridesTarget = os.path.join(self.options.overridesDir, self.options.overridesName + suffix + ".isp")
        print "Camera Overrides Target: [%s]" % overridesTarget

        for setting in testSettings:
            runSettings = self.settingsBase + "ae.MeanAlg.TargetBrightness = " + str(setting[0]) + ";\n"
            result = self.applySettings(runSettings)

            if(result == NvCSTestResult.ERROR):
                return NvCSTestResult.ERROR

            # Reload graph to force overrides
            self.obGraph.stopAndDeleteGraph()
            self.obGraph.createAndRunGraph()

            self.obCamera.startPreview()
            config = NvCSTestConfiguration("", "aeCheck_%d_test" % (setting[0]), -1, -1)
            self.aeCapture(config)
            self.obCamera.stopPreview()

            invGamma = (float(setting[0]) / 255) ** 2.2

            data = self.CaptureData("Test%sExpo" % (setting[1]), setting[0], invGamma,
                config.Avg["Color"][0], config.Avg["Color"][1], config.Avg["Color"][2])

            finishedTestData.append(data)

        self.wipeSettings()

        linearity = [False, False, False, False]
        rSquared = NvCSTestConfiguration.calculateRSquared(finishedTestData, "X", "Y", 4)

        fail = False
        for index in range(0, 4):
            linearity[index] = True if (rSquared[index] >= self.rSquaredMin) else False
            if(linearity[index] == False):
                fail = True

        self.logger.info("")
        self.logger.info("Camera Overrides Target [%s]" % overridesTarget)
        self.logger.info("")
        self.logger.info("___Auto_Exposure_Stability_________________________________")
        self.logger.info("|                 |TARGET |RED    |GREEN  |BLUE   |AVERAGE|")
        self.logger.info("|_________________|_______|_______|_______|_______|_______|")
        for i in range(0, len(finishedTestData)):
            t = finishedTestData[i]
            self.logger.info("|%16s |%7d|%7.2f|%7.2f|%7.2f|%7.2f|" %
                (t.ConfigName, t.Target, t.getR(), t.getG(),
                t.getB(), t.getAvg()))
        self.logger.info("|_________________|_______|_______|_______|_______|_______|")
        self.logger.info("| R^2             |       |%7.4f|%7.4f|%7.4f|%7.4f|" %
            (rSquared[0], rSquared[1], rSquared[2], rSquared[3]))
        self.logger.info("|_________________|_______|_______|_______|_______|_______|")
        self.logger.info("| RESULT          |       |%6s |%6s |%6s |%6s |" %
            ("PASS" if linearity[0] else "FAIL", "PASS" if linearity[1] else "FAIL",
            "PASS" if linearity[2] else "FAIL", "PASS" if linearity[3] else "FAIL"))
        self.logger.info("|_________________|_______|_______|_______|_______|_______|")
        self.logger.info("")

        # verify target 200 > 2 * target 50
        for i in range(0, len(finishedTestData[0].Y)):
            y1 = finishedTestData[0].Y[i] # y val for first data point (200)
            y2 = finishedTestData[-1].Y[i] # y val for last data point (50)
            if (y1 < (2 * y2)):
                fail = True
                self.logger.error("Brightness Target %d [%f] was not greater than 2 * Brightness Target %d [%f]"
                    % (finishedTestData[0].Target, finishedTestData[0].Y[i],
                       finishedTestData[-1].Target, finishedTestData[-1].Y[i]))

        return NvCSTestResult.ERROR if fail else NvCSTestResult.SUCCESS

    class CaptureData():

        def __init__(self, ConfigName, Target, X, R, G, B):
            self.ConfigName = ConfigName
            self.Target = Target
            self.X = X
            self.Y = [R, G, B, float(R + G + B) / 3]

        def getName(self):
            return self.ConfigName
        def getTarget(self):
            return self.Target
        def getR(self):
            return self.Y[0]
        def getG(self):
            return self.Y[1]
        def getB(self):
            return self.Y[2]
        def getAvg(self):
            return self.Y[3]

class NvCSHDRRatioTest(NvCSTestBase):
    "HDR Ratio test"

    # constants
    numRatios = 7
    MaxErrorTolerance = 5.0

    # internal variables
    ratioR = 0.0
    ratioGR = 0.0
    ratioGB = 0.0
    ratioB = 0.0
    utilGain = 1.0
    utilExposure = 0.200

    # queried variables
    qMinGain = 0
    qMaxGain = 0
    qMinExpTime = 0
    qMaxExpTime = 0
    qMinFocusPos = 0
    qMaxFocusPos = 0

    # not actually queried now, but will be eventually
    qMinSensorHDRRatio = 1.0
    qMaxSensorHDRRatio = 8.0

    def __init__(self, options, logger):
        NvCSTestBase.__init__(self, logger, "Ratio")
        self.options = options

    def setupGraph(self):
        self.obGraph.setImager(self.options.imager_id)
        self.obGraph.setGraphType(GraphType.RAW)
        return NvCSTestResult.SUCCESS

    def needTestSetup(self):
        return True

    def getSetupString(self):
        return "\n\nThis test must be run with a controlled, uniformly lit scene.  Cover the sensor with the light source on its lowest setting.\n\n"

    def runTest(self, args):
        # query exposure, gain, focus limits
        self.obCamera.startPreview()
        gainRange = self.obCamera.getAttr(nvcamera.attr_gainrange)
        # TODO: remove this check when Bug 1480305 is fixed
        if (gainRange[1] > 16):
            self.logger.warning("Max Sensor Gain Higher than 16 [%d] - waiting for next preview frame" % (gainRange[1]))
            time.sleep(0.150)
            gainRange = self.obCamera.getAttr(nvcamera.attr_gainrange)
            if (gainRange[1] > 16):
                self.logger.warning("Max Sensor Gain STILL Higher than 16 [%d] after wait. Continuing with test anyway." % (gainRange[1]))
        exposureTimeRange = self.obCamera.getAttr(nvcamera.attr_exposuretimerange)
        if (self.obCamera.isFocuserSupported()):
            focusRange = self.obCamera.getAttr(nvcamera.attr_focuspositionphysicalrange)
        self.obCamera.stopPreview()

        if((gainRange[1] == 0) or (exposureTimeRange[1] == 0)):
            self.logger.info("")
            self.logger.info("")
            self.logger.error("TEST FAILED TO QUERY GAIN AND EXPOSURETIME RANGE!")
            self.logger.error("Make sure your driver supports range/limit queries like your reference driver")
            self.logger.info("")
            self.logger.info("")
            return NvCSTestResult.ERROR

        self.qMinGain = gainRange[0]
        self.qMaxGain = gainRange[1]
        self.qMinExpTime = exposureTimeRange[0]
        self.qMaxExpTime = exposureTimeRange[1]
        if (self.obCamera.isFocuserSupported()):
            self.qMinFocusPos = focusRange[0]
            self.qMaxFocusPos = focusRange[1]

        # Take captures to reference as black level
        BlackLevel = self.getBlackLevel()

        # override the default gain and exposure if specified in command line
        if (self.options.utilGain > 0.0):
            if (self.options.utilGain <= self.qMaxGain and self.options.utilGain >= self.qMinGain):
                self.utilGain = self.options.utilGain
            else:
                self.logger.info("requested gain %f falls outside the supported range (%f, %f)." %
                                (self.options.utilGain, self.qMinGain, self.qMaxGain))
                self.logger.info("Gain set to default: %f" % self.utilGain)
        if (self.options.utilExposure > 0.0):
            if (self.options.utilExposure <= self.qMaxExpTime and self.options.utilExposure >= self.qMinExpTime):
                self.utilExposure = self.options.utilExposure
            else:
                self.logger.info("requested long exposure time %f falls outside the supported range (%f, %f)." %
                                (self.options.utilExposure, self.qMinExpTime, self.qMaxExpTime))
                self.logger.info("Using default scan to determine long exposure time")
                self.runEnvCheck(Blacklevel)

        RatioList = []
        HDRRatioConfigList = []
        if (self.options.targetRatio == None):
            # Run the test in auto mode.
            # assemble the list of ratios to be tested
            if self.numRatios < 2:
                self.numRatios = 2
            ratioStep = ((self.qMaxSensorHDRRatio - self.qMinSensorHDRRatio) /
                         (self.numRatios - 1))
            # first entry is minimum
            RatioList.append(self.qMinSensorHDRRatio)
            # fill in middle values
            for i in range(self.numRatios - 2):
                RatioList.append(self.qMinSensorHDRRatio + ((i + 1) * ratioStep))
            # last entry is maximum
            RatioList.append(self.qMaxSensorHDRRatio)

            for ratio in RatioList:
                # put ratio in terms of short exposure
                shortExposure = self.utilExposure / ratio
                HDRRatioConfigList.append(NvCSTestConfiguration(
                    subtest = "HDRRatioTest",
                    filepath = "HDRRatioTest_%.5f_%.2f_%.5f" %
                        (self.utilExposure, self.utilGain, ratio),
                    exposure = self.utilExposure,
                    gain = self.utilGain,
                    focus = self.qMinFocusPos,
                    short_exposure = shortExposure))
        else:
            # Run the test in manual mode.
            # This assumes the user-specified ratio is hardcoded in the driver.
            ratio = self.options.targetRatio
            RatioList.append(ratio)
            HDRRatioConfigList.append(NvCSTestConfiguration(
                subtest = "HDRRatioTest",
                filepath = "HDRRatioTest_%.5f_%.2f_%.5f" %
                    (self.utilExposure, self.utilGain, ratio),
                exposure = self.utilExposure,
                gain = self.utilGain,
                focus = self.qMinFocusPos))

        result = NvCSTestConfiguration.runConfigList(self, HDRRatioConfigList)
        # return ERROR for both ERROR and SKIPPED
        if (result != NvCSTestResult.SUCCESS):
            self.logger.error("Test configuration did not run to completion!")
            return NvCSTestResult.ERROR

        PercentErrorList = []

        outputTable = ""
        outputTable += ("\n")
        outputTable += ("|------- ------  -----   ----    ---     --      -\n")
        for i in range(len(RatioList)):
            HDRRatioConfigList[i].ratioR = ((HDRRatioConfigList[i].AvgPhaseR - BlackLevel[0]) /
                           (HDRRatioConfigList[i].AvgPhaseRShort - BlackLevel[0]))
            HDRRatioConfigList[i].ratioGR = ((HDRRatioConfigList[i].AvgPhaseGR - BlackLevel[1]) /
                           (HDRRatioConfigList[i].AvgPhaseGRShort - BlackLevel[1]))
            HDRRatioConfigList[i].ratioGB = ((HDRRatioConfigList[i].AvgPhaseGB - BlackLevel[2]) /
                           (HDRRatioConfigList[i].AvgPhaseGBShort - BlackLevel[2]))
            HDRRatioConfigList[i].ratioB = ((HDRRatioConfigList[i].AvgPhaseB - BlackLevel[3]) /
                           (HDRRatioConfigList[i].AvgPhaseBShort - BlackLevel[3]))
            outputTable += ("| Requested Ratio:\t%f\n" % RatioList[i])
            outputTable += ("|  Measured Ratios:\tR: %f\tGR: %f\tGB: %f\tB: %f\n" % (HDRRatioConfigList[i].ratioR, HDRRatioConfigList[i].ratioGR, HDRRatioConfigList[i].ratioGB, HDRRatioConfigList[i].ratioB))
            PercentErrorList.append([abs((HDRRatioConfigList[i].ratioR - RatioList[i]) / RatioList[i] * 100),
                                     abs((HDRRatioConfigList[i].ratioGR - RatioList[i]) / RatioList[i] * 100),
                                     abs((HDRRatioConfigList[i].ratioGB - RatioList[i]) / RatioList[i] * 100),
                                     abs((HDRRatioConfigList[i].ratioB - RatioList[i]) / RatioList[i] * 100)])
            outputTable += ("|  Percent Error:\tR: %f%%\tGR: %f%%\tGB: %f%%\tB: %f%%\n" % (
                                     PercentErrorList[i][0],
                                     PercentErrorList[i][1],
                                     PercentErrorList[i][2],
                                     PercentErrorList[i][3]))
            outputTable += ("|------- ------  -----   ----    ---     --      -\n")
        outputTable += ("\n")
        self.logger.info(outputTable)

        # check if any measured ratios are too far off
        ColorList = ["Red", "Green (R)", "Green (B)", "Blue"]
        ret = NvCSTestResult.SUCCESS
        for i in range(len(PercentErrorList)):
            for j in range(len(PercentErrorList[i])):
                if (PercentErrorList[i][j] > self.MaxErrorTolerance):
                    self.logger.error("For Requested Ratio %f, %s Channel %% Error is too high! (%f%% > %f%%)" % (RatioList[i], ColorList[j], PercentErrorList[i][j], self.MaxErrorTolerance))
                    ret = NvCSTestResult.ERROR
        return ret

    def getBlackLevel(self):

        # Test BlackLevel using minimum settings reported
        # Also test a smaller exposure as the driver should be limiting the config
        BLList = []
        BLList.append(NvCSTestConfiguration(
                "BlackLevelHDR", "BL_%.5f_%.2f" % (self.qMinExpTime, self.qMinGain),
                self.qMinExpTime, self.qMinGain, self.qMinFocusPos))
        BLList.append(NvCSTestConfiguration(
                "BlackLevelHDR", "BL_%.5f_%.2f" % (self.qMinExpTime*0.1, self.qMinGain),
                self.qMinExpTime*0.1, self.qMinGain, self.qMinFocusPos))

        self.logger.info("The test will now take captures for black levels.")

        NvCSTestConfiguration.runConfigList(self, BLList)

        self.logger.info("\n\nDone.\n\n")

        self.MaxPixelVal = math.pow(2, BLList[0].BitsPerPixel) - 1.0

        # Setting initial black levels to be overwritten
        BlackLevel = [self.MaxPixelVal, self.MaxPixelVal, self.MaxPixelVal, self.MaxPixelVal]

        for imageStat in BLList:
            # Leaving in a sanity check of the blacklevel configuration
            if (imageStat.testing == "BlackLevelHDR"):
                if(BlackLevel[0] > imageStat.AvgPhaseR):
                    BlackLevel[0] = imageStat.AvgPhaseR
                if(BlackLevel[1] > imageStat.AvgPhaseGR):
                    BlackLevel[1] = imageStat.AvgPhaseGR
                if(BlackLevel[2] > imageStat.AvgPhaseGB):
                    BlackLevel[2] = imageStat.AvgPhaseGB
                if(BlackLevel[3] > imageStat.AvgPhaseB):
                    BlackLevel[3] = imageStat.AvgPhaseB
            else:
                self.logger.info("\nIT SHOULD NOT HAVE REACHED THIS CODE. IT RAN INTO NON BLACKLEVEL CONFIG\n")

        self.logger.info("\nPerceived Black Level:  R:%.1f, GR:%.1f, GB:%.1f, B:%.1f\tMax Possible Pixel Value: %d\n" %
            (BlackLevel[0], BlackLevel[1], BlackLevel[2], BlackLevel[3], self.MaxPixelVal))

        return BlackLevel
