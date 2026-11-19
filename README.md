# mrubyc-debug

A debugger library for mruby/c on device.

> [!IMPORTANT]
> This library is **experimental and unstable!** Its features and documentation are insufficient.

## Installation

Add this library to your firmware build. It depends on mruby/c headers.

> [!WARNING]
> The features required by this library are not yet available in the official mruby/c.
> Use the `feat/debugger` branch of the fork: https://github.com/tufusa/mrubyc/tree/feat/debugger

Include `debug.h` and call `debug_init` for initialization. Since the mruby/c VM does not have standard input, you need to inject `read_bytes` into the debugger.
Following is an example using ESP-IDF:
```c
#include "debug.h"

// ...

int debug_read_bytes(void *buf, int nbytes) {
  return uart_read_bytes(UART_NUM_0, buf, nbytes, 0);
}

void app_main(void) {
  // ...
  debug_init(debug_read_bytes);
  mrbc_run();
}
```

## Usage

| Command                 | Alias | Description                                                                       |
| ----------------------- | ----- | --------------------------------------------------------------------------------- |
| `continue`              | `c`   | Resume until the next breakpoint.                                                 |
| `step`                  | `s`   | Step to the next executed line.                                                   |
| `next`                  | `n`   | **Not implemented**; currently same as `step`.                                    |
| `finish`                | `fin` | **Not implemented**; currently same as `step`.                                    |
| `break [<file>:]<line>` | `b`   | Add a breakpoint, or list all breakpoints with no argument.                       |
| `delete [<n>]`          | `d`   | Delete breakpoint `<n>`, or delete all with no argument.                          |
| `info`                  | `i`   | **Not fully implemented**; currently show the most inner scope's local variables. |
| `backtrace`             | `bt`  | Show the call stack.                                                              |

- It always pauses on the first line.
- Filenames match by base name.

## Acknowledgement

This library is strongly inspired by [yuuu/mruby-debug](https://github.com/yuuu/mruby-debug).
