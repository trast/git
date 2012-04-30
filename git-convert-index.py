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

# readreucextensiondata {{{
def readreucextensiondata(f):
    extensionsize = fread(4)

    read = 0
    extensiondata = list()
    while read < int(convert(extensionsize)):
        path = ""
        byte = fread(1)
        read += 1
        while byte != '\0':
            path += byte
            byte = fread(1)
            read += 1

        entry_mode = list()
        i = 0
        while i < 3:
            byte = fread(1)
            read += 1
            nr = ""
            while byte != '\0':
                nr += byte
                byte = fread(1)
                read += 1
            i += 1

            entry_mode.append(int(nr))

        i = 0
        obj_names = list()
        while i < 3:
            if entry_mode[i] != 0:
                obj_names.append(fread(20))
                read += 20
            else:
                obj_names[i] = ""
            i += 1

        extensiondata.append(dict({"path": path, "entry_mode0": entry_mode[0], "entry_mode1": entry_mode[1], "entry_mode2": entry_mode[2], "obj_names0": obj_names[0], "obj_names1": obj_names[1], "obj_names2": obj_names[2]}))

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

# printreucextensiondata {{{
def printreucextensiondata(extensiondata):
    for e in extensiondata:
        print "Path: " + e["path"]
        print "Entrymode 1: " + str(e["entry_mode0"]) + " Entrymode 2: " + str(e["entry_mode1"]) + " Entrymode 3: " + str(e["entry_mode2"])
        print "Objectnames 1: " + binascii.hexlify(e["obj_names0"]) + " Objectnames 2: " + binascii.hexlify(e["obj_names1"]) + " Objectnames 3: " + binascii.hexlify(e["obj_names2"])
# }}}

# Write stuff for index-v4 draft0 {{{
# writev4_0header {{{
def writev4_0header(header, paths, files):
    fwrite(header["signature"])
    fwrite(struct.pack("!IIIIQ", header["version"], len(paths), len(files), header["nrofentries"], 0))
# }}}

# writev4_0directories {{{
def writev4_0directories(paths, treeextensiondata):
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
            if p != "/":
                fwrite(struct.pack("!ii", -1, subtreenr[p]))
            else:
                fwrite(struct.pack("!ii", -1, subtreenr[""]))
    return offsets
# }}}

# writev4_0files {{{
def writev4_0files(files):
    offsets = dict()
    for f in files:
        offsets[f] = writtenbytes
        fwrite(f)
        fwrite("\0")
    return offsets
# }}}

# writev4_0fileentries {{{
def writev4_0fileentries(entries, fileoffsets):
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

# writev4_0fileoffsets {{{
def writev4_0fileoffsets(diroffsets, fileoffsets, dircrcoffset):
    for d in sorted(diroffsets):
        fw.seek(diroffsets[d])
        fw.write(struct.pack("!Q", fileoffsets[d]))

    # Calculate crc32
    #f.seek(0)
    #data = f.read(dircrcoffset)
    fw.seek(dircrcoffset)
    fw.write(struct.pack("!I", binascii.crc32(writtendata) & 0xffffffff))

# }}}

# writev4_0reucextensiondata {{{
def writev4_0reucextensiondata(data):
    pass

# }}}

# }}}

# writecrc32 {{{
def writecrc32():
    global writtendata
    fwrite(struct.pack("!I", binascii.crc32(writtendata) & 0xffffffff))
    writtendata = "" # Reset writtendata for next crc32
# }}}


header = readheader(f)

indexentries, byte, paths, files = readindexentries(f)

filedata = filedata[:-1]
ext = byte + f.read(3)
extensiondata = []

ext2 = ""
if ext == "TREE":
    filedata += ext
    treeextensiondata = readextensiondata(f)
    ext2 = f.read(4)
else:
    treeextensiondata = dict()

if ext == "REUC" or ext2 == "REUC":
    if ext == "REUC":
        filedata += ext
    else:
        filedata += ext2
    reucextensiondata = readreucextensiondata(f)
else:
    reucextensiondata = list()

# printheader(header)
# printindexentries(indexentries)
# printextensiondata(extensiondata)
# printreucextensiondata(reucextensiondata)


if ext != "TREE" and ext != "REUC" and ext2 != "REUC":
    sha1read = ext + f.read(16)
elif ext2 != "REUC" and ext == "TREE":
    sha1read = ext2 + f.read(16)
else:
    sha1read = f.read(20)

print "SHA1 over the whole file: " + str(binascii.hexlify(sha1read))

sha1 = hashlib.sha1()
sha1.update(filedata)
print "SHA1 over filedata: " + str(sha1.hexdigest())

if sha1.hexdigest() == binascii.hexlify(sha1read):
    writev4_0header(header, paths, files)
    diroffsets = writev4_0directories(sorted(paths), treeextensiondata)
    fileoffsets = writev4_0files(files)
    dircrcoffset = writtenbytes

    # TODO: Replace with something faster. Doesn't make sense to calculate crc32 here
    writecrc32()
    fileoffsets = writev4_0fileentries(indexentries, fileoffsets)
    writev4_0fileoffsets(diroffsets, fileoffsets, dircrcoffset)
    writev4_0reucextensiondata(reucextensiondata)
else:
    print "File is corrupted"
