`default_nettype none

module top (
    output wire [1:0] O_psram_ck,
    output wire [1:0] O_psram_ck_n,
    inout wire [15:0] IO_psram_dq,
    inout wire [1:0] IO_psram_rwds,
    output wire [1:0] O_psram_cs_n,
    output wire [1:0] O_psram_reset_n,
    
    input wire in_clk,
    output reg [5:0] led,
    input wire btn1,
    input wire btn2,
    output wire uartTx,
    input wire uartRx
);

// WRAPPER PSRAM
wire WRPR_req_valid;
wire WRPR_req_ready;
wire WRPR_req_we;
wire [20:0] WRPR_req_addr;
wire [63:0] WRPR_write_data;
wire [63:0] WRPR_read_data;


wire psram_clk; // 159m
wire clk_lock;
wire sys_clk;
wire lcd_clk;
wire sys_nrst = btn1;


// GOWIN PSRAM IP CORE signals
wire PSRAM_init_calib, PSRAM_rd_data_valid;
wire [63:0] PSRAM_rd_data;
wire [63:0] PSRAM_wr_data;
wire [20:0] PSRAM_addr;
wire PSRAM_cmd;
wire PSRAM_cmd_en;

arbitrer arbitrer_inst (
    .nrst(nrst),
    .sys_clk(sys_clk),

    // wrapper psram port
    .WRPR_req_valid(WRPR_req_valid),
    .WRPR_req_ready(WRPR_req_ready),
    .WRPR_req_we(WRPR_req_we),
    .WRPR_req_addr(WRPR_req_addr),
    .WRPR_write_data(WRPR_write_data),
    .WRPR_read_data(WRPR_read_data),

    // cpu cache port (rw)
    .CPU_req_valid(),
    .CPU_req_ready(),
    .CPU_req_we(),
    .CPU_req_addr(),
    .CPU_write_data(),
    .CPU_read_data(),

    // lcd port (ro)
    .LCD_req_valid(),
    .LCD_req_ready(),
    .LCD_req_addr(),
    .LCD_read_data()
);

psram_wrapper psram_wrapper_inst (
    // system
    .nrst(sys_nrst),
    .clk(sys_clk),

    // psram
    .init_calib(PSRAM_init_calib),
    .rd_data_valid(PSRAM_rd_data_valid),
    .rd_data(PSRAM_rd_data),
    .wr_data(PSRAM_wr_data),
    .addr(PSRAM_addr),
    .cmd(PSRAM_cmd),
    .cmd_en(PSRAM_cmd_en),

    // interface
    .req_valid(WRPR_req_valid),
    .req_ready(WRPR_req_ready),
    .req_we(WRPR_req_we),
    .req_addr(WRPR_req_addr),

    // data
    .write_data(WRPR_write_data),
    .read_data(WRPR_read_data)
);

rPLL #( // For GW1NR-9 C6/I5
  .FCLKIN("27"),
  .IDIV_SEL(2), // -> PFD = 9 MHz (range: 3-400 MHz)
  .FBDIV_SEL(13), // -> CLKOUT = 126 MHz (range: 3.125-500 MHz)
  .DYN_SDIV_SEL(14),   .ODIV_SEL(4) // -> VCO = 504 MHz (range: 400-1000 MHz)
) pll (.CLKOUTP(), .CLKOUTD3(), .RESET(1'b0), .RESET_P(1'b0), .CLKFB(1'b0), .FBDSEL(6'b0), .IDSEL(6'b0), .ODSEL(6'b0), .PSDA(4'b0), .DUTYDA(4'b0), .FDLY(4'b0),
  .CLKIN(in_clk), // 27 MHz
  .CLKOUT(psram_clk), // 126 MHz
  .CLKOUTD(lcd_clk), // 9 MHz
  .LOCK(clk_lock)
);


PSRAM_Memory_Interface_HS_Top psram_inst(
    .clk(psram_clk), //input clk
    .memory_clk(psram_clk), //input memory_clk
    .pll_lock(clk_lock), //input pll_lock
    .rst_n(sys_nrst), //input rst_n
    .O_psram_ck(O_psram_ck), //output [1:0] O_psram_ck
    .O_psram_ck_n(O_psram_ck_n), //output [1:0] O_psram_ck_n
    .IO_psram_dq(IO_psram_dq), //inout [15:0] IO_psram_dq
    .IO_psram_rwds(IO_psram_rwds), //inout [1:0] IO_psram_rwds
    .O_psram_cs_n(O_psram_cs_n), //output [1:0] O_psram_cs_n
    .O_psram_reset_n(O_psram_reset_n), //output [1:0] O_psram_reset_n
    .wr_data(PSRAM_wr_data), //input [63:0] wr_data
    .rd_data(PSRAM_rd_data), //output [63:0] rd_data
    .rd_data_valid(PSRAM_rd_data_valid), //output rd_data_valid
    .addr(PSRAM_addr), //input [20:0] addr
    .cmd(PSRAM_cmd), //input cmd
    .cmd_en(PSRAM_cmd_en), //input cmd_en
    .init_calib(PSRAM_init_calib), //output init_calib
    .clk_out(sys_clk), //output clk_out
    .data_mask(8'd0) //input [7:0] data_mask
);


endmodule