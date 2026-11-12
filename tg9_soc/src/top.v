`default_nettype none

module top (
    output wire [1:0] O_psram_ck,
    output wire [1:0] O_psram_ck_n,
    inout wire [15:0] IO_psram_dq,
    inout wire [1:0] IO_psram_rwds,
    output wire [1:0] O_psram_cs_n,
    output wire [1:0] O_psram_reset_n,

    output wire [15:0] LCD_PORT,
    output wire LCD_HSYNC,
    output wire LCD_VSYNC,
    output wire LCD_DEN,
    output wire LCD_CLK,
    
    input wire in_clk,
    output wire [5:0] led,
    input wire btn1,
    input wire btn2,
    output wire uartTx,
    input wire uartRx
);
assign uartTx = 1'b1;

// WRAPPER PSRAM
wire WRPR_req_valid;
wire WRPR_req_ready;
wire WRPR_req_we;
wire [20:0] WRPR_req_addr;
wire [63:0] WRPR_write_data;
wire [63:0] WRPR_read_data;

// GOWIN PSRAM IP CORE signals
wire PSRAM_init_calib, PSRAM_rd_data_valid;
wire [63:0] PSRAM_rd_data;
wire [63:0] PSRAM_wr_data;
wire [20:0] PSRAM_addr;
wire PSRAM_cmd;
wire PSRAM_cmd_en;

wire psram_clk; // 159m
wire clk_lock;
wire sys_clk;
wire lcd_clk;
wire sys_nrst = btn1;

reg [3:0] rst_sh;
always @(posedge sys_clk or negedge sys_nrst)
    if (!sys_nrst) rst_sh <= 4'd0;
    else           rst_sh <= {rst_sh[2:0], PSRAM_init_calib};
wire nrst = rst_sh[3];

// cache_d / rom <=> cpu
wire dcpu_valid;
wire [31:0] dcpu_addr;
wire [31:0] dcpu_wdata;
wire [3:0] dcpu_wstrb;
// wire dcpu_flush,
wire [31:0] dcache_rdata;
wire dcache_ready;
wire [31:0] drom_rdata;
wire drom_ready;
wire dcpu_is_rom = dcpu_addr[31];
wire [31:0] dcpu_rdata = dcpu_is_rom ? drom_rdata : dcache_rdata;
wire dcpu_ready = dcpu_is_rom ? drom_ready : dcache_ready;

wire LCD_req_valid;
wire LCD_req_ready;
wire [20:0] LCD_req_addr;
wire [63:0] LCD_read_data;

// cache_i <=> cpu
wire icpu_valid;
wire [31:0] icpu_addr;
// wire dcpu_flush,
wire [31:0] icpu_rdata;
wire icpu_ready;

// cache_d <=> arbitrer
wire CPUD_req_valid;
wire CPUD_req_ready;
wire CPUD_req_we;
wire [20:0] CPUD_req_addr;
wire [63:0] CPUD_write_data;
wire [63:0] CPUD_read_data;

// cache_i <=> arbitrer
wire CPUI_req_valid;
wire CPUI_req_ready;
wire [20:0] CPUI_req_addr;
wire [63:0] CPUI_read_data;

// CPU GOES HERE

// selftest #(.CLK_HZ(45_000_000), .BAUD(115200)) selftest_inst (
//     .sys_clk(sys_clk), .nrst(nrst),
//     .dcpu_valid(dcpu_valid), .dcpu_addr(dcpu_addr), .dcpu_wdata(dcpu_wdata),
//     .dcpu_wstrb(dcpu_wstrb), .dcpu_rdata(dcpu_rdata), .dcpu_ready(dcpu_ready),
//     .icpu_valid(icpu_valid), .icpu_addr(icpu_addr),
//     .icpu_rdata(icpu_rdata), .icpu_ready(icpu_ready),
//     .led(led), .uart_tx(uartTx)
// );

cpu cpu_inst (
    .sys_clk(sys_clk), .nrst(nrst),
    .dcpu_valid(dcpu_valid), .dcpu_addr(dcpu_addr), .dcpu_wdata(dcpu_wdata),
    .dcpu_wstrb(dcpu_wstrb), .dcpu_rdata(dcpu_rdata), .dcpu_ready(dcpu_ready),
    .icpu_valid(icpu_valid), .icpu_addr(icpu_addr),
    .icpu_rdata(icpu_rdata), .icpu_ready(icpu_ready),
    .led(led)
);



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

    // cpu dcache port (rw)
    .CPUD_req_valid(CPUD_req_valid),
    .CPUD_req_ready(CPUD_req_ready),
    .CPUD_req_we(CPUD_req_we),
    .CPUD_req_addr(CPUD_req_addr),
    .CPUD_write_data(CPUD_write_data),
    .CPUD_read_data(CPUD_read_data),

    // cpu icache port (ro)
    .CPUI_req_valid(CPUI_req_valid),
    .CPUI_req_ready(CPUI_req_ready),
    .CPUI_req_addr(CPUI_req_addr),
    .CPUI_read_data(CPUI_read_data),

    // lcd port (ro)
    .LCD_req_valid(LCD_req_valid),
    .LCD_req_ready(LCD_req_ready),
    .LCD_req_addr(LCD_req_addr),
    .LCD_read_data(LCD_read_data)
);

lcd_driver lcd_inst(
    .nrst(nrst),
    .sys_clk(sys_clk),
    .clk_lcd(lcd_clk),
    .color_port(LCD_PORT),
    .hsync(LCD_HSYNC),
    .vsync(LCD_VSYNC),
    .de(LCD_DEN),
    .lcd_clk(LCD_CLK),
    .LCD_req_valid(LCD_req_valid),
    .LCD_req_ready(LCD_req_ready),
    .LCD_req_addr(LCD_req_addr),
    .LCD_read_data(LCD_read_data)
);



rwcache dcache_inst (
    // system
    .sys_clk(sys_clk),
    .nrst(nrst),

    // cpu port
    .cpu_valid(dcpu_valid && !dcpu_is_rom),
    .cpu_addr(dcpu_addr),
    .cpu_wdata(dcpu_wdata),
    .cpu_wstrb(dcpu_wstrb),
    // input wire cpu_flush,
    .cpu_rdata(dcache_rdata),
    .cpu_ready(dcache_ready),

    // arbitrer port
    .CPU_req_valid(CPUD_req_valid),
    .CPU_req_ready(CPUD_req_ready),
    .CPU_req_we(CPUD_req_we),
    .CPU_req_addr(CPUD_req_addr),
    .CPU_write_data(CPUD_write_data),
    .CPU_read_data(CPUD_read_data)
    
);


rocache icache_inst (
    // system
    .sys_clk(sys_clk),
    .nrst(nrst),

    // cpu port
    .cpu_valid(icpu_valid),
    .cpu_addr(icpu_addr),
    // input wire cpu_flush,
    .cpu_rdata(icpu_rdata),
    .cpu_ready(icpu_ready),

    // data-side ROM port
    .drom_valid(dcpu_valid && dcpu_is_rom),
    .drom_addr(dcpu_addr),
    .drom_rdata(drom_rdata),
    .drom_ready(drom_ready),

    // arbitrer port
    .CPU_req_valid(CPUI_req_valid),
    .CPU_req_ready(CPUI_req_ready),
    .CPU_req_addr(CPUI_req_addr),
    .CPU_read_data(CPUI_read_data)
    
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


rPLL #( // For GW1NR-9C C6/I5 (Tang Nano 9K proto dev board)
  .FCLKIN("27"),
  .IDIV_SEL(2), // -> PFD = 9 MHz (range: 3-400 MHz)
  .FBDIV_SEL(9), // -> CLKOUT = 90 MHz (range: 3.125-600 MHz)
  .DYN_SDIV_SEL(10),   .ODIV_SEL(8) // -> VCO = 720 MHz (range: 400-1200 MHz)
) pll (.CLKOUTP(), .CLKOUTD3(), .RESET(1'b0), .RESET_P(1'b0), .CLKFB(1'b0), .FBDSEL(6'b0), .IDSEL(6'b0), .ODSEL(6'b0), .PSDA(4'b0), .DUTYDA(4'b0), .FDLY(4'b0),
  .CLKIN(in_clk), // 27 MHz
  .CLKOUT(psram_clk), // 90 MHz
  .CLKOUTD(lcd_clk), // 9 MHz
  .LOCK(clk_lock)
);

//  rPLL #( // For GW1NR-9C C6/I5 (Tang Nano 9K proto dev board)
//   .FCLKIN("27"),
//   .IDIV_SEL(2), // -> PFD = 9 MHz (range: 3-400 MHz)
//   .FBDIV_SEL(7), // -> CLKOUT = 72 MHz (range: 3.125-600 MHz)
//   .DYN_SDIV_SEL(8),   .ODIV_SEL(8) // -> VCO = 576 MHz (range: 400-1200 MHz)
// ) pll (.CLKOUTP(), .CLKOUTD3(), .RESET(1'b0), .RESET_P(1'b0), .CLKFB(1'b0), .FBDSEL(6'b0), .IDSEL(6'b0), .ODSEL(6'b0), .PSDA(4'b0), .DUTYDA(4'b0), .FDLY(4'b0),
//   .CLKIN(in_clk), // 27 MHz
//   .CLKOUT(psram_clk), // 72 MHz
//   .CLKOUTD(lcd_clk), // 9 MHz
//   .LOCK(clk_lock)
// );


// rPLL #( // For GW1NR-9C C6/I5 (Tang Nano 9K proto dev board)
//   .FCLKIN("27"),
//   .IDIV_SEL(0), // -> PFD = 27 MHz (range: 3-400 MHz)
//   .FBDIV_SEL(5), // -> CLKOUT = 162 MHz (range: 3.125-600 MHz)
//   .DYN_SDIV_SEL(18),   .ODIV_SEL(4) // -> VCO = 648 MHz (range: 400-1200 MHz)
// ) pll (.CLKOUTP(), .CLKOUTD3(), .RESET(1'b0), .RESET_P(1'b0), .CLKFB(1'b0), .FBDSEL(6'b0), .IDSEL(6'b0), .ODSEL(6'b0), .PSDA(4'b0), .DUTYDA(4'b0), .FDLY(4'b0),
//   .CLKIN(in_clk), // 27 MHz
//   .CLKOUT(psram_clk), // 162 MHz
//   .CLKOUTD(lcd_clk), // 9 MHz
//   .LOCK(clk_lock)
// );

// rPLL #( // For GW1NR-9C C6/I5 (Tang Nano 9K proto dev board)
//   .FCLKIN("27"),
//   .IDIV_SEL(8), // -> PFD = 3 MHz (range: 3-400 MHz)
//   .FBDIV_SEL(39), // -> CLKOUT = 120 MHz (range: 3.125-600 MHz)
//   .DYN_SDIV_SEL(12),   .ODIV_SEL(4) // -> VCO = 480 MHz (range: 400-1200 MHz)
// ) pll (.CLKOUTP(), .CLKOUTD3(), .RESET(1'b0), .RESET_P(1'b0), .CLKFB(1'b0), .FBDSEL(6'b0), .IDSEL(6'b0), .ODSEL(6'b0), .PSDA(4'b0), .DUTYDA(4'b0), .FDLY(4'b0),
//   .CLKIN(in_clk), // 27 MHz
//   .CLKOUT(psram_clk), // 120 MHz
//   .CLKOUTD(lcd_clk), // 10 MHz
//   .LOCK(clk_lock)
// );

// rPLL #( // For GW1NR-9C C6/I5 (Tang Nano 9K proto dev board)
//   .FCLKIN("27"),
//   .IDIV_SEL(8), // -> PFD = 3 MHz (range: 3-400 MHz)
//   .FBDIV_SEL(36), // -> CLKOUT = 111 MHz (range: 3.125-600 MHz)
//   .DYN_SDIV_SEL(12),   .ODIV_SEL(4) // -> VCO = 444 MHz (range: 400-1200 MHz)
// ) pll (.CLKOUTP(), .CLKOUTD3(), .RESET(1'b0), .RESET_P(1'b0), .CLKFB(1'b0), .FBDSEL(6'b0), .IDSEL(6'b0), .ODSEL(6'b0), .PSDA(4'b0), .DUTYDA(4'b0), .FDLY(4'b0),
//   .CLKIN(in_clk), // 27 MHz
//   .CLKOUT(psram_clk), // 111 MHz
//   .CLKOUTD(lcd_clk), // 9.25 MHz
//   .LOCK(clk_lock)
// );

//  rPLL #( // For GW1NR-9C C6/I5 (Tang Nano 9K proto dev board)
//   .FCLKIN("27"),
//   .IDIV_SEL(0), // -> PFD = 27 MHz (range: 3-400 MHz)
//   .FBDIV_SEL(0), // -> CLKOUT = 27 MHz (range: 3.125-600 MHz)
//   .DYN_SDIV_SEL(4),   .ODIV_SEL(16) // -> VCO = 432 MHz (range: 400-1200 MHz)
// ) pll (.CLKOUTP(), .CLKOUTD3(), .RESET(1'b0), .RESET_P(1'b0), .CLKFB(1'b0), .FBDSEL(6'b0), .IDSEL(6'b0), .ODSEL(6'b0), .PSDA(4'b0), .DUTYDA(4'b0), .FDLY(4'b0),
//   .CLKIN(in_clk), // 27 MHz
//   .CLKOUT(psram_clk), // 27 MHz
//   .CLKOUTD(lcd_clk), // 6.75 MHz
//   .LOCK(clk_lock)
// );


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