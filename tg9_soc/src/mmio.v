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
localparam BIOSFLASH_CTRL = 32'hC000_0014; // [20]nvstr [19]prog [18]erase [17]se [16]ye [15]xe [14:6]xadr [5:0]yadr
localparam BIOSFLASH_DOUT = 32'hC000_0018;
localparam BIOSFLASH_DIN = 32'hC000_001C;
localparam TIMER_US = 32'hC000_0020;
localparam MTIME_LO = 32'hC000_0024;
localparam MTIME_HI = 32'hC000_0028;

reg [5:0] us_prescaler;
reg [31:0] timer_us_reg;
reg [63:0] mtime_reg;

reg [31:0] UARTCTRL_reg;
reg UART_tx_start;
wire UART_tx_rdy;
wire [7:0] UART_rx_data;
wire UART_rx_valid;

// reg [31:0] fastsram[0:4095];

wire nvstr, prog, erase, se, ye, xe;
wire [8:0] xadr;
wire [5:0] yadr;
wire [31:0] din;
wire [31:0] dout;
reg [31:0] BIOSFLASH_ctrl_reg;
reg [31:0] BIOSFLASH_din_reg;
assign nvstr = BIOSFLASH_ctrl_reg[20];
assign prog = BIOSFLASH_ctrl_reg[19];
assign erase = BIOSFLASH_ctrl_reg[18];
assign se = BIOSFLASH_ctrl_reg[17];
assign ye = BIOSFLASH_ctrl_reg[16];
assign xe = BIOSFLASH_ctrl_reg[15];
assign xadr = BIOSFLASH_ctrl_reg[14:6];
assign yadr = BIOSFLASH_ctrl_reg[5:0];
assign din = BIOSFLASH_din_reg;

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
        us_prescaler <= 6'd0;
        timer_us_reg <= 32'd0;
        mtime_reg <= 64'd0;
        BIOSFLASH_ctrl_reg <= 32'd0;
    end else begin
        mtime_reg <= mtime_reg + 64'd1;
        if (us_prescaler == 6'd59) begin
            us_prescaler <= 6'd0;
            timer_us_reg <= timer_us_reg + 32'd1;
        end else begin
            us_prescaler <= us_prescaler + 6'd1;
        end
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

                BIOSFLASH_CTRL: begin
                    if (wstrb[0]) BIOSFLASH_ctrl_reg[7:0] <= data_in[7:0];
                    if (wstrb[1]) BIOSFLASH_ctrl_reg[15:8] <= data_in[15:8];
                    if (wstrb[2]) BIOSFLASH_ctrl_reg[23:16] <= data_in[23:16];
                    if (wstrb[3]) BIOSFLASH_ctrl_reg[31:24] <= data_in[31:24];
                end

                BIOSFLASH_DIN: begin
                    if (wstrb[0]) BIOSFLASH_din_reg[7:0] <= data_in[7:0];
                    if (wstrb[1]) BIOSFLASH_din_reg[15:8] <= data_in[15:8];
                    if (wstrb[2]) BIOSFLASH_din_reg[23:16] <= data_in[23:16];
                    if (wstrb[3]) BIOSFLASH_din_reg[31:24] <= data_in[31:24];
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
            TIMER_US: data_out <= timer_us_reg;
            BIOSFLASH_CTRL: data_out <= BIOSFLASH_ctrl_reg;
            BIOSFLASH_DIN: data_out <= BIOSFLASH_din_reg;
            BIOSFLASH_DOUT: data_out <= dout;
            MTIME_LO: data_out <= mtime_reg[31:0];
            MTIME_HI: data_out <= mtime_reg[63:32];
            default: data_out <= 32'd0;
        endcase
    end
end

Gowin_User_Flash bios_flash (
    .dout(dout), //output [31:0] dout
    .xe(xe), //input xe
    .ye(ye), //input ye
    .se(se), //input se
    .prog(prog), //input prog
    .erase(erase), //input erase
    .nvstr(nvstr), //input nvstr
    .xadr(xadr), //input [8:0] xadr
    .yadr(yadr), //input [5:0] yadr
    .din(din) //input [31:0] din
);

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