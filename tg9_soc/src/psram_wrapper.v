// interface

// write/read
// clk:         _-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-
// req_valid:   __-------_______
// req_ready:   _______------
// data rd:     _______dddddddd
// data wr:     __d0___ddddddd
// addr:        __aaaaaaaaaaaa

module psram_wrapper (
    input nrst,
    input clk,
    input init_calib,
    input rd_data_valid,

    // interface
    input wire req_valid,
    output reg req_ready,
    input req_we,
    input [20:0] req_addr,

    // data
    input wire [63:0] write_data,
    output reg [63:0] read_data,
    input wire [63:0] rd_data,
    output reg [63:0] wr_data,
    output reg [20:0] addr,
    output reg cmd,
    output reg cmd_en
);

localparam DOWN = 0;
localparam READY = 1;
localparam READ = 2;
localparam WRITE = 3;
reg [1:0] state;


reg [4:0] tclk;

// 18Tclk for 32 burst
// 32 burst - read/write takes 8 clk cycle
localparam Tclk_val = 18;
reg [2:0] rw_clk;

always @(posedge clk or negedge nrst) begin
if(!nrst) begin
    state <= DOWN;
    req_ready <= 0;
    read_data <= 64'd0;
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
        if(tclk == 0 && req_valid) begin
            tclk <= Tclk_val;
            cmd <= req_we; // 0 read 1 write
            cmd_en <= 1;
            addr <= req_addr;
            if(req_we) begin
                req_ready <= 1;
                wr_data <= write_data;
                state <= WRITE;
            end else begin
                state <= READ;
            end
        
        end
    end READ: begin
        cmd_en <= 0;
        if(rd_data_valid) begin
            req_ready <= 1;
            read_data <= rd_data;
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
        req_ready <= 1;
        if(rw_clk == 3'd6) begin
            state <= READY;
            rw_clk <= 0;
            req_ready <= 0;
        end

    end
endcase
end
end



endmodule


