import socket
import hashlib
import binascii
import struct
import os.path

# fread {{{
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
# }}}

# fwrite {{{
fw = open(".git/index-v4", "wb")
writtenbytes = 0
def fwrite(data):
    global writtenbytes
    writtenbytes += len(data)
    fw.write(data)
# }}}

# convert {{{
def convert(n):
    return str(struct.unpack('!I', n)[0])
# }}}

# readheader {{{
def readheader(f):
    # Signature
    signature = fread(4)
    header = struct.unpack('!II', fread(8))
    return dict({"signature": signature, "version": header[0], "nrofentries": header[1]})
# }}}

# readindexentries {{{
def readindexentries(f):
    indexentries = []
    paths = set()
    files = set()
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


        paths.add(os.path.dirname(string))
        files.add(os.path.basename(string))

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

    return indexentries, byte, paths, files
# }}}

# readextensiondata {{{
def readextensiondata(f):
    extensionsize = fread(4)

    read = 0
    subtreenr = [0]
    subtree = [""]
    listsize = 0
    extensiondata = dict()
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

        extensiondata[fpath] = dict({"path": fpath, "entry_count": entry_count,
            "subtrees": subtrees, "sha1": sha1})

    return extensiondata
# }}}

# printheader {{{
def printheader(header):
    print "Signature: " + header["signature"]
    print "Version: " + str(header["version"])
    print "Number of entries: " + str(header["nrofentries"])
# }}}

# printindexentries {{{
def printindexentries(indexentries):
    for entry in indexentries:
        print entry["filename"]
        print "  ctime: " + str(entry["ctimesec"]) + ":" + str(entry["ctimensec"])
        print "  mtime: " + str(entry["mtimesec"]) + ":" + str(entry["mtimensec"])
        print "  dev: " + str(entry["dev"]) + "\tino: " + str(entry["ino"])
        print "  uid: " + str(entry["uid"]) + "\tgid: " + str(entry["gid"])
        print "  size: " + str(entry["filesize"]) + "\tflags: " + "%x" % entry["flags"]
# }}}

# {{{ printextensiondata
def printextensiondata(extensiondata):
    for entry in extensiondata.viewvalues():
        print entry["sha1"] + " " + entry["path"] + " (" + entry["entry_count"] + " entries, " + entry["subtrees"] + " subtrees)"
# }}}

# writeheader {{{
def writeheader(f, header, paths, files):
    fwrite(header["signature"])
    fwrite(struct.pack("!IIII", header["version"], len(paths), len(files), header["nrofentries"]))
# }}}

# writedirectories {{{
def writedirectories(f, paths, treeextensiondata):
    offsets = dict()
    for p in paths:
        offsets[p] = writtenbytes
        fwrite(struct.pack("!Q", 0))
        fwrite(p + "\0")
        p += "/"
        if p in treeextensiondata:
            fwrite(struct.pack("!ll", int(treeextensiondata[p]["entry_count"]), int(treeextensiondata[p]["subtrees"])))
            if (treeextensiondata[p]["entry_count"] != "-1"):
                fwrite(binascii.unhexlify(treeextensiondata[p]["sha1"]))

        else: # If there is no cache-tree data we assume the entry is invalid
            # TODO: Subtree stuff
            fwrite(struct.pack("!II", -1, 0))
    return offsets
# }}}

header = readheader(f)

indexentries, byte, paths, files = readindexentries(f)

sup = fread(3)
byte = byte + sup
extensiondata = []

if byte == "TREE":
    treeextensiondata = readextensiondata(f)

# printheader(header)
# printindexentries(indexentries)
# printextensiondata(extensiondata)

sha1 = hashlib.sha1()
sha1.update(filedata)
print "SHA1 over filedata: " + str(sha1.hexdigest())

sha1read = f.read(20)
print "SHA1 over the whole file: " + str(binascii.hexlify(sha1read))

if sha1.hexdigest() == binascii.hexlify(sha1read):
    writeheader(fw, header, paths, files)
    offsets = writedirectories(fw, sorted(paths), treeextensiondata)

    # Test for the offsets
    i = 1
    for p in paths:
        print offsets[p]
        fw.seek(offsets[p])
        fw.write(struct.pack("!Q", i))
        i += 1

    print "Write index"
    # Write new index

