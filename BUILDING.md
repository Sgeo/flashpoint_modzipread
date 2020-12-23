# Build instructions for potential use in Flashpoint

* Install vcpkg into this directory, or change tasks/command line
* `vcpkg install zziplib:x86-windows-static`
    * Using static linking to avoid relying on the zlib1.dll that comes with Flashpoint, which seems to be buggy when interacting with this code.
* Copy `Flashpoint Core 9/Legacy/include` files into `apache/include`
* Copy `Flashpoint Core 9/Legacy/lib` files into `apache/lib`
* Flashpoint's Apache inexplicably excluded `apr_perms_set.h`, so grab a copy from https://www.apachelounge.com/download/ , using Apache 2.4.46 Win32 VS16
* A VS Code tasks is included, hopefully it will be easy to tell what to do for direct command line use
* Remember that Apache needs both zziplib.dll and mod_zipread.so