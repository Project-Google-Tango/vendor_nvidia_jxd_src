# Copyright (c) 2012-2014, NVIDIA Corporation.  All rights reserved.
#
# NVIDIA Corporation and its licensors retain all intellectual property
# and proprietary rights in and to this software, related documentation
# and any modifications thereto.  Any use, reproduction, disclosure or
# distribution of this software and related documentation without an express
# license agreement from NVIDIA Corporation is strictly prohibited.
#

from nvcstestcore import *
import nvcstestutils

class NvCSFocusPosTest(NvCSTestBase):
    "Focus Position test"

    iterations = 6

    def __init__(self, options, logger):
        NvCSTestBase.__init__(self, logger, "FocusPos")
        self.options = options

    def setupGraph(self):
        self.obGraph.setImager(self.options.imager_id)
        self.obGraph.setGraphType(GraphType.RAW)

        return NvCSTestResult.SUCCESS

    def runPreTest(self):
        # check if the focuser is supported
        if(not self.obCamera.isFocuserSupported()):
            return NvCSTestResult.SKIPPED

        return NvCSTestResult.SUCCESS

    def runTest(self, args):
        self.failIfAohdrEnabled()
        retVal = NvCSTestResult.SUCCESS

        # query+print focus position physical range
        physicalRange = self.obCamera.getAttr(nvcamera.attr_focuspositionphysicalrange)
        self.logger.info("focuser physical range: [%d, %d]" % (physicalRange[0], physicalRange[1]))

        # calculate working range
        positionInf = self.obCamera.getAttr(nvcamera.attr_focuspositioninf)
        positionInfOffset = self.obCamera.getAttr(nvcamera.attr_focuspositioninfoffset)
        positionmMacro = self.obCamera.getAttr(nvcamera.attr_focuspositionmacro)
        positionmMacroOffset = self.obCamera.getAttr(nvcamera.attr_focuspositionmacrooffset)

        workingPositionLow = positionInf + positionInfOffset
        workingPositionHigh = positionmMacro + positionmMacroOffset
        self.logger.info("focuser working range: [%d, %d]" % (workingPositionLow, workingPositionHigh))

        # check for unusual values and flag warning
        if(physicalRange[0] > physicalRange[1]):
            self.logger.warning("focus position infinity: %d, focus position macro: %d" %
                                (physicalRange[0], physicalRange[1]))
        if((physicalRange[0] > 0) or (physicalRange[1] < 0)):
            self.logger.warning("focuser physical range (%d, %d) does not include 0." %
                                (physicalRange[0], physicalRange[1]))
        totalSteps = abs(physicalRange[1] - physicalRange[0]) + 1
        totalStepsTemp = totalSteps
        while (totalStepsTemp & 0x0001 == 0) and (totalStepsTemp != 0):
            totalStepsTemp = totalStepsTemp >> 1
        if totalStepsTemp != 1:
            self.logger.error("Total focuser steps (%d) is not a power of 2" %
                              totalSteps)
            return NvCSTestResult.ERROR

        # set to min working position per Bug 1283378
        self.obCamera.startPreview()
        self.obCamera.setAttr(nvcamera.attr_focuspos, workingPositionLow)
        self.obCamera.stopPreview()

        startFocusPosition =  physicalRange[0]
        stopFocusPosition = physicalRange[1]
        if(self.options.numTimes > 1):
            self.iterations = self.options.numTimes
        elif(self.options.numTimes == 1):
            self.logger.warning("User entered invalid number of samples (1).  Using default (%d)" % self.iterations)
        step = int((physicalRange[1] - physicalRange[0])/(self.iterations - 1))

        # assemble list of focus positions
        focusPositions = []
        while(startFocusPosition <= stopFocusPosition):
            focusPositions.append(startFocusPosition)
            startFocusPosition = startFocusPosition + step
        # make sure we always try to set focus position to 0
        # (even if it is out of the reported range)
        if (not 0 in focusPositions):
            self.logger.info("Focuspos=0 is out of range or would not be tested.")
            focusPositions.append(0)
            self.logger.info("Focuspos=0 has been added to the test queue and WILL be tested.")

        # test the focus positions
        for focusPos in focusPositions:
            self.logger.info("capturing raw image with focuspos set to %d..." % focusPos)

            self.obCamera.startPreview()

            self.obCamera.setAttr(nvcamera.attr_focuspos, focusPos)
            if focusPos < 0:
                negStr = "_neg"
            else:
                negStr = ""

            # take an image
            fileName = "%s%s_%d_test" % (self.testID, negStr, abs(focusPos))
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

            if (self.nvrf._focusPosition !=  focusPos):
                self.logger.error("FocusPosition value is not correct in the raw header: %d" % self.nvrf._focusPosition)
                retVal = NvCSTestResult.ERROR
                break

            focusPosVal = self.obCamera.getAttr(nvcamera.attr_focuspos)
            if (focusPos != focusPosVal):
                self.logger.error("focus position value is not correct in the driver: %d..." % focusPosVal)
                retVal = NvCSTestResult.ERROR
                break

            # Check no values over 2**bitsPerSample
            if (self.nvrf.getMaxPixelValue() >= (2**self.nvrf._bitsPerSample)):
                self.logger.error("pixel value is over %d." % 2**self.nvrf._bitsPerSample)
                retVal = NvCSTestResult.ERROR
                break

        return retVal

