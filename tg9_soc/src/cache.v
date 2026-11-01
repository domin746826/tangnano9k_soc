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

always @(posedge clk or negedge nrst) begin
if(!nrst) begin
    
end else begin

end
end
endmodule

