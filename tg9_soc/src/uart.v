`default_nettype none

module uart_core (
    input wire clk,
    input wire nrst,
    input wire en,
    input wire [12:0] baud_div,

    // tx
    input wire [7:0] tx_data,
    input wire tx_start,
    output wire tx_rdy,
    output reg uart_tx,

    // rx
    output reg [7:0] rx_data,
    output reg rx_valid,
    input wire uart_rx
);

// rx sync
reg rx_sync0, rx_sync1, rx_prev;
always @(posedge clk) begin
    if (!nrst) begin
        rx_sync0 <= 1'b1;
        rx_sync1 <= 1'b1;
        rx_prev <= 1'b1;
    end else begin
        rx_sync0 <= uart_rx;
        rx_sync1 <= rx_sync0;
        rx_prev <= rx_sync1;
    end
end

wire rx_pin = rx_sync1;
wire rx_falling_edge = (rx_prev == 1'b1) && (rx_sync1 == 1'b0);

wire [12:0] safe_div = (baud_div < 13'd2) ? 13'd2 : baud_div;
wire [12:0] half_div = {1'b0, safe_div[12:1]};


localparam TX_IDLE  = 2'd0;
localparam TX_START = 2'd1;
localparam TX_DATA  = 2'd2;
localparam TX_STOP  = 2'd3;

reg [1:0]  tx_state;
reg [12:0] tx_cnt;
reg [2:0]  tx_bit_idx;
reg [7:0]  tx_shreg;

assign tx_rdy = (tx_state == TX_IDLE) && !tx_start;

always @(posedge clk) begin
    if (!nrst || !en) begin
        tx_state <= TX_IDLE;
        tx_cnt <= 13'd0;
        tx_bit_idx <= 3'd0;
        tx_shreg <= 8'd0;
        uart_tx  <= 1'b1;
    end else begin
        case (tx_state)
            TX_IDLE: begin
                uart_tx <= 1'b1;
                if (tx_start) begin
                    tx_shreg <= tx_data;
                    tx_cnt <= safe_div - 13'd1;
                    tx_state <= TX_START;
                    uart_tx <= 1'b0;
                end
            end

            TX_START: begin
                if (tx_cnt == 13'd0) begin
                    tx_cnt <= safe_div - 13'd1;
                    tx_bit_idx <= 3'd0;
                    uart_tx <= tx_shreg[0];
                    tx_shreg <= {1'b0, tx_shreg[7:1]};
                    tx_state <= TX_DATA;
                end else begin
                    tx_cnt <= tx_cnt - 13'd1;
                end
            end

            TX_DATA: begin
                if (tx_cnt == 13'd0) begin
                    tx_cnt <= safe_div - 13'd1;
                    if (tx_bit_idx == 3'd7) begin
                        uart_tx <= 1'b1;
                        tx_state <= TX_STOP;
                    end else begin
                        tx_bit_idx <= tx_bit_idx + 3'd1;
                        uart_tx <= tx_shreg[0];
                        tx_shreg <= {1'b0, tx_shreg[7:1]};
                    end
                end else begin
                    tx_cnt <= tx_cnt - 13'd1;
                end
            end

            TX_STOP: begin
                if (tx_cnt == 13'd0) begin
                    tx_state <= TX_IDLE;
                end else begin
                    tx_cnt <= tx_cnt - 13'd1;
                end
            end
        endcase
    end
end


localparam RX_IDLE  = 2'd0;
localparam RX_START = 2'd1;
localparam RX_DATA  = 2'd2;
localparam RX_STOP  = 2'd3;

reg [1:0]  rx_state;
reg [12:0] rx_cnt;
reg [2:0]  rx_bit_idx;
reg [7:0]  rx_shreg;

always @(posedge clk) begin
    if (!nrst || !en) begin
        rx_state <= RX_IDLE;
        rx_cnt <= 13'd0;
        rx_bit_idx <= 3'd0;
        rx_shreg <= 8'd0;
        rx_data <= 8'd0;
        rx_valid <= 1'b0;
    end else begin
        rx_valid <= 1'b0;
        case (rx_state)
            RX_IDLE: begin
                if (rx_falling_edge) begin
                    rx_cnt <= half_div - 13'd1;
                    rx_state <= RX_START;
                end
            end

            RX_START: begin
                if (rx_cnt == 13'd0) begin
                    if (rx_pin == 1'b0) begin
                        rx_cnt <= safe_div - 13'd1;
                        rx_bit_idx <= 3'd0;
                        rx_state <= RX_DATA;
                    end else begin
                        rx_state <= RX_IDLE;
                    end
                end else begin
                    rx_cnt <= rx_cnt - 13'd1;
                end
            end

            RX_DATA: begin
                if (rx_cnt == 13'd0) begin
                    rx_cnt <= safe_div - 13'd1;
                    rx_shreg <= {rx_pin, rx_shreg[7:1]};
                    if (rx_bit_idx == 3'd7) begin
                        rx_state <= RX_STOP;
                    end else begin
                        rx_bit_idx <= rx_bit_idx + 3'd1;
                    end
                end else begin
                    rx_cnt <= rx_cnt - 13'd1;
                end
            end

            RX_STOP: begin
                if (rx_cnt == 13'd0) begin
                    if (rx_pin == 1'b1) begin
                        rx_data <= rx_shreg;
                        rx_valid <= 1'b1;
                    end
                    rx_state <= RX_IDLE;
                end else begin
                    rx_cnt <= rx_cnt - 13'd1;
                end
            end
        endcase
    end
end

endmodule