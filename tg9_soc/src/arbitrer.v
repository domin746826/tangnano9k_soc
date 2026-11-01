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

    // cpu cache port (rw)
    input wire CPU_req_valid,
    output reg CPU_req_ready,
    input wire CPU_req_we,
    input wire [20:0] CPU_req_addr,
    input wire [63:0] CPU_write_data,
    output reg [63:0] CPU_read_data,

    // lcd port (ro)
    input wire LCD_req_valid,
    output reg LCD_req_ready,
    input wire [20:0] LCD_req_addr,
    output reg [63:0] LCD_read_data
);

reg [1:0] state;
reg WRPR_req_ready_delayed;


localparam IDLE = 0;
localparam CPU = 1;
localparam LCD = 2;

always @(*) begin
WRPR_req_valid = 1'b0;
WRPR_req_addr = 21'd0;
WRPR_req_we = 1'b0;
WRPR_write_data = 64'd0;
CPU_req_ready = 1'b0;
CPU_read_data = WRPR_read_data;
LCD_req_ready = 1'b0;
LCD_read_data = WRPR_read_data;


case (state)
    CPU: begin
        WRPR_req_valid = CPU_req_valid;
        WRPR_req_we = CPU_req_we;
        WRPR_req_addr = CPU_req_addr;
        WRPR_write_data = CPU_write_data;
        CPU_req_ready = WRPR_req_ready;
    end

    LCD: begin
        WRPR_req_valid = LCD_req_valid;
        WRPR_req_addr = LCD_req_addr;
        LCD_req_ready = WRPR_req_ready;
    end
endcase
end

always @(posedge sys_clk or negedge nrst) begin
if(!nrst) begin
    state <= IDLE;
    WRPR_req_ready_delayed <= 0;
end else begin
    WRPR_req_ready_delayed <= WRPR_req_ready;
    case(state)
        IDLE: begin
            if(LCD_req_valid) begin
                state <= LCD;
            end else if(CPU_req_valid) begin
                state <= CPU;
            end
        end

        CPU: begin
            if(!WRPR_req_ready & WRPR_req_ready_delayed) begin
                state <= IDLE;
            end
        end

        LCD: begin
            if(!WRPR_req_ready & WRPR_req_ready_delayed) begin
                state <= IDLE;
            end
        end

        default: begin state <= IDLE; end
    endcase
end
end

endmodule
