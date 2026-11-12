//Copyright (C)2014-2025 Gowin Semiconductor Corporation.
//All rights reserved.
//File Title: IP file
//Tool Version: V1.9.11.03 Education
//Part Number: GW1NR-LV9QN88PC6/I5
//Device: GW1NR-9
//Device Version: C
//Created Time: Thu Nov 12 11:31:56 2026

module Gowin_MULT (dout, a, b, ce, clk, reset);

output [65:0] dout;
input [32:0] a;
input [32:0] b;
input ce;
input clk;
input reset;

wire [5:0] dout_w;
wire gw_vcc;

assign gw_vcc = 1'b1;

MULT36X36 mult36x36_inst (
    .DOUT({dout_w[5:0],dout[65:0]}),
    .A({a[32],a[32],a[32],a[32:0]}),
    .B({b[32],b[32],b[32],b[32:0]}),
    .ASIGN(gw_vcc),
    .BSIGN(gw_vcc),
    .CE(ce),
    .CLK(clk),
    .RESET(reset)
);

defparam mult36x36_inst.AREG = 1'b1;
defparam mult36x36_inst.BREG = 1'b1;
defparam mult36x36_inst.OUT0_REG = 1'b1;
defparam mult36x36_inst.PIPE_REG = 1'b0;
defparam mult36x36_inst.ASIGN_REG = 1'b0;
defparam mult36x36_inst.BSIGN_REG = 1'b0;
defparam mult36x36_inst.MULT_RESET_MODE = "SYNC";

endmodule //Gowin_MULT
