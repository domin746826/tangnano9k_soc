// divider.v - Radix-4 z rozcieta sciezka krytyczna w IDLE (Timing-Friendly!)
module divider (
    input  wire        clk,
    input  wire        start,
    input  wire        is_signed,
    input  wire [31:0] a,
    input  wire [31:0] b,
    output reg  [31:0] quo,
    output reg  [31:0] rem,
    output reg         done
);

localparam IDLE = 2'd0;
localparam PREP = 2'd1; // Dodatkowy takt rozcinajacy sciezke rs2_reg -> abs_b -> cmp!
localparam RUN  = 2'd2;
localparam FIN  = 2'd3;

reg [1:0]  state = IDLE;
reg [4:0]  cnt;
reg [31:0] num;
reg [31:0] den;
reg [33:0] den_x3;
reg [31:0] r;
reg        neg_q, neg_r, by_zero;
reg [31:0] a_orig;

wire a_neg = is_signed & a[31];
wire b_neg = is_signed & b[31];

// Radix-4: wciagamy 2 bity dzielnej naraz
wire [33:0] r_sh   = {r, num[31:30]};
wire [33:0] den_x1 = {2'b00, den};
wire [33:0] den_x2 = {1'b0, den, 1'b0};

wire [34:0] sub1 = {1'b0, r_sh} - {1'b0, den_x1};
wire [34:0] sub2 = {1'b0, r_sh} - {1'b0, den_x2};
wire [34:0] sub3 = {1'b0, r_sh} - {1'b0, den_x3};

wire ge1 = ~sub1[34];
wire ge2 = ~sub2[34];
wire ge3 = ~sub3[34];

wire [1:0]  q_bits = ge3 ? 2'd3 : (ge2 ? 2'd2 : (ge1 ? 2'd1 : 2'd0));
wire [31:0] r_next = ge3 ? sub3[31:0] : (ge2 ? sub2[31:0] : (ge1 ? sub1[31:0] : r_sh[31:0]));

initial begin quo = 32'd0; rem = 32'd0; done = 1'b0; end

always @(posedge clk) begin
    done <= 1'b0;
    case (state)
        IDLE: begin
            if (start) begin
                // W IDLE robimy TYLKO pojedyncza negacje U2 bez kaskadowych porownan!
                a_orig  <= a;
                by_zero <= (b == 32'd0);
                neg_q   <= a_neg ^ b_neg;
                neg_r   <= a_neg;
                num     <= a_neg ? (32'd0 - a) : a;
                den     <= b_neg ? (32'd0 - b) : b;
                r       <= 32'd0;
                state   <= PREP;
            end
        end

        PREP: begin
            // Tutaj num (|a|) i den (|b|) sa juz w rejestrach FF -> super krotka sciezka!
            den_x3 <= {2'b00, den} + {1'b0, den, 1'b0};
            if (by_zero || num < den) begin
                r     <= num;
                num   <= 32'd0;
                state <= FIN;
            end else if (num[31:8] == 24'd0) begin
                num   <= {num[7:0], 24'd0};
                cnt   <= 5'd4;
                state <= RUN;
            end else if (num[31:16] == 16'd0) begin
                num   <= {num[15:0], 16'd0};
                cnt   <= 5'd8;
                state <= RUN;
            end else if (num[31:24] == 8'd0) begin
                num   <= {num[23:0], 8'd0};
                cnt   <= 5'd12;
                state <= RUN;
            end else begin
                cnt   <= 5'd16;
                state <= RUN;
            end
        end

        RUN: begin
            num <= {num[29:0], q_bits};
            r   <= r_next;
            cnt <= cnt - 5'd1;
            if (cnt == 5'd1) state <= FIN;
        end

        FIN: begin
            if (by_zero) begin
                quo <= 32'hFFFF_FFFF;
                rem <= a_orig;
            end else begin
                quo <= neg_q ? (32'd0 - num) : num;
                rem <= neg_r ? (32'd0 - r)   : r;
            end
            done  <= 1'b1;
            state <= IDLE;
        end
    endcase
end

endmodule