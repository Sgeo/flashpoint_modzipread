# Build instructions for potential use in Flashpoint

* Install vcpkg into this directory, or change tasks/command line
* `vcpkg install libzip:x86-windows-static`
    * Static build to ensure no conflicts with already existent .dlls in Apache, which have caused issues
    * libzip for zip64 support for >2GB .zip files. zziplib's support is untested and might be tricky to get working on 32-bit Windows.
* Copy `Flashpoint Core 9/Legacy/include` files into `apache/include`
* Copy `Flashpoint Core 9/Legacy/lib` files into `apache/lib`
* Flashpoint's Apache inexplicably excluded `apr_perms_set.h`, so grab a copy from https://www.apachelounge.com/download/ , using Apache 2.4.46 Win32 VS16
* A VS Code tasks is included, hopefully it will be easy to tell what to do for direct command line use
* Remember that Apache needs both zziplib.dll and mod_zipread.so