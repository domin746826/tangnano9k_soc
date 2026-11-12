// module lcd_driver (
//     input clk_sys,
//     output reg [15:0] color_port,
//     output reg hsync,
//     output reg vsync,
//     output reg de,
//     output wire lcd_clk,

//     // lcd port (ro)
//     input wire LCD_req_valid,
//     output reg LCD_req_ready,
//     input wire [20:0] LCD_req_addr,
//     output reg [63:0] LCD_read_data
// );

// // --- TIMINGI 480x272 @ 60Hz, 9MHz ---
// localparam H_ACTIVE = 480;
// localparam H_TOTAL  = 525;
// localparam H_FP     = 2;
// localparam H_SYNC   = 41;

// localparam V_ACTIVE = 272;
// localparam V_TOTAL  = 286;
// localparam V_FP     = 2;
// localparam V_SYNC   = 10;

// // --- LCD DOMAIN REGISTERS ---
// reg [9:0] h_counter = 0;
// reg [8:0] v_counter = 0;
// reg [8:0] vram_rd_ptr = 0;
// wire [15:0] render_color_data;

// // --- PSRAM DOMAIN REGISTERS ---
// reg [8:0] vram_wr_ptr = 0;
// reg [15:0] user_data_in;
// reg [8:0] user_addr;
// reg user_we;

// // --- CDC SYNCHRONIZATION ---
// reg [8:0] rd_ptr_sync1, rd_ptr_sync2;
// always @(posedge psram_clk) begin
//     rd_ptr_sync1 <= vram_rd_ptr;
//     rd_ptr_sync2 <= rd_ptr_sync1;
// end

// wire [9:0] buffer_fill_level;
// assign buffer_fill_level = (vram_wr_ptr >= rd_ptr_sync2) ? 
//                            (vram_wr_ptr - rd_ptr_sync2) : 
//                            (10'd512 - rd_ptr_sync2 + vram_wr_ptr);

// // --- VRAM INSTANTIATION ---
// vram vram_512w_buffer (
//     .render_data(render_color_data),
//     .render_addr(vram_rd_ptr),
//     .render_clk(clk_sys),
//     .user_data_in(user_data_in),
//     .user_data_out(),
//     .user_we(user_we),
//     .user_addr(user_addr),
//     .user_clk(psram_clk)
// );

// assign lcd_clk = clk_sys;

// // --- PSRAM FETCHING STATE MACHINE (81 MHz) ---
// reg [2:0] state = 0;
// localparam S_IDLE = 0, S_CMD = 1, S_WAIT_DATA = 2;
// reg [8:0] burst_cnt;
// reg [17:0] total_pixels_written; 

// always @(posedge psram_clk) begin
//     // Default pulses
//     cmd_valid <= 0;
//     user_we <= 0;
    
//     // Reset adresu po każdej ramce (prosty licznik)
//     if (total_pixels_written >= (H_ACTIVE * V_ACTIVE)) begin
//         addr <= 0;
//         total_pixels_written <= 0;
//     end

//     case(state)
//         S_IDLE: begin
//             // Jeśli mamy miejsce w buforze na 256 pixeli, pobieramy
//             if (buffer_fill_level <= 200) begin // Margines bezpieczeństwa
//                 state <= S_CMD;
//             end
//         end
        
//         S_CMD: begin
//             // Czekamy aż kontroler będzie gotowy przyjąć rozkaz
//             if (cmd_ready) begin
//                 cmd_valid <= 1;
//                 cmd_write <= 0; // READ
//                 // addr jest ustawiany dynamicznie
//                 burst_next <= 1;
//                 burst_cnt <= 0;
//                 state <= S_WAIT_DATA;
//             end
//         end
        
//         S_WAIT_DATA: begin
//             if (rd_valid) begin
//                 // Otrzymaliśmy dane z PSRAM!
//                 user_we <= 1;
//                 user_addr <= vram_wr_ptr;
                
