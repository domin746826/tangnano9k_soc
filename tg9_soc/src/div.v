// divider.v - szybki dzielnik 32-bit (Radix-4 + Zero-Skip + Fast Bypass)
// Porty w 100% kompatybilne z poprzednia wersja!
module divider (
    input  wire        clk,
    input  wire        start,
    input  wire        is_signed,
    input  wire [31:0] a,          // dzielna
    input  wire [31:0] b,          // dzielnik
    output reg  [31:0] quo,
    output reg  [31:0] rem,
    output reg         done
);

localparam IDLE = 2'd0;
localparam RUN  = 2'd1;
localparam FIN  = 2'd2;

reg [1:0]  state = IDLE;
reg [4:0]  cnt;            // licznik par bitow (max 16 krokow zamiast 32!)
reg [31:0] num;            // |a|, przesuwany o 2 bity/takt, od dolu wsuwamy iloraz
reg [31:0] den;            // 1 * |b|
reg [33:0] den_x3;         // 3 * |b| (policzone raz na starcie!)
reg [31:0] r;              // reszta robocza
reg        neg_q, neg_r, by_zero;
reg [31:0] a_orig;

// Wartosci bezwzgledne liczone od razu w IDLE przy starcie
wire a_neg = is_signed & a[31];
wire b_neg = is_signed & b[31];
wire [31:0] abs_a = a_neg ? (32'd0 - a) : a;
wire [31:0] abs_b = b_neg ? (32'd0 - b) : b;

// Radix-4: wciagamy 2 bity dzielnej naraz
wire [33:0] r_sh   = {r, num[31:30]};
wire [33:0] den_x1 = {2'b00, den};
wire [33:0] den_x2 = {1'b0, den, 1'b0};

// 3 rownolegle komparatory/subtraktory (brak kaskady -> wysoki Fmax!)
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
                a_orig  <= a;
                by_zero <= (b == 32'd0);
                neg_q   <= a_neg ^ b_neg;
                neg_r   <= a_neg;
                den     <= abs_b;
                den_x3  <= {2'b00, abs_b} + {1'b0, abs_b, 1'b0}; // 3 * |b|

                // Szybka sciezka (Fast-Path):
                if (b == 32'd0 || abs_a < abs_b) begin
                    // Dzielenie przez 0 lub |a| < |b| -> wynik znamy od razu!
                    num   <= 32'd0;
                    r     <= abs_a;
                    state <= FIN;
                end else if (abs_a[31:8] == 24'd0) begin
                    // Liczba 8-bitowa (0..255) -> tylko 4 kroki Radix-4!
                    num   <= {abs_a[7:0], 24'd0};
                    r     <= 32'd0;
                    cnt   <= 5'd4;
                    state <= RUN;
                end else if (abs_a[31:16] == 16'd0) begin
                    // Liczba 16-bitowa (0..65535) -> tylko 8 krokow Radix-4!
                    num   <= {abs_a[15:0], 16'd0};
                    r     <= 32'd0;
                    cnt   <= 5'd8;
                    state <= RUN;
                end else if (abs_a[31:24] == 8'd0) begin
                    // Liczba 24-bitowa -> 12 krokow Radix-4!
                    num   <= {abs_a[23:0], 8'd0};
                    r     <= 32'd0;
                    cnt   <= 5'd12;
                    state <= RUN;
                end else begin
                    // Pelna liczba 32-bitowa -> 16 krokow Radix-4 (zamiast 32!)
                    num   <= abs_a;
                    r     <= 32'd0;
                    cnt   <= 5'd16;
                    state <= RUN;
                end
            end
        end

        RUN: begin
            num <= {num[29:0], q_bits}; // wsuwamy 2 bity ilorazu naraz
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

        default: state <= IDLE;
    endcase
end

endmodule