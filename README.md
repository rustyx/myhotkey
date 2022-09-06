## myhotkey

Windows Shell allows a hotkey to be associated with an application or action.

Unfortunately there is often a delay of 3 seconds between pressing the hotkey combination and the associated action.
The reason is explained [here](https://devblogs.microsoft.com/oldnewthing/?p=7723).

This application bypasses the Windows Shell and executes applications and other shell actions on pre-configured hotkey combinations.

The end result is *immediate* reaction on pressed hot keys.

### Configuration

The application has no GUI; it is configured via a config file `myhotkey.conf` which is loaded from the application's startup directory.

The config file looks like this:

```
# myhotkey.conf - configuration file for myhotkey
#
# Format:
# Modifiers [TAB] Hotkey [TAB] Executable [TAB] Work Dir [TAB] Params
# Where Modifiers = letters C,A,S,W corresponding to Ctrl, Alt, Shift, Win.
# A special modifier "+" can be used to allow multiple application instances.
#
# Notes:
# * no spaces to separate columns, use the TAB character,
# * use '-' to skip a column, '#' to skip a line
# * Hotkey can be defined as a normal character (0-9A-Z) or as a virtual key code -
#   prefix it with a tilde followed by the decimal key code, e.g. ~112 means VK_F1.
#   See https://docs.microsoft.com/en-us/windows/win32/inputdev/virtual-key-codes
#
CA	C	calc
CA	Z	cmd	%USERPROFILE%
CA	D	bash	%USERPROFILE%
CA	O	-	-	rundll32 shell32.dll,Control_RunDLL sysdm.cpl
CS	~112    -	msedge
```

### Building

The application can be build with Visual Studio 2015 or later or with Mingw32 using the provided build script.

### Installation

* Build or [download](https://github.com/rustyx/myhotkey/releases) the executable
* Unzip the application somewhere
* Add a shortcut to `myhotkey.exe` in the Startup folder (Start -> Run, type `shell:startup`)
* Execute the shortcut

### Known issues

The main window of UWP apps like the calculator cannot be detected, which results in a new instance being started for every hotkey press.