//                 // DEBUG: Jeśli dane to 0000 (czarny), wstaw zieloną kropkę co 32px
//                 if (rd_data == 0 && burst_cnt[4:0] == 0)
//                     user_data_in <= 16'h07E0; // Zielony
//                 else
//                     user_data_in <= rd_data;

//                 // Inkrementacja FIFO
//                 vram_wr_ptr <= vram_wr_ptr + 1;
//                 burst_cnt <= burst_cnt + 1;
//                 total_pixels_written <= total_pixels_written + 1;

//                 // Kontrola burstu (256 słów)
//                 if (burst_cnt == 255) begin
//                     burst_next <= 0;
//                     state <= S_IDLE;
//                     addr <= addr + 22'd512; // +512 bajtów (256 słów)
//                 end
//             end
//         end
//     endcase
// end

// // --- LCD TIMING GENERATOR (9 MHz) ---
// always @(posedge clk_sys) begin
//     if (h_counter < H_TOTAL - 1) h_counter <= h_counter + 1;
//     else begin
//         h_counter <= 0;
//         if (v_counter < V_TOTAL - 1) v_counter <= v_counter + 1;
//         else v_counter <= 0;
//     end

//     // Sync
//     hsync <= ~((h_counter >= H_ACTIVE + H_FP) && (h_counter < H_ACTIVE + H_FP + H_SYNC));
//     vsync <= ~((v_counter >= V_ACTIVE + V_FP) && (v_counter < V_ACTIVE + V_FP + V_SYNC));
    
//     // Data Enable & Read
//     if (h_counter < H_ACTIVE && v_counter < V_ACTIVE) begin
//         de <= 1;
//         color_port <= render_color_data; // Czytamy z VRAM
//         vram_rd_ptr <= vram_rd_ptr + 1;
//     end else begin
//         de <= 0;
//         color_port <= 0;
//     end
    
//     // Synchronizacja wskaźników na VSYNC
//     if (v_counter == V_TOTAL-1 && h_counter == H_TOTAL-1) begin
//          vram_rd_ptr <= 0; 
//     end
// end

// endmodule

// lcd_driver - 480x272 RGB565, framebuffer w PSRAM, odczyt przez arbiter (port LCD, ro)
//
// Protokol jak w rwcache (strona "klienta" arbitra / wrappera):
//   - ustaw LCD_req_valid + LCD_req_addr (adres w slowach 32-bit, wyrownany do 16 = 64 B)
//   - wrapper po przyjeciu zlecenia daje 8x LCD_req_ready; w kazdym takcie z ready
//     LCD_read_data niesie kolejny qword (beat 0 = najnizszy adres)
//   - valid zdejmujemy przy pierwszym ready (arbiter wraca do IDLE na opadajacym zboczu ready)
//
// Piksel 0 burstu = LCD_read_data[15:0], piksel 3 = [63:48] (little endian jak CPU).
// 1 burst = 32 piksele, linia = 15 burstow, ramka = 4080 burstow = 261120 B.
//
// Domeny zegarow:
//   sys_clk  - ten sam co arbiter / psram_wrapper
//   clk_lcd  - zegar pikseli (9 MHz)
// Miedzy domenami idzie tylko: licznik zuzytych burstow (Gray, 2FF) i flaga "primed".

module lcd_driver #(
    parameter [20:0] FB_BASE = 21'h1F0000   // slowo 32-bit => bajt 0x7C0000, framebuffer konczy sie tuz przed 8 MB
)(
    input  wire        nrst,

    // domena pamieci
    input  wire        sys_clk,

    // domena LCD
    input  wire        clk_lcd,
    output reg  [15:0] color_port,
    output reg         hsync,
    output reg         vsync,
    output reg         de,
    output wire        lcd_clk,

    // port do arbitra (ro)
    output reg         LCD_req_valid,
    input  wire        LCD_req_ready,
    output reg  [20:0] LCD_req_addr,
    input  wire [63:0] LCD_read_data
);