class NvCSSharpnessTest(NvCSTestBase):

    # crop dimentions for sharpness calculations
    # This should select cropWidth x cropHeight image from the center of the image.
    # If the image dimensions are lower than crop size then we re-adjust
    # the crop size to match image dimensions.
    cropWidth = 600
    cropHeight = 600

    # sharpness error margin
    errMargin = 10.0/100.0

    # number of iterations to search for the focus position
    # with different sharpness than both end focus position
    # from the focuser physical range
    numSearchIterations = 3

    # consitency check error margin
    # we have seen the some focuser fails during the
    # consitency check, so we are relaxing error margin
    # to catch the real focuser issues
    consistencyCheckErrMargin = 20.0/100.0

    def __init__(self, options, logger):
        NvCSTestBase.__init__(self, logger, "Sharpness")
        self.options = options

    def needTestSetup(self):
        return True

    def getSetupString(self):
        return "The camera should be pointing at the target with the camera view approximately " \
            +  "aligned to the target's alignment markers."

    def setupGraph(self):
        self.obGraph.setImager(self.options.imager_id)
        self.obGraph.setGraphType(GraphType.RAW)

        return NvCSTestResult.SUCCESS

    def runPreTest(self):
        # check if the focuser is supported
        if(not self.obCamera.isFocuserSupported()):
            return NvCSTestResult.SKIPPED

        # lock AE before running the test
        self.obCamera.startPreview()
        self.obCamera.lockAutoAlgs()

        return NvCSTestResult.SUCCESS

    def runTest(self, args):
        self.failIfAohdrEnabled()
        """
        Algorithm for sharpness test:
            1. Take sharpness at min
            2. Take sharpness at 1/2
            3. Take sharpness at max (make sure it is different than min)
            4. If sharpness at 1/2 is different from both max and min, skip to step 9
            5. Else take sharpness at 1/4 and compare to max and min, if different skip to step 9
            6. Else, check sharpness at 3/4, if different skip to step 9
            7. Keep going for 1/8, 3/8, 5/8, 7/8, skip to step 9 as soon as position with
               different sharpness is found
            8. Quit with error if none of the above works.
            9. Do consistency check with another sweep at min, max and the found point, then quit
        """
        retVal = NvCSTestResult.SUCCESS

        # query+print focus position physical range
        physicalRange = self.obCamera.getAttr(nvcamera.attr_focuspositionphysicalrange)
        self.logger.info("focuser physical range: [%d, %d]" % (physicalRange[0], physicalRange[1]))

        # check for unusual values
        if(physicalRange[0] > physicalRange[1]):
            self.logger.warning("focus position infinity: %d, focus position macro: %d" %
                                (physicalRange[0], physicalRange[1]))
        if((physicalRange[0] > 0) or (physicalRange[1] < 0)):
            self.logger.warning("focuser physical range (%d, %d) does not include 0." %
                                (physicalRange[0], physicalRange[1]))
        totalSteps = abs(physicalRange[1] - physicalRange[0]) + 1
        totalStepsTemp = totalSteps
        while (totalStepsTemp & 0x0001 == 0) and (totalStepsTemp != 0):
            totalStepsTemp = totalStepsTemp >> 1
        if totalStepsTemp != 1:
            self.logger.error("Total focuser steps (%d) is not a power of 2" %
                              totalSteps)
            return NvCSTestResult.ERROR

        # set to min working position per Bug 1283378
        positionInf = self.obCamera.getAttr(nvcamera.attr_focuspositioninf)
        positionInfOffset = self.obCamera.getAttr(nvcamera.attr_focuspositioninfoffset)
        workingPositionLow = positionInf + positionInfOffset
        self.obCamera.setAttr(nvcamera.attr_focuspos, workingPositionLow)

        sharpnessDictIteration1 = {}
        sharpnessDictIteration2 = {}

        # first iteration
        #
        # Compare sharpness at the min and max focus positions and
        # find a position between min and max that has different
        # sharpness than both the ends
        #
        # If we are not able to find the position between min and max
        # focuser is not moving and hence fail the test

        focusPositions = [physicalRange[0], physicalRange[1]]

        # capture images for at the ends
        for focusPosition in focusPositions:
            sharpnessDictIteration1[focusPosition] = self._getSharpnessAtFocusPosition(focusPosition)

        # compare sharpness at ends of the focuser range
        # they should be different
        if (
             not self._compareSharpness(
                                        sharpnessDictIteration1[focusPositions[0]],
                                        sharpnessDictIteration1[focusPositions[-1]],
                                        self.errMargin,
                                        False
                                       )
           ):
            self.logger.error("sharpness has not changed as expected after we moved "
                              "focuser from %d to %d" % (focusPositions[0], focusPositions[-1]))
            self._printSummary(focusPositions, sharpnessDictIteration1, sharpnessDictIteration2)
            return NvCSTestResult.ERROR

        # find a position between min and max that has different
        # sharpness than both the ends
        isDifferent = False
        endPointSum = focusPositions[0] + focusPositions[1]
        for iteration in range(self.numSearchIterations):
            if(isDifferent == True):
                break

            numNodes = int(math.pow(2, iteration))

            for i in range(numNodes):
                focusPos = (((2 * i) + 1) * endPointSum) / (numNodes * 2)
                sharpnessDictIteration1[focusPos] = self._getSharpnessAtFocusPosition(focusPos)
                if (self._compareSharpness(
                                           sharpnessDictIteration1[focusPos],
                                           sharpnessDictIteration1[focusPositions[0]],
                                           self.errMargin,
                                           False
                                          )
                  and
                    self._compareSharpness(
                                           sharpnessDictIteration1[focusPos],
                                           sharpnessDictIteration1[focusPositions[1]],
                                           self.errMargin,
                                           False
                                          )
                   ):
                    isDifferent = True
                    # we have found the position so store it for the next
                    # iteration
                    focusPositions.insert(1, focusPos)
                    break

        if (not isDifferent):
            self.logger.error("sharpness has not changed as expected after we moved "
                              "focuser from %d to any of the positions between the "
                              "focuser range" % focusPos)
            self._printSummary(focusPositions, sharpnessDictIteration1, sharpnessDictIteration2)
            return NvCSTestResult.ERROR

        # Do consistency check with another sweep at min, max and the found point
        for focusPosition in focusPositions:
            self.logger.info("Consistency check for focus position: %d" % focusPosition)
            sharpnessDictIteration2[focusPosition] = self._getSharpnessAtFocusPosition(focusPosition)

            # check if the shapness matches(~ within 10% difference) with the previous
            # sharpness measurements
            if (not self._compareSharpness(
                                          sharpnessDictIteration1[focusPosition],
                                          sharpnessDictIteration2[focusPosition],
                                          self.consistencyCheckErrMargin,
                                          True
                                         )
               ):
                self.logger.error("sharpness is not approximately the same as the previous "
                                  "measurement for focus position %d" % focusPosition)
                self._printSummary(focusPositions, sharpnessDictIteration1, sharpnessDictIteration2)
                return NvCSTestResult.ERROR

        # print the summary information
        self._printSummary(focusPositions, sharpnessDictIteration1, sharpnessDictIteration2)

        return retVal

    def runPostTest(self):
        # unlock AE
        self.obCamera.unlockAutoAlgs()
        self.obCamera.stopPreview()
        return NvCSTestResult.SUCCESS

    def _printSummary(self, focusPositions, sharpnessDictIteration1, sharpnessDictIteration2):
        # print the information
        self.logger.info("%15s %15s %15s %15s" %
                    ("Focus Position", "Sharpness", "Normalized", "Iteration"))

        sharpnessList = sharpnessDictIteration1.values()
        sharpnessList.extend(sharpnessDictIteration2.values())

        maxSharpness = max(sharpnessList)

        for focusPosition in sharpnessDictIteration1.iterkeys():
            if focusPosition in sharpnessDictIteration1:
                self.logger.info("%15d %15.3f %15.3f %15s" %
                    (focusPosition,
                     sharpnessDictIteration1[focusPosition],
                     sharpnessDictIteration1[focusPosition] / maxSharpness,
                     "One"
                    )
                )
            if focusPosition in sharpnessDictIteration2:
                self.logger.info("%15d %15.3f %15.3f %15s" %
                    (focusPosition,
                     sharpnessDictIteration2[focusPosition],
                     sharpnessDictIteration2[focusPosition] / maxSharpness,
                     "Two"
                    )
                )

    # compare sharpnessA with sharpnessB
    # if the status flag equality is True then this function
    # compares sharpness for equality otherwise it will
    # compare for non-equality
    def _compareSharpness(self, sharpnessA, sharpnessB, errThreshold, equality=True):
        # check if the sharpness is different at both ends of the range
        minCheckSharpness = sharpnessA - (sharpnessA * errThreshold)
        maxCheckSharpness = sharpnessA + (sharpnessA * errThreshold)

        equal = False
        if ((sharpnessB >= minCheckSharpness) and (sharpnessB <= maxCheckSharpness)):
            equal = True

        if(equality == True):
            return equal
        else:
            return not equal

    def _getSharpnessAtFocusPosition(self, focusPosition):
        rawFilePath = self._captureImageAtFocusPosition(focusPosition)

        if(not os.path.exists(rawFilePath)):
            self.logger.error("raw file %s is not present" % rawFilePath)
            return NvCSTestResult.ERROR

        # open the rawfile
        if not self.nvrf.readFile(rawFilePath):
            self.logger.error("couldn't open the file: %s" % rawFilePath)
            return NvCSTestResult.ERROR

        # crop the raw image before we calculate sharpness
        # this is for speedup
        retVal = nvcameraimageutils.cropRawImageFromCenter(self.nvrf, self.cropWidth, self.cropHeight)
        if(not retVal):
            self.logger.error("Unable to crop the image %s for crop width %d, height: %d " %
                              (rawFilePath, self.cropWidth, self.cropHeight))

        # Check no values over 2**bitsPerSample
        if (self.nvrf.getMaxPixelValue() >= (2**self.nvrf._bitsPerSample)):
            self.logger.error("pixel value is over %d." % 2**self.nvrf._bitsPerSample)
            return NvCSTestResult.ERROR

        # get the sharpness
        sharpness = nvcameraimageutils.calculateSharpness(self.nvrf)
        return sharpness

    def _captureImageAtFocusPosition(self, focusPos):
        # set focus position
        self.obCamera.setAttr(nvcamera.attr_focuspos, focusPos)

        if focusPos < 0:
            negStr = "_neg"
        else:
            negStr = ""

        # take an image
        fileName = "%s%s_%d_test" % (self.testID, negStr, abs(focusPos))
        rawFilePath = os.path.join(self.testDir, fileName + ".nvraw")

        # HACKY way to appned "_1" to the the filename
        # if file already exists
        if(os.path.exists(rawFilePath)):
            fileName = fileName + "_1"

        rawFilePath = os.path.join(self.testDir, fileName + ".nvraw")

        # capture raw image
        try:
            # second paramter to captureRAWImage is False because
            # we don't need halfpress while capturing raw image since
            # we have already locked Auto Algs.
            self.obCamera.captureRAWImage(rawFilePath, False)
        except nvcamera.NvCameraException, err:
            if (err.value == nvcamera.NvError_NotSupported):
                self.logger.info("raw capture is not supported")
                return NvCSTestResult.SKIPPED
            else:
                raise

        return rawFilePath
