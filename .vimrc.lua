-- Powered by projectlocal.vim
-- https://github.com/creativenull/projectlocal.vim

local dap = require('dap')

local function flash_img()
    dap.pause()
    dap.repl.execute("-exec mon reset halt")
    dap.repl.execute("-exec mon program build/zbco2/zephyr/zephyr.bin 0x26000 reset")
end

local function reset()
    dap.pause()
    dap.repl.execute("-exec mon reset halt")
end

vim.keymap.set('n', '<Space>Df', flash_img, {desc="[D]evice [f]lash image over GDB mon"})
vim.keymap.set('n', '<Space>Dr', reset, {desc="[D]evice [r]eset device over GDB mon"})
