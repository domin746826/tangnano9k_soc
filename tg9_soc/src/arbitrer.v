module arbitrer (
    input wire nrst,
    input wire sys_clk,

    // wrapper psram port
    output reg WRPR_req_valid,
    input wire WRPR_req_ready,
    output reg WRPR_req_we,
    output reg [20:0] WRPR_req_addr,
    output reg [63:0] WRPR_write_data,
    input wire [63:0] WRPR_read_data,

    // cpu D cache port (rw)
    input wire CPUD_req_valid,
    output reg CPUD_req_ready,
    input wire CPUD_req_we,
    input wire [20:0] CPUD_req_addr,
    input wire [63:0] CPUD_write_data,
    output reg [63:0] CPUD_read_data,

    // cpu I cache port (ro)
    input wire CPUI_req_valid,
    output reg CPUI_req_ready,
    input wire [20:0] CPUI_req_addr,
    output reg [63:0] CPUI_read_data,

    // lcd port (ro)
    input wire LCD_req_valid,
    output reg LCD_req_ready,
    input wire [20:0] LCD_req_addr,
    output reg [63:0] LCD_read_data
);

reg [1:0] state;
reg WRPR_req_ready_delayed;

localparam IDLE = 2'd0;
localparam CPU_I = 2'd1;
localparam CPU_D = 2'd2;
localparam LCD = 2'd3;

always @(*) begin
WRPR_req_valid = 1'b0;
WRPR_req_addr = 21'd0;
WRPR_req_we = 1'b0;
WRPR_write_data = 64'd0;
CPUD_req_ready = 1'b0;
CPUD_read_data = WRPR_read_data;
CPUI_req_ready = 1'b0;
CPUI_read_data = WRPR_read_data;
LCD_req_ready = 1'b0;
LCD_read_data = WRPR_read_data;

case (state)
    CPU_D: begin
        WRPR_req_valid = CPUD_req_valid;
        WRPR_req_we = CPUD_req_we;
        WRPR_req_addr = CPUD_req_addr;
        WRPR_write_data = CPUD_write_data;
        CPUD_req_ready = WRPR_req_ready;
    end

    CPU_I: begin
        WRPR_req_valid = CPUI_req_valid;
        WRPR_req_addr = CPUI_req_addr;
        CPUI_req_ready = WRPR_req_ready;
    end

    LCD: begin
        WRPR_req_valid = LCD_req_valid;
        WRPR_req_addr = LCD_req_addr;
        LCD_req_ready = WRPR_req_ready;
    end

    default: begin end
endcase
end

always @(posedge sys_clk or negedge nrst) begin
if (!nrst) begin
    state <= IDLE;
    WRPR_req_ready_delayed <= 1'b0;
end else begin
    WRPR_req_ready_delayed <= WRPR_req_ready;

    case (state)
        IDLE: begin
            if (LCD_req_valid) begin
                state <= LCD;
            end else if (CPUD_req_valid) begin
                state <= CPU_D;
            end else if (CPUI_req_valid) begin
                state <= CPU_I;
            end
        end

        LCD, CPU_I, CPU_D: begin
            if (!WRPR_req_ready && WRPR_req_ready_delayed) begin
                state <= IDLE;
            end
        end

        default: begin
            state <= IDLE;
        end
    endcase
end
end

endmodule