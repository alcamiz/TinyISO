<div align="center">

# TinyISO

Small library for reading and parsing ISO-9660 images, with support for
Microsoft's Joliet extension.

</div>

## Features:

- POSIX compatibility for cross-platform support.
- Support for multi-extent, non-contiguous files.
- Callback system for traversing directories/files.
- UTF-8 conversion of file names with iconv.
- Access to filesystem information such as LBA offsets.

## Usage:

Programs under the test directory show how to set up and
run the library. For a more in-depth explanation, look
at each function's comment on the file ```tni.h```

## Building/Testing:

You can test the library with the provided example. Both
gcc and libiconv must be installed. Compliling on Windows
may require Cygwin or MSYS2.

```
git clone https://github.com/alcamiz/TinyIso
cd TinyIso
make test-iter
```

The resulting executable will be placed in the ```bin``` directory
of the project.

## License

[![GNU GPLv3 Image](https://www.gnu.org/graphics/gplv3-127x51.png)](http://www.gnu.org/licenses/gpl-3.0.en.html)

TinyIso is Free Software: You can use, study share and improve it at your
will. Specifically you can redistribute and/or modify it under the terms of the
[GNU General Public License](https://www.gnu.org/licenses/gpl.html) as
published by the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
