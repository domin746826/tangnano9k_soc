`default_nettype none

module registers (
    input wire clk,
    input wire [4:0] rs1_addr,
    input wire [4:0] rs2_addr,
    output reg [31:0] rs1_data,
    output reg [31:0] rs2_data,
    input wire [4:0] rd_addr,
    input wire [31:0] rd_data,
    input wire rd_we
);

(* syn_ramstyle = "block_mram", read_mode_0 = "outreg", read_mode_1 = "outreg" *) reg [31:0] rs1_mem[0:31];
(* syn_ramstyle = "block_mram", read_mode_0 = "outreg", read_mode_1 = "outreg" *) reg [31:0] rs2_mem[0:31];

integer i;
initial begin
    for (i = 0; i < 32; i = i + 1) begin
        rs1_mem[i] = 32'd0;
        rs2_mem[i] = 32'd0;
    end
end

always @(posedge clk) begin
    rs1_data <= rs1_mem[rs1_addr];
    rs2_data <= rs2_mem[rs2_addr];

    if (rd_we && (rd_addr != 5'd0)) begin
        rs1_mem[rd_addr] <= rd_data;
        rs2_mem[rd_addr] <= rd_data;
    end
end

endmodule
