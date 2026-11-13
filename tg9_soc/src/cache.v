// cache_d
module rwcache (
    // system
    input wire sys_clk,
    input wire nrst,

    // cpu port
    input wire cpu_valid,
    input wire [31:0] cpu_addr,
    input wire [31:0] cpu_wdata,
    input wire [3:0] cpu_wstrb,
    // input wire cpu_flush,
    output reg [31:0] cpu_rdata,
    output reg cpu_ready,

    // arbitrer port
    output reg CPU_req_valid,
    input wire CPU_req_ready,
    output reg CPU_req_we,
    output reg [20:0] CPU_req_addr,
    output reg [63:0] CPU_write_data,
    input wire [63:0] CPU_read_data
    
);

// one cache line size 64 bytes
// or 8 qwords
// psram burst - 8 qwords
// 64 lines in cache
// total 512 qwords
// 

wire cpu_addr_nibble = cpu_addr[2]; // low/high nibble (for 0/1)
wire [2:0] cpu_addr_offset = cpu_addr[5:3]; // which word in cache line
wire [5:0] cpu_addr_ram_line = cpu_addr[11:6]; // which line in cache
wire [19:0] cpu_addr_tag = cpu_addr[31:12]; // msb of real address

reg [19:0] tag_mem[0:63]; // tag_mem[n][19:0]
reg [63:0] tag_mem_valid;
reg [63:0] tag_mem_dirty;
reg [63:0] data_mem[0:511];

reg [19:0] fetched_tag;
reg [63:0] fetched_data;


reg [2:0] state;
localparam IDLE = 3'd0;
localparam COMPARE = 3'd1;
localparam WRITEBACK = 3'd2;
localparam REFILL = 3'd3;
localparam FLUSH_SCAN = 3'd4; // find dirty lines
localparam FLUSH_PREP = 3'd5;
localparam FLUSH_WB   = 3'd6;
reg [5:0] flush_line; // flush line cnt

reg [2:0] burst_cnt;
reg burst_active;

reg [63:0] modified_word;
always @(*) begin
    modified_word = fetched_data;
    if (cpu_addr_nibble) begin
        if (cpu_wstrb[0]) modified_word[39:32] = cpu_wdata[7:0];
        if (cpu_wstrb[1]) modified_word[47:40] = cpu_wdata[15:8];
        if (cpu_wstrb[2]) modified_word[55:48] = cpu_wdata[23:16];
        if (cpu_wstrb[3]) modified_word[63:56] = cpu_wdata[31:24];
    end else begin
        if (cpu_wstrb[0]) modified_word[7:0] = cpu_wdata[7:0];
        if (cpu_wstrb[1]) modified_word[15:8] = cpu_wdata[15:8];
        if (cpu_wstrb[2]) modified_word[23:16] = cpu_wdata[23:16];
        if (cpu_wstrb[3]) modified_word[31:24] = cpu_wdata[31:24];
    end
end

