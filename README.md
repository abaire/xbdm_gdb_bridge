# xbdm_util

Provides utilities to interface with an XBOX running XBDM.

See https://xboxdevwiki.net/Xbox_Debug_Monitor for a description of the XBDM protocol.


# git hooks

This project uses [git hooks](https://git-scm.com/book/en/v2/Customizing-Git-Git-Hooks)
to automate some aspects of keeping the code base healthy.

Please copy the files from the `githooks` subdirectory into `.git/hooks` to
enable them.


# Design

* `Shell` performs interactive command processing. 
* `XBOXInterface` holds the various connections and interaction queues.  

## Threading model

* The main thread runs the `Shell`.
* `XBOXInterface` spawns a number of threads:
    * `SelectThread` performs low level reading and writing of all sockets associated with the interface. E.g., the
      XBDM client, GDB server and any attached clients, etc...
    * `xbdm_control_executor_` is a `thread_pool` instance that sequences XBDM requests.
    * `gdb_control_executor_` is a `thread_pool` instance that sequences GDB requests. Generally a GDB request will 
      spawn one or more XBDM requests that will be processed by the `xbdm_control_executor_`.

