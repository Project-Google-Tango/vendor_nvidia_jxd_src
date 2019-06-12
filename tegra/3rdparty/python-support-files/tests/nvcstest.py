#-------------------------------------------------------------------------------
# Name:        nvcstest.py
# Purpose:
#
# Created:     01/23/2012
#
# Copyright (c) 2012-2014, NVIDIA Corporation.  All rights reserved.
#
# NVIDIA Corporation and its licensors retain all intellectual property
# and proprietary rights in and to this software, related documentation
# and any modifications thereto.  Any use, reproduction, disclosure or
# distribution of this software and related documentation without an express
# license agreement from NVIDIA Corporation is strictly prohibited.
#
#-------------------------------------------------------------------------------
#!/usr/bin/env python
import nvcamera
import string
import os
import time
import shutil
import math
import traceback
import re
import nvrawfile
import nvcstestutils
from nvcssensortests import *
from nvcsflashtests import *
from nvcsfocusertests import *
from nvcshostsensortests import *
from nvcstestcore import *

# nvcstest module version
__version__ = "7.0.7"

def printVersionInformation(logger=None):
    if (logger is None):
        logger = nvcstestutils.Logger()

    # print nvcstest version
    logger.info("nvcstest version: %s" % __version__)

    # print nvcamera module/nvcs version
    logger.info("nvcamera version: %s" % nvcamera.__version__)

class NvCSJPEGCapTest(NvCSTestBase):
    "JPEG Capture Test"

    def __init__(self, options, logger):
        NvCSTestBase.__init__(self, logger, "Jpeg_Capture")
        self.options = options

    def setupGraph(self):
        self.obGraph.setImager(self.options.imager_id)

    def runTest(self, args):
        print "capturing jpeg image..."

        self.obCamera.startPreview()

        if(self.options.width != 0 and self.options.height != 0):
            scaleDims = [self.options.width, self.options.height]
            self.obCamera.setAttr(nvcamera.attr_scalesize, scaleDims)

        fileName = "%s_test" % self.testID
        jpegFilePath = os.path.join(self.testDir, fileName + ".jpg")
        self.obCamera.captureJpegImage(jpegFilePath, False)

        self.obCamera.stopPreview()

        if (os.path.exists(jpegFilePath) != True):
            self.logger.error("Couldn't capture the jpeg image: %s" % jpegFilePath)
            retVal = NvCSTestResult.ERROR

        return NvCSTestResult.SUCCESS

class NvCSMultipleRawTest(NvCSTestBase):
    "Multiple Raw Image Test"

    imageNumValues = range(1, 101)

    def __init__(self, options, logger):
        NvCSTestBase.__init__(self, logger, "Multiple_Raw")
        self.options = options

    def setupGraph(self):
        self.obGraph.setImager(self.options.imager_id)
        self.obGraph.setGraphType(GraphType.RAW)

        return NvCSTestResult.SUCCESS

    def __del__(self):
        if (os.path.exists(self.testDir)):
            shutil.rmtree(self.testDir)

    def runTest(self, args):
        self.failIfAohdrEnabled()
        retVal = NvCSTestResult.SUCCESS

        for imageNum in self.imageNumValues:
            # take an image
            fileName = "%s_%d_test" % (self.testID, imageNum)
            rawFilePath = os.path.join(self.testDir, fileName + ".nvraw")

            self.obCamera.startPreview()

            self.logger.debug("capturing image: %d" % imageNum)

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

            if os.path.exists(rawFilePath) != True:
                self.logger.error("couldn't find file: %s" % rawfilePath)
                retVal = NvCSTestResult.ERROR
                break

            if not self.nvrf.readFile(rawFilePath):
                self.logger.error("couldn't open the file: %s" % rawFilePath)
                retVal = NvCSTestResult.ERROR
                break

            # Check no values over 2**bitsPerSample
            if (self.nvrf.getMaxPixelValue() >= (2**self.nvrf._bitsPerSample)):
                self.logger.error("pixel value is over %d." % 2**self.nvrf._bitsPerSample)
                retVal = NvCSTestResult.ERROR
                break

            pattern = ".*"
            if((imageNum % 10) == 0):
                regexOb = re.compile(pattern)
                for fname in os.listdir(self.testDir):
                    if regexOb.search(fname):
                        self.logger.debug("removing file %s" % fname)
                        os.remove(os.path.join(self.testDir, fname))

        return retVal