wire is_hit = (cpu_addr_tag == fetched_tag) && tag_mem_valid[cpu_addr_ram_line];
wire data_mem_we = (state == COMPARE && is_hit && (cpu_wstrb != 4'b0000)) || (state == REFILL  && (CPU_req_ready || burst_active));
wire [63:0] data_mem_wdata = (state == COMPARE) ? modified_word : CPU_read_data;

reg [2:0] mem_word_idx; // data_mem addr
always @(*) begin
    case (state)
        IDLE: begin
            mem_word_idx = cpu_addr_offset;
        end
        COMPARE: begin
            mem_word_idx = is_hit ? cpu_addr_offset : 3'd0;
        end
        WRITEBACK: begin
            mem_word_idx = (CPU_req_ready || burst_active) ? (burst_cnt + 3'd1) : burst_cnt;
        end
        REFILL: begin
            mem_word_idx = burst_cnt;
        end
        default: mem_word_idx = cpu_addr_offset;
    endcase
end

wire [8:0] data_mem_addr = {cpu_addr_ram_line, mem_word_idx};
wire tag_mem_we = (state == REFILL && burst_active && (burst_cnt == 3'd7));

// bsram
always @(posedge sys_clk) begin
    fetched_data <= data_mem[data_mem_addr];
    if (data_mem_we) begin
        data_mem[data_mem_addr] <= data_mem_wdata;
    end

    fetched_tag <= tag_mem[cpu_addr_ram_line];
    if (tag_mem_we) begin
        tag_mem[cpu_addr_ram_line] <= cpu_addr_tag;
    end
end

always @(*) begin
    CPU_write_data = fetched_data;
end

always @(posedge sys_clk or negedge nrst) begin
if(!nrst) begin
    state <= IDLE;
    tag_mem_dirty <= 64'd0;
    tag_mem_valid <= 64'd0;
    cpu_ready <= 1'b0;
    cpu_rdata <= 32'd0;
    CPU_req_valid <= 1'b0;
    CPU_req_we <= 1'b0;
    CPU_req_addr <= 21'd0;
    burst_cnt  <= 3'd0;
    burst_active <= 1'b0;
end else begin
    cpu_ready <= 1'b0;
    case (state)
        IDLE: begin
            if(cpu_valid && !cpu_ready) begin
                state <= COMPARE;
            end
        end

        COMPARE: begin
                if (is_hit) begin // HIT
                    if (cpu_wstrb == 4'b0000) begin // READ
                        cpu_rdata <= cpu_addr_nibble ? fetched_data[63:32] : fetched_data[31:0];
                    end else begin // WRITE
                        tag_mem_dirty[cpu_addr_ram_line] <= 1'b1;
                    end
                    cpu_ready <= 1'b1;
                    state <= IDLE;
                end else begin // MISS
                    burst_cnt <= 3'd0;
                    burst_active <= 1'b0;

                    if (tag_mem_valid[cpu_addr_ram_line] && tag_mem_dirty[cpu_addr_ram_line]) begin
                        CPU_req_valid <= 1'b1;
                        CPU_req_we <= 1'b1;
                        CPU_req_addr <= {fetched_tag[10:0], cpu_addr_ram_line, 4'b0000};
                        state         <= WRITEBACK;
                    end else begin
                        CPU_req_valid <= 1'b1;
                        CPU_req_we <= 1'b0;
                        CPU_req_addr <= {cpu_addr_tag[10:0], cpu_addr_ram_line, 4'b0000};
                        state <= REFILL;
                    end
                end
            end

        WRITEBACK: begin
                if (CPU_req_ready && !burst_active) begin
                    CPU_req_valid <= 1'b0;
                    burst_active <= 1'b1;
                    burst_cnt <= 3'd1;
                end else if (burst_active) begin
                    if (burst_cnt == 3'd7) begin
                        burst_active <= 1'b0;
                        burst_cnt <= 3'd0;
                        CPU_req_valid <= 1'b1;
                        CPU_req_we <= 1'b0;
                        CPU_req_addr <= {cpu_addr_tag[10:0], cpu_addr_ram_line, 4'b0000};
                        state <= REFILL;
                    end else begin
                        burst_cnt <= burst_cnt + 3'd1;
                    end
                end
            end

            REFILL: begin
                if (CPU_req_ready && !burst_active) begin
                    CPU_req_valid <= 1'b0;
                    burst_active <= 1'b1;
                    burst_cnt <= 3'd1;
                end else if (burst_active) begin
                    if (burst_cnt == 3'd7) begin
                        burst_active <= 1'b0;
                        burst_cnt <= 3'd0;
                        tag_mem_valid[cpu_addr_ram_line] <= 1'b1;
                        tag_mem_dirty[cpu_addr_ram_line] <= 1'b0;
                        state <= IDLE;
                    end else begin
                        burst_cnt <= burst_cnt + 3'd1;
                    end
                end
            end
    endcase


end
end
endmodule

