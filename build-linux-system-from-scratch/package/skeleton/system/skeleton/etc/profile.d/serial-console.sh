# QEMU -nographic / serial console: fix terminal size and backspace for vim/shell.
export TERM=vt100
stty rows 30 cols 100 2>/dev/null
stty erase ^? 2>/dev/null
