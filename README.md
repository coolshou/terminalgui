# Terminal GUI

***This project is super duper early prototype work.***

Set of Qt-based widgets for your terminal! Create textual GUIs in your console
easily.

![Terminal GUI showcase](doc/img/terminal-gui.gif "Terminal GUI showcase")

## Features

* a multitude of widgets (Label, LineEdit, Button, ComboBox, RadioButton, ...)
* layouts (fill, row, column, grid, ...)
* fully dynamic widget properties, with support for signals and slots, 
animations, and other meta-object hight-level control
* custom style support
* convenient and intuitive API
* support for Linux and Windows (WIP)
* backed by the power of Qt: file access, networking, remote objects,
serial port etc. support

![Terminal GUI widgets](doc/img/terminal-gui-2.webm "Terminal GUI widgets")

## Builds

What? Builds? Not available yet. It's still a very young project!

## Compile

All that Terminal GUI requires is:
* C++17
* CMake
* QtCore module

By default, TG builds itself as a set of libraries (backend and widgets). It is
easy to link to them (see examples/CMakeLists.txt). If you prefer, you can 
include all the sources in your code, too. The license is *extremely* 
permissive.

## License

See LICENSE.md - TG is available under WTFPL. It links to Qt, though, so you 
need to keep that in mind.
