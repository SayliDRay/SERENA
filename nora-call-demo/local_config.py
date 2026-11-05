"""Windows-user encrypted settings. Never writes plaintext credentials to disk."""
import ctypes
from ctypes import wintypes
import json
import os
from pathlib import Path

PATH = Path(__file__).resolve().parent.parent / '.local' / 'phone-settings.dpapi'


class Blob(ctypes.Structure):
    _fields_ = [('size', wintypes.DWORD), ('data', ctypes.POINTER(ctypes.c_ubyte))]


def crypt(data, protect):
    if os.name != 'nt':
        raise RuntimeError('Saved calling settings require Windows DPAPI.')
    buffer = ctypes.create_string_buffer(data)
    source = Blob(len(data), ctypes.cast(buffer, ctypes.POINTER(ctypes.c_ubyte)))
    target = Blob()
    library = ctypes.WinDLL('crypt32', use_last_error=True)
    kernel = ctypes.WinDLL('kernel32', use_last_error=True)
    if protect:
        fn = library.CryptProtectData
        fn.argtypes = [ctypes.POINTER(Blob),wintypes.LPCWSTR,ctypes.c_void_p,ctypes.c_void_p,ctypes.c_void_p,wintypes.DWORD,ctypes.POINTER(Blob)]
    else:
        fn = library.CryptUnprotectData
        fn.argtypes = [ctypes.POINTER(Blob),ctypes.c_void_p,ctypes.c_void_p,ctypes.c_void_p,ctypes.c_void_p,wintypes.DWORD,ctypes.POINTER(Blob)]
    fn.restype = wintypes.BOOL
    if not fn(ctypes.byref(source), None, None, None, None, 1, ctypes.byref(target)):
        raise ctypes.WinError(ctypes.get_last_error())
    kernel.LocalFree.argtypes = [ctypes.c_void_p]
    kernel.LocalFree.restype = ctypes.c_void_p
    try:
        return ctypes.string_at(target.data, target.size)
    finally:
        kernel.LocalFree(ctypes.cast(target.data, ctypes.c_void_p))


def save_config(settings):
    encrypted = crypt(json.dumps(settings).encode(), True)
    PATH.parent.mkdir(exist_ok=True)
    temporary = PATH.with_suffix('.tmp')
    temporary.write_bytes(encrypted)
    temporary.replace(PATH)


def load_config():
    if not PATH.exists():
        return {}
    try:
        result = json.loads(crypt(PATH.read_bytes(), False))
        return result if isinstance(result, dict) else {}
    except Exception:
        raise SystemExit('Saved settings could not be decrypted for this Windows user. Run start-phone.ps1 to configure them again.')
