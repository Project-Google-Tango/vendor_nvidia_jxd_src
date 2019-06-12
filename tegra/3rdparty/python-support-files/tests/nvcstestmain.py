#-------------------------------------------------------------------------------
# Name:        nvcstestmain.py
# Purpose:
#
# Created:     01/23/2012
#
# Copyright (c) 2012-2014 NVIDIA Corporation.  All rights reserved.
#
# NVIDIA Corporation and its licensors retain all intellectual property
# and proprietary rights in and to this software, related documentation
# and any modifications thereto.  Any use, reproduction, disclosure or
# distribution of this software and related documentation without an express
# license agreement from NVIDIA Corporation is strictly prohibited.
#
#-------------------------------------------------------------------------------
#!/usr/bin/env python

# global mapping between testname and
# test objects
import nvcstest
import nvcstestutils
import os
import shutil
from optparse import OptionParser
from optparse import make_option
import sys

class TestFactory:

    testDict = {                # enabled
        "jpeg_capture"          : 1,
        "raw_capture"           : 1,
        "concurrent_raw"        : 1,
        "exposuretime"          : 1,
        "gain"                  : 1,
        "focuspos"              : 1,
        "multiple_raw"          : 1,
        "linearity"             : 1,
        "linearity3"            : 1,
        "bayerphase"            : 1,
        "host_sensor"           : 1,
        "sharpness"             : 1,
        "blacklevel"            : 1,
        "fuseid"                : 1,
        "crop"                  : 1,
        "ae_stability"          : 1,
        "hdr_ratio"             : 1
    }

    sanityNameList = [
                        "jpeg_capture",
                        "raw_capture",
                        "concurrent_raw",
                        "exposuretime",
                        "gain",
                        "focuspos",
                        "fuseid",
                        "crop",
                        "multiple_raw",
                        "host_sensor"
                     ]

    sanityStagingNameList = [
                                #"aStagingTestName"
                            ]

    conformanceNameList = [
                                "blacklevel",
                                "linearity",
                                "sharpness"
                          ]

    conformanceStagingNameList = [
                                    "linearity3"
                                 ]

    utilityNameList = [
                          "ae_stability",
                          "hdr_ratio"
                      ]

    def __init__(self):
        pass

    @classmethod
    def getTestObject(self, testName, options, logger):
        if (testName == "jpeg_capture"):
            return nvcstest.NvCSJPEGCapTest(options, logger)
        elif (testName == "raw_capture"):
            return nvcstest.NvCSRAWCapTest(options, logger)
        elif (testName == "concurrent_raw"):
            return nvcstest.NvCSConcurrentRawCapTest(options, logger)
        elif (testName == "exposuretime"):
            return nvcstest.NvCSExposureTimeTest(options, logger)
        elif (testName == "gain"):
            return nvcstest.NvCSGainTest(options, logger)
        elif (testName == "focuspos"):
            return nvcstest.NvCSFocusPosTest(options, logger)
        elif (testName == "multiple_raw"):
            return nvcstest.NvCSMultipleRawTest(options, logger)
        elif (testName == "linearity"):
            return nvcstest.NvCSLinearityRawTest(options, logger)
        elif (testName == "linearity3"):
            return nvcstest.NvCSLinearityV3Test(options, logger)
        elif (testName == "blacklevel"):
            return nvcstest.NvCSBlackLevelRawTest(options, logger)
        elif (testName == "bayerphase"):
            return nvcstest.NvCSBayerPhaseTest(options, logger)
        elif (testName == "host_sensor"):
            return nvcstest.NvCSHostSensorTest(options, logger)
        elif (testName == "sharpness"):
            return nvcstest.NvCSSharpnessTest(options, logger)
        elif (testName == "fuseid"):
            return nvcstest.NvCSFuseIDTest(options, logger)
        elif (testName == "crop"):
            return nvcstest.NvCSCropTest(options, logger)
        elif (testName == "ae_stability"):
            return nvcstest.NvCSAutoExposureTest(options, logger)
        elif (testName == "hdr_ratio"):
            return nvcstest.NvCSHDRRatioTest(options, logger)
        else:
            return None

    @classmethod
    def hasTest(self, testName):
        return self.testDict.has_key(testName)

    @classmethod
    def disableTest(self, testName):
        if (self.testDict.has_key(testName)):
            self.testDict[testName] = 0

    @classmethod
    def enableTest(self, testName):
        if (self.testDict.has_key(testName)):
            self.testDict[testName] = 1

    @classmethod
    def isEnabled(self, testName):
        if (self.testDict.has_key(testName)):
            return self.testDict[testName]
        else:
            return 0


