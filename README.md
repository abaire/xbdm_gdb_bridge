# xbdm_gdb_bridge

Provides utilities to interface with an XBOX running XBDM.

See https://xboxdevwiki.net/Xbox_Debug_Monitor for a description of the XBDM protocol.

# General usage

## Connecting

`xbdm_gdb_bridge <xbox_ip>:<xbdm_port>`

where `<xbox_ip>` alone will default to port `731` and port alone will default to
IP `127.0.0.1`.

_Note_: Connecting to a qemu-based emulator using NAT may require the use of
`hostfwd_add` to forward a localhost port to the emulated machine. 

## Using the shell

When started without any initial commands or when using the `-s` option,
`xbdm_gdb_bridge` will enter shell mode, allowing any number of commands to be
executed.

Within the shell, use the `?` command without any arguments to get a list of available 
commands. `? <command_name>` can be used to get more information about any given command
(e.g., `? gdb`).

## Running a command without the shell

`xbdm_gdb_bridge` shell commands can also be given in the initial invocation by passing
them after a ``--`:

`xbdm_gdb_bridge 10.0.0.24 -- reboot`

Multiple commands can be given by separating them with `&&`:

`xbdm_gdb_bridge 10.0.0.24 -- memwalk && reboot`

# Features

## GDB bridge

Provides a GDB stub that translates from GDB <-> XBDM commands. This allows debugging
with GDB on hardware and may improve parts of the debugging experience for emulators
as well (XBDM is aware of OS-level threads in a way that the GDB stub in qemu-based
emulators is not, for example).

Usage:

Assuming an XBOX at IP `10.0.0.24`:

`xbdm_gdb_bridge 10.0.0.24 -s -- gdb :1999`

will connect to XBDM, stand up a GDB bridge on port `1999`, and leave `xbdm_gdb_bridge`
in shell mode (`-s`) to allow further commands to be issued. GDB can then be attached 
using the appropriate `target remote` command (e.g., `target remote 127.0.0.1:1999`).


`xbdm_gdb_bridge 10.0.0.24 -v8 -s -- gdb :1999 && /launch e:\pgraph`

will enable very verbose logging connect to XBDM, stand up GDB on port 1999, and attempt
to launch the `default.xbe` located at `e:\pgraph` on the XBOX.

## Dynamic DXT code injection

Installs a bootstrap and provides a loading mechanism for debugger extension DLLs.
See [the demo project](https://github.com/abaire/nxdk_demo_dyndxt) for more information.

Usage:

`@load <dll_path>` will load and install the extension.

The various `@`-prefixed functions can be used to interact with the extension once 
installed.

E.g., `@m demo!sendmultiline` will ask the `demo` handler installed by 
[the demo project](https://github.com/abaire/nxdk_demo_dyndxt) to send back a multiline
response and will print out each returned line.

# Development

## git hooks

This project uses [git hooks](https://git-scm.com/book/en/v2/Customizing-Git-Git-Hooks)
to automate some aspects of keeping the code base healthy, in particular `clang-format`
invocation.

Please copy the files from the `githooks` subdirectory into `.git/hooks` to
enable them.

## Building

### Dependencies

* SDL2 and SDL_Image for screenshot support.

### `cmake`

* The `NXDK_DIR` environment variable must be set to the root of the [nxdk](https://github.com/XboxDev/nxdk)
* `cmake -S . -B build && cmake --build build --verbose`

Tests can be executed afterwards using
* `ctest --test-dir build --output-on-failure`

#### Configuration variables
* `ENABLE_HIGH_VERBOSITY_LOGGING` - Enables low level logging of socket traffic. May have a negative impact on
   performance. E.g., `-DENABLE_HIGH_VERBOSITY_LOGGING=ON`

### CLion

The CMake target can be configured to set the `NXDK_DIR` env var via the properties page:

* Environment

  `NXDK_DIR=<absolute_path_to_nxdk>`

On macOS you may also have to modify PATH such that a homebrew version of LLVM
is preferred over Xcode's (to supply `dlltool`).

# Design

* `Shell` performs interactive command processing. 
* `XBOXInterface` holds the various connections and thread pools.

## Threading model

* The main thread runs the `Shell`.
* `XBOXInterface` spawns a number of threads:
    * `SelectThread` performs low level reading and writing of all sockets associated with the interface. E.g., the
      XBDM client, GDB server and any attached clients, etc...
    * `xbdm_control_executor_` is a `thread_pool` instance that sequences XBDM requests.
    * `gdb_control_executor_` is a `thread_pool` instance that sequences GDB requests. Generally a GDB request will 
      spawn one or more XBDM requests that will be processed by the `xbdm_control_executor_`.
    * During a debugging session, a `notification_executor_` `thread_pool` is created to handle push notifications from
      XBDM.