// --- TIMINGI 480x272 @ 60Hz, 9MHz ---
localparam H_ACTIVE = 480;
localparam H_TOTAL  = 525;
localparam H_FP     = 2;
localparam H_SYNC   = 41;

localparam V_ACTIVE = 272;
localparam V_TOTAL  = 286;
localparam V_FP     = 2;
localparam V_SYNC   = 10;

localparam BURSTS_PER_FRAME = (H_ACTIVE * V_ACTIVE) / 32;   // 4080 (= 16 * 255, wiec slot ringu trzyma sie ramki)
localparam PRIME_BURSTS     = 8;                            // tyle burstow w buforze zanim ruszy obraz

assign lcd_clk = clk_lcd;

// ---------------------------------------------------------------
// bufor pierscieniowy: 16 burstow x 8 qwordow = 128 x 64 bit (512 pikseli)
// zapis sys_clk (64 bit), odczyt clk_lcd (64 bit, pixel wybierany muxem)
// ---------------------------------------------------------------
reg [63:0] vram [0:127];

// ===============================================================
// DOMENA LCD - rejestry wspolne (potrzebne w CDC)
// ===============================================================
reg [4:0] rd_cnt;    // zuzyte burstow (mod 32)
reg [4:0] rd_gray;   // to samo w Gray

// ===============================================================
// DOMENA PAMIECI (sys_clk)
// ===============================================================
localparam S_IDLE = 1'd0;
localparam S_XFER = 1'd1;

reg        state;
reg [4:0]  wr_cnt;        // zapisane bursty (mod 32)
reg [2:0]  beat;
reg [11:0] frame_burst;
reg [20:0] fb_addr;
reg [2:0]  cool;          // krotka przerwa po burscie, zeby arbiter zdazyl wrocic do IDLE
reg        primed;

reg [4:0] rd_gray_s1, rd_gray_s2;
always @(posedge sys_clk) begin
    rd_gray_s1 <= rd_gray;
    rd_gray_s2 <= rd_gray_s1;
end

wire [4:0] rd_bin;
assign rd_bin[4] = rd_gray_s2[4];
assign rd_bin[3] = rd_bin[4] ^ rd_gray_s2[3];
assign rd_bin[2] = rd_bin[3] ^ rd_gray_s2[2];
assign rd_bin[1] = rd_bin[2] ^ rd_gray_s2[1];
assign rd_bin[0] = rd_bin[1] ^ rd_gray_s2[0];

wire [4:0] fill = wr_cnt - rd_bin;     // 0..16 burstow w buforze (opoznione CDC => zachowawczo)

wire       vram_we    = (state == S_XFER) && LCD_req_ready;
wire [6:0] vram_waddr = {wr_cnt[3:0], beat};

always @(posedge sys_clk) begin
    if (vram_we)
        vram[vram_waddr] <= LCD_read_data;
end