class NvCSRAWCapTest(NvCSTestBase):
    "RAW Capture Test"

    def __init__(self, options, logger):
        NvCSTestBase.__init__(self, logger, "RAW_Capture")
        self.options = options

    def setupGraph(self):
        self.obGraph.setImager(self.options.imager_id)
        self.obGraph.setGraphType(GraphType.RAW)

        return NvCSTestResult.SUCCESS

    def runTest(self, args):
        self.failIfAohdrEnabled()
        retVal = NvCSTestResult.SUCCESS

        self.obCamera.startPreview()

        fileName = "%s_test" % self.testID
        filePath = os.path.join(self.testDir, fileName + ".nvraw")

        try:
            self.obCamera.captureRAWImage(filePath, False)
        except nvcamera.NvCameraException, err:
            if (err.value == nvcamera.NvError_NotSupported):
                self.logger.info("raw capture is not supported")
                return NvCSTestResult.SKIPPED
            else:
                raise

        self.obCamera.stopPreview()

        if (os.path.exists(filePath) != True):
            self.logger.error("Couldn't capture the raw image: %s" % filePath)
            retVal = NvCSTestResult.ERROR

        if not self.nvrf.readFile(filePath):
            self.logger.error("couldn't open the file: %s" % filePath)
            retVal = NvCSTestResult.ERROR

        # Check no values over 2**bitsPerSample
        if (self.nvrf.getMaxPixelValue() >= (2**self.nvrf._bitsPerSample)):
            self.logger.error("pixel value is over %d." % 2**self.nvrf._bitsPerSample)
            retVal = NvCSTestResult.ERROR

        return retVal

class NvCSConcurrentRawCapTest(NvCSTestBase):
    "Concurrent raw capture test"

    def __init__(self, options, logger):
        NvCSTestBase.__init__(self, logger, "Concurrent_Raw_Capture")
        self.options = options

    def setupGraph(self):
        self.obGraph.setImager(self.options.imager_id)

    def runPreTest(self):
        if (not self.obCamera.isConcurrentRawCaptureSupported()):
            self.logger.info("raw capture is not supported")
            return NvCSTestResult.SKIPPED
        else:
            return NvCSTestResult.SUCCESS

    def runTest(self, args):
        self.failIfAohdrEnabled()
        retVal = NvCSTestResult.SUCCESS

        self.obCamera.startPreview()

        fileName = "%s_test" % self.testID
        jpegFilePath = os.path.join(self.testDir, fileName + ".jpg")
        rawFilePath = os.path.join(self.testDir, fileName + ".nvraw")

        self.obCamera.captureConcurrentJpegAndRawImage(jpegFilePath, False)

        self.obCamera.stopPreview()

        if (os.path.exists(jpegFilePath) != True):
            self.logger.error("Couldn't capture the jpeg image: %s" % jpegFilePath)
            retVal = NvCSTestResult.ERROR

        if (os.path.exists(rawFilePath) != True):
            self.logger.error("Couldn't capture the raw image: %s" % rawFilePath)
            retVal = NvCSTestResult.ERROR

        if not self.nvrf.readFile(rawFilePath):
            self.logger.error("couldn't open the file: %s" % rawFilePath)
            retVal = NvCSTestResult.ERROR

        # Check no values over 2**bitsPerSample
        if (self.nvrf.getMaxPixelValue() >= (2**self.nvrf._bitsPerSample)):
            self.logger.error("pixel value is over %d." % 2**self.nvrf._bitsPerSample)
            retVal = NvCSTestResult.ERROR

        return retVal

class NvCSCropTest(NvCSTestBase):
    "Crop Test"

    # left, top, right, bottom
    cropDims = [0, 0 , 320, 240]

    def __init__(self, options, logger):
        NvCSTestBase.__init__(self, logger, "Crop")
        self.options = options

    def setupGraph(self):
        self.obGraph.setImager(self.options.imager_id)
        self.obGraph.setGraphType(GraphType.RAW)

        return NvCSTestResult.SUCCESS

    def runTest(self, args):
        self.failIfAohdrEnabled()
        retVal = NvCSTestResult.SUCCESS

        self.obCamera.startPreview()

        rawFileName = "%s_test" % self.testID
        rawFilePath = os.path.join(self.testDir, rawFileName + ".nvraw")

        try:
            # crop: [left, top, right, bottom]
            self.obCamera.setAttr(nvcamera.attr_crop, self.cropDims)
            self.logger.info("capturing raw image with crop dimnentions"
                             "[left, top, right, bottom] = %s" % self.cropDims)
            self.obCamera.captureRAWImage(rawFilePath, False)
        except nvcamera.NvCameraException, err:
            if (err.value == nvcamera.NvError_NotSupported):
                self.logger.info("raw capture is not supported")
                return NvCSTestResult.SKIPPED
            else:
                raise

        self.obCamera.stopPreview()

        if (os.path.exists(rawFilePath) != True):
            self.logger.error("Couldn't capture the raw image: %s" % filePath)
            return NvCSTestResult.ERROR

        if not self.nvrf.readFile(rawFilePath):
            self.logger.error("couldn't open the file: %s" % rawFilePath)
            return NvCSTestResult.ERROR

        # check for raw image dimentions
        expectedImageWidth = self.cropDims[2] - self.cropDims[0]
        expectedImageHeight = self.cropDims[3] - self.cropDims[1]
        if(self.nvrf._width !=  expectedImageWidth or
           self.nvrf._height != expectedImageHeight):
            self.logger.error("Captured imaged is not cropped.")
            self.logger.error("Expected image widthxhight = %dx%d" %
                              (expectedImageWidth, expectedImageHeight))
            self.logger.error("Got image with widthxhight = %dx%d" %
                              (self.nvrf._width, self.nvrf._height))
            return NvCSTestResult.ERROR

        return retVal
