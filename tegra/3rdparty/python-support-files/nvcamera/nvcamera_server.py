import nvcamera
import socket
import sys
import traceback
import os
import os.path
import shutil
import struct
import string
import time


class SocketStream:
    "Socket stream class"
    sock = None
    sockBufSize = None

    def __init__(self, sock):
        self.sock = sock
        self.sockBufSize = 4096

    def receiveMessage(self):
        # get message length
        msglen  = self.getInt()
        if not msglen: return None
        #print msglen

        readin  = 0
        msg = ""

        # get the message content
        while(msglen > readin):
            try:
                recvedmsg = self.sock.recv(self.sockBufSize)
                msg = msg + recvedmsg
                readin = readin + len(msg)
            except socket.errorTab, msg:
                msg = None

        return msg

    def sendMessage(self, msg):
        # send message length
        msglen = len(msg);
        #print msglen

        # convert to network byte order
        msglen = struct.pack('!i', msglen)
        self.sock.sendall(msglen)

        #print msg

        # send message
        self.sock.sendall(msg)

    def getInt(self):
        bytes = self.sock.recv(4)
        if not bytes: return None
        intVal = struct.unpack('!i', bytes[:4])[0]
        #print intVal
        return intVal


class nvCameraServerImpl:
    "nvcamera Server implementaion class"

    graph = None
    camera = None

    def __init__(self):
        self.graph = nvcamera.Graph()
        self.camera = nvcamera.Camera()

    def executeCommand(self, command):
        "execute command received from server"

        retVal =  None
        cmdObj = string.split(command, '.')
        if cmdObj[0] == 'graph' or cmdObj[0] == 'camera':
            command = 'self.' + command
        retVal = eval(command)
        return retVal

class nvCameraServer:
    "nvcamera Server class"
    oServerImpl = None

    # socket
    sock = None
    port = None
    host = None
    sockStream = None

    def __init__(self, host = 'localhost', port = 12321):
        self.oServerImpl = nvCameraServerImpl()
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.host = host
        self.port = int(port)

    def startServer(self):

        # create the nvcc_test_images directory
        # if it doesn't exist
        # delete nvcc directory
        if os.path.exists('/data/nvcc/'):
            shutil.rmtree('/data/nvcc/')
        os.mkdir('/data/nvcc/')
        # associate the socket with the port
        self.sock.bind((self.host, self.port))

        # accept call from a client
        self.sock.listen(1)
        conn, addr = self.sock.accept()

        #print 'client is at', addr

        # create a file like object
        self.sockStream = SocketStream(conn)

        while 1:
            #print "waiting to receive message..."
            cmd = self.sockStream.receiveMessage()
            #print "received %s..." % cmd
            if not cmd: break

            cmd = string.strip(cmd)
            if (cmd != '') and (cmd[:1] != '#'):
                #print "Executing command: %s" % cmd
                if (cmd[:4] == 'pull'):
                    #print "pulling the file:" + cmd[5:]
                    self.sendfile(cmd[5:], conn)
                else:
                    try:
                        # execute the command string
                        retVal = self.oServerImpl.executeCommand(cmd)
                    except Exception, err:
                        traceback.print_exc("Error: %s\n" % err)
                        msgstr = 'Exception ' + str(err)
                        self.sockStream.sendMessage(msgstr)
                    else:
                        msgstr = self.getStrFromReturnValue(retVal)
                        self.sockStream.sendMessage(msgstr)

            else:
                self.sockStream.sendMessage(cmd)

        # delete nvcc directory
        shutil.rmtree('/data/nvcc')
        conn.close()

    def sendfile(self, filename, conn):
        # get the dirname from the path
        dirName = os.path.dirname(filename)
        #print 'dirName: %s' % dirName
        fname = filename
        if os.path.exists(filename):
            pass
        else:
            fileList = os.listdir(dirName)
            #print "%s/%s" % (dirName, fileList[0])
            if os.path.exists("%s/%s" % (dirName, fileList[0])):
                fname = "%s/%s" % (dirName, fileList[0])
            else:
                err = "Error: file %s doesn't exist\n" % filename
                traceback.print_exc(err)
                self.sockStream.sendMessage('Exception ' + str(err))
                return
        #print 'filename: ' + fname
        fin = open(fname, 'rb')

        data = fin.read()
        self.sockStream.sendMessage(data)

        #print 'sent image: %s' % fname
        fin.close()

        # removing the file after sending it back to client
        os.remove(fname)

    def getStrFromReturnValue(self, value):
        strRetValue = 'None'
        if(isinstance(value, int)):
            strRetValue = 'int ' + str(value)
            return strRetValue
        elif(isinstance(value, str)):
            strRetValue = 'string ' + value
            return strRetValue
        elif(isinstance(value, float)):
            strRetValue = 'float ' + str(value)
            return strRetValue
        elif(isinstance(value, list)):
            # check for the first value of the list
            # to determine if it is a number or string array
            strRetValue = 'array '
            if(len(value) != 0):
                if(isinstance(value[0], int)):
                    strRetValue += 'int '
                    strRetValue += ' '.join([`litem` for litem in value])
                    return strRetValue
                elif(isinstance(value[0], float)):
                    strRetValue += 'float '
                    strRetValue += ' '.join([`litem` for litem in value])
                    return strRetValue
                else:
                    strRetValue += 'string '
                    strRetValue += ' '.join([`litem` for litem in value])
                    return strRetValue
            # this should never happen
            else:
                raise Exception("List is empty")

        else:
            return strRetValue

    def startDebugServer(self, filename):
        "starts debug server which basically \
         reads a file and interprets it line by line."

        filero = open(filename)

        for line in filero:
            line = line.strip()
            print "Line: %s" % line
            if (line != '') and (line[:1] != '#'):
                retVal = self.oServerImpl.executeCommand(line)
                print retVal

        filero.close()

def main():
    oServer = nvCameraServer()
    #oServer.startDebugServer(sys.argv[1])
    oServer.startServer()

# calls main function
if __name__ == '__main__':
    main()
