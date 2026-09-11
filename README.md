# cINI

A C library for parsing, editing, and watching INI configuration files.

> The main known limitation is the way source lines are represented internally,
> which can cause comments and formatting to not be perfectly preserved when
> writing a modified configuration.

## Features

- Standard INI-style configuration
- Hierarchical sections
- Variable interpolation
- Typed value access
- Runtime configuration editing
- Change callbacks
- Live file updates
- Reading and writing configuration files
