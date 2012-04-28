import socket
import hashlib
import binascii
import struct

f = open(".git/index", "rb")
filedata = ""
def fread(n):
    global filedata
    data = f.read(n)
    if filedata == "":
        filedata = data
    else:
        filedata += data
    return data

def convert(n):
    return str(struct.unpack('!I', n)[0])

def readheader(f):
    # Signature
    signature = fread(4)
    header = struct.unpack('!II', fread(8))
    return dict({"signature": signature, "version": header[0], "nrofentries": header[1]})

def readindexentries(f):
    indexentries = []
    byte = fread(1)
    i = 0
    # Read index entries
    while i < header["nrofentries"]:
        entry = struct.unpack('!IIIIIIIIII', byte + fread(39)) # stat data
        entry = entry + (str(binascii.hexlify(fread(20))),)    # SHA-1

        if (header["version"] == 3):
            entry = entry + struct.unpack('!hh', fread(4))     # Flags + extended flags
        else:
            entry = entry + struct.unpack('!h', fread(2))      # Flags

        string = ""
        byte = fread(1)
        while byte != '\0':
            string = string + byte
            byte = fread(1)

        entry = entry + (string, )                              # Filename

        if (header["version"] == 3):
            dictentry = dict(zip(('ctimesec', 'ctimensec', 'mtimesec', 'mtimensec', 
                'dev', 'ino', 'mode', 'uid', 'gid', 'filesize', 'sha1', 'flags',
                'xtflags', 'filename'), entry))
        else:
            dictentry = dict(zip(('ctimesec', 'ctimensec', 'mtimesec', 'mtimensec', 
                'dev', 'ino', 'mode', 'uid', 'gid', 'filesize', 'sha1', 'flags',
                'filename'), entry))

        while byte == '\0':
            byte = fread(1)

        indexentries.append(dictentry)

        i = i + 1

    return indexentries, byte

def readextensiondata(f):
    extensionsize = fread(4)

    read = 0
    subtreenr = [0]
    subtree = [""]
    listsize = 0
    while read < int(convert(extensionsize)):
        path = ""
        byte = fread(1)
        read += 1
        while byte != '\0':
            path += byte
            byte = fread(1)
            read += 1

        while listsize >= 0 and subtreenr[listsize] == 0:
            subtreenr.pop()
            subtree.pop()
            listsize -= 1

        fpath = ""
        if listsize > 0: 
            for p in subtree:
                if p != "": 
                    fpath += p + "/"
            subtreenr[listsize] = subtreenr[listsize] - 1
        fpath += path + "/"

        entry_count = ""
        byte = fread(1)
        read += 1
        while byte != " ":
            entry_count += byte
            byte = fread(1)
            read += 1

        subtrees = ""
        byte = fread(1)
        read += 1
        while byte != "\n":
            subtrees += byte
            byte = fread(1)
            read += 1

        subtreenr.append(int(subtrees))
        subtree.append(path)
        listsize += 1

        if entry_count != "-1":
            sha1 = binascii.hexlify(fread(20))
            read += 20
        else:
            sha1 = "invalid"

        extensiondata.append(dict({"path": fpath, "entry_count": entry_count,
            "subtrees": subtrees, "sha1": sha1}))

    return extensiondata

def printheader(header):
    print "Signature: " + header["signature"]
    print "Version: " + str(header["version"])
    print "Number of entries: " + str(header["nrofentries"])

def printindexentries(indexentries):
    for entry in indexentries:
        print entry["filename"]
        print "  ctime: " + str(entry["ctimesec"]) + ":" + str(entry["ctimensec"])
        print "  mtime: " + str(entry["mtimesec"]) + ":" + str(entry["mtimensec"])
        print "  dev: " + str(entry["dev"]) + "\tino: " + str(entry["ino"])
        print "  uid: " + str(entry["uid"]) + "\tgid: " + str(entry["gid"])
        print "  size: " + str(entry["filesize"]) + "\tflags: " + "%x" % entry["flags"]

def printextensiondata(extensiondata):
    for entry in extensiondata:
        print entry["sha1"] + " " + entry["path"] + " (" + entry["entry_count"] + " entries, " + entry["subtrees"] + " subtrees)"

fw = open(".git/index-v4", "wb")

header = readheader(f)

indexentries, byte = readindexentries(f)

sup = fread(3)
byte = byte + sup
extensiondata = []

if byte == "TREE":
    extensiondata = readextensiondata(f)

printheader(header)
printindexentries(indexentries)
printextensiondata(extensiondata)

sha1 = hashlib.sha1()
sha1.update(filedata)
print "SHA1 over filedata: " + str(sha1.hexdigest())

sha1 = f.read(20)
print "SHA1 over the whole file: " + str(binascii.hexlify(sha1))

