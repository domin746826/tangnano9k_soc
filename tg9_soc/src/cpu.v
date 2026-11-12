module cpu (
    input wire sys_clk,
    input wire nrst,

    // dcache cpu port
    output reg dcpu_valid,
    output reg [31:0] dcpu_addr,
    output reg [31:0] dcpu_wdata,
    output reg [3:0] dcpu_wstrb,
    input wire [31:0] dcpu_rdata,
    input wire dcpu_ready,

    // icache cpu port
    output reg icpu_valid,
    output wire [31:0] icpu_addr,
    input wire [31:0] icpu_rdata,
    input wire icpu_ready,

    output reg [5:0] led
);

reg [31:0] PC;
assign icpu_addr = PC;

// instruction decomposition
wire [6:0] opcode = icpu_rdata[6:0];
wire [4:0] rs1_addr = icpu_rdata[19:15];
wire [4:0] rs2_addr = icpu_rdata[24:20];
wire [4:0] rd_addr = icpu_rdata[11:7];
wire [2:0] funct3 = icpu_rdata[14:12];
wire [6:0] funct7 = icpu_rdata[31:25];
wire [31:0] imm_i = {{20{icpu_rdata[31]}}, icpu_rdata[31:20]};
wire [31:0] imm_s = {{20{icpu_rdata[31]}}, icpu_rdata[31:25], icpu_rdata[11:7]};
wire [31:0] imm_b = {{19{icpu_rdata[31]}}, icpu_rdata[31], icpu_rdata[7], icpu_rdata[30:25], icpu_rdata[11:8], 1'b0};
wire [31:0] imm_u = {icpu_rdata[31:12], 12'b0};
wire [31:0] imm_j = {{11{icpu_rdata[31]}}, icpu_rdata[31], icpu_rdata[19:12], icpu_rdata[20], icpu_rdata[30:21], 1'b0};
//
localparam ADDI_OP = 7'b0010011; 
localparam ADD_OP = 7'b0110011; 
localparam LUI_OP = 7'b0110111; 
localparam SW_OP = 7'b0100011; 
localparam LW_OP = 7'b0000011; 
localparam BEQ_OP = 7'b1100011; 
localparam JAL_OP = 7'b1101111; 
localparam JALR_OP = 7'b1100111; 
localparam AUIPC_OP = 7'b0010111; 

wire [4:0] REGS_rs1_addr = rs1_addr;
wire [4:0] REGS_rs2_addr = rs2_addr;
wire [4:0] REGS_rd_addr = rd_addr;
wire [31:0] REGS_rs1_data;
wire [31:0] REGS_rs2_data;
reg [31:0] REGS_rd_data;
reg REGS_rd_we;


reg [31:0] rs1_reg;
reg [31:0] rs2_reg;
reg [31:0] pc_branch_target;
wire [31:0] store_addr = rs1_reg + imm_s; // SW
wire [31:0] load_addr = rs1_reg + imm_i; // LW

reg [1:0] state;
localparam FETCH = 0;
localparam DECODE = 1;
localparam EXECUTE = 2;
localparam MEM = 3;

always @(posedge sys_clk) begin
if(!nrst) begin
    state <= FETCH;
    icpu_valid <= 1;
    PC <= 32'h8000_0000; // rom
    REGS_rd_we <= 0;
    dcpu_valid <= 0;
    dcpu_addr <= 32'd0;
    dcpu_wdata <= 32'd0;
    dcpu_wstrb <= 4'd0;
    led <= 6'b111111;
    rs1_reg <= 32'd0;
    rs2_reg <= 32'd0;
    pc_branch_target <= 32'd0;
end else begin
    case(state)
        FETCH: begin
            REGS_rd_we <= 0;
            if(icpu_ready) begin
                icpu_valid <= 0;
                state <= DECODE; // got opcode, let comb logic pull rs1 rs2 vals
            end

        end

        DECODE: begin
            pc_branch_target <= PC + imm_b;
            rs1_reg <= REGS_rs1_data;
            rs2_reg <= REGS_rs2_data;
            state <= EXECUTE;
        end

        EXECUTE: begin
            REGS_rd_we <= 1;
            icpu_valid <= 1;
            PC <= PC + 4;
            state <= FETCH;
            case(opcode)
                ADDI_OP: begin
                    case(funct3)
                        3'b000: REGS_rd_data <= rs1_reg + imm_i; // ADDI
                        3'b010: REGS_rd_data <= $signed(rs1_reg) < $signed(imm_i)? 1:0;// SLTI
                        3'b011: REGS_rd_data <= rs1_reg < imm_i? 1:0; // SLTIU
                        3'b100: REGS_rd_data <= rs1_reg ^ imm_i; // XORI
                        3'b001: REGS_rd_data <= rs1_reg << rs2_addr; // SLLI
                        3'b101: REGS_rd_data <= funct7[5] ? ($signed(rs1_reg) >>> rs2_addr) : (rs1_reg >> rs2_addr); // SRAI / SRLI
                        3'b110: REGS_rd_data <= rs1_reg | imm_i; // ORI
                        3'b111: REGS_rd_data <= rs1_reg & imm_i; // ANDI
                        default: REGS_rd_data <= 0;
                    endcase

                end

                SW_OP: begin
                    REGS_rd_we <= 0;
                    if(store_addr[31:30] == 2'b11) begin // 0xC000_0000 - MMIO
                        led <= rs2_reg[5:0];
                    end else begin // mem
                        PC <= PC; // stall for write
                        state <= MEM;
                        dcpu_valid <= 1'b1;
                        icpu_valid <= 1'b0;
                        dcpu_addr <= {store_addr[31:2], 2'd0};
                        case(funct3)
                            3'b000: begin // SB
                                dcpu_wdata <= {4{rs2_reg[7:0]}};
                                dcpu_wstrb <= 4'b0001 << store_addr[1:0];
                            end
                            3'b001: begin // SH
                                dcpu_wdata <= {2{rs2_reg[15:0]}};
                                dcpu_wstrb <= store_addr[1]? 4'b1100:4'b0011;
                            end
                            3'b010: begin // SW
                                dcpu_wdata <= rs2_reg;
                                dcpu_wstrb <= 4'b1111;
                            end
                        endcase
                    end
                end
                LW_OP: begin
                    REGS_rd_we <= 0;
                    if(load_addr[31:30] == 2'b11) begin // 0xC000_0000 - MMIO
                        // led <= rs2_reg[5:0];
                    end else begin // mem
                        PC <= PC; // stall for read
                        state <= MEM;
                        dcpu_valid <= 1'b1;
                        icpu_valid <= 1'b0;
                        dcpu_addr <= {load_addr[31:2], 2'd0};
                        dcpu_wstrb <= 4'd0;
                    end
                end
                LUI_OP: begin
                    REGS_rd_data <= imm_u;
                end
                AUIPC_OP: begin
                    REGS_rd_data <= PC + imm_u;
                end
                ADD_OP: begin
                    case(funct3)
                        3'b000: REGS_rd_data <= funct7[5] ? (rs1_reg - rs2_reg) : (rs1_reg + rs2_reg); // SUB / ADD
                        3'b001: REGS_rd_data <= rs1_reg << rs2_reg[4:0]; // SLL
                        3'b010: REGS_rd_data <= ($signed(rs1_reg) < $signed(rs2_reg)) ? 32'd1 : 32'd0; // SLT
                        3'b011: REGS_rd_data <= (rs1_reg < rs2_reg) ? 32'd1 : 32'd0; // SLTU
                        3'b100: REGS_rd_data <= rs1_reg ^ rs2_reg; // XOR
                        3'b101: REGS_rd_data <= funct7[5] ? ($signed(rs1_reg) >>> rs2_reg[4:0]) : (rs1_reg >> rs2_reg[4:0]); // SRA / SRL
                        3'b110: REGS_rd_data <= rs1_reg | rs2_reg; // OR
                        3'b111: REGS_rd_data <= rs1_reg & rs2_reg; // AND
                        default: REGS_rd_data <= 32'd0;
                    endcase
                end
                JAL_OP: begin
                    PC <= imm_j + PC;
                    REGS_rd_data <= PC + 32'd4;
                end
                JALR_OP: begin
                    PC <= (rs1_reg + imm_i) & ~32'd1;
                    REGS_rd_data <= PC + 32'd4;
                end
                BEQ_OP: begin
                    REGS_rd_we <= 0;
                    case(funct3)
                        3'b000: if (rs1_reg == rs2_reg) PC <= pc_branch_target; // BEQ
                        3'b001: if (rs1_reg != rs2_reg) PC <= pc_branch_target; // BNE
                        3'b100: if ($signed(rs1_reg) < $signed(rs2_reg)) PC <= pc_branch_target; // BLT
                        3'b101: if ($signed(rs1_reg) >= $signed(rs2_reg))PC <= pc_branch_target; // BGE
                        3'b110: if (rs1_reg < rs2_reg) PC <= pc_branch_target; // BLTU
                        3'b111: if (rs1_reg >= rs2_reg) PC <= pc_branch_target; // BGEU
                    endcase
                end
            endcase
        end

        MEM: begin
            if(dcpu_ready) begin // wait for mem
                dcpu_valid <= 1'b0;
                dcpu_wstrb <= 4'd0;
                PC <= PC + 4; // resume
                icpu_valid <= 1'b1;
                state <= FETCH;
                if(opcode == LW_OP) begin
                    REGS_rd_we <= 1;
                    case(funct3)
                        3'b000: begin // LB
                            case(load_addr[1:0])
                                2'd0: REGS_rd_data <= {{24{dcpu_rdata[7]}},  dcpu_rdata[7:0]};
                                2'd1: REGS_rd_data <= {{24{dcpu_rdata[15]}}, dcpu_rdata[15:8]};
                                2'd2: REGS_rd_data <= {{24{dcpu_rdata[23]}}, dcpu_rdata[23:16]};
                                2'd3: REGS_rd_data <= {{24{dcpu_rdata[31]}}, dcpu_rdata[31:24]};
                            endcase
                        end
                        3'b100: begin // LBU
                            case(load_addr[1:0])
                                2'd0: REGS_rd_data <= {24'd0,  dcpu_rdata[7:0]};
                                2'd1: REGS_rd_data <= {24'd0, dcpu_rdata[15:8]};
                                2'd2: REGS_rd_data <= {24'd0, dcpu_rdata[23:16]};
                                2'd3: REGS_rd_data <= {24'd0, dcpu_rdata[31:24]};
                            endcase
                        end
                        3'b001: begin // LH
                            if(load_addr[1]) begin
                                REGS_rd_data <= {{16{dcpu_rdata[31]}}, dcpu_rdata[31:16]};
                            end else begin
                                REGS_rd_data <= {{16{dcpu_rdata[15]}}, dcpu_rdata[15:0]};
                            end
                        end
                        3'b101: begin // LHU
                            if(load_addr[1]) begin
                                REGS_rd_data <= {16'd0, dcpu_rdata[31:16]};
                            end else begin
                                REGS_rd_data <= {16'd0, dcpu_rdata[15:0]};
                            end
                        end
                        default: begin REGS_rd_data <= dcpu_rdata; end
                    endcase
                end
            end
        end
    endcase
end
end

registers registers_file (
    .clk(sys_clk),
    .rs1_addr(REGS_rs1_addr),
    .rs2_addr(REGS_rs2_addr),
    .rs1_data(REGS_rs1_data),
    .rs2_data(REGS_rs2_data),
    .rd_addr(REGS_rd_addr),
    .rd_data(REGS_rd_data),
    .rd_we(REGS_rd_we)
);

endmodule