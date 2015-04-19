# IO-UI distribution package

## Package contents

- `bin/`: io-ui pre-built executables
- `res/`: project resources; put your icon there
- `package.js`: package creation script
- `LICENSE.txt`: project license; you must redistribute it with your app as well
- `README.md`: this file

## Running

Suppose you have your app located in my-app folder and you are using Mac OS X. Then you should run it with:  

`$ bin/darwin/node my-app`

or, if you want to see console output:

`$ bin/darwin/node ~/app 2>&1 | grep .`

The same on Windows and Linux, just replace darwin with win32 or linux.

## Packaging

Create a package from your app with this command:

`package.js my-app`

Output file will be created in `build` folder.  
To put the result in another location, use:

`package.js my-app output-folder`

This script will create executables for Windows (`<name>.exe`), Mac (`<name>.app`) and Linux (`<name>`). 

Package script doesn't create an installer, you should do it yourself.