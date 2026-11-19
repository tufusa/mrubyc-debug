set(srcs
  "src/breakpoint.c"
  "src/command.c"
  "src/common.c"
  "src/debug.c"
  "src/input.c"
)

idf_component_register(SRCS "${srcs}"
                       INCLUDE_DIRS src
                       REQUIRES mrubyc-4.0.0)
