## Warning!

The project has been created as a commercial project, and also as a proof-of-concept. Project author doesn't have active interest in this project, and it's not being actively developed for now, until there's no extra demand and finance for such specific kind of tool.

## Project website
This readme describes how to work with sources. For general and usage info, please go to:  
[antelle.github.io/io-ui](http://antelle.github.io/io-ui)

## Building
Prerequisties:
  
- git (with git-bash on Windows)
- node.js
- make on Linux and Mac or Visual Studio 2013 Community on Windows
- nodeunit
- [all prerequesties](https://github.com/joyent/node#to-build) for building node.js

To download dependencies and build the project with examples, use:  

- `make`

On windows, CEF is required and must be download manually into deps/cef folder. It can be downloaded from here: http://www.magpcss.net/cef_downloads/ (win32 build). After downloading, you should build libcef_dll_wrapper project in Release configuration.

Run unit tests with:

- `nodeunit`

Other build scripts:

- `download-deps.js` - download and install dependencies
- `build.js` - build the project
- `build-examples.js` - build examples

## IDEs
To edit the code, you can use XCode on Mac OS X, Visual Studio 2013 Community on Windows and CodeLite on Ubuntu, it works ok. Project files are in project folder.
