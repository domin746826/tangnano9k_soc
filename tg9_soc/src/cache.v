module cache (
    // system
    input wire sys_clk,
    input wire nrst,

    // cpu port
    input wire cpu_valid,
    input wire [31:0] cpu_addr,
    input wire [31:0] cpu_wdata,
    input wire [3:0] cpu_wstrb,
    // input wire cpu_flush,
    output wire [31:0] cpu_rdata,
    output wire cpu_ready,

    // arbitrer port
    output reg CPU_req_valid,
    input wire CPU_req_ready,
    output reg CPU_req_we,
    output reg [20:0] CPU_req_addr,
    output reg [63:0] CPU_write_data,
    input wire [63:0] CPU_read_data
    
);

wire cpu_addr_nibble = cpu_addr[2]; // low/high nibble
wire [1:0] cpu_addr_offset = cpu_addr[5:3]; // which word in cache line
wire [5:0] cpu_addr_ram_line = cpu_addr[11:6]; // which line in cache
wire [20:0] cpu_addr_tag = cpu_addr[31:12]; // msb of real address

reg [21:0] tag_mem[0:63];
reg [63:0] data_mem[0:511];

reg [1:0] state;
localparam IDLE = 0;
localparam COMPARE = 1;
localparam WRITEBACK = 2;
localparam REFILL = 3;

always @(posedge sys_clk or negedge nrst) begin
if(!nrst) begin
    state <= IDLE;
end else begin
    case (state)
        IDLE: begin
        end

        COMPARE: begin
        end

        WRITEBACK: begin
        end

        REFILL: begin
        end
    endcase


end
end
endmodule

