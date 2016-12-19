"""
This is a helper module.
Its purpose is to supply tools that are used to generate version specific code.
Goal is to generate code that work on both python2x and python3x.
"""
from numpy import generic as npscalar
from numpy import ndarray as nparray
from numpy import string_ as npbytes
from numpy import unicode_ as npunicode
from sys import version_info as pyver
import os
ispy3 = pyver>(3,)
ispy2 = pyver<(3,)
isNt = os.name=='nt'
npstr = npunicode if ispy3 else npbytes
# __builtins__ is dict
has_long      = 'long'       in __builtins__
has_unicode   = 'unicode'    in __builtins__
has_basestring= 'basestring' in __builtins__
has_bytes     = 'bytes'      in __builtins__
has_buffer    = 'buffer'     in __builtins__
has_xrange    = 'xrange'     in __builtins__
has_mapclass  = isinstance(map,(type,))

def load_library(name):
    import ctypes as C,platform
    if os.sys.platform.startswith('darwin') and not os.getenv('DYLD_LIBRARY_PATH'):
        if os.getenv('MDSPLUS_DIR'):
            os.environ['DYLD_LIBRARY_PATH'] = os.path.join(os.getenv('MDSPLUS_DIR'),'lib')
        else:
            os.environ['DYLD_LIBRARY_PATH'] = '/usr/local/mdsplus/lib'
    from ctypes.util import find_library
    libnam = find_library(name)
    if libnam is None:
        if os.sys.platform.startswith('win'):
            return C.CDLL('%s.dll'%name)
        if os.sys.platform.startswith('darwin'):
            return C.CDLL('lib%s.dylib'%name)
        try: return C.CDLL('lib%s.so'%name)
        except:raise Exception("Error finding library: "+name)
    else:
        try:   return C.CDLL(libnam)
        except:pass
        try:   return C.CDLL(name)
        except:pass
        try:   return C.CDLL(os.path.basename(libnam))
        except:print('Could not load CDLL: '+libnam)

from types import GeneratorType as generator  # analysis:ignore

# substitute missing builtins
if has_long:
    long = long
else:
    long = int
if has_basestring:
    basestring = basestring
elif has_bytes:
    basestring = (str, bytes)
else:
    basestring = str
if has_unicode:
    unicode = unicode
else:  # py3 str is unicode
    unicode = str
if has_bytes:
    bytes = bytes
else:  # py2 str is bytes
    bytes = str
if has_buffer:
    buffer = buffer
else:
    buffer = memoryview
if has_mapclass:
    mapclass = map
else:
    mapclass = tuple

# helper variant string
if has_unicode:
    varstr = unicode
else:
    varstr = bytes
if has_xrange:
    xrange = xrange
else:
    xrange = range

if has_xrange:
    xrange = xrange
else:
    xrange = range

def _decode(string):
    try:
        return string.decode('utf-8', 'backslashreplace')
    except:
        return string.decode('CP1252', 'backslashreplace')

def _encode(string):
    return string.encode('utf-8', 'backslashreplace')

def _tostring(string, targ, nptarg, conv, lstres):
    if isinstance(string, targ):  # short cut
        return targ(string)
    if isinstance(string, npscalar):
        try:
            return targ(string.astype(nptarg))
        except:  # might happen on non ansii chars
            return targ(conv(str(string)))
    if isinstance(string, basestring):
        return targ(conv(string))
    if isinstance(string, nparray):
        string = string.astype(nptarg).tolist()
    if isinstance(string, (list, tuple)):
        return type(string)(_tostring(s, targ, nptarg, conv, lstres) for s in string)
    return lstres(string)


def tostr(string):
    if isinstance(string,(list, tuple)):
        return string.__class__(tostr(item) for item in string)
    return _tostring(string, str, npstr, _decode, str)


if ispy2:
    _bytes = bytes
    def _unicode(string):
        return _decode(str(string))
else:
    def _bytes(string):
        return _encode(str(string))
    _unicode = unicode

def tobytes(string):
    if isinstance(string,(list, tuple)):
        return string.__class__(tobytes(item) for item in string)
    return _tostring(string, bytes, npbytes, _encode, _bytes)


def tounicode(string):
    if isinstance(string,(list, tuple)):
        return string.__class__(tounicode(item) for item in string)
    return _tostring(string, unicode, npunicode, _decode, _unicode)

# Extract the code attribute of a function. Different implementations
# are for Python 2/3 compatibility.

if ispy2:
    def func_code(f):
        return f.func_code
else:
    def func_code(f):
        return f.__code__
