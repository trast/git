import socket
import hashlib
import binascii
import zlib
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
writtendata = ""
def fwrite(data):
    global writtenbytes
    global writtendata
    writtendata += data
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


        pathname = os.path.dirname(string)
        filename = os.path.basename(string)
        paths.add(pathname)
        files.add(filename)

        entry = entry + (pathname, filename)           # Filename

        if (header["version"] == 3):
            dictentry = dict(zip(('ctimesec', 'ctimensec', 'mtimesec', 'mtimensec', 
                'dev', 'ino', 'mode', 'uid', 'gid', 'filesize', 'sha1', 'flags',
                'xtflags', 'pathname', 'filename'), entry))
        else:
            dictentry = dict(zip(('ctimesec', 'ctimensec', 'mtimesec', 'mtimensec', 
                'dev', 'ino', 'mode', 'uid', 'gid', 'filesize', 'sha1', 'flags',
                'pathname', 'filename'), entry))

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
        if entry["pathname"] != "":
            print entry["pathname"] + "/" + entry["filename"]
        else:
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
def writeheader(header, paths, files):
    fwrite(header["signature"])
    fwrite(struct.pack("!IIII", header["version"], len(paths), len(files), header["nrofentries"]))
# }}}

# writedirectories {{{
def writedirectories(paths, treeextensiondata):
    offsets = dict()
    subtreenr = dict()
    # Calculate subtree numbers
    for p in sorted(paths, reverse=True):
        splited = p.split("/")
        if p not in subtreenr:
            subtreenr[p] = 0
        if len(splited) > 1:
            i = 0
            path = ""
            while i < len(splited) - 1:
                path += "/" + splited[i]
                i += 1
            if path[1:] not in subtreenr:
                subtreenr[path[1:]] = 1
            else:
                subtreenr[path[1:]] += 1
    
    for p in paths:
        offsets[p] = writtenbytes
        fwrite(struct.pack("!Q", 0))
        fwrite(p.split("/")[-1] + "\0")
        p += "/"
        if p in treeextensiondata:
            fwrite(struct.pack("!ll", int(treeextensiondata[p]["entry_count"]), int(treeextensiondata[p]["subtrees"])))
            if (treeextensiondata[p]["entry_count"] != "-1"):
                fwrite(binascii.unhexlify(treeextensiondata[p]["sha1"]))

        else: # If there is no cache-tree data we assume the entry is invalid
            fwrite(struct.pack("!II", -1, subtreenr[p]))
    return offsets
# }}}

# writefiles {{{
def writefiles(files):
    offsets = dict()
    for f in files:
        offsets[f] = writtenbytes
        fwrite(f)
        fwrite("\0")
    return offsets
# }}}

# writefileentries {{{
def writefileentries(entries, fileoffsets):
    offsets = dict()
    for e in sorted(entries, key=lambda k: k['pathname']):
        if e["pathname"] not in offsets:
            offsets[e["pathname"]] = writtenbytes
        fwrite(struct.pack("!IIIIIIIIII", e["ctimesec"], e["ctimensec"],
            e["mtimesec"], e["mtimensec"], e["dev"], e["ino"], e["mode"],
            e["uid"], e["gid"], e["filesize"]))
        fwrite(binascii.unhexlify(e["sha1"]))

        try:
            fwrite(struct.pack("!III", e["flags"], e["xtflags"], 
                fileoffsets[e["filename"]]))
        except KeyError:
            fwrite(struct.pack("!III", e["flags"], 0, 
                fileoffsets[e["filename"]]))

        writecrc32()
    return offsets
# }}}

# writefileoffsets {{{
def writefileoffsets(diroffsets, fileoffsets, dircrcoffset):
    for d in sorted(diroffsets):
        fw.seek(diroffsets[d])
        fw.write(struct.pack("!Q", fileoffsets[d]))

    # Calculate crc32
    #f.seek(0)
    #data = f.read(dircrcoffset)
    fw.seek(dircrcoffset)
    fw.write(struct.pack("!I", binascii.crc32(writtendata) & 0xffffffff))

# }}}

# writecrc32 {{{
def writecrc32():
    global writtendata
    fwrite(struct.pack("!I", binascii.crc32(writtendata) & 0xffffffff))
    writtendata = "" # Reset writtendata for next crc32
# }}}


header = readheader(f)

indexentries, byte, paths, files = readindexentries(f)

ext = fread(3)
ext = byte + ext
extensiondata = []

if ext == "TREE":
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
    writeheader(header, paths, files)
    diroffsets = writedirectories(sorted(paths), treeextensiondata)
    fileoffsets = writefiles(files)
    dircrcoffset = writtenbytes

    # TODO: Replace with something faster. Doesn't make sense to calculate crc32 here
    writecrc32()
    fileoffsets = writefileentries(indexentries, fileoffsets)
    writefileoffsets(diroffsets, fileoffsets, dircrcoffset)
else:
    print "File is corrupted"
