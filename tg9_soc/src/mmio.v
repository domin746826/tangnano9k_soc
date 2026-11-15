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
    input  wire uart_rx,
    input wire [8:0] fb_line,
    output reg [31:0] FBCTRL_reg
);

localparam LED_REG = 32'hC000_0000;
localparam BTN_REG = 32'hC000_0004;
localparam UARTCTRL = 32'hC000_0008; // BAUDBAUD_BAUDB<TXRDY><RXRDY><EN>_RXRXRXRX_TXTXTXTX
localparam FBCTRL = 32'hC000_000C; // <EN>*******_*FBADDRF_BADDRFBA_DDRFBADD
localparam FBSTAT = 32'hC000_0010; // ********_********_*******L_INENRLIN


reg [31:0] UARTCTRL_reg;
reg UART_tx_start;
wire UART_tx_rdy;
wire [7:0] UART_rx_data;
wire UART_rx_valid;

// lcd line sync
reg [8:0] line_sync0, line_sync1, line_stable;
always @(posedge sys_clk) begin
    line_sync0 <= fb_line;
    line_sync1 <= line_sync0;
    if (line_sync0 == line_sync1)
        line_stable <= line_sync1;
end

always @(posedge sys_clk) begin
    if (!nrst) begin
        led <= 6'b111111;
        data_out <= 32'd0;
        UARTCTRL_reg <= {13'd521, 3'b000, 16'h0000}; // defualt 115200 @ 60 MHz
        UART_tx_start <= 1'b0;
        FBCTRL_reg <= 32'h007C_0000; // default last 255kb fb, disabled output
    end else begin
        UART_tx_start <= 1'b0;
        if (|wstrb) begin
            case ({addr[31:2], 2'b00})
                LED_REG: begin
                    if (wstrb[0]) led <= data_in[5:0];
                end

                UARTCTRL: begin
                    if(wstrb[0]) begin
                        UARTCTRL_reg[7:0] <= data_in[7:0];
                        UART_tx_start <= 1'b1;
                    end
                    if (wstrb[1]) begin
                        UARTCTRL_reg[17] <= 1'b0; // delete rx ready flag when writing anything to rx
                    end
                    if (wstrb[2]) begin
                        UARTCTRL_reg[16] <= data_in[16]; // en
                        UARTCTRL_reg[23:19] <= data_in[23:19]; // baud
                    end
                    if (wstrb[3]) begin
                        UARTCTRL_reg[31:24] <= data_in[31:24]; // baud
                    end
                end

                FBCTRL: begin
                    if (wstrb[0]) FBCTRL_reg[7:0] <= data_in[7:0];
                    if (wstrb[1]) FBCTRL_reg[15:8] <= data_in[15:8];
                    if (wstrb[2]) FBCTRL_reg[22:16] <= data_in[22:16];
                    if (wstrb[3]) FBCTRL_reg[31] <= data_in[31]; // EN
                end
                default: begin end
            endcase
        end
        // UART
        if(UART_rx_valid) begin
            UARTCTRL_reg[15:8] <= UART_rx_data;
            UARTCTRL_reg[17] <= 1'b1;
        end

        case ({addr[31:2], 2'b00})
            LED_REG: data_out <= {26'd0, led};
            BTN_REG: data_out <= {31'd0, btn};
            UARTCTRL: data_out <= {UARTCTRL_reg[31:19], (UART_tx_rdy && !UART_tx_start), UARTCTRL_reg[17:0]};
            FBCTRL: data_out <= FBCTRL_reg;
            FBSTAT: data_out <= {23'd0, line_stable};
            default: data_out <= 32'd0;
        endcase
    end
end

uart_core uart_inst (
    .clk(sys_clk),
    .nrst(nrst),
    .en(UARTCTRL_reg[16]),
    .baud_div(UARTCTRL_reg[31:19]),

    // tx
    .tx_data(UARTCTRL_reg[7:0]),
    .tx_start(UART_tx_start),
    .tx_rdy(UART_tx_rdy),
    .uart_tx(uart_tx),

    // rx
    .rx_data(UART_rx_data),
    .rx_valid(UART_rx_valid),
    .uart_rx(uart_rx)
);

endmodule