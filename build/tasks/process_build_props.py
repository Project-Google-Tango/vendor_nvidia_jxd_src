#!/usr/bin/env python
#
# Copyright (c) 2013-2014 NVIDIA Corporation.  All Rights Reserved.
#
# NVIDIA Corporation and its licensors retain all intellectual property
# and proprietary rights in and to this software, related documentation
# and any modifications thereto.  Any use, reproduction, disclosure or
# distribution of this software and related documentation without an express
# license agreement from NVIDIA Corporation is strictly prohibited.
#
# Script to change properties in build.prop on the fly
#

import sys
import os
import optparse
import logging
import re
import codecs
from xml.dom.minidom import parse

# Functions
def createLogger():
    """
    This function returns a logger instance which should be used for
    any output in the script.

    To log, use: logger.info("message1"), logger.warning("message2"), ...
    The default levels are debug, info, warning, error, critical.
    To change the reporting level, use e.g. logger.setLevel(logging.INFO) .
    """
    mylogger = logging.getLogger("mangle")
    formatter = logging.Formatter('%(filename)s:%(lineno)s %(levelname)s: %(message)s')
    handler = logging.StreamHandler()
    handler.setFormatter(formatter)
    mylogger.addHandler(handler)

    return mylogger

def parseCmdline(logger):
    """
    Parses the command line arguments
    """
    parser = optparse.OptionParser()
    usage = "%s [options]" % os.path.basename(sys.argv[0])
    parser.set_usage(usage)
    parser.add_option("-s", "--skuid", help="Name of the sku of the product")
    parser.add_option("-m", "--manifest", help="Full path of product manifest file")
    parser.add_option("-b", "--buildprop", help="Full path of build.prop file")
    parser.add_option("--debug", action="store_true", help="Show debug messages")
    (options, args) = parser.parse_args()
    mandatories = ['skuid', 'manifest', 'buildprop']
    for m in mandatories:
        if not options.__dict__[m]:
            logger.error("Mandatory options are missing!")
            parser.print_help()
            sys.exit(2)

    if len(args) != 0:
        logger.error("No arguments required by this script!")
        parser.print_help()
        sys.exit(2)

    options.manifest = os.path.abspath(options.manifest)
    if not os.access(options.manifest, os.R_OK):
        logger.error("%s is not accessible" % options.manifest)
        sys.exit(2)

    options.buildprop = os.path.abspath(options.buildprop)
    if not os.access(options.buildprop, os.R_OK):
        logger.error("%s is not accessible" % options.buildprop)
        sys.exit(2)

    # Default logger level is INFO
    logger.setLevel(logging.INFO)
    if options.debug:
        logger.setLevel(logging.DEBUG)

    return options, args

def changeProperties(logger, pObj, name, value):
    """
    Changes the value of existing property in build.prop
    """
    oldVal = pObj.get(name)
    if value and oldVal:
        logger.info("Property: %s" % name)
        logger.info("Old Value: %s" % oldVal)
        logger.info("New Value: %s\n" % value)
        pObj.put(name,value)

def getBuildFingerprint(logger, sObj, pObj):
    """
    Generate build fingerprint based on data in sku manifest
    """
    buildFingerprint = {}
    regExp = r'(?P<brand>.+?)/(?P<product>.+?)/(?P<device>.+?)(:.+)'
    oldVal = pObj.get('ro.build.fingerprint')
    regObj = re.compile(regExp).match(oldVal)
    if oldVal and regObj:
        brand = regObj.group('brand')
        if sObj.getProperty('ro.product.brand'):
            brand = sObj.getProperty('ro.product.brand')
        product = regObj.group('product')
        if sObj.getProperty('ro.product.name'):
            product = sObj.getProperty('ro.product.name')
        device = regObj.group('device')
        if sObj.getProperty('ro.product.device'):
            device = sObj.getProperty('ro.product.device')
        trio = brand+'/'+product+'/'+device
        buildFingerprint['ro.build.fingerprint'] = trio+regObj.group(4)
    else:
        buildFingerprint['ro.build.fingerprint'] = oldVal

    return buildFingerprint

