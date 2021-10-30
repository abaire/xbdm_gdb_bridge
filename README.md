# xbdm_util

Provides utilities to interface with an XBOX running XBDM.

See https://xboxdevwiki.net/Xbox_Debug_Monitor for a description of the XBDM protocol.

## util.py

Command line utility to send a subset of XBDM messages to a target.
Includes a "shell" command to open an interactive REPL.

## discover.py

Sends NAP broadcasts to discover any XBDM instances on the network.

## application.py

wxPython application providing discovery and interactivity.

# git hooks

This project uses [git hooks](https://git-scm.com/book/en/v2/Customizing-Git-Git-Hooks)
to automate some aspects of keeping the code base healthy.

Please copy the files from the `githooks` subdirectory into `.git/hooks` to
enable them.