def main():

    numFailures = 0
    numTests = 0
    numSkippedTests = 0
    numStagingFailures = 0
    numStagingTests = 0
    numStagingSkippedTests = 0
    RunSuccessStr = ""
    with nvcstestutils.Logger() as logger:

        # parse arguments
        usage = "\n" \
                "%prog <options> - execute the Conformance Test Suite \n\n" \
                "Description:\n" \
                "\t Sanity tests consists of a series of simple basic\n" \
                "\t tests to make sure that the nvcs infrastructure, basic\n" \
                "\t odm functionalities, camera core and algorithm paths are\n" \
                "\t functional.\n" \
                "\n \t Imager tests are more sophisticated and specific\n" \
                "\t setup is required. Each test is designed to catch issues\n" \
                "\t which have been seen by camera team while working on\n" \
                "\t different customer devices. The Sanity tests must pass\n" \
                "\t in order to run the Imager tests."

        standard_options = [
                make_option('-v', '--version', dest='version', default=False, action="store_true",
                        help = 'Display version information'),
                make_option('-l', '--list', dest='listTestNames', default=False, action="store_true",
                        help = 'Display list of sanity and conformance test names'),
                make_option('-s', dest='sanity', default=False, action="store_true",
                        help = 'Run Sanity tests. If no -i option is specified, the command will'
                             + ' run using default imager id 0. Use -l option to look at the'
                             + ' list of sanity tests'),
                make_option('-c', dest='conformance', default=False, action="store_true",
                        help = 'Run Imager tests. Use -l option to look at the'
                             + ' list of Imager tests'),
                make_option('--classic', dest='useClassicRanges', default=False, action="store_true",
                        help = 'Do not query sensor for gain and exposure range.'
                             + 'This option is only supported for \"linearity\" test at present'),
                make_option('-t', dest='test_names', default=None, type="string", action = "append",
                        metavar="TEST_NAME",
                        help = "Execute the specified test name. Multiple -t options are allowed to execute"
                             + "more than one tests using a single command\n"
                             + "example:\n"
                             + "1) Command to run \"jpeg_capture\" test for imager id 0:\n"
                             + "   python %s -t jpeg_capture -i 0\n" %  sys.argv[0]
                             + "2) Command to run multiple tests \"jpeg_capture\" and \n"
                             + "  \"focuspos\" for imager id 0 using same command:\n"
                             + "   python %s -t jpeg_capture -t focuspos -i 0" % sys.argv[0]),
                make_option('-d', dest='disabled_test_names', default=None, type="string", action = "append",
                        metavar="TEST_NAME",
                        help = 'Disables the test. The test name specified as an argument will not'
                             + ' execute. Multiple -d options are supported'),
                make_option('-i', dest='imager_id', default=0, type="int", metavar="IMAGER_ID",
                        help = "set imager id. Most of the time imager id 0 is assigned for\n"
                             + "rear facing and imager id 1 is assigned for front facing\n"
                             + "camera sensor, but this might not be the case always\n"
                             + "example:\n"
                             + "- Command to run sanity test suite with imager id 1\n"
                             + "  python %s -s -i 1" % sys.argv[0]),
                make_option('--nofocus', dest='ignoreFocuser', default=False, action="store_true",
                        help = 'Ignore ALL focuser commands in the test'),
                make_option('-f', dest='force', default=False, action="store_true",
                        help = 'run the tests without any prompts', metavar="FORCE"),
                make_option('--runs', dest='totalTestRuns', default=1, type="int",
                        help = 'Set number of times to run each test.'
                             + ' Saves NVRaws from the last run only.  Saves logs from all runs.'),
                make_option('-h', '-?', '--help', dest='help', action='store_true', default=False,
                        help = 'print this help message')
            ]
        advanced_options = [
                make_option('--nv', dest='advanced', action='store_true', default=False,
                        help = 'Enable advanced options.'
                             + ' This will enable options for advanced testing and debugging of tests'),
                make_option('-n', dest='numTimes', default=0, type="int",
                        help = 'Set number of samples to use in select tests.'
                             + ' Applies to \"blacklevel\", \"linearity\", \"gain\", and \"exposure\".'),
                make_option('--nostaging', dest='stagingOff', default=False, action="store_true",
                        help = 'Do not run any of the staging tests in sanity or conformance suite'
                             + ' Staging tests are ran to provide information internally to qualify new tests'),
                make_option('--noshuffle', dest='noShuffle', action='store_true', default=False,
                        help = 'Do not shuffle capture order'
                             + ' Currently only \"linearity\" test supports this option'),
                make_option('--eL', dest='exposureLow', default=0, type="float",
                        help = 'Set minimum exposure for linearity3'),
                make_option('--eH', dest='exposureHigh', default=0, type="float",
                        help = 'Set maximum expsoure for linearity3.'),
                make_option('--threshold', dest='threshold', default=0, type="float",
                        help = 'Set threshold for test (float).'
                             + ' Currently only \"blacklevel\" test supports this option and it sets'
                             + ' threshold for difference between the maximum and minimum black level'
                             + ' variance'),
                make_option('--width', dest='width', default=2592, type="int",
                        help = 'Set width for host sensor mode'
                             + ' Default width used by host sensor mode is 2592'),
                make_option('--height', dest='height', default=1944, type="int",
                        help = 'Set height for host sensor mode'
                             + ' Default height used by host sensor mode is 1944'),
                make_option('--gain', dest='utilGain', default=0, type="float",
                        help = 'Set the gain for the capture.'
                             + ' Currently only the \"hdr_ratio\" test supports this option'),
                make_option('--exposure', dest='utilExposure', default=0, type="float",
                        help = 'Set the exposure for the capture.'
                             + ' Currently only the \"hdr_ratio\" test supports this option'),
                make_option('--overridesdir', dest='overridesDir', default='/data', type="string", metavar=" ",
                        help = 'Set Directory for used to set Camera Overrides'),
                make_option('--overridesname', dest='overridesName', default='camera_overrides', type="string", metavar=" ",
                        help = "Set the Name of the file used to set Camera Overrides\n"
                             + "Note: Should not include the assumed \'.isp\' extension"),
                make_option('--targetratio', dest='targetRatio', default=None, type="float",
                        help = "Specify the driver-hardcoded ET ratio for use with the\n"
                             + "manual HDR test.  Setting this argument automatically\n"
                             + "switches the HDR test into manual mode."),
            ]

        parser = OptionParser(usage, option_list=standard_options + advanced_options,
                              add_help_option=False,
                              formatter=nvcstestutils.HelpFormatterWithNewLine())

        # make a standard option for error checking if --nv is not specified
        standard_parser = OptionParser(usage, option_list=standard_options,
                                       add_help_option=False,
                                       formatter=nvcstestutils.HelpFormatterWithNewLine())

        # parse the command line arguments
        (test_options, args) = parser.parse_args()

        # parse the standard command line argument of the --nv is not present
        if(not test_options.advanced):
            (options, test_args) = standard_parser.parse_args()

        if(test_options.help):
            if(test_options.advanced):
                # print help message with advanced options
                parser.print_help()
            else:
                # print help message wihtout advanced options
                standard_parser.print_help()
            sys.exit(0)

        if (test_options.listTestNames):
            # print test names

            # print sanity test names
            print "SANITY TEST NAMES:"
            for sanity_test in TestFactory.sanityNameList:
                print "\t%s" % sanity_test

            # print sanity staging test names
            print "SANITY STAGING TEST NAMES:"
            for staging_test in TestFactory.sanityStagingNameList:
                print "\t%s" % staging_test

            print ""

            # print conformance test names
            print "CONFORMANCE TEST NAMES:"
            for conformance_test in TestFactory.conformanceNameList:
                print "\t%s" % conformance_test

            # print conformance staging test names
            print "CONFORMANCE STAGING TEST NAMES:"
            for staging_test in TestFactory.conformanceStagingNameList:
                print "\t%s" % staging_test

            print ""

            print "UTILITY TEST NAMES:"
            for utility_test in TestFactory.utilityNameList:
                print "\t%s" % utility_test

            print ""

            sys.exit(0)

        # print version infortion
        nvcstest.printVersionInformation(logger)

        if(test_options.version):
            # Already printed version in previous statement
            sys.exit(0)

        stagingNamesList = []
        testNamesList = []
        if (test_options.sanity):
            testNamesList = TestFactory.sanityNameList
            stagingNamesList = TestFactory.sanityStagingNameList

        elif (test_options.conformance):
            testNamesList = TestFactory.conformanceNameList
            stagingNamesList = TestFactory.conformanceStagingNameList

        elif (test_options.test_names != None):
            testNamesList = test_options.test_names

        __validateTestNames(testNamesList)

        # create a dictionary for disabled test names
        if (test_options.disabled_test_names != None):
            for testName in test_options.disabled_test_names:
                # get the test object
                if (TestFactory.hasTest(testName)):
                    TestFactory.disableTest(testName)

        logger.info("Imager ID: %d" % test_options.imager_id)
        for testrun in range(0, test_options.totalTestRuns):
            if (test_options.stagingOff == False):
                for testName in stagingNamesList:
                    # get the test object
                    try:
                        # if the test is enabled, execute the test
                        if (TestFactory.isEnabled(testName)):
                            numStagingTests = numStagingTests + 1

                            testOb = TestFactory.getTestObject(testName, test_options, logger)
                            retVal = testOb.run()

                            if (retVal == nvcstest.NvCSTestResult.SUCCESS):
                                logger.info("STAGE TEST RESULT: PASS\n")
                            elif (retVal == nvcstest.NvCSTestResult.SKIPPED):
                                logger.info("STAGE RESULT: SKIP\n")
                                numStagingSkippedTests = numStagingSkippedTests + 1
                            else:
                                logger.info("STAGE RESULT: FAIL\n")
                                numStagingFailures = numStagingFailures + 1

                    except Exception, err:
                        print err
                        numStagingFailures = numStagingFailures + 1

            for testName in testNamesList:
                # get the test object
                try:
                    # if the test is enabled, execute the test
                    if (TestFactory.isEnabled(testName)):
                        numTests = numTests + 1

                        testOb = TestFactory.getTestObject(testName, test_options, logger)
                        retVal = testOb.run()

                        if (retVal == nvcstest.NvCSTestResult.SUCCESS):
                            logger.info("RESULT: PASS\n")
                        elif (retVal == nvcstest.NvCSTestResult.SKIPPED):
                            logger.info("RESULT: SKIP\n")
                            numSkippedTests = numSkippedTests + 1
                        else:
                            logger.info("RESULT: FAIL\n")
                            numFailures = numFailures + 1

                except Exception, err:
                    print err
                    numFailures = numFailures + 1
                    logger.info("RESULT: FAIL\n")

        logger.setClientName("")

        if (numFailures == 0):
            RunSuccessStr = "PASS"
        else:
            RunSuccessStr = "FAIL"

        logger.info("=================================================================")
        logger.dumpWarnings("Accumulated WARNING messages from test run:")
        logger.dumpErrors("Accumulated ERROR messages from test run:")
        logger.info("=================================================================")
        nvcstest.printVersionInformation(logger)
        logger.info("=================================================================")
        if (numStagingTests > 0):
            logger.info("(STAGE RUNS DO NOT AFFECT NVCS FINAL RESULT)")
            logger.info("STAGE RUNS: %d" % numStagingTests)
            logger.info("STAGE FAILURES: %d" % numStagingFailures)
            logger.info("STAGE SKIPPED TESTS: %d" % numStagingSkippedTests)
            logger.info("=================================================================")
        logger.info("TOTAL TEST RUNS: %d" % numTests)
        logger.info("TOTAL FAILURES: %d" % numFailures)
        logger.info("TOTAL SKIPPED TESTS: %d" % numSkippedTests)
        logger.info("=================================================================")
        logger.info("NVCS Final Result: %s" % (RunSuccessStr))
        logger.info("=================================================================")

    if (numFailures > 0):
        sys.exit(1)

def __validateTestNames(testNamesList):
    success = True
    for testName in testNamesList:
        if (TestFactory.hasTest(testName) != True):
            success = False
            print "ERROR: Incorrect test name: %s" % testName

    if (success == False):
        sys.exit(1)

if __name__ == '__main__':
    main()
