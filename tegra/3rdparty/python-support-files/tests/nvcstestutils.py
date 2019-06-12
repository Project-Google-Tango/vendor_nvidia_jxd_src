# Copyright (c) 2012-2013, NVIDIA Corporation.  All rights reserved.
#
# NVIDIA Corporation and its licensors retain all intellectual property
# and proprietary rights in and to this software, related documentation
# and any modifications thereto.  Any use, reproduction, disclosure or
# distribution of this software and related documentation without an express
# license agreement from NVIDIA Corporation is strictly prohibited.
#

# Copyright (c) 2001-2003 Gregory P. Ward.  All rights reserved.
# Copyright (c) 2002-2003 Python Software Foundation.  All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#  * Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
#
#  * Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
#  * Neither the name of the author nor the names of its
#    contributors may be used to endorse or promote products derived from
#    this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
# IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
# TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
# PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR
# CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
# EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
# PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
# PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
# LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
# NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
# SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

import os
import shutil
import datetime
import optparse
import textwrap
import nvrawfile

class LogLevel(object):
    debug = 1
    info = 2
    warning = 3
    error = 4
    fatal = 5

class Singleton(object):
    """ A Pythonic Singleton """
    def __new__(cls, *args, **kwargs):
        if '_inst' not in vars(cls):
            cls._inst = object.__new__(cls, *args, **kwargs)
        return cls._inst

class Logger(Singleton):
    "Logger class"

    logLevel = LogLevel.debug
    logFileHandle = None
    logFileName = "summarylog.txt"
    logDir = "/data/NVCSTest"
    logWarningList = ""
    logErrorList = ""
    logFatalList = ""
    clientName = ""

    def __enter__(self):
        if(self.logFileHandle == None):
            if(os.path.isdir(self.logDir)):
                shutil.rmtree(self.logDir + "/")
            os.mkdir(self.logDir)
            logFilePath = os.path.join(self.logDir, self.logFileName)
            self.logFileHandle = open(logFilePath,"w")
        return self

    def __exit__(self, type, value, traceback):
        if (self.logFileHandle != None):
            print "close file"
            self.logFileHandle.close()
            self.logFileHandle = None

    def __getLevelString(self, level):
        if (level == LogLevel.debug):
            return "DEBUG"
        elif (level == LogLevel.info):
            return "INFO"
        elif (level == LogLevel.warning):
            return "WARNING"
        elif (level == LogLevel.error):
            return "ERROR"
        elif (level == LogLevel.fatal):
            return "FATAL"

    def setLevel(self, level):
        self.setLevel = level

    def setClientName(self, name):
        self.clientName = name

    def info(self, msg):
        self.__log(LogLevel.info, msg)

    def debug(self, msg):
        self.__log(LogLevel.debug, msg)

    def warning(self, msg):
        self.__log(LogLevel.warning, msg)

    def error(self, msg):
        self.__log(LogLevel.error, msg)

    def fatal(self, msg):
        self.__log(LogLevel.fatal, msg)

    def __log(self, level, msg):
        if (level < self.logLevel):
            return
        levelString = self.__getLevelString(level)
        timeStampString = str(datetime.datetime.now())
        levelStringMsg = "%s: %s" % (levelString, msg)

        if (level == LogLevel.warning):
            self.logWarningList += "%s         %s: %s\n" % (timeStampString, self.clientName, levelStringMsg)
        elif (level == LogLevel.error):
            self.logErrorList += "%s         %s: %s\n" % (timeStampString, self.clientName, levelStringMsg)
        elif (level == LogLevel.fatal):
            self.logFatalList += "%s         %s: %s\n" % (timeStampString, self.clientName, levelStringMsg)

        print "%s" % (levelStringMsg)
        self.logFileHandle.write("%s %7s %s\n" % (timeStampString, levelString, msg))

    def dumpWarnings(self, header):
        if (self.logWarningList != ""):
            print "%s" % (header)
            print "%s" % (self.logWarningList)
            timeStampString = str(datetime.datetime.now())
            self.logFileHandle.write("%s         %s\n" % (timeStampString, header))
            self.logFileHandle.write("%s" % (self.logWarningList))

    def dumpErrors(self, header):
        if (self.logErrorList != ""):
            print "%s" % (header)
            print "%s" % (self.logErrorList)
            timeStampString = str(datetime.datetime.now())
            self.logFileHandle.write("%s         %s\n" % (timeStampString, header))
            self.logFileHandle.write("%s" % (self.logErrorList))

    def dumpFatals(self, header):
        if (self.logFatalList != ""):
            print "%s" % (self.logFatalList)
            timeStampString = str(datetime.datetime.now())
            self.logFileHandle.write("%s         %s\n" % (timeStampString, header))
            self.logFileHandle.write("%s" % (self.logFatalList))


