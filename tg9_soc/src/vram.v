module vram #(
    parameter CAPACITY = 512
)(
    output reg [15:0] render_data,
    input wire [$clog2(CAPACITY)-1:0] render_addr,
    input wire render_clk,

    input wire [15:0] user_data_in,
    output reg [15:0] user_data_out,
    input wire user_we,
    input wire [$clog2(CAPACITY)-1:0] user_addr,
    input wire user_clk
);

(* ram_style="block" *) reg [15:0] mem [CAPACITY-1:0];

// initial begin
//     $readmemh("image.hex", mem);
// end

always @(posedge render_clk) begin
    render_data <= mem[render_addr];
end

always @(posedge user_clk) begin
    if (user_we) mem[user_addr] <= user_data_in;
end

endmodule