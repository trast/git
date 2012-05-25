import struct
import binascii

# Format strings
HEADER_FORMAT = """\
Signature: %(signature)s
Version: %(version)s
Number of entries: %(nrofentries)s"""

ENTRIES_FORMAT = """\
ctime: %(ctimesec)s:%(ctimensec)s
mtime: %(mtimesec)s:%(mtimensec)s
dev: %(dev)s\tino: %(ino)s
uid: %(uid)s\tgid: %(gid)s
size: %(filesize)s\tflags: """

EXTENSION_FORMAT = """\
%(sha1)s %(path)s (%(entry_count)s entries, %(subtrees)s subtrees)"""

REUCEXTENSION_FORMAT = """\
Path: %(path)s
Entrymode 1: %(entry_mode0)s Entrymode 2: %(entry_mode1)s Entrymode 3:\
        %(entry_mode2)s"""

EXTENSION_FORMAT_WITHOUT_SHA = """\
invalid %(path)s (%(entry_count)s entries, %(subtrees)s subtrees)"""

HEADER_V5_FORMAT = """\
Signature: %(signature)s\t\t\tVersion: %(vnr)s
Number of directories: %(ndir)s\tNumber of files: %(nfile)s
Number of extensions: %(nextensions)s"""

DIRECTORY_FORMAT = """\
path: %(pathname)s flags: %(flags)s foffset: %(foffset)s
ncr: %(ncr)s cr: %(cr)s nfiles: %(nfiles)s
nentries: %(nentries)s objname: %(objname)s"""

FILES_FORMAT = """\
%(name)s (%(objhash)s)\nmtime: %(mtimes)s:%(mtimens)s
mode: %(mode)s flags: %(flags)s\nstatcrc: """

HEADER_STRUCT = struct.Struct("!4sII")
HEADER_V5_STRUCT = struct.Struct("!4sIIIII")

SIZE_STRUCT = struct.Struct("!I")

STAT_DATA_STRUCT = struct.Struct("!IIIIIIIIII 20sh")

XTFLAGS_STRUCT = struct.Struct("!h")

CRC_STRUCT = struct.Struct("!I")

DIRECTORY_DATA_STRUCT = struct.Struct("!HIIIIII 20s")

STAT_DATA_CRC_STRUCT = struct.Struct("!IIIIIIII")

FILE_DATA_STRUCT = struct.Struct("!HHIII 20s")

OFFSET_STRUCT = struct.Struct("!I")
EXTENSION_OFFSET_STRUCT = struct.Struct("!I")
DIR_OFFSET_STRUCT = struct.Struct("!I")
FBLOCK_OFFSET_STRUCT = struct.Struct("!I")
FILE_OFFSET_STRUCT = struct.Struct("!I")

NR_CONFLICT_STRUCT = struct.Struct("!I")
CONFLICT_STRUCT = struct.Struct("!HH 20s")

class SHAError(Exception):
    pass

class SignatureError(Exception):
    pass

class VersionError(Exception):
    pass

class CrcError(Exception):
    pass

class FilesizeError(Exception):
    pass

def calculate_crc(data, partialcrc=0):
    return binascii.crc32(data, partialcrc) & 0xffffffff


def get_sub_paths(path):
    path = path.split("/")

    pathname = ""
    paths = list()
    for p in path:
        pathname += p + "/"
        paths.append(pathname.strip("/"))
    return paths
