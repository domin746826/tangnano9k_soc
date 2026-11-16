module rocache (
    // system
    input wire sys_clk,
    input wire nrst,

    // cpu port ro
    input wire cpu_valid,
    input wire [31:0] cpu_addr,
    input wire cpu_flush,
    output reg [31:0] cpu_rdata,
    output reg cpu_ready,

    // data-side ROM port (0x8000_0000..0x8000_1FFF)
    input wire drom_valid,
    input wire [31:0] drom_addr,
    output reg [31:0] drom_rdata,
    output reg drom_ready,

    // arbitrer port ro
    output reg CPU_req_valid,
    input wire CPU_req_ready,
    output reg [20:0] CPU_req_addr,
    input wire [63:0] CPU_read_data
);

wire cpu_addr_nibble = cpu_addr[2];
wire [2:0] cpu_addr_offset = cpu_addr[5:3];
wire [5:0] cpu_addr_ram_line = cpu_addr[11:6];
wire [19:0] cpu_addr_tag = cpu_addr[31:12];

reg [19:0] tag_mem[0:63];
reg [63:0] tag_mem_valid;
reg [63:0] data_mem[0:511];

reg [19:0] fetched_tag;
reg [63:0] fetched_data;

(* ram_style="block" *) reg [31:0] bios_rom [0:2047]; // 8 KB
reg [31:0] fetched_rom;

initial begin
    $readmemh("../../asm/bios/bios.hex", bios_rom);
end

reg [1:0] state;
localparam IDLE    = 2'd0;
localparam COMPARE = 2'd1;
localparam REFILL  = 2'd2;

reg [2:0] burst_cnt;
reg burst_active;
reg flush_pending;

wire is_rom = cpu_addr[31];
wire is_hit = (cpu_addr_tag == fetched_tag) && tag_mem_valid[cpu_addr_ram_line];

wire data_mem_we = (state == REFILL) && (CPU_req_ready || burst_active);
wire [2:0] mem_word_idx = (state == REFILL) ? burst_cnt : cpu_addr_offset;

wire [8:0] data_mem_addr = {cpu_addr_ram_line, mem_word_idx};
wire tag_mem_we = (state == REFILL && burst_active && (burst_cnt == 3'd7));

always @(posedge sys_clk) begin
    fetched_data <= data_mem[data_mem_addr];
    if (data_mem_we) begin
        data_mem[data_mem_addr] <= CPU_read_data;
    end

    fetched_tag <= tag_mem[cpu_addr_ram_line];
    if (tag_mem_we) begin
        tag_mem[cpu_addr_ram_line] <= cpu_addr_tag;
    end

    if (drom_valid) begin
        drom_rdata <= bios_rom[drom_addr[12:2]];
    end else begin
        fetched_rom <= bios_rom[cpu_addr[12:2]];
    end
end

always @(posedge sys_clk or negedge nrst) begin
if (!nrst) begin
    state <= IDLE;
    tag_mem_valid <= 64'd0;
    cpu_ready <= 1'b0;
    cpu_rdata <= 32'd0;
    drom_ready <= 1'b0;
    CPU_req_valid <= 1'b0;
    CPU_req_addr <= 21'd0;
    burst_cnt <= 3'd0;
    burst_active <= 1'b0;
    flush_pending <= 1'b0;
end else begin
    cpu_ready <= 1'b0;
    drom_ready <= drom_valid && !drom_ready;

    if (cpu_flush) begin
        flush_pending <= 1'b1;
    end

    case (state)
        IDLE: begin
            if (cpu_flush || flush_pending) begin
                tag_mem_valid <= 64'd0;
                flush_pending <= 1'b0;
            end else if (cpu_valid && !cpu_ready && !drom_valid) begin
                state <= COMPARE;
            end
        end

        COMPARE: begin
            if (is_rom) begin
                cpu_rdata <= fetched_rom;
                cpu_ready <= 1'b1;
                state <= IDLE;
            end else if (is_hit) begin
                cpu_rdata <= cpu_addr_nibble ? fetched_data[63:32] : fetched_data[31:0];
                cpu_ready <= 1'b1;
                state <= IDLE;
            end else begin // MISS
                burst_cnt <= 3'd0;
                burst_active <= 1'b0;
                CPU_req_valid <= 1'b1;
                CPU_req_addr <= {cpu_addr_tag[10:0], cpu_addr_ram_line, 4'b0000};
                state <= REFILL;
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
                    state <= IDLE;
                end else begin
                    burst_cnt <= burst_cnt + 3'd1;
                end
            end
        end

        default: state <= IDLE;
    endcase
end
end

endmodule