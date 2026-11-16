//Copyright (C)2014-2025 Gowin Semiconductor Corporation.
//All rights reserved.
//File Title: Template file for instantiation
//Tool Version: V1.9.11.03 Education
//Part Number: GW1NR-LV9QN88PC6/I5
//Device: GW1NR-9
//Device Version: C
//Created Time: Sun Nov 15 19:23:17 2026

//Change the instance name and port connections to the signal names
//--------Copy here to design--------

    Gowin_User_Flash your_instance_name(
        .dout(dout), //output [31:0] dout
        .xe(xe), //input xe
        .ye(ye), //input ye
        .se(se), //input se
        .prog(prog), //input prog
        .erase(erase), //input erase
        .nvstr(nvstr), //input nvstr
        .xadr(xadr), //input [8:0] xadr
        .yadr(yadr), //input [5:0] yadr
        .din(din) //input [31:0] din
    );

//--------Copy end-------------------
