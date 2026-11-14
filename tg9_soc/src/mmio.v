module mmio (
    input  wire sys_clk,
    input  wire nrst,
    input  wire [31:0] addr,
    input  wire [31:0] data_in,
    input  wire [3:0] wstrb,
    output reg [31:0] data_out,

    output reg [5:0] led,
    input  wire btn,
    output wire uart_tx,
    input  wire uart_rx
);

localparam LED_REG = 32'hC000_0000;
localparam BTN_REG = 32'hC000_0004;

assign uart_tx = 1'b1;

always @(posedge sys_clk) begin
    if (!nrst) begin
        led <= 6'b111111;
        data_out <= 32'd0;
    end else begin
        if (|wstrb) begin
            case ({addr[31:2], 2'b00})
                LED_REG: begin
                    if (wstrb[0]) led <= data_in[5:0];
                end
                default: begin end
            endcase
        end

        case ({addr[31:2], 2'b00})
            LED_REG: data_out <= {26'd0, led};
            BTN_REG: data_out <= {31'd0, btn};
            default: data_out <= 32'd0;
        endcase
    end
end

endmodule