module top (
    output [0:0] O_psram_ck,
    output [0:0] O_psram_ck_n,
    inout [7:0] IO_psram_dq,
    inout [0:0] IO_psram_rwds,
    output [0:0] O_psram_cs_n,
    output [0:0] O_psram_reset_n,
    input nrst,
    input clk_psram,
    output clk_out,
    input pll_lock,

    // interface
    input wire req_valid,
    output reg req_ready,
    input req_we,
    input [20:0] req_addr,

    // data
    input wire [63:0] write_data,
    output reg [63:0] read_data,
    output reg read_valid
);

localparam DOWN = 0;
localparam READY = 1;
localparam READ = 2;
localparam WRITE = 3;
reg [1:0] state;

wire rst_n, init_calib, rd_data_valid;
wire [63:0] rd_data;
reg [63:0] wr_data;
reg [20:0] addr;
reg cmd;
reg cmd_en;
reg [4:0] tclk;

// 18Tclk for 32 burst
// 32 burst - read/write takes 8 clk cycle
localparam Tclk_val = 18;
reg [2:0] rw_clk;

always @(posedge clk_out or negedge nrst) begin
if(!nrst) begin
    state <= DOWN;
    req_ready <= 0;
    read_data <= 64'd0;
    read_valid <= 0;
    wr_data <= 64'd0;
    addr <= 21'd0;
    cmd <= 0;
    cmd_en <= 0;
    tclk <= 0;
    rw_clk <= 0;
end else begin
req_ready <= 0;
if(tclk != 0) begin
    tclk <= tclk - 1;
end
case (state)
    DOWN: begin
        if(init_calib) begin
            state <= READY;
        end
    end READY: begin
        read_valid <= 0;
        if(tclk == 0) begin
            req_ready <= 1;
            if(req_valid) begin
                tclk <= Tclk_val;
                cmd <= req_we; // 0 read 1 write
                cmd_en <= 1;
                addr <= req_addr;
                if(req_we) begin
                    wr_data <= write_data;
                    state <= WRITE;
                end else begin
                    state <= READ;
                end
            end
        end
    end READ: begin
        cmd_en <= 0;
        if(rd_data_valid) begin
            read_data <= rd_data;
            read_valid <= 1;
            rw_clk <= rw_clk + 1;
            if(rw_clk == 3'd7) begin
                state <= READY;
                rw_clk <= 0;
            end
        end
        

    end WRITE: begin
        cmd_en <= 0;
        rw_clk <= rw_clk + 1;
        wr_data <= write_data;
        if(rw_clk == 3'd6) begin
            state <= READY;
            rw_clk <= 0;
        end

    end
endcase
end
end



PSRAM_Memory_Interface_HS_Top psram_inst(
    .clk(clk_psram), //input clk
    .memory_clk(clk_psram), //input memory_clk
    .pll_lock(pll_lock), //input pll_lock
    .rst_n(nrst), //input rst_n
    .O_psram_ck(O_psram_ck), //output [1:0] O_psram_ck
    .O_psram_ck_n(O_psram_ck_n), //output [1:0] O_psram_ck_n
    .IO_psram_dq(IO_psram_dq), //inout [15:0] IO_psram_dq
    .IO_psram_rwds(IO_psram_rwds), //inout [1:0] IO_psram_rwds
    .O_psram_cs_n(O_psram_cs_n), //output [1:0] O_psram_cs_n
    .O_psram_reset_n(O_psram_reset_n), //output [1:0] O_psram_reset_n
    .wr_data(wr_data), //input [63:0] wr_data
    .rd_data(rd_data), //output [63:0] rd_data
    .rd_data_valid(rd_data_valid), //output rd_data_valid
    .addr(addr), //input [20:0] addr
    .cmd(cmd), //input cmd
    .cmd_en(cmd_en), //input cmd_en
    .init_calib(init_calib), //output init_calib
    .clk_out(clk_out), //output clk_out
    .data_mask(8'd0) //input [7:0] data_mask
);

endmodule


