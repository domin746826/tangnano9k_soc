`default_nettype none

module spi (
    input wire sys_clk,
    input wire nrst,

    input  wire en,
    input  wire cs_en,
    input  wire start,
    input  wire [15:0] clk_div,
    input  wire [7:0] data_in,
    output reg [7:0] data_out,
    output reg busy,
    output reg done,

    output reg sck,
    output reg mosi,
    input wire miso,
    output reg cs_n
);

reg [15:0] clk_cnt;
reg [3:0] bit_cnt;
reg [7:0] sh_reg_tx;
reg [7:0] sh_reg_rx;

localparam IDLE = 1'd0;
localparam TRANSFER = 1'd1;

reg state;

always @(posedge sys_clk or negedge nrst) begin
    if (!nrst) begin
        state <= IDLE;
        sck <= 1'b0;
        mosi <= 1'b1;
        cs_n <= 1'b1;
        busy <= 1'b0;
        done <= 1'b0;
        data_out <= 8'd0;
        clk_cnt <= 16'd0;
        bit_cnt <= 4'd0;
        sh_reg_tx <= 8'hFF;
        sh_reg_rx <= 8'd0;
    end else begin
        done <= 1'b0;

        if (!en) begin
            state <= IDLE;
            cs_n  <= 1'b1;
            sck   <= 1'b0;
            mosi  <= 1'b1;
            busy  <= 1'b0;
        end else begin
            cs_n <= ~cs_en;

            case (state)
                IDLE: begin
                    sck  <= 1'b0;
                    mosi <= 1'b1;
                    busy <= 1'b0;
                    if (start) begin
                        mosi <= data_in[7];
                        sh_reg_tx <= {data_in[6:0], 1'b1};
                        busy <= 1'b1;
                        clk_cnt <= clk_div;
                        bit_cnt <= 4'd8;
                        state <= TRANSFER;
                    end
                end

                TRANSFER: begin
                    if (clk_cnt == 16'd0) begin
                        clk_cnt <= clk_div;

                        if (sck == 1'b0) begin
                            sck <= 1'b1;
                            sh_reg_rx <= {sh_reg_rx[6:0], miso};
                        end else begin
                            sck <= 1'b0;
                            if (bit_cnt == 4'd1) begin
                                data_out <= sh_reg_rx;
                                mosi <= 1'b1;
                                busy <= 1'b0;
                                done <= 1'b1;
                                state <= IDLE;
                            end else begin
                                bit_cnt <= bit_cnt - 4'd1;
                                mosi <= sh_reg_tx[7];
                                sh_reg_tx <= {sh_reg_tx[6:0], 1'b1};
                            end
                        end
                    end else begin
                        clk_cnt <= clk_cnt - 16'd1;
                    end
                end

                default: state <= IDLE;
            endcase
        end
    end
end

endmodule