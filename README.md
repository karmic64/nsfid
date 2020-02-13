# nsfid
Play routine/driver identifier for binary music rips from computers and video game consoles. This program was inspired by "sidid" by Lasse Öörni (Cadaver), modified to search NSF files by Karmic, and then rewritten from scratch to allow for greater extensibility.

Enter `nsfid -?` on the command line for usage information.


## Config file format
Configuration files may start with a `FILETYPES` directive, followed by a space-separated list of extensions that indicates which file types the configuration file is associated with, and then `END`. If this is not provided, all files are searched. This may be overridden using the `-f` and `-a` switches.

From then on, the file is a list of driver names and their search strings. Any amount of search strings may be associated with a driver name. Driver names may not contain spaces, commas, or be interpretable as any one of the following tokens.

Search strings may consist of space-separated:
- Hexadecimal bytes, e.g. `00`, `1E`, `B9`, `FF`
- `??`, the wildcard
- `?xy`, the range-length wildcard, where `x` and `y` are single-digit numbers. Indicates that there may be `x` to `y` inclusive bytes between two substrings. If only one number is provided, `x` = 0.
- `AND`, the variable-length wildcard. Indicates that there may be any amount of bytes between two substrings.
- `END`, indicating the end of the string.

You may also use "sub-IDs" to provide a greater level of granularity. The names of sub-IDs start and end with parentheses and act identically to any other driver name.

Any text on a line following a `#` are comments and are ignored.