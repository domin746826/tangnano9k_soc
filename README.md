# tangnano9k_soc

AI-Free rv32i SoC implementation on Tang Nano 9K, with PSRAM, Cache and maybe with display in the future

Goal is to run Linux on it and write all code without using any LLM generated code

Roadmap
- [x] PSRAM wrapper
- [x] Memory Arbiter
- [x] L1 Cache
- [ ] RV32I *in progress*
- [ ] UART & some timers
- [ ] LCD/Framebuffer
- [ ] SD Card support
- [ ] RV32IMAC
- [ ] Add Zicsr & Zifencei support to RV32IMAC Core
- [ ] Privileged Modes
- [ ] Boot Linux on it (Buildroot + BusyBox + DTB)
- [ ] Framebuffer driver
- [ ] Run Doom (fbdoom)
- [ ] Independent Linux machine: Edit C code in vi, compile, and run
- [ ] Run Xorg (Nano-X)