class HelpFormatterWithNewLine(optparse.HelpFormatter):
    """Extend optparse.HelpFormatter to allow using
       escape sequences in options help message"""

    def __init__(self,
        indentIncrement=2,
        maxHelpPosition=24,
        width=None,
        shortFirst=1):
        optparse.HelpFormatter.__init__(
            self, indentIncrement, maxHelpPosition, width, shortFirst)

    def format_description(self, description):
        descLines = []

        for line in description.split("\n"):
            descLines.extend(self._format_text(line))

        descLines.append("\n")

        return "\n".join(descLines)

    def format_option(self, option):
        # The help for each option consists of two parts:
        #   * the opt strings and metavars
        #     eg. ("-x", or "-fFILENAME, --file=FILENAME")
        #   * the user-supplied help string
        #     eg. ("turn on expert mode", "read data from FILENAME")
        #
        # If possible, we write both of these on the same line:
        #   -x      turn on expert mode
        #
        # But if the opt string list is too long, we put the help
        # string on a second line, indented to the same column it would
        # start in if it fit on the first line.
        #   -fFILENAME, --file=FILENAME
        #           read data from FILENAME
        result = []
        opts = self.option_strings[option]
        opt_width = self.help_position - self.current_indent - 2
        if len(opts) > opt_width:
            opts = "%*s%s\n" % (self.current_indent, "", opts)
            indent_first = self.help_position
        else:                       # start help on same line as opts
            opts = "%*s%-*s  " % (self.current_indent, "", opt_width, opts)
            indent_first = 0
        result.append(opts)
        if option.help:
            help_text = self.expand_default(option)
            help_lines = []
            [help_lines.extend(
                textwrap.wrap(line, self.help_width)
            ) for line in help_text.split("\n")]
            result.append("%*s%s\n" % (indent_first, "", help_lines[0]))
            result.extend(["%*s%s\n" % (self.help_position, "", line)
                           for line in help_lines[1:]])
        elif opts[-1] != "\n":
            result.append("\n")
        return "".join(result)

    def format_usage(self, usage):
        return "Usage: %s\n" % usage

    def format_heading(self, heading):
        return "%*s%s:\n" % (self.current_indent, "", heading)

class NvCSRawFile(nvrawfile.NvRawFile):
    "NvCS NvRawFile Wrapper Class"

    def __init__(self):
        nvrawfile.NvRawFile.__init__(self)

    def readFile(self, filename):
        result = nvrawfile.NvRawFile.readFile(self, filename)
        self.loadHeader()
        return result

    def getPixelValue(self, x, y):
        if (self._loaded == False):
            return NvCSTestResult.ERROR
        value = self._pixelData[y * self._width + x]
        if (hasattr(self, '_pixelFormat')):
            if (self._pixelFormat == "s1.14"):
                bitShift = 14-self._bitsPerSample
                return (value >> bitShift)
            elif (self._pixelFormat == "int16"):
                return value
        else:
            return value

    def getMaxPixelValue(self):
        if (self._loaded == False):
            return NvCSTestResult.ERROR
        maxVal = max(self._pixelData.tolist())
        if (hasattr(self, '_pixelFormat')):
            if (self._pixelFormat == "s1.14"):
                bitShift = 14-self._bitsPerSample
                return (maxVal >> bitShift)
            elif (self._pixelFormat == "int16"):
                return maxVal
        else:
            return maxVal

    def isHDRImage(self):
        if (self._hdrNumberOfExposures > 0):
            return True
        else:
            return False

    def loadHeader(self):
        # sensorGains
        if (self.isHDRImage()):
            self._sensorGains = self._hdrExposureInfos[0].analogGains
        # exposureTime
        if (self.isHDRImage()):
            self._exposureTime = self._hdrExposureInfos[0].exposureTime
        return True
