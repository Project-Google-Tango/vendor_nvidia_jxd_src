# Copyright (c) 2011-2014 NVIDIA Corporation.  All rights reserved.
#
# NVIDIA Corporation and its licensors retain all intellectual property
# and proprietary rights in and to this software, related documentation
# and any modifications thereto.  Any use, reproduction, disclosure or
# distribution of this software and related documentation without an express
# license agreement from NVIDIA Corporation is strictly prohibited.
#
#
from optparse import OptionParser
from optparse import OptionValueError
import traceback
import nvcamera
import time
import os

OUTTYPE_UNSPECIFIED = 0
OUTTYPE_RAW = 1
OUTTYPE_JPEG = 2
OUTTYPE_CONCURRENT = 3 # 1 + 2

def main():
    oGraph = None

    try:
        # parse arguments
        usage = "\nUsage: %prog <options>\n\nCaptures an image with AE, AWB, and AF defaulted to Automatic"
        parser = OptionParser(usage)

        parser.add_option('-i', '--imager', dest='imager_id', default=0, type="int", metavar="IMAGER_ID",
                    help = 'Sets Camera to be used for Captures                     \
                            Default = 0                                             \
                            0 = Rear Facing, 1 = Front Facing                       ')

        parser.add_option('-f', '--focus', dest='focus', default=400, type="int", metavar="FOCUS",
                    help = 'Sets the Focus Position                                 \
                            Default = 400                                           ')

        parser.add_option('-g', '--gain', dest='gain', default=1.0, type="float", metavar="GAIN",
                    help = 'Sets the amount of Digital Gain                         \
                            Default = 1.0                                           ')

        parser.add_option('-e', '--exp', dest='exp_time', default=0.1, type="float", metavar="EXP_TIME",
                    help = 'Sets the Exposure Time (in seconds)                     \
                            Default = 0.1                                           ')

        parser.add_option('-p', '--preview', dest='preview', default=(0, 0), type="int",
                    metavar="PREVIEW_RES", nargs=2,
                    help = 'Sets the Image Preview Resolution                       \
                            Default = 0 0 [Maximum Supported Preview Resolution]  \
                            --preview=<width> <height>                              ')

        parser.add_option('-w', '--preview_wait', dest='preview_wait', default=0, type="int", metavar="PREVIEW_WAIT",
                    help = 'Sets the number of seconds to wait before capture       \
                            Default = 0                                             ')

        parser.add_option('-s', '--still', dest='still', default=(0, 0), type="int",
                    metavar="STILL_RES", nargs=2,
                    help = 'Sets the Capture Resolution                             \
                            Default = 0 0 [Maximum Supported Capture Resolution]  \
                            --still=<width> <height>                                \
                            NOTE: Does not affect Raw Captures                      ')

        parser.add_option('-n', '--name', dest='fname', default="nvcs_test", type="str", metavar="FILE_NAME",
                    help = 'Sets the Name of the Output File                        \
                            Default = nvcs_test                                     ')

        parser.add_option('-o', '--outdir', dest='output_dir',  type="str", metavar="OUTPUT_DIR", default = '/data/raw',
                    help = 'Sets the Name of the Output Directory                   \
                            Default = /data/raw                                     ')

        parser.add_option('-t', '--outputtype', dest='output_type', type="str", metavar="OUTPUT_TYPE", default = 'raw',
                    help = 'Sets the Type of Output                                 \
                            Default = raw                                           \
                            Supported Types: raw, jpeg, concurrent                  ')

        parser.add_option('-v', '--version', dest='version', action='store_true', metavar="VERSION", default = False,
                    help = 'Prints NvCamera Module Version                          \
                            Default = False                                         ')

        # parse the command line arguments
        (options, args) = parser.parse_args()

        # print nvcamera module version
        if (options.version):
            print "nvcamera version: %s" %nvcamera.__version__
            return

        rawImageDir = "/data/raw" # Unrelated to Output Directory Option
        outType = OUTTYPE_UNSPECIFIED

        # Determine Output Type
        if (options.output_type[0] == 'r'): #RAW
            outType = OUTTYPE_RAW

        elif (options.output_type[0] == 'j'): #JPEG
            outType = OUTTYPE_JPEG

        elif (options.output_type[0] == 'c'): #CONCURRENT
            outType = OUTTYPE_CONCURRENT

            # Create Raw Dump Directory for Driver
            if not os.path.exists(rawImageDir):
                os.mkdir(options.output_dir)

        else: #Unknown
            raise Exception("Unknown Capture Type")

        # create and run camera graph
        oGraph = nvcamera.Graph()
        oGraph.setImager(options.imager_id)

        # set preview resolution
        oGraph.preview(options.preview[0], options.preview[1])

        graphType = "Jpeg"
        if(outType == OUTTYPE_RAW):
            graphType = "Bayer"

        # set capture resolution
        oGraph.still(options.still[0], options.still[1], graphType)

        oGraph.run()

        oCamera = nvcamera.Camera()
        oCamera.setAttr(nvcamera.attr_pauseaftercapture, 1)

        # Create Output Directory
        if not os.path.exists(options.output_dir):
            os.mkdir(options.output_dir)

        # Start Camera and Wait until Ready
        oCamera.startPreview()
        oCamera.waitForEvent(12000, nvcamera.CamConst_FIRST_PREVIEW_FRAME)

        # Set Camera Attributes
        if (options.gain != -1):
            oCamera.setAttr(nvcamera.attr_bayergains, [options.gain, options.gain, options.gain, options.gain])
        oCamera.setAttr(nvcamera.attr_exposuretime, options.exp_time)

        hasFocuser = True
        try:
            physicalRange = oCamera.getAttr(nvcamera.attr_focuspositionphysicalrange)
        except nvcamera.NvCameraException, err:
            print("focuser is not supported")
            hasFocuser = False
        else:
            if (physicalRange[0] == physicalRange[1]):
                print("focuser is not present")
                hasFocuser = False
        if((hasFocuser != True) and (options.focus != -1)):
            raise OptionValueError("\nERROR: Focuser is not supported. Can't use -f or --focus")

        if(hasFocuser):
            if(options.focus != -1):
                oCamera.setAttr(nvcamera.attr_focuspos, options.focus)
            else:
                oCamera.setAttr(nvcamera.attr_autofocus, 1)

        time.sleep(options.preview_wait)

        # Generate Image Path
        if(outType == OUTTYPE_RAW):
            filePath = os.path.join(options.output_dir, options.fname + ".nvraw")
        else:
            filePath = os.path.join(options.output_dir, options.fname + ".jpg")

        # If output file already exists, delete it first.
        if os.path.exists(filePath):
            os.remove(filePath)

        if(outType == OUTTYPE_CONCURRENT):
            rawFilesList = os.listdir(rawImageDir)
            oCamera.setAttr(nvcamera.attr_concurrentrawdumpflag, 7)

        # ---- IMAGE CAPTURE START ---- #
        oCamera.halfpress(10000)
        oCamera.waitForEvent(12000, nvcamera.CamConst_ALGS)

        # Capture an Image
        oCamera.still(filePath)

        #oCamera.waitForEvent(12000, nvcamera.CamConst_CAP_READY, nvcamera.CamConst_CAP_FILE_READY)
        oCamera.waitForEvent(12000, nvcamera.CamConst_CAP_READY)

        oCamera.hp_release()
        # ---- IMAGE CAPTURE STOP ---- #

        oCamera.stopPreview()

        oCamera.waitForEvent(12000, nvcamera.CamConst_PREVIEW_EOS)

        # stop and close the graph
        oGraph.stop()
        oGraph.close()

        outputStr = ""
        outputStr += "Settings:\n"
        outputStr += " - Gain:      [%s]\n" % (str(options.gain) if (options.gain != -1) else "Auto")
        outputStr += " - Exposure:  [%s]\n" % (str(options.exp_time) if (options.exp_time != -1) else "Auto")
        outputStr += " - Focus:     [%s]\n" % (str(options.focus) if (options.focus != -1) else "Auto")

        outputStr += "------------------------------------\n"

        outputStr += "Output:\n"

        if(outType & OUTTYPE_JPEG):
            outputStr += " - JPEG:      [%s]\n" % filePath
        elif(outType == OUTTYPE_RAW):
            outputStr += " - RAW:       [%s]\n" % filePath

        # Find New Raw File & Rename it (Only for Concurrent Jpeg & Raw)
        if(outType == OUTTYPE_CONCURRENT):
            newList = os.listdir(rawImageDir)
            for key in newList:
                if key not in rawFilesList:
                    if 'nvraw' in key:
                        os.rename(os.path.join(rawImageDir, key), os.path.join(options.output_dir, options.fname + ".nvraw"))
                        outputStr += " - RAW:       [%s]\n" % os.path.join(options.output_dir, options.fname + ".nvraw")
                        break

        if(outputStr[len(outputStr)-1] == '\n'):
            outputStr = outputStr[:-1]

        print "<><><><><><><><><><><><><><><><><><>"
        print outputStr
        print "<><><><><><><><><><><><><><><><><><>"

    except Exception, err:
        print traceback.print_exc() # Add line for debugging
        print err
        if(oGraph):
            oGraph.stop()
            oGraph.close()
        exit(1)

if __name__ == '__main__':
    main()