always @(posedge sys_clk or negedge nrst) begin
    if (!nrst) begin
        state         <= S_IDLE;
        wr_cnt        <= 5'd0;
        beat          <= 3'd0;
        frame_burst   <= 12'd0;
        fb_addr       <= FB_BASE;
        cool          <= 3'd0;
        primed        <= 1'b0;
        LCD_req_valid <= 1'b0;
        LCD_req_addr  <= 21'd0;
    end else begin
        if (cool != 3'd0) cool <= cool - 3'd1;
        if (wr_cnt >= PRIME_BURSTS) primed <= 1'b1;

        case (state)
            S_IDLE: begin
                // jest wolny slot w ringu i minela przerwa po poprzednim burscie
                if (fill != 5'd16 && cool == 3'd0) begin
                    LCD_req_valid <= 1'b1;
                    LCD_req_addr  <= fb_addr;
                    beat          <= 3'd0;
                    state         <= S_XFER;
                end
            end

            S_XFER: begin
                if (LCD_req_ready) begin
                    LCD_req_valid <= 1'b0;          // pierwszy ready = zlecenie przyjete
                    beat          <= beat + 3'd1;
                    if (beat == 3'd7) begin
                        wr_cnt <= wr_cnt + 5'd1;
                        cool   <= 3'd4;
                        state  <= S_IDLE;
                        if (frame_burst == BURSTS_PER_FRAME - 1) begin
                            frame_burst <= 12'd0;
                            fb_addr     <= FB_BASE;
                        end else begin
                            frame_burst <= frame_burst + 12'd1;
                            fb_addr     <= fb_addr + 21'd16;
                        end
                    end
                end
            end
        endcase
    end
end

// ===============================================================
// DOMENA LCD (clk_lcd)
// ===============================================================
reg run_s1, run_s2;     // primed -> clk_lcd
always @(posedge clk_lcd or negedge nrst) begin
    if (!nrst) begin
        run_s1 <= 1'b0;
        run_s2 <= 1'b0;
    end else begin
        run_s1 <= primed;
        run_s2 <= run_s1;
    end
end

reg [9:0] h_cnt;
reg [8:0] v_cnt;
reg [8:0] pix_cnt;      // pozycja w ringu w pikselach (512, zawija sie dokladnie co 255 ramek*...: 130560 = 255*512)

wire active = (h_cnt < H_ACTIVE) && (v_cnt < V_ACTIVE);
wire hs_c   = ~((h_cnt >= H_ACTIVE + H_FP) && (h_cnt < H_ACTIVE + H_FP + H_SYNC));
wire vs_c   = ~((v_cnt >= V_ACTIVE + V_FP) && (v_cnt < V_ACTIVE + V_FP + V_SYNC));

wire [4:0] rd_cnt_n = rd_cnt + 5'd1;

// odczyt BSRAM (1 takt)
reg [63:0] qword_q;
always @(posedge clk_lcd) begin
    qword_q <= vram[pix_cnt[8:2]];
end

reg [1:0] lane_q;
reg       act_q, hs_q, vs_q;

reg [15:0] px;
always @(*) begin
    case (lane_q)
        2'd0:    px = qword_q[15:0];
        2'd1:    px = qword_q[31:16];
        2'd2:    px = qword_q[47:32];
        default: px = qword_q[63:48];
    endcase
end

always @(posedge clk_lcd or negedge nrst) begin
    if (!nrst) begin
        h_cnt      <= 10'd0;
        v_cnt      <= 9'd0;
        pix_cnt    <= 9'd0;
        rd_cnt     <= 5'd0;
        rd_gray    <= 5'd0;
        lane_q     <= 2'd0;
        act_q      <= 1'b0;
        hs_q       <= 1'b1;
        vs_q       <= 1'b1;
        color_port <= 16'd0;
        hsync      <= 1'b1;
        vsync      <= 1'b1;
        de         <= 1'b0;
    end else begin
        if (run_s2) begin
            // generator timingow
            if (h_cnt < H_TOTAL - 1) h_cnt <= h_cnt + 10'd1;
            else begin
                h_cnt <= 10'd0;
                if (v_cnt < V_TOTAL - 1) v_cnt <= v_cnt + 9'd1;
                else v_cnt <= 9'd0;
            end

            // konsumpcja pikseli
            if (active) begin
                pix_cnt <= pix_cnt + 9'd1;
                if (pix_cnt[4:0] == 5'd31) begin        // ostatni piksel burstu => slot wolny
                    rd_cnt  <= rd_cnt_n;
                    rd_gray <= rd_cnt_n ^ (rd_cnt_n >> 1);
                end
            end
        end

        // stopien A: adres poszedl do BSRAM, czekamy na dane
        lane_q <= pix_cnt[1:0];
        act_q  <= active && run_s2;
        hs_q   <= hs_c;
        vs_q   <= vs_c;

        // stopien B: wyjscie (wszystko opoznione tak samo)
        color_port <= act_q ? px : 16'd0;
        de         <= act_q;
        hsync      <= hs_q;
        vsync      <= vs_q;
    end
end

endmodule