def getBuildDesc(logger, sObj, pObj):
    """
    Generate build description based on data in sku manifest
    """
    buildDesc = {}
    regExp = r'(?P<product>.+?)(?P<variant>-.+?)(\s.+)'
    oldVal = pObj.get('ro.build.description')
    regObj = re.compile(regExp).match(oldVal)
    if oldVal and regObj:
        product = regObj.group('product')
        if sObj.getProperty('ro.product.name'):
            product = sObj.getProperty('ro.product.name')
        buildDesc['ro.build.description'] = product+regObj.group(2)+regObj.group(3)
    else:
        buildDesc['ro.build.description'] = oldVal

    return buildDesc

def getBuildDisp(logger, sObj, pObj):
    """
    Generate build display based on data in sku manifest
    """
    buildDisp = {}
    regExp = r'(?P<product>.+?)(?P<variant>-.+?)(\s.+)'
    oldVal = pObj.get('ro.build.display.id')
    regObj = re.compile(regExp).match(oldVal)
    if oldVal and regObj:
        product = regObj.group('product')
        if sObj.getProperty('ro.product.name'):
            product = sObj.getProperty('ro.product.name')
        buildDisp['ro.build.display.id'] = product+regObj.group(2)+regObj.group(3)
    else:
        buildDisp['ro.build.display.id'] = oldVal

    return buildDisp

# Classes
class SkuProp:
    """
    This class handles sku manifest
    """
    def __init__(self, logger, skuid, manifest):
        self.logger = logger
        DOMTree = parse(manifest)
        product = DOMTree.documentElement
        skus = product.getElementsByTagName("sku")
        for s in skus:
            if s.hasAttribute("id") and s.getAttribute("id") == skuid:
                logger.info("Processing node %s\n" % s.getAttribute("id"))
                self.sku = s
                break
        else:
            logger.error("No matching node found in %s for skuid %s\n" % (manifest, skuid))
            sys.exit(2)

    def getProperty(self, name):
        if self.sku:
            for p in self.sku.getElementsByTagName("property"):
                if p.getAttribute("name") == name:
                    return p.getAttribute("value")
        return

    def getAllProperties(self):
        properties = {}
        if self.sku:
            for p in self.sku.getElementsByTagName("property"):
                properties[p.getAttribute("name")] = p.getAttribute("value")
        return properties


class BuildProp:
    """
    This class handles build.prop
    """
    def __init__(self, logger, buildprop):
        self.logger = logger
        self.buildprop = buildprop
        f = codecs.open(self.buildprop, 'r', 'utf-8')
        self.lines = f.readlines()
        f.close()
        self.lines = [s[:-1] for s in self.lines]

    def get(self, name):
        key = name + "="
        for line in self.lines:
            self.logger.debug("Line: %s" % line)
            if line.startswith(key):
                return line[len(key):]
        return

    def put(self, name, value):
        key = name + "="
        for i in range(0,len(self.lines)):
            if self.lines[i].startswith(key):
                self.lines[i] = key + value
                return
        self.lines.append(key + value)

    def write(self):
        f = codecs.open(self.buildprop, 'w+', 'utf-8')
        f.write("\n".join(self.lines))
        f.write("\n")
        f.close

# Main()
def main():
    """
    main program
    """
    # Logger to be used throughout this program
    logger = createLogger()
    (options, arguments) = parseCmdline(logger)
    skuid = options.skuid
    manifest = options.manifest
    buildprop = options.buildprop
    skuObj = SkuProp(logger, skuid, manifest)
    propObj = BuildProp(logger, buildprop)
    properties = skuObj.getAllProperties()
    properties.update(getBuildFingerprint(logger, skuObj, propObj))
    properties.update(getBuildDesc(logger, skuObj, propObj))
    properties.update(getBuildDisp(logger, skuObj, propObj))
    for p in properties:
        changeProperties(logger, propObj, p, properties[p])
    propObj.write()

if __name__ == "__main__":
    main()
