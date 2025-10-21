// gdkGBA by gdkchan
// https://github.com/gdkchan/gdkGBA
// https://github.com/jakibaki/gdkGBA

/*
This is free and unencumbered software released into the public domain.

Anyone is free to copy, modify, publish, use, compile, sell, or
distribute this software, either in source code form or as a compiled
binary, for any purpose, commercial or non-commercial, and by any
means.

In jurisdictions that recognize copyright laws, the author or authors
of this software dedicate any and all copyright interest in the
software to the public domain. We make this dedication for the benefit
of the public at large and to the detriment of our heirs and
successors. We intend this dedication to be an overt act of
relinquishment in perpetuity of all present and future rights to this
software under copyright law.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR
OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
OTHER DEALINGS IN THE SOFTWARE.

For more information, please refer to <http://unlicense.org/>
*/

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/soundcard.h>
#include <time.h>

#include <sys/ioctl.h>
#include <linux/kd.h>
#include <termios.h>

// If you run this program with 'nice -n -20 ...'
// it seems to run at 45 FPS generally.
// Otherwise, keep it at 20 FPS.

int fps_rate = 20; // values: 60, 45, 30, 20
int fps_counter = 0;
int fps_graphics = 0;
int fps_audio = 0;
int fps_wait = 0;

//#include <SDL2/SDL.h>
//#include <SDL2/SDL_audio.h>

// arm.h / arm.c

#define ROR(val, s)  (((val) >> (s)) | ((val) << (32 - (s))))
#define SBIT(op)     ((op & (1 << 20)) ? 1 : 0)

#define ARM_BYTE_SZ   1
#define ARM_HWORD_SZ  2
#define ARM_WORD_SZ   4
#define ARM_DWORD_SZ  8

//PSR flags
#define ARM_N  (1 << 31) //Negative
#define ARM_Z  (1 << 30) //Zero
#define ARM_C  (1 << 29) //Carry
#define ARM_V  (1 << 28) //Overflow
#define ARM_Q  (1 << 27) //Saturation
#define ARM_A  (1 <<  8) //Abort off
#define ARM_I  (1 <<  7) //IRQ off
#define ARM_F  (1 <<  6) //FIQ off
#define ARM_T  (1 <<  5) //Thumb

//Modes
#define ARM_USR  0b10000 //User
#define ARM_FIQ  0b10001 //Fast IRQ
#define ARM_IRQ  0b10010 //IRQ
#define ARM_SVC  0b10011 //Supervisor Call
#define ARM_MON  0b10110 //Monitor
#define ARM_ABT  0b10111 //Abort
#define ARM_UND  0b11011 //Undefined
#define ARM_SYS  0b11111 //System

//Interrupt addresses
#define ARM_VEC_RESET   0x00 //Reset
#define ARM_VEC_UND     0x04 //Undefined
#define ARM_VEC_SVC     0x08 //Supervisor Call
#define ARM_VEC_PABT    0x0c //Prefetch Abort
#define ARM_VEC_DABT    0x10 //Data Abort
#define ARM_VEC_ADDR26  0x14 //Address exceeds 26 bits (legacy)
#define ARM_VEC_IRQ     0x18 //IRQ
#define ARM_VEC_FIQ     0x1c //Fast IRQ

typedef union {
    int32_t w;

    struct {
        int16_t lo;
        int16_t hi;
    } h;

    struct {
        int8_t b0;
        int8_t b1;
        int8_t b2;
        int8_t b3;
    } b;
} arm_word;

typedef struct {
    uint32_t r[16];

    uint32_t r8_usr;
    uint32_t r9_usr;
    uint32_t r10_usr;
    uint32_t r11_usr;
    uint32_t r12_usr;
    uint32_t r13_usr;
    uint32_t r14_usr;

    uint32_t r8_fiq;
    uint32_t r9_fiq;
    uint32_t r10_fiq;
    uint32_t r11_fiq;
    uint32_t r12_fiq;
    uint32_t r13_fiq;
    uint32_t r14_fiq;

    uint32_t r13_irq;
    uint32_t r14_irq;

    uint32_t r13_svc;
    uint32_t r14_svc;

    uint32_t r13_mon;
    uint32_t r14_mon;

    uint32_t r13_abt;
    uint32_t r14_abt;

    uint32_t r13_und;
    uint32_t r14_und;

    uint32_t cpsr;

    uint32_t spsr_fiq;
    uint32_t spsr_irq;
    uint32_t spsr_svc;
    uint32_t spsr_abt;
    uint32_t spsr_und;
    uint32_t spsr_mon;
} arm_regs_t;

arm_regs_t arm_r;

uint32_t arm_op;
uint32_t arm_pipe[2];
uint32_t arm_cycles;

bool int_halt;
bool pipe_reload;

void arm_init();
void arm_uninit();

void arm_exec(uint32_t target_cycles);

void arm_int(uint32_t address, int8_t mode);

void arm_check_irq();

void arm_reset();

// arm_mem.h

uint8_t *bios;
uint8_t *wram;
uint8_t *iwram;
uint8_t *pram;
uint8_t *vram;
uint8_t *oam;
uint8_t *rom;
uint8_t *eeprom;
uint8_t *sram;
uint8_t *flash;

uint32_t palette[0x200];

uint32_t bios_op;

int64_t cart_rom_size;
uint32_t cart_rom_mask;

uint16_t eeprom_idx;

typedef enum {
    NON_SEQ,
    SEQUENTIAL
} access_type_e;

uint8_t arm_readb(uint32_t address);
uint32_t arm_readh(uint32_t address);
uint32_t arm_read(uint32_t address);
uint8_t arm_readb_n(uint32_t address);
uint32_t arm_readh_n(uint32_t address);
uint32_t arm_read_n(uint32_t address);
uint8_t arm_readb_s(uint32_t address);
uint32_t arm_readh_s(uint32_t address);
uint32_t arm_read_s(uint32_t address);

void arm_writeb(uint32_t address, uint8_t value);
void arm_writeh(uint32_t address, uint16_t value);
void arm_write(uint32_t address, uint32_t value);
void arm_writeb_n(uint32_t address, uint8_t value);
void arm_writeh_n(uint32_t address, uint16_t value);
void arm_write_n(uint32_t address, uint32_t value);
void arm_writeb_s(uint32_t address, uint8_t value);
void arm_writeh_s(uint32_t address, uint16_t value);
void arm_write_s(uint32_t address, uint32_t value);


// io.h

typedef union {
    uint32_t w;

    struct {
        uint8_t b0;
        uint8_t b1;
        uint8_t b2;
        uint8_t b3;
    } b;
} io_reg;

#define VBLK_FLAG  (1 <<  0)
#define HBLK_FLAG  (1 <<  1)
#define VCNT_FLAG  (1 <<  2)
#define TMR0_FLAG  (1 <<  3)
#define TMR1_FLAG  (1 <<  4)
#define TMR2_FLAG  (1 <<  5)
#define TMR3_FLAG  (1 <<  6)
#define DMA0_FLAG  (1 <<  8)
#define DMA1_FLAG  (1 <<  9)
#define DMA2_FLAG  (1 << 10)
#define DMA3_FLAG  (1 << 11)

#define MAP_1D_FLAG  (1 <<  6)
#define BG0_ENB      (1 <<  8)
#define BG1_ENB      (1 <<  9)
#define BG2_ENB      (1 << 10)
#define BG3_ENB      (1 << 11)
#define OBJ_ENB      (1 << 12)

#define VBLK_IRQ  (1 <<  3)
#define HBLK_IRQ  (1 <<  4)
#define VCNT_IRQ  (1 <<  5)

io_reg disp_cnt;
io_reg green_inv;
io_reg disp_stat;
io_reg v_count;

typedef struct {
    io_reg ctrl;
    io_reg xofs;
    io_reg yofs;
} bg_t;

bg_t bg[4];

io_reg bg_pa[4];
io_reg bg_pb[4];
io_reg bg_pc[4];
io_reg bg_pd[4];

io_reg bg_refxe[4];
io_reg bg_refye[4];

io_reg bg_refxi[4];
io_reg bg_refyi[4];

io_reg win0_h;
io_reg win1_h;

io_reg win0_v;
io_reg win1_v;

io_reg win_in;
io_reg win_out;

io_reg bld_cnt;
io_reg bld_alpha;
io_reg bld_bright;

#define SWEEP_DEC  (1 <<  3)
#define ENV_INC    (1 << 11)
#define CH_LEN     (1 << 14)
#define WAVE_64    (1 <<  5)
#define WAVE_PLAY  (1 <<  7)
#define NOISE_7    (1 <<  3)

typedef struct {
    io_reg sweep;
    io_reg tone;
    io_reg ctrl;
} snd_sqr_ch_t;

typedef struct {
    io_reg wave;
    io_reg volume;
    io_reg ctrl;
} snd_wave_ch_t;

typedef struct {
    io_reg env;
    io_reg ctrl;
} snd_noise_ch_t;

snd_sqr_ch_t   sqr_ch[2];
snd_wave_ch_t  wave_ch;
snd_noise_ch_t noise_ch;

#define CH_SQR1_R   (1 <<  8)
#define CH_SQR2_R   (1 <<  9)
#define CH_WAVE_R   (1 << 10)
#define CH_NOISE_R  (1 << 11)
#define CH_SQR1_L   (1 << 12)
#define CH_SQR2_L   (1 << 13)
#define CH_WAVE_L   (1 << 14)
#define CH_NOISE_L  (1 << 15)
#define CH_DMAA_R   (1 <<  8)
#define CH_DMAA_L   (1 <<  9)
#define CH_DMAB_R   (1 << 12)
#define CH_DMAB_L   (1 << 13)
#define CH_SQR1     (1 <<  0)
#define CH_SQR2     (1 <<  1)
#define CH_WAVE     (1 <<  2)
#define CH_NOISE    (1 <<  3)
#define PSG_ENB     (1 <<  7)

io_reg snd_psg_vol;
io_reg snd_pcm_vol;
io_reg snd_psg_enb;
io_reg snd_bias;

uint8_t wave_ram[0x20];

int8_t snd_fifo_a_0;
int8_t snd_fifo_a_1;
int8_t snd_fifo_a_2;
int8_t snd_fifo_a_3;

int8_t snd_fifo_b_0;
int8_t snd_fifo_b_1;
int8_t snd_fifo_b_2;
int8_t snd_fifo_b_3;

typedef struct {
    io_reg src;
    io_reg dst;
    io_reg count;
    io_reg ctrl;
} dma_ch_t;

dma_ch_t dma_ch[4];

#define BTN_A    (1 << 0)
#define BTN_B    (1 << 1)
#define BTN_SEL  (1 << 2)
#define BTN_STA  (1 << 3)
#define BTN_R    (1 << 4)
#define BTN_L    (1 << 5)
#define BTN_U    (1 << 6)
#define BTN_D    (1 << 7)
#define BTN_RT   (1 << 8)
#define BTN_LT   (1 << 9)

typedef struct {
    io_reg count;
    io_reg reload;
    io_reg ctrl;
} tmr_t;

tmr_t tmr[4];

io_reg r_cnt;
io_reg sio_cnt;
io_reg sio_data8;
io_reg sio_data32;

io_reg key_input;

io_reg int_enb;
io_reg int_ack;
io_reg wait_cnt;
io_reg int_enb_m;

uint8_t ws_n[4];
uint8_t ws_s[4];

uint8_t ws_n_arm[4];
uint8_t ws_s_arm[4];

uint8_t ws_n_t16[4];
uint8_t ws_s_t16[4];

uint8_t post_boot;

bool io_open_bus;

uint8_t io_read(uint32_t address);

void io_write(uint32_t address, uint8_t value);

void trigger_irq(uint16_t flag);

void update_ws();


// sound.h / sound.c

#define CPU_FREQ_HZ       16777216
#define SND_FREQUENCY     30581 //32768
#define SND_CHANNELS      2
#define SND_SAMPLES       512
#define SAMP_CYCLES       (CPU_FREQ_HZ / SND_FREQUENCY)
#define BUFF_SAMPLES      ((SND_SAMPLES) * 16 * 2)
#define BUFF_SAMPLES_MSK  ((BUFF_SAMPLES) - 1)

int8_t fifo_a[0x20];
int8_t fifo_b[0x20];

uint8_t fifo_a_len;
uint8_t fifo_b_len;

typedef struct {
    bool     phase;       //Square 1/2 only
    uint16_t lfsr;        //Noise only
    double   samples;     //All
    double   length_time; //All
    double   sweep_time;  //Square 1 only
    double   env_time;    //All except Wave
} snd_ch_state_t;

snd_ch_state_t snd_ch_state[4];

uint8_t wave_position;
uint8_t wave_samples;

void wave_reset();

void sound_buffer_wrap();

void sound_mix(void *data, uint8_t *stream, int32_t len);
void sound_clock(uint32_t cycles);

void fifo_a_copy();
void fifo_b_copy();

void fifo_a_load();
void fifo_b_load();


#define PSG_MAX   0x7f
#define PSG_MIN  -0x80

#define SAMP_MAX   0x1ff
#define SAMP_MIN  -0x200

//How much time a single sample takes (in seconds)
#define SAMPLE_TIME  (1.0 / (SND_FREQUENCY))

static double duty_lut[4]   = { 0.125, 0.250, 0.500, 0.750 };
static double duty_lut_i[4] = { 0.875, 0.750, 0.500, 0.250 };

static int8_t square_sample(uint8_t ch) {
    if (!(snd_psg_enb.w & (CH_SQR1 << ch))) return 0;

    uint8_t  sweep_time = (sqr_ch[ch].sweep.w >>  4) & 0x7;
    uint8_t  duty       = (sqr_ch[ch].tone.w  >>  6) & 0x3;
    uint8_t  env_step   = (sqr_ch[ch].tone.w  >>  8) & 0x7;
    uint8_t  envelope   = (sqr_ch[ch].tone.w  >> 12) & 0xf;
    uint8_t  snd_len    = (sqr_ch[ch].tone.w  >>  0) & 0x3f;
    uint16_t freq_hz    = (sqr_ch[ch].ctrl.w  >>  0) & 0x7ff;

    //Actual frequency in Hertz
    double frequency = 131072 / (2048 - freq_hz);

    //Full length of the generated wave (if enabled) in seconds
    double length = (64 - snd_len) / 256.0;

    //Frquency sweep change interval in seconds
    double sweep_interval = 0.0078 * (sweep_time + 1);

    //Envelope volume change interval in seconds
    double envelope_interval = env_step / 64.0;

    //Numbers of samples that a single cycle (wave phase change 1 -> 0) takes at output sample rate
    double cycle_samples = SND_FREQUENCY / frequency;

    //Length reached check (if so, just disable the channel and return silence)
    if (sqr_ch[ch].ctrl.w & CH_LEN) {
        snd_ch_state[ch].length_time += SAMPLE_TIME;

        if (snd_ch_state[ch].length_time >= length) {
            //Disable channel
            snd_psg_enb.w &= ~(CH_SQR1 << ch);

            //And return silence
            return 0;
        }
    }

    //Frequency sweep (Square 1 channel only)
    if (ch == 0) {
        snd_ch_state[0].sweep_time += SAMPLE_TIME;

        if (snd_ch_state[0].sweep_time >= sweep_interval) {
            snd_ch_state[0].sweep_time -= sweep_interval;

            //A Sweep Shift of 0 means that Sweep is disabled
            uint8_t sweep_shift = sqr_ch[0].sweep.w & 7;

            if (sweep_shift) {
                uint32_t disp = freq_hz >> sweep_shift;

                if (sqr_ch[0].sweep.w & SWEEP_DEC)
                    freq_hz -= disp;
                else
                    freq_hz += disp;

                if (freq_hz < 0x7ff) {
                    //Update frequency
                    sqr_ch[0].ctrl.w &= ~0x7ff;
                    sqr_ch[0].ctrl.w |= freq_hz;
                } else {
                    //Disable channel
                    snd_psg_enb.w &= ~CH_SQR1;
                }
            }
        }
    }

    //Envelope volume
    if (env_step) {
        snd_ch_state[ch].env_time += SAMPLE_TIME;

        if (snd_ch_state[ch].env_time >= envelope_interval) {
            snd_ch_state[ch].env_time -= envelope_interval;

            if (sqr_ch[ch].tone.w & ENV_INC) {
                if (envelope < 0xf) envelope++;
            } else {
                if (envelope > 0x0) envelope--;
            }

            sqr_ch[ch].tone.w &= ~0xf000;
            sqr_ch[ch].tone.w |= envelope << 12;
        }
    }

    //Phase change (when the wave goes from Low to High or High to Low, the Square Wave pattern)
    snd_ch_state[ch].samples++;

    if (snd_ch_state[ch].phase) {
        //1 -> 0
        double phase_change = cycle_samples * duty_lut[duty];

        if (snd_ch_state[ch].samples >  phase_change) {
            snd_ch_state[ch].samples -= phase_change;
            snd_ch_state[ch].phase = false;
        }
    } else {
        //0 -> 1
        double phase_change = cycle_samples * duty_lut_i[duty];

        if (snd_ch_state[ch].samples >  phase_change) {
            snd_ch_state[ch].samples -= phase_change;
            snd_ch_state[ch].phase = true;
        }
    }

    return snd_ch_state[ch].phase
        ? (envelope / 15.0) * PSG_MAX
        : (envelope / 15.0) * PSG_MIN;
}

static int8_t wave_sample() {
    if (!((snd_psg_enb.w & CH_WAVE) && (wave_ch.wave.w & WAVE_PLAY))) return 0;

    uint8_t  snd_len = (wave_ch.volume.w >>  0) & 0xff;
    uint8_t  volume  = (wave_ch.volume.w >> 13) & 0x7;
    uint16_t freq_hz = (wave_ch.ctrl.w   >>  0) & 0x7ff;

    //Actual frequency in Hertz
    double frequency = 2097152 / (2048 - freq_hz);

    //Full length of the generated wave (if enabled) in seconds
    double length = (256 - snd_len) / 256.0;

    //Numbers of samples that a single "cycle" (all entries on Wave RAM) takes at output sample rate
    double cycle_samples = SND_FREQUENCY / frequency;

    //Length reached check (if so, just disable the channel and return silence)
    if (wave_ch.ctrl.w & CH_LEN) {
        snd_ch_state[2].length_time += SAMPLE_TIME;

        if (snd_ch_state[2].length_time >= length) {
            //Disable channel
            snd_psg_enb.w &= ~CH_WAVE;

            //And return silence
            return 0;
        }
    }

    snd_ch_state[2].samples++;

    if (snd_ch_state[2].samples >= cycle_samples) {
        snd_ch_state[2].samples -= cycle_samples;

        if (--wave_samples)
            wave_position = (wave_position + 1) & 0x3f;
        else
            wave_reset();
    }

    int8_t samp = wave_position & 1
        ? ((wave_ram[(wave_position >> 1) & 0x1f] >> 0) & 0xf) - 8
        : ((wave_ram[(wave_position >> 1) & 0x1f] >> 4) & 0xf) - 8;

    switch (volume) {
        case 0: samp   = 0; break; //Mute
        case 1: samp >>= 0; break; //100%
        case 2: samp >>= 1; break; //50%
        case 3: samp >>= 2; break; //25%
        default: samp = (samp >> 2) * 3; break; //75%
    }

    return samp >= 0
        ? (samp /  7.0) * PSG_MAX
        : (samp / -8.0) * PSG_MIN;
}

static int8_t noise_sample() {
    if (!(snd_psg_enb.w & CH_NOISE)) return 0;

    uint8_t env_step = (noise_ch.env.w  >>  8) & 0x7;
    uint8_t envelope = (noise_ch.env.w  >> 12) & 0xf;
    uint8_t snd_len  = (noise_ch.env.w  >>  0) & 0x3f;
    uint8_t freq_div = (noise_ch.ctrl.w >>  0) & 0x7;
    uint8_t freq_rsh = (noise_ch.ctrl.w >>  4) & 0xf;

    //Actual frequency in Hertz
    double frequency = freq_div
        ? (524288 / freq_div) >> (freq_rsh + 1)
        : (524288 *        2) >> (freq_rsh + 1);

    //Full length of the generated wave (if enabled) in seconds
    double length = (64 - snd_len) / 256.0;

    //Envelope volume change interval in seconds
    double envelope_interval = env_step / 64.0;

    //Numbers of samples that a single cycle (pseudo-random noise value) takes at output sample rate
    double cycle_samples = SND_FREQUENCY / frequency;

    //Length reached check (if so, just disable the channel and return silence)
    if (noise_ch.ctrl.w & CH_LEN) {
        snd_ch_state[3].length_time += SAMPLE_TIME;

        if (snd_ch_state[3].length_time >= length) {
            //Disable channel
            snd_psg_enb.w &= ~CH_NOISE;

            //And return silence
            return 0;
        }
    }

    //Envelope volume
    if (env_step) {
        snd_ch_state[3].env_time += SAMPLE_TIME;

        if (snd_ch_state[3].env_time >= envelope_interval) {
            snd_ch_state[3].env_time -= envelope_interval;

            if (noise_ch.env.w & ENV_INC) {
                if (envelope < 0xf) envelope++;
            } else {
                if (envelope > 0x0) envelope--;
            }

            noise_ch.env.w &= ~0xf000;
            noise_ch.env.w |= envelope << 12;
        }
    }

    uint8_t carry = snd_ch_state[3].lfsr & 1;

    snd_ch_state[3].samples++;

    if (snd_ch_state[3].samples >= cycle_samples) {
        snd_ch_state[3].samples -= cycle_samples;

        snd_ch_state[3].lfsr >>= 1;

        uint8_t high = (snd_ch_state[3].lfsr & 1) ^ carry;

        if (noise_ch.ctrl.w & NOISE_7)
            snd_ch_state[3].lfsr |= (high <<  6);
        else
            snd_ch_state[3].lfsr |= (high << 14);
    }

    return carry
        ? (envelope / 15.0) * PSG_MAX
        : (envelope / 15.0) * PSG_MIN;
}

int16_t snd_buffer[BUFF_SAMPLES];

uint32_t snd_cur_play  = 0;
uint32_t snd_cur_write = 0x200;

void wave_reset() {
    if (wave_ch.wave.w & WAVE_64) {
        //64 samples (at 4 bits each, uses both banks so initial position is always 0)
        wave_position = 0;
        wave_samples  = 64;
    } else {
        //32 samples (at 4 bits each, bank selectable through Wave Control register)
        wave_position = (wave_ch.wave.w >> 1) & 0x20;
        wave_samples  = 32;
    }
}

void sound_buffer_wrap() {
    /*
     * This prevents the cursor from overflowing
     * Call after some time (like per frame, or per second...)
     */
    if ((snd_cur_play / BUFF_SAMPLES) == (snd_cur_write / BUFF_SAMPLES)) {
        snd_cur_play  &= BUFF_SAMPLES_MSK;
        snd_cur_write &= BUFF_SAMPLES_MSK;
    }
}

void sound_mix(void *data, uint8_t *stream, int32_t len) {
    uint16_t i;

    for (i = 0; i < len; i += 4) {
        *(int16_t *)(stream + (i | 0)) = snd_buffer[snd_cur_play++ & BUFF_SAMPLES_MSK] << 6;
        *(int16_t *)(stream + (i | 2)) = snd_buffer[snd_cur_play++ & BUFF_SAMPLES_MSK] << 6;
    }

    //Avoid desync between the Play cursor and the Write cursor
    snd_cur_play += ((int32_t)(snd_cur_write - snd_cur_play) >> 8) & ~1;
}

void fifo_a_copy() {
    if (fifo_a_len + 4 > 0x20) return; //FIFO A full

    fifo_a[fifo_a_len++] = snd_fifo_a_0;
    fifo_a[fifo_a_len++] = snd_fifo_a_1;
    fifo_a[fifo_a_len++] = snd_fifo_a_2;
    fifo_a[fifo_a_len++] = snd_fifo_a_3;
}

void fifo_b_copy() {
    if (fifo_b_len + 4 > 0x20) return; //FIFO B full

    fifo_b[fifo_b_len++] = snd_fifo_b_0;
    fifo_b[fifo_b_len++] = snd_fifo_b_1;
    fifo_b[fifo_b_len++] = snd_fifo_b_2;
    fifo_b[fifo_b_len++] = snd_fifo_b_3;
}

int8_t fifo_a_samp;
int8_t fifo_b_samp;

void fifo_a_load() {
    if (fifo_a_len) {
        fifo_a_samp = fifo_a[0];
        fifo_a_len--;

        uint8_t i;

        for (i = 0; i < fifo_a_len; i++) {
            fifo_a[i] = fifo_a[i + 1];
        }
    }
}

void fifo_b_load() {
    if (fifo_b_len) {
        fifo_b_samp = fifo_b[0];
        fifo_b_len--;

        uint8_t i;

        for (i = 0; i < fifo_b_len; i++) {
            fifo_b[i] = fifo_b[i + 1];
        }
    }
}

uint32_t snd_cycles = 0;

static int32_t psg_vol_lut[8] = { 0x000, 0x024, 0x049, 0x06d, 0x092, 0x0b6, 0x0db, 0x100 };
static int32_t psg_rsh_lut[4] = { 0xa, 0x9, 0x8, 0x7 };

static int16_t clip(int32_t value) {
    if (value > SAMP_MAX) value = SAMP_MAX;
    if (value < SAMP_MIN) value = SAMP_MIN;

    return value;
}

void sound_clock(uint32_t cycles) {
    snd_cycles += cycles;

    int16_t samp_pcm_l = 0;
    int16_t samp_pcm_r = 0;

    int16_t samp_ch4 = (fifo_a_samp << 1) >> !(snd_pcm_vol.w & 4);
    int16_t samp_ch5 = (fifo_b_samp << 1) >> !(snd_pcm_vol.w & 8);

    if (snd_pcm_vol.w & CH_DMAA_L) samp_pcm_l = clip(samp_pcm_l + samp_ch4);
    if (snd_pcm_vol.w & CH_DMAB_L) samp_pcm_l = clip(samp_pcm_l + samp_ch5);

    if (snd_pcm_vol.w & CH_DMAA_R) samp_pcm_r = clip(samp_pcm_r + samp_ch4);
    if (snd_pcm_vol.w & CH_DMAB_R) samp_pcm_r = clip(samp_pcm_r + samp_ch5);

    while (snd_cycles >= SAMP_CYCLES) {
        int16_t samp_ch0 = square_sample(0);
        int16_t samp_ch1 = square_sample(1);
        int16_t samp_ch2 = wave_sample();
        int16_t samp_ch3 = noise_sample();

        int32_t samp_psg_l = 0;
        int32_t samp_psg_r = 0;

        if (snd_psg_vol.w & CH_SQR1_L)  samp_psg_l = clip(samp_psg_l + samp_ch0);
        if (snd_psg_vol.w & CH_SQR2_L)  samp_psg_l = clip(samp_psg_l + samp_ch1);
        if (snd_psg_vol.w & CH_WAVE_L)  samp_psg_l = clip(samp_psg_l + samp_ch2);
        if (snd_psg_vol.w & CH_NOISE_L) samp_psg_l = clip(samp_psg_l + samp_ch3);

        if (snd_psg_vol.w & CH_SQR1_R)  samp_psg_r = clip(samp_psg_r + samp_ch0);
        if (snd_psg_vol.w & CH_SQR2_R)  samp_psg_r = clip(samp_psg_r + samp_ch1);
        if (snd_psg_vol.w & CH_WAVE_R)  samp_psg_r = clip(samp_psg_r + samp_ch2);
        if (snd_psg_vol.w & CH_NOISE_R) samp_psg_r = clip(samp_psg_r + samp_ch3);

        samp_psg_l  *= psg_vol_lut[(snd_psg_vol.w >> 4) & 7];
        samp_psg_r  *= psg_vol_lut[(snd_psg_vol.w >> 0) & 7];

        samp_psg_l >>= psg_rsh_lut[(snd_pcm_vol.w >> 0) & 3];
        samp_psg_r >>= psg_rsh_lut[(snd_pcm_vol.w >> 0) & 3];

        snd_buffer[snd_cur_write & BUFF_SAMPLES_MSK] = clip(samp_psg_l + samp_pcm_l);
		snd_cur_write++;
		if (snd_cur_write >= BUFF_SAMPLES) snd_cur_write -= BUFF_SAMPLES;

        snd_buffer[snd_cur_write & BUFF_SAMPLES_MSK] = clip(samp_psg_r + samp_pcm_r);
		snd_cur_write++;
		if (snd_cur_write >= BUFF_SAMPLES) snd_cur_write -= BUFF_SAMPLES;

        snd_cycles -= SAMP_CYCLES;
    }
}

// timer.h / timer.c

#define TMR_CASCADE  (1 << 2)
#define TMR_IRQ      (1 << 6)
#define TMR_ENB      (1 << 7)

uint32_t tmr_icnt[4];

uint8_t tmr_enb;
uint8_t tmr_irq;
uint8_t tmr_ie;

void timers_clock(uint32_t cycles);

static const uint8_t pscale_shift_lut[4]  = { 0, 6, 8, 10 };

void timers_clock(uint32_t cycles) {
    uint8_t idx;
    bool overflow = false;

    for (idx = 0; idx < 4; idx++) {
        if (!(tmr[idx].ctrl.w & TMR_ENB)) {
            overflow = false;
            continue;
        }

        if (tmr[idx].ctrl.w & TMR_CASCADE) {
            if (overflow) tmr[idx].count.w++;
        } else {
            uint8_t shift = pscale_shift_lut[tmr[idx].ctrl.w & 3];
            uint32_t inc = (tmr_icnt[idx] += cycles) >> shift;

            tmr[idx].count.w += inc;
            tmr_icnt[idx] -= inc << shift;
        }

        if ((overflow = (tmr[idx].count.w > 0xffff))) {
            tmr[idx].count.w = tmr[idx].reload.w + (tmr[idx].count.w - 0x10000);

            if (((snd_pcm_vol.w >> 10) & 1) == idx) {
                //DMA Sound A FIFO
                fifo_a_load();

                if (fifo_a_len <= 0x10) dma_transfer_fifo(1);
            }

            if (((snd_pcm_vol.w >> 14) & 1) == idx) {
                //DMA Sound B FIFO
                fifo_b_load();

                if (fifo_b_len <= 0x10) dma_transfer_fifo(2);
            }
        }

        if ((tmr[idx].ctrl.w & TMR_IRQ) && overflow)
            trigger_irq(TMR0_FLAG << idx);
    }
}

// dma.h / dma.c

#define DMA_REP  (1 <<  9)
#define DMA_32   (1 << 10)
#define DMA_IRQ  (1 << 14)
#define DMA_ENB  (1 << 15)

typedef enum {
    IMMEDIATELY = 0,
    VBLANK      = 1,
    HBLANK      = 2,
    SPECIAL     = 3
} dma_timing_e;

uint32_t dma_src_addr[4];
uint32_t dma_dst_addr[4];

uint32_t dma_count[4];

void dma_transfer(dma_timing_e timing);
void dma_transfer_fifo(uint8_t ch);


//TODO: Timing - DMA should take some amount of cycles

void dma_transfer(dma_timing_e timing) {
    uint8_t ch;

    for (ch = 0; ch < 4; ch++) {
        if (!(dma_ch[ch].ctrl.w & DMA_ENB) ||
            ((dma_ch[ch].ctrl.w >> 12) & 3) != timing)
            continue;

        if (ch == 3)
            eeprom_idx = 0;

        int8_t unit_size = (dma_ch[ch].ctrl.w & DMA_32) ? 4 : 2;

        bool dst_reload = false;

        int8_t dst_inc = 0;
        int8_t src_inc = 0;

        switch ((dma_ch[ch].ctrl.w >> 5) & 3) {
            case 0: dst_inc =  unit_size; break;
            case 1: dst_inc = -unit_size; break;
            case 3:
                dst_inc = unit_size;
                dst_reload = true;
                break;
        }

        switch ((dma_ch[ch].ctrl.w >> 7) & 3) {
            case 0: src_inc =  unit_size; break;
            case 1: src_inc = -unit_size; break;
        }

        while (dma_count[ch]--) {
            if (dma_ch[ch].ctrl.w & DMA_32)
                arm_write(dma_dst_addr[ch],  arm_read(dma_src_addr[ch]));
            else
                arm_writeh(dma_dst_addr[ch], arm_readh(dma_src_addr[ch]));

            dma_dst_addr[ch] += dst_inc;
            dma_src_addr[ch] += src_inc;
        }

        if (dma_ch[ch].ctrl.w & DMA_IRQ)
            trigger_irq(DMA0_FLAG << ch);

        if (dma_ch[ch].ctrl.w & DMA_REP) {
            dma_count[ch] = dma_ch[ch].count.w;

            if (dst_reload) {
                dma_dst_addr[ch] = dma_ch[ch].dst.w;
            }

            continue;
        }

        dma_ch[ch].ctrl.w &= ~DMA_ENB;
    }
}

void dma_transfer_fifo(uint8_t ch) {
    if (!(dma_ch[ch].ctrl.w & DMA_ENB) ||
        ((dma_ch[ch].ctrl.w >> 12) & 3) != SPECIAL)
        return;

    uint8_t i;

    for (i = 0; i < 4; i++) {
        arm_write(dma_dst_addr[ch], arm_read(dma_src_addr[ch]));

        if (ch == 1)
            fifo_a_copy();
        else
            fifo_b_copy();

        switch ((dma_ch[ch].ctrl.w >> 7) & 3) {
            case 0: dma_src_addr[ch] += 4; break;
            case 1: dma_src_addr[ch] -= 4; break;
        }
    }

    if (dma_ch[ch].ctrl.w & DMA_IRQ)
        trigger_irq(DMA0_FLAG << ch);
}

// io.c

uint8_t io_read(uint32_t address) {
    io_open_bus = false;

    switch (address) {
        case 0x04000000: return disp_cnt.b.b0        & 0xff;
        case 0x04000001: return disp_cnt.b.b1        & 0xff;
        case 0x04000002: return green_inv.b.b0       & 0x01;
        case 0x04000003: return green_inv.b.b1       & 0x00;
        case 0x04000004: return disp_stat.b.b0       & 0xff;
        case 0x04000005: return disp_stat.b.b1       & 0xff;
        case 0x04000006: return v_count.b.b0         & 0xff;
        case 0x04000007: return v_count.b.b1         & 0x00;

        case 0x04000008: return bg[0].ctrl.b.b0      & 0xff;
        case 0x04000009: return bg[0].ctrl.b.b1      & 0xdf;
        case 0x0400000a: return bg[1].ctrl.b.b0      & 0xff;
        case 0x0400000b: return bg[1].ctrl.b.b1      & 0xdf;
        case 0x0400000c: return bg[2].ctrl.b.b0      & 0xff;
        case 0x0400000d: return bg[2].ctrl.b.b1      & 0xff;
        case 0x0400000e: return bg[3].ctrl.b.b0      & 0xff;
        case 0x0400000f: return bg[3].ctrl.b.b1      & 0xff;

        case 0x04000048: return win_in.b.b0          & 0xff;
        case 0x04000049: return win_in.b.b1          & 0xff;
        case 0x0400004a: return win_out.b.b0         & 0xff;
        case 0x0400004b: return win_out.b.b1         & 0xff;

        case 0x04000050: return bld_cnt.b.b0         & 0xff;
        case 0x04000051: return bld_cnt.b.b1         & 0x3f;
        case 0x04000052: return bld_alpha.b.b0       & 0x1f;
        case 0x04000053: return bld_alpha.b.b1       & 0x1f;

        case 0x04000060: return sqr_ch[0].sweep.b.b0 & 0x7f;
        case 0x04000061: return sqr_ch[0].sweep.b.b1 & 0x00;
        case 0x04000062: return sqr_ch[0].tone.b.b0  & 0xc0;
        case 0x04000063: return sqr_ch[0].tone.b.b1  & 0xff;
        case 0x04000064: return sqr_ch[0].ctrl.b.b0  & 0x00;
        case 0x04000065: return sqr_ch[0].ctrl.b.b1  & 0x40;
        case 0x04000066: return sqr_ch[0].ctrl.b.b2  & 0x00;
        case 0x04000067: return sqr_ch[0].ctrl.b.b3  & 0x00;

        case 0x04000068: return sqr_ch[1].tone.b.b0  & 0xc0;
        case 0x04000069: return sqr_ch[1].tone.b.b1  & 0xff;
        case 0x0400006c: return sqr_ch[1].ctrl.b.b0  & 0x00;
        case 0x0400006d: return sqr_ch[1].ctrl.b.b1  & 0x40;
        case 0x0400006e: return sqr_ch[1].ctrl.b.b2  & 0x00;
        case 0x0400006f: return sqr_ch[1].ctrl.b.b3  & 0x00;

        case 0x04000070: return wave_ch.wave.b.b0    & 0xe0;
        case 0x04000071: return wave_ch.wave.b.b1    & 0x00;
        case 0x04000072: return wave_ch.volume.b.b0  & 0x00;
        case 0x04000073: return wave_ch.volume.b.b1  & 0xe0;
        case 0x04000074: return wave_ch.ctrl.b.b0    & 0x00;
        case 0x04000075: return wave_ch.ctrl.b.b1    & 0x40;
        case 0x04000076: return wave_ch.ctrl.b.b2    & 0x00;
        case 0x04000077: return wave_ch.ctrl.b.b3    & 0x00;

        case 0x04000078: return noise_ch.env.b.b0    & 0x00;
        case 0x04000079: return noise_ch.env.b.b1    & 0xff;
        case 0x0400007a: return noise_ch.env.b.b2    & 0x00;
        case 0x0400007b: return noise_ch.env.b.b3    & 0x00;
        case 0x0400007c: return noise_ch.ctrl.b.b0   & 0xff;
        case 0x0400007d: return noise_ch.ctrl.b.b1   & 0x40;
        case 0x0400007e: return noise_ch.ctrl.b.b2   & 0x00;
        case 0x0400007f: return noise_ch.ctrl.b.b3   & 0x00;

        case 0x04000080: return snd_psg_vol.b.b0     & 0x77;
        case 0x04000081: return snd_psg_vol.b.b1     & 0xff;
        case 0x04000082: return snd_pcm_vol.b.b0     & 0x0f;
        case 0x04000083: return snd_pcm_vol.b.b1     & 0x77;
        case 0x04000084: return snd_psg_enb.b.b0     & 0x8f;
        case 0x04000085: return snd_psg_enb.b.b1     & 0x00;
        case 0x04000086: return snd_psg_enb.b.b2     & 0x00;
        case 0x04000087: return snd_psg_enb.b.b3     & 0x00;
        case 0x04000088: return snd_bias.b.b0        & 0xff;
        case 0x04000089: return snd_bias.b.b1        & 0xc3;
        case 0x0400008a: return snd_bias.b.b2        & 0x00;
        case 0x0400008b: return snd_bias.b.b3        & 0x00;

        case 0x04000090:
        case 0x04000091:
        case 0x04000092:
        case 0x04000093:
        case 0x04000094:
        case 0x04000095:
        case 0x04000096:
        case 0x04000097:
        case 0x04000098:
        case 0x04000099:
        case 0x0400009a:
        case 0x0400009b:
        case 0x0400009c:
        case 0x0400009d:
        case 0x0400009e:
        case 0x0400009f: {
            uint8_t wave_bank = (wave_ch.wave.w >> 2) & 0x10;
            uint8_t wave_idx  = (wave_bank ^ 0x10) | (address & 0xf);

            return wave_ram[wave_idx];
        }

        case 0x040000b8: return dma_ch[0].count.b.b0 & 0x00;
        case 0x040000b9: return dma_ch[0].count.b.b1 & 0x00;
        case 0x040000ba: return dma_ch[0].ctrl.b.b0  & 0xe0;
        case 0x040000bb: return dma_ch[0].ctrl.b.b1  & 0xf7;

        case 0x040000c4: return dma_ch[1].count.b.b0 & 0x00;
        case 0x040000c5: return dma_ch[1].count.b.b1 & 0x00;
        case 0x040000c6: return dma_ch[1].ctrl.b.b0  & 0xe0;
        case 0x040000c7: return dma_ch[1].ctrl.b.b1  & 0xf7;

        case 0x040000d0: return dma_ch[2].count.b.b0 & 0x00;
        case 0x040000d1: return dma_ch[2].count.b.b1 & 0x00;
        case 0x040000d2: return dma_ch[2].ctrl.b.b0  & 0xe0;
        case 0x040000d3: return dma_ch[2].ctrl.b.b1  & 0xf7;

        case 0x040000dc: return dma_ch[3].count.b.b0 & 0x00;
        case 0x040000dd: return dma_ch[3].count.b.b1 & 0x00;
        case 0x040000de: return dma_ch[3].ctrl.b.b0  & 0xe0;
        case 0x040000df: return dma_ch[3].ctrl.b.b1  & 0xff;

        case 0x04000100: return tmr[0].count.b.b0    & 0xff;
        case 0x04000101: return tmr[0].count.b.b1    & 0xff;
        case 0x04000102: return tmr[0].ctrl.b.b0     & 0xc7;
        case 0x04000103: return tmr[0].ctrl.b.b1     & 0x00;

        case 0x04000104: return tmr[1].count.b.b0    & 0xff;
        case 0x04000105: return tmr[1].count.b.b1    & 0xff;
        case 0x04000106: return tmr[1].ctrl.b.b0     & 0xc7;
        case 0x04000107: return tmr[1].ctrl.b.b1     & 0x00;

        case 0x04000108: return tmr[2].count.b.b0    & 0xff;
        case 0x04000109: return tmr[2].count.b.b1    & 0xff;
        case 0x0400010a: return tmr[2].ctrl.b.b0     & 0xc7;
        case 0x0400010b: return tmr[2].ctrl.b.b1     & 0x00;

        case 0x0400010c: return tmr[3].count.b.b0    & 0xff;
        case 0x0400010d: return tmr[3].count.b.b1    & 0xff;
        case 0x0400010e: return tmr[3].ctrl.b.b0     & 0xc7;
        case 0x0400010f: return tmr[3].ctrl.b.b1     & 0x00;

        case 0x04000120: return sio_data32.b.b0      & 0xff;
        case 0x04000121: return sio_data32.b.b1      & 0xff;
        case 0x04000122: return sio_data32.b.b2      & 0xff;
        case 0x04000123: return sio_data32.b.b3      & 0xff;
        case 0x04000128: return sio_cnt.b.b0         & 0xff;
        case 0x04000129: return sio_cnt.b.b1         & 0xff;
        case 0x0400012a: return sio_data8.b.b0       & 0xff;
        case 0x04000134: return r_cnt.b.b0           & 0xff;
        case 0x04000135: return r_cnt.b.b1           & 0xff;

        case 0x04000130: return key_input.b.b0       & 0xff;
        case 0x04000131: return key_input.b.b1       & 0x3f;

        case 0x04000200: return int_enb.b.b0         & 0xff;
        case 0x04000201: return int_enb.b.b1         & 0x3f;
        case 0x04000202: return int_ack.b.b0         & 0xff;
        case 0x04000203: return int_ack.b.b1         & 0x3f;
        case 0x04000204: return wait_cnt.b.b0        & 0xff;
        case 0x04000205: return wait_cnt.b.b1        & 0xdf;
        case 0x04000206: return wait_cnt.b.b2        & 0x00;
        case 0x04000207: return wait_cnt.b.b3        & 0x00;
        case 0x04000208: return int_enb_m.b.b0       & 0x01;
        case 0x04000209: return int_enb_m.b.b1       & 0x00;
        case 0x0400020a: return int_enb_m.b.b2       & 0x00;
        case 0x0400020b: return int_enb_m.b.b3       & 0x00;

        case 0x04000300: return post_boot            & 0x01;
        case 0x04000301: return int_halt             & 0x00;
    }

    io_open_bus = true;

    return 0;
}

static void dma_load(uint8_t ch, uint8_t value) {
    uint8_t old = dma_ch[ch].ctrl.b.b1;

    dma_ch[ch].ctrl.b.b1 = value;

    if ((old ^ value) & value & 0x80) {
        dma_dst_addr[ch] = dma_ch[ch].dst.w;
        dma_src_addr[ch] = dma_ch[ch].src.w;

        if (dma_ch[ch].ctrl.w & DMA_32) {
            dma_dst_addr[ch] &= ~3;
            dma_src_addr[ch] &= ~3;
        } else {
            dma_dst_addr[ch] &= ~1;
            dma_src_addr[ch] &= ~1;
        }

        dma_count[ch] = dma_ch[ch].count.w;

        dma_transfer(IMMEDIATELY);
    }
}

static void tmr_load(uint8_t idx, uint8_t value) {
    uint8_t old = tmr[idx].ctrl.b.b0;

    tmr[idx].ctrl.b.b0 = value;

    if (value & TMR_ENB)
        tmr_enb |=  (1 << idx);
    else
        tmr_enb &= ~(1 << idx);

    if ((old ^ value) & value & TMR_ENB) {
        tmr[idx].count.w = tmr[idx].reload.w;

        tmr_icnt[idx] = 0;
    }
}

static void snd_reset_state(uint8_t ch, bool enb) {
    if (enb) {
        snd_ch_state[ch].phase       = false;
        snd_ch_state[ch].samples     = 0;
        snd_ch_state[ch].length_time = 0;
        snd_ch_state[ch].sweep_time  = 0;
        snd_ch_state[ch].env_time    = 0;

        if (ch == 2) wave_reset();

        if (ch == 3) {
            if (noise_ch.ctrl.w & NOISE_7)
                snd_ch_state[ch].lfsr = 0x007f;
            else
                snd_ch_state[ch].lfsr = 0x7fff;
        }

        snd_psg_enb.w |=  (1 << ch);
    }
}

void io_write(uint32_t address, uint8_t value) {
    switch (address) {
        case 0x04000000:
            if (arm_r.r[15] >= 0x4000) {
                //The CGB mode enable bit 3 can only be set by the bios
                value &= 0xf7;
            }

            disp_cnt.b.b0 = value;
        break;
        case 0x04000001: disp_cnt.b.b1        =  value; break;
        case 0x04000002: green_inv.b.b0       =  value; break;
        case 0x04000003: green_inv.b.b1       =  value; break;
        case 0x04000004:
            disp_stat.b.b0 &=          0x47;
            disp_stat.b.b0 |= value & ~0x47;
        break;
        case 0x04000005: disp_stat.b.b1       =  value; break;

        case 0x04000008: bg[0].ctrl.b.b0      =  value; break;
        case 0x04000009: bg[0].ctrl.b.b1      =  value; break;
        case 0x0400000a: bg[1].ctrl.b.b0      =  value; break;
        case 0x0400000b: bg[1].ctrl.b.b1      =  value; break;
        case 0x0400000c: bg[2].ctrl.b.b0      =  value; break;
        case 0x0400000d: bg[2].ctrl.b.b1      =  value; break;
        case 0x0400000e: bg[3].ctrl.b.b0      =  value; break;
        case 0x0400000f: bg[3].ctrl.b.b1      =  value; break;

        case 0x04000010: bg[0].xofs.b.b0      =  value; break;
        case 0x04000011: bg[0].xofs.b.b1      =  value; break;
        case 0x04000012: bg[0].yofs.b.b0      =  value; break;
        case 0x04000013: bg[0].yofs.b.b1      =  value; break;
        case 0x04000014: bg[1].xofs.b.b0      =  value; break;
        case 0x04000015: bg[1].xofs.b.b1      =  value; break;
        case 0x04000016: bg[1].yofs.b.b0      =  value; break;
        case 0x04000017: bg[1].yofs.b.b1      =  value; break;
        case 0x04000018: bg[2].xofs.b.b0      =  value; break;
        case 0x04000019: bg[2].xofs.b.b1      =  value; break;
        case 0x0400001a: bg[2].yofs.b.b0      =  value; break;
        case 0x0400001b: bg[2].yofs.b.b1      =  value; break;
        case 0x0400001c: bg[3].xofs.b.b0      =  value; break;
        case 0x0400001d: bg[3].xofs.b.b1      =  value; break;
        case 0x0400001e: bg[3].yofs.b.b0      =  value; break;
        case 0x0400001f: bg[3].yofs.b.b1      =  value; break;

        case 0x04000020: bg_pa[2].b.b0        =  value; break;
        case 0x04000021: bg_pa[2].b.b1        =  value; break;
        case 0x04000022: bg_pb[2].b.b0        =  value; break;
        case 0x04000023: bg_pb[2].b.b1        =  value; break;
        case 0x04000024: bg_pc[2].b.b0        =  value; break;
        case 0x04000025: bg_pc[2].b.b1        =  value; break;
        case 0x04000026: bg_pd[2].b.b0        =  value; break;
        case 0x04000027: bg_pd[2].b.b1        =  value; break;

        case 0x04000028:
            bg_refxe[2].b.b0 =
            bg_refxi[2].b.b0 = value;
        break;
        case 0x04000029:
            bg_refxe[2].b.b1 =
            bg_refxi[2].b.b1 = value;
        break;
        case 0x0400002a:
            bg_refxe[2].b.b2 =
            bg_refxi[2].b.b2 = value;
        break;
        case 0x0400002b:
            bg_refxe[2].b.b3 =
            bg_refxi[2].b.b3 = value;
        break;
        case 0x0400002c:
            bg_refye[2].b.b0 =
            bg_refyi[2].b.b0 = value;
        break;
        case 0x0400002d:
            bg_refye[2].b.b1 =
            bg_refyi[2].b.b1 = value;
        break;
        case 0x0400002e:
            bg_refye[2].b.b2 =
            bg_refyi[2].b.b2 = value;
        break;
        case 0x0400002f:
            bg_refye[2].b.b3 =
            bg_refyi[2].b.b3 = value;
        break;

        case 0x04000030: bg_pa[3].b.b0        =  value; break;
        case 0x04000031: bg_pa[3].b.b1        =  value; break;
        case 0x04000032: bg_pb[3].b.b0        =  value; break;
        case 0x04000033: bg_pb[3].b.b1        =  value; break;
        case 0x04000034: bg_pc[3].b.b0        =  value; break;
        case 0x04000035: bg_pc[3].b.b1        =  value; break;
        case 0x04000036: bg_pd[3].b.b0        =  value; break;
        case 0x04000037: bg_pd[3].b.b1        =  value; break;

        case 0x04000038:
            bg_refxe[3].b.b0 =
            bg_refxi[3].b.b0 = value;
        break;
        case 0x04000039:
            bg_refxe[3].b.b1 =
            bg_refxi[3].b.b1 = value;
        break;
        case 0x0400003a:
            bg_refxe[3].b.b2 =
            bg_refxi[3].b.b2 = value;
        break;
        case 0x0400003b:
            bg_refxe[3].b.b3 =
            bg_refxi[3].b.b3 = value;
        break;
        case 0x0400003c:
            bg_refye[3].b.b0 =
            bg_refyi[3].b.b0 = value;
        break;
        case 0x0400003d:
            bg_refye[3].b.b1 =
            bg_refyi[3].b.b1 = value;
        break;
        case 0x0400003e:
            bg_refye[3].b.b2 =
            bg_refyi[3].b.b2 = value;
        break;
        case 0x0400003f:
            bg_refye[3].b.b3 =
            bg_refyi[3].b.b3 = value;
        break;

		case 0x04000040: win0_h.b.b0          =  value; break;
        case 0x04000041: win0_h.b.b1          =  value; break;
        case 0x04000042: win1_h.b.b0          =  value; break;
        case 0x04000043: win1_h.b.b1          =  value; break;

        case 0x04000044: win0_v.b.b0          =  value; break;
        case 0x04000045: win0_v.b.b1          =  value; break;
        case 0x04000046: win1_v.b.b0          =  value; break;
        case 0x04000047: win1_v.b.b1          =  value; break;

        case 0x04000048: win_in.b.b0          =  value; break;
        case 0x04000049: win_in.b.b1          =  value; break;
        case 0x0400004a: win_out.b.b0         =  value; break;
        case 0x0400004b: win_out.b.b1         =  value; break;

        case 0x04000050: bld_cnt.b.b0         =  value; break;
        case 0x04000051: bld_cnt.b.b1         =  value; break;
        case 0x04000052: bld_alpha.b.b0       =  value; break;
        case 0x04000053: bld_alpha.b.b1       =  value; break;
        case 0x04000054: bld_bright.b.b0      =  value; break;
        case 0x04000055: bld_bright.b.b1      =  value; break;
        case 0x04000056: bld_bright.b.b2      =  value; break;
        case 0x04000057: bld_bright.b.b3      =  value; break;

        case 0x04000060:
            if (snd_psg_enb.w & PSG_ENB)
                sqr_ch[0].sweep.b.b0 = value;
        break;
        case 0x04000061:
            if (snd_psg_enb.w & PSG_ENB)
                sqr_ch[0].sweep.b.b1 = value;
        break;
        case 0x04000062:
            if (snd_psg_enb.w & PSG_ENB)
                sqr_ch[0].tone.b.b0 = value;
        break;
        case 0x04000063:
            if (snd_psg_enb.w & PSG_ENB)
                sqr_ch[0].tone.b.b1 = value;
        break;
        case 0x04000064:
            if (snd_psg_enb.w & PSG_ENB)
                sqr_ch[0].ctrl.b.b0 = value;
        break;
        case 0x04000065:
            if (snd_psg_enb.w & PSG_ENB) {
                sqr_ch[0].ctrl.b.b1 = value;

                snd_reset_state(0, value & 0x80);
            }
        break;
        case 0x04000066:
            if (snd_psg_enb.w & PSG_ENB)
                sqr_ch[0].ctrl.b.b2 = value;
        break;
        case 0x04000067:
            if (snd_psg_enb.w & PSG_ENB)
                sqr_ch[0].ctrl.b.b3 = value;
        break;

        case 0x04000068:
            if (snd_psg_enb.w & PSG_ENB)
                sqr_ch[1].tone.b.b0 = value;
        break;
        case 0x04000069:
            if (snd_psg_enb.w & PSG_ENB)
                sqr_ch[1].tone.b.b1 = value;
        break;
        case 0x0400006c:
            if (snd_psg_enb.w & PSG_ENB)
                sqr_ch[1].ctrl.b.b0 = value;
        break;
        case 0x0400006d:
            if (snd_psg_enb.w & PSG_ENB) {
                sqr_ch[1].ctrl.b.b1 = value;

                snd_reset_state(1, value & 0x80);
            }
        break;
        case 0x0400006e:
            if (snd_psg_enb.w & PSG_ENB)
                sqr_ch[1].ctrl.b.b2 = value;
        break;
        case 0x0400006f:
            if (snd_psg_enb.w & PSG_ENB)
                sqr_ch[1].ctrl.b.b3 = value;
        break;

        case 0x04000070:
            if (snd_psg_enb.w & PSG_ENB)
                wave_ch.wave.b.b0 = value;
        break;
        case 0x04000071:
            if (snd_psg_enb.w & PSG_ENB)
                wave_ch.wave.b.b1 = value;
        break;
        case 0x04000072:
            if (snd_psg_enb.w & PSG_ENB)
                wave_ch.volume.b.b0 = value;
        break;
        case 0x04000073:
            if (snd_psg_enb.w & PSG_ENB)
                wave_ch.volume.b.b1 = value;
        break;
        case 0x04000074:
            if (snd_psg_enb.w & PSG_ENB)
                wave_ch.ctrl.b.b0 = value;
        break;
        case 0x04000075:
            if (snd_psg_enb.w & PSG_ENB) {
                wave_ch.ctrl.b.b1 = value;

                snd_reset_state(2, value & 0x80);
            }
        break;
        case 0x04000076:
            if (snd_psg_enb.w & PSG_ENB)
                wave_ch.ctrl.b.b2 = value;
        break;
        case 0x04000077:
            if (snd_psg_enb.w & PSG_ENB)
                wave_ch.ctrl.b.b3 = value;
        break;

        case 0x04000078:
            if (snd_psg_enb.w & PSG_ENB)
                noise_ch.env.b.b0 = value;
        break;
        case 0x04000079:
            if (snd_psg_enb.w & PSG_ENB)
                noise_ch.env.b.b1 = value;
        break;
        case 0x0400007a:
            if (snd_psg_enb.w & PSG_ENB)
                noise_ch.env.b.b2 = value;
        break;
        case 0x0400007b:
            if (snd_psg_enb.w & PSG_ENB)
                noise_ch.env.b.b3 = value;
        break;
        case 0x0400007c:
            if (snd_psg_enb.w & PSG_ENB)
                noise_ch.ctrl.b.b0 = value;
        break;
        case 0x0400007d:
            if (snd_psg_enb.w & PSG_ENB) {
                noise_ch.ctrl.b.b1 = value;

                snd_reset_state(3, value & 0x80);
            }
        break;
        case 0x0400007e:
            if (snd_psg_enb.w & PSG_ENB)
                noise_ch.ctrl.b.b2 = value;
        break;
        case 0x0400007f:
            if (snd_psg_enb.w & PSG_ENB)
                noise_ch.ctrl.b.b3 = value;
        break;

        case 0x04000080:
            if (snd_psg_enb.w & PSG_ENB)
                snd_psg_vol.b.b0 = value;
        break;
        case 0x04000081:
            if (snd_psg_enb.w & PSG_ENB)
                snd_psg_vol.b.b1 = value;
        break;
        case 0x04000082:
            //PCM is not affected by the PSG Enable flag
            snd_pcm_vol.b.b0 = value;
        break;
        case 0x04000083:
            snd_pcm_vol.b.b1 = value;

            if (value & 0x08) fifo_a_len = 0;
            if (value & 0x80) fifo_b_len = 0;
        break;
        case 0x04000084:
            snd_psg_enb.b.b0 &=          0xf;
            snd_psg_enb.b.b0 |= value & ~0xf;

            if (!(value & PSG_ENB)) {
                sqr_ch[0].sweep.w = 0;
                sqr_ch[0].tone.w  = 0;
                sqr_ch[0].ctrl.w  = 0;

                sqr_ch[1].tone.w  = 0;
                sqr_ch[1].ctrl.w  = 0;

                wave_ch.wave.w    = 0;
                wave_ch.volume.w  = 0;
                wave_ch.ctrl.w    = 0;

                noise_ch.env.w    = 0;
                noise_ch.ctrl.w   = 0;

                snd_psg_vol.w     = 0;
                snd_psg_enb.w     = 0;
            }
        break;
        case 0x04000085: snd_psg_enb.b.b1     =  value; break;
        case 0x04000086: snd_psg_enb.b.b2     =  value; break;
        case 0x04000087: snd_psg_enb.b.b3     =  value; break;
        case 0x04000088: snd_bias.b.b0        =  value; break;
        case 0x04000089: snd_bias.b.b1        =  value; break;
        case 0x0400008a: snd_bias.b.b2        =  value; break;
        case 0x0400008b: snd_bias.b.b3        =  value; break;

        case 0x04000090:
        case 0x04000091:
        case 0x04000092:
        case 0x04000093:
        case 0x04000094:
        case 0x04000095:
        case 0x04000096:
        case 0x04000097:
        case 0x04000098:
        case 0x04000099:
        case 0x0400009a:
        case 0x0400009b:
        case 0x0400009c:
        case 0x0400009d:
        case 0x0400009e:
        case 0x0400009f: {
            uint8_t wave_bank = (wave_ch.wave.w >> 2) & 0x10;
            uint8_t wave_idx  = (wave_bank ^ 0x10) | (address & 0xf);

            wave_ram[wave_idx] = value;
        }
        break;

        case 0x040000a0: snd_fifo_a_0         =  value; break;
        case 0x040000a1: snd_fifo_a_1         =  value; break;
        case 0x040000a2: snd_fifo_a_2         =  value; break;
        case 0x040000a3: snd_fifo_a_3         =  value; break;

        case 0x040000a4: snd_fifo_b_0         =  value; break;
        case 0x040000a5: snd_fifo_b_1         =  value; break;
        case 0x040000a6: snd_fifo_b_2         =  value; break;
        case 0x040000a7: snd_fifo_b_3         =  value; break;

        case 0x040000b0: dma_ch[0].src.b.b0   =  value; break;
        case 0x040000b1: dma_ch[0].src.b.b1   =  value; break;
        case 0x040000b2: dma_ch[0].src.b.b2   =  value; break;
        case 0x040000b3: dma_ch[0].src.b.b3   =  value; break;
        case 0x040000b4: dma_ch[0].dst.b.b0   =  value; break;
        case 0x040000b5: dma_ch[0].dst.b.b1   =  value; break;
        case 0x040000b6: dma_ch[0].dst.b.b2   =  value; break;
        case 0x040000b7: dma_ch[0].dst.b.b3   =  value; break;
        case 0x040000b8: dma_ch[0].count.b.b0 =  value; break;
        case 0x040000b9: dma_ch[0].count.b.b1 =  value; break;
        case 0x040000ba: dma_ch[0].ctrl.b.b0  =  value; break;
        case 0x040000bb: dma_load(0, value);            break;

        case 0x040000bc: dma_ch[1].src.b.b0   =  value; break;
        case 0x040000bd: dma_ch[1].src.b.b1   =  value; break;
        case 0x040000be: dma_ch[1].src.b.b2   =  value; break;
        case 0x040000bf: dma_ch[1].src.b.b3   =  value; break;
        case 0x040000c0: dma_ch[1].dst.b.b0   =  value; break;
        case 0x040000c1: dma_ch[1].dst.b.b1   =  value; break;
        case 0x040000c2: dma_ch[1].dst.b.b2   =  value; break;
        case 0x040000c3: dma_ch[1].dst.b.b3   =  value; break;
        case 0x040000c4: dma_ch[1].count.b.b0 =  value; break;
        case 0x040000c5: dma_ch[1].count.b.b1 =  value; break;
        case 0x040000c6: dma_ch[1].ctrl.b.b0  =  value; break;
        case 0x040000c7: dma_load(1, value);            break;

        case 0x040000c8: dma_ch[2].src.b.b0   =  value; break;
        case 0x040000c9: dma_ch[2].src.b.b1   =  value; break;
        case 0x040000ca: dma_ch[2].src.b.b2   =  value; break;
        case 0x040000cb: dma_ch[2].src.b.b3   =  value; break;
        case 0x040000cc: dma_ch[2].dst.b.b0   =  value; break;
        case 0x040000cd: dma_ch[2].dst.b.b1   =  value; break;
        case 0x040000ce: dma_ch[2].dst.b.b2   =  value; break;
        case 0x040000cf: dma_ch[2].dst.b.b3   =  value; break;
        case 0x040000d0: dma_ch[2].count.b.b0 =  value; break;
        case 0x040000d1: dma_ch[2].count.b.b1 =  value; break;
        case 0x040000d2: dma_ch[2].ctrl.b.b0  =  value; break;
        case 0x040000d3: dma_load(2, value);            break;

        case 0x040000d4: dma_ch[3].src.b.b0   =  value; break;
        case 0x040000d5: dma_ch[3].src.b.b1   =  value; break;
        case 0x040000d6: dma_ch[3].src.b.b2   =  value; break;
        case 0x040000d7: dma_ch[3].src.b.b3   =  value; break;
        case 0x040000d8: dma_ch[3].dst.b.b0   =  value; break;
        case 0x040000d9: dma_ch[3].dst.b.b1   =  value; break;
        case 0x040000da: dma_ch[3].dst.b.b2   =  value; break;
        case 0x040000db: dma_ch[3].dst.b.b3   =  value; break;
        case 0x040000dc: dma_ch[3].count.b.b0 =  value; break;
        case 0x040000dd: dma_ch[3].count.b.b1 =  value; break;
        case 0x040000de: dma_ch[3].ctrl.b.b0  =  value; break;
        case 0x040000df: dma_load(3, value);            break;

        case 0x04000100: tmr[0].reload.b.b0   =  value; break;
        case 0x04000101: tmr[0].reload.b.b1   =  value; break;
        case 0x04000102: tmr_load(0, value);            break;
        case 0x04000103: tmr[0].ctrl.b.b1     =  value; break;

        case 0x04000104: tmr[1].reload.b.b0   =  value; break;
        case 0x04000105: tmr[1].reload.b.b1   =  value; break;
        case 0x04000106: tmr_load(1, value);            break;
        case 0x04000107: tmr[1].ctrl.b.b1     =  value; break;

        case 0x04000108: tmr[2].reload.b.b0   =  value; break;
        case 0x04000109: tmr[2].reload.b.b1   =  value; break;
        case 0x0400010a: tmr_load(2, value);            break;
        case 0x0400010b: tmr[2].ctrl.b.b1     =  value; break;

        case 0x0400010c: tmr[3].reload.b.b0   =  value; break;
        case 0x0400010d: tmr[3].reload.b.b1   =  value; break;
        case 0x0400010e: tmr_load(3, value);            break;
        case 0x0400010f: tmr[3].ctrl.b.b1     =  value; break;

        case 0x04000120: sio_data32.b.b0      =  value; break;
        case 0x04000121: sio_data32.b.b1      =  value; break;
        case 0x04000122: sio_data32.b.b2      =  value; break;
        case 0x04000123: sio_data32.b.b3      =  value; break;
        case 0x04000128: sio_cnt.b.b0         =  value; break;
        case 0x04000129: sio_cnt.b.b1         =  value; break;
        case 0x0400012a: sio_data8.b.b0       =  value; break;
        case 0x04000134: r_cnt.b.b0           =  value; break;
        case 0x04000135: r_cnt.b.b1           =  value; break;

        case 0x04000200:
            int_enb.b.b0 = value;
            arm_check_irq();
        break;
        case 0x04000201:
            int_enb.b.b1 = value;
            arm_check_irq();
        break;
        case 0x04000202: int_ack.b.b0        &= ~value; break;
        case 0x04000203: int_ack.b.b1        &= ~value; break;
        case 0x04000204:
            wait_cnt.b.b0 = value;
            update_ws();
        break;
        case 0x04000205:
            wait_cnt.b.b1 = value;
            update_ws();
        break;
        case 0x04000206:
            wait_cnt.b.b2 = value;
            update_ws();
        break;
        case 0x04000207:
            wait_cnt.b.b3 = value;
            update_ws();
        break;
        case 0x04000208:
            int_enb_m.b.b0 = value;
            arm_check_irq();
        break;
        case 0x04000209:
            int_enb_m.b.b1 = value;
            arm_check_irq();
        break;
        case 0x0400020a:
            int_enb_m.b.b2 = value;
            arm_check_irq();
        break;
        case 0x0400020b:
            int_enb_m.b.b3 = value;
            arm_check_irq();
        break;

        case 0x04000300: post_boot            =  value; break;
        case 0x04000301: int_halt             =  true;  break;
    }
}

void trigger_irq(uint16_t flag) {
    int_ack.w |= flag;

    int_halt = false;

    arm_check_irq();
}

static const uint8_t ws0_s_lut[2] = { 2, 1 };
static const uint8_t ws1_s_lut[2] = { 4, 1 };
static const uint8_t ws2_s_lut[2] = { 8, 1 };
static const uint8_t ws_n_lut[4]  = { 4, 3, 2, 8 };

void update_ws() {
    ws_n[0] = ws_n_lut[(wait_cnt.w >> 2) & 3];
    ws_n[1] = ws_n_lut[(wait_cnt.w >> 5) & 3];
    ws_n[2] = ws_n_lut[(wait_cnt.w >> 8) & 3];

    ws_s[0] = ws0_s_lut[(wait_cnt.w >>  4) & 1];
    ws_s[1] = ws1_s_lut[(wait_cnt.w >>  7) & 1];
    ws_s[2] = ws2_s_lut[(wait_cnt.w >> 10) & 1];

    int8_t i;

    for (i = 0; i < 3; i++) {
        ws_n_t16[i] = ws_n[i] + 1;
        ws_s_t16[i] = ws_s[i] + 1;

        ws_n_arm[i] = ws_n_t16[i] + ws_s_t16[i];
        ws_s_arm[i] = ws_s_t16[i] << 1;
    }
}

// arm_mem.c

#define EEPROM_WRITE  2
#define EEPROM_READ   3

uint32_t flash_bank = 0;

typedef enum {
    IDLE,
    ERASE,
    WRITE,
    BANK_SWITCH
} flash_mode_e;

flash_mode_e flash_mode = IDLE;

bool flash_id_mode = false;

bool flash_used = false;

bool eeprom_used = false;
bool eeprom_read = false;

uint32_t eeprom_addr      = 0;
uint32_t eeprom_addr_read = 0;

uint8_t eeprom_buff[0x100];

static const uint8_t bus_size_lut[16]  = { 4, 4, 2, 4, 4, 2, 2, 4, 2, 2, 2, 2, 2, 2, 1, 1 };

static void arm_access(uint32_t address, access_type_e at) {
    uint8_t cycles = 1;

    if (address & 0x08000000) {
        if (at == NON_SEQ)
            cycles += ws_n[(address >> 25) & 3];
        else
            cycles += ws_s[(address >> 25) & 3];
    } else if ((address >> 24) == 2) {
        cycles += 2;
    }

    arm_cycles += cycles;
}

static void arm_access_bus(uint32_t address, uint8_t size, access_type_e at) {
    uint8_t lut_idx = (address >> 24) & 0xf;
    uint8_t bus_sz = bus_size_lut[lut_idx];

    if (bus_sz < size) {
        arm_access(address + 0, at);
        arm_access(address + 2, SEQUENTIAL);
    } else {
        arm_access(address, at);
    }
}

//Memory read
static uint8_t bios_read(uint32_t address) {
    if ((address | arm_r.r[15]) < 0x4000)
        return bios[address & 0x3fff];
    else
        return bios_op;
}

static uint8_t wram_read(uint32_t address) {
    return wram[address & 0x3ffff];
}

static uint8_t iwram_read(uint32_t address) {
    return iwram[address & 0x7fff];
}

static uint8_t pram_read(uint32_t address) {
    return pram[address & 0x3ff];
}

static uint8_t vram_read(uint32_t address) {
    return vram[address & (address & 0x10000 ? 0x17fff : 0x1ffff)];
}

static uint8_t oam_read(uint32_t address) {
    return oam[address & 0x3ff];
}

static uint8_t rom_read(uint32_t address) {
    return rom[address & cart_rom_mask];
}

static uint8_t rom_eep_read(uint32_t address, uint8_t offset) {
    if (eeprom_used &&
        ((cart_rom_size >  0x1000000 && (address >>  8) == 0x0dffff) ||
         (cart_rom_size <= 0x1000000 && (address >> 24) == 0x00000d))) {
         if (!offset) {
             uint8_t mode = eeprom_buff[0] >> 6;

             switch (mode) {
                 case EEPROM_WRITE: return 1;
                 case EEPROM_READ: {
                    uint8_t value = 0;

                    if (eeprom_idx >= 4) {
                        uint8_t idx = ((eeprom_idx - 4) >> 3) & 7;
                        uint8_t bit = ((eeprom_idx - 4) >> 0) & 7;

                        value = (eeprom[eeprom_addr_read | idx] >> (bit ^ 7)) & 1;
                    }

                    eeprom_idx++;

                    return value;
                 }
             }
         }
    } else {
        return rom_read(address);
    }

    return 0;
}

static uint8_t flash_read(uint32_t address) {
    if (flash_id_mode) {
        //This is the Flash ROM ID, we return Sanyo ID code
        switch (address) {
            case 0x0e000000: return 0x62;
            case 0x0e000001: return 0x13;
        }
    } else if (flash_used) {
        return flash[flash_bank | (address & 0xffff)];
    } else {
        return sram[address & 0xffff];
    }

    return 0;
}

static uint8_t arm_read_(uint32_t address, uint8_t offset) {
    switch (address >> 24) {
        case 0x0: return bios_read(address);
        case 0x2: return wram_read(address);
        case 0x3: return iwram_read(address);
        case 0x4: return io_read(address);
        case 0x5: return pram_read(address);
        case 0x6: return vram_read(address);
        case 0x7: return oam_read(address);

        case 0x8:
        case 0x9:
            return rom_read(address);

        case 0xa:
        case 0xb:
            return rom_read(address);

        case 0xc:
        case 0xd:
            return rom_eep_read(address, offset);

        case 0xe:
        case 0xf:
            return flash_read(address);
    }

    return 0;
}

#define IS_OPEN_BUS(a)  (((a) >> 28) || ((a) >= 0x00004000 && (a) < 0x02000000))

uint8_t arm_readb(uint32_t address) {
    uint8_t value = arm_read_(address, 0);

    if (!(address & 0x08000000)) {
        io_open_bus &= ((address >> 24) == 4);

        if (IS_OPEN_BUS(address) || io_open_bus)
            value = arm_pipe[1];
    }

    return value;
}

uint32_t arm_readh(uint32_t address) {
    uint32_t a = address & ~1;
    uint8_t  s = address &  1;

    uint32_t value =
        arm_read_(a | 0, 0) << 0 |
        arm_read_(a | 1, 1) << 8;

    if (!(a & 0x08000000)) {
        io_open_bus &= ((a >> 24) == 4);

        if (a < 0x4000 && arm_r.r[15] >= 0x4000)
            value = bios_op     & 0xffff;
        else if (IS_OPEN_BUS(a) || io_open_bus)
            value = arm_pipe[1] & 0xffff;
    }

    return ROR(value, s << 3);
}

uint32_t arm_read(uint32_t address) {
    uint32_t a = address & ~3;
    uint8_t  s = address &  3;

    uint32_t value =
        arm_read_(a | 0, 0) <<  0 |
        arm_read_(a | 1, 1) <<  8 |
        arm_read_(a | 2, 2) << 16 |
        arm_read_(a | 3, 3) << 24;

    if (!(a & 0x08000000)) {
        io_open_bus &= ((a >> 24) == 4);

        if (a < 0x4000 && arm_r.r[15] >= 0x4000)
            value = bios_op;
        else if (IS_OPEN_BUS(a) || io_open_bus)
            value = arm_pipe[1];
    }

    return ROR(value, s << 3);
}

uint8_t arm_readb_n(uint32_t address) {
    arm_access_bus(address, ARM_BYTE_SZ, NON_SEQ);

    return arm_readb(address);
}

uint32_t arm_readh_n(uint32_t address) {
    arm_access_bus(address, ARM_HWORD_SZ, NON_SEQ);

    return arm_readh(address);
}

uint32_t arm_read_n(uint32_t address) {
    arm_access_bus(address, ARM_WORD_SZ, NON_SEQ);

    return arm_read(address);
}

uint8_t arm_readb_s(uint32_t address) {
    arm_access_bus(address, ARM_BYTE_SZ, SEQUENTIAL);

    return arm_readb(address);
}

uint32_t arm_readh_s(uint32_t address) {
    arm_access_bus(address, ARM_HWORD_SZ, SEQUENTIAL);

    return arm_readh(address);
}

uint32_t arm_read_s(uint32_t address) {
    arm_access_bus(address, ARM_WORD_SZ, SEQUENTIAL);

    return arm_read(address);
}

//Memory write
static void wram_write(uint32_t address, uint8_t value) {
    wram[address & 0x3ffff] = value;
}

static void iwram_write(uint32_t address, uint8_t value) {
    iwram[address & 0x7fff] = value;
}

static void pram_write(uint32_t address, uint8_t value) {
    pram[address & 0x3ff] = value;

    address &= 0x3fe;

    uint16_t pixel = pram[address] | (pram[address + 1] << 8);

    uint8_t r = ((pixel >>  0) & 0x1f) << 3;
    uint8_t g = ((pixel >>  5) & 0x1f) << 3;
    uint8_t b = ((pixel >> 10) & 0x1f) << 3;

    uint32_t rgba = 0xff;

    rgba |= (r | (r >> 5)) <<  8;
    rgba |= (g | (g >> 5)) << 16;
    rgba |= (b | (b >> 5)) << 24;

    palette[address >> 1] = rgba;
}

static void vram_write(uint32_t address, uint8_t value) {
    vram[address & (address & 0x10000 ? 0x17fff : 0x1ffff)] = value;
}

static void oam_write(uint32_t address, uint8_t value) {
    oam[address & 0x3ff] = value;
}

static void eeprom_write(uint32_t address, uint8_t offset, uint8_t value) {
    if (!offset &&
    ((cart_rom_size >  0x1000000 && (address >>  8) == 0x0dffff) ||
     (cart_rom_size <= 0x1000000 && (address >> 24) == 0x00000d))) {
        if (eeprom_idx == 0) {
            //First write, erase buffer
            eeprom_read = false;

            uint16_t i;

            for (i = 0; i < 0x100; i++)
                eeprom_buff[i] = 0;
        }

        uint8_t idx = (eeprom_idx >> 3) & 0xff;
        uint8_t bit = (eeprom_idx >> 0) & 0x7;

        eeprom_buff[idx] |= (value & 1) << (bit ^ 7);

        if (++eeprom_idx == dma_ch[3].count.w) {
            //Last write, process buffer
            uint8_t mode = eeprom_buff[0] >> 6;

            //Value is only valid if bit 1 is set (2 or 3, READ = 2)
            if (mode & EEPROM_READ) {
                bool eep_512b = (eeprom_idx == 2 + 6 + (mode == EEPROM_WRITE ? 64 : 0) + 1);

                if (eep_512b)
                    eeprom_addr =   eeprom_buff[0] & 0x3f;
                else
                    eeprom_addr = ((eeprom_buff[0] & 0x3f) << 8) | eeprom_buff[1];

                eeprom_addr <<= 3;

                if (mode == EEPROM_WRITE) {
                    //Perform write to actual EEPROM buffer
                    uint8_t buff_addr = eep_512b ? 1 : 2;

                    uint64_t value = *(uint64_t *)(eeprom_buff + buff_addr);

                    *(uint64_t *)(eeprom + eeprom_addr) = value;
                } else {
                    eeprom_addr_read = eeprom_addr;
                }

                eeprom_idx = 0;
            }
        }

        eeprom_used = true;
    }
}

static void flash_write(uint32_t address, uint8_t value) {
    if (flash_mode == WRITE) {
        flash[flash_bank | (address & 0xffff)] = value;

        flash_mode = IDLE;
    } else if (flash_mode == BANK_SWITCH && address == 0x0e000000) {
        flash_bank = (value & 1) << 16;

        flash_mode = IDLE;
    } else if (sram[0x5555] == 0xaa && sram[0x2aaa] == 0x55) {
        if (address == 0x0e005555) {
            //Command to do something on Flash ROM
            switch (value) {
                //Erase all
                case 0x10:
                    if (flash_mode == ERASE) {
                        uint32_t idx;

                        for (idx = 0; idx < 0x20000; idx++) {
                            flash[idx] = 0xff;
                        }

                        flash_mode = IDLE;
                    }
                break;

                case 0x80: flash_mode    = ERASE;       break;
                case 0x90: flash_id_mode = true;        break;
                case 0xa0: flash_mode    = WRITE;       break;
                case 0xb0: flash_mode    = BANK_SWITCH; break;
                case 0xf0: flash_id_mode = false;       break;
            }

            //We try to guess that the game uses flash if it
            //writes the specific flash commands to the save memory region
            if (flash_mode || flash_id_mode) {
                flash_used = true;
            }
        } else if (flash_mode == ERASE && value == 0x30) {
            uint32_t bank_s = address & 0xf000;
            uint32_t bank_e = bank_s + 0x1000;
            uint32_t idx;

            for (idx = bank_s; idx < bank_e; idx++) {
                flash[flash_bank | idx] = 0xff;
            }

            flash_mode = IDLE;
        }
    }

    sram[address & 0xffff] = value;
}

static void arm_write_(uint32_t address, uint8_t offset, uint8_t value) {
    switch (address >> 24) {
        case 0x2: wram_write(address, value); break;
        case 0x3: iwram_write(address, value); break;
        case 0x4: io_write(address, value); break;
        case 0x5: pram_write(address, value); break;
        case 0x6: vram_write(address, value); break;
        case 0x7: oam_write(address, value); break;

        case 0xc:
        case 0xd:
            eeprom_write(address, offset, value); break;

        case 0xe:
        case 0xf:
            flash_write(address, value); break;
    }
}

void arm_writeb(uint32_t address, uint8_t value) {
    uint8_t ah = address >> 24;

    if (ah == 7) return; //OAM doesn't supposrt 8 bits writes

    if (ah > 4 && ah < 8) {
        arm_write_(address + 0, 0, value);
        arm_write_(address + 1, 1, value);
    } else {
        arm_write_(address, 0, value);
    }
}

void arm_writeh(uint32_t address, uint16_t value) {
    uint32_t a = address & ~1;

    arm_write_(a | 0, 0, (uint8_t)(value >> 0));
    arm_write_(a | 1, 1, (uint8_t)(value >> 8));
}

void arm_write(uint32_t address, uint32_t value) {
    uint32_t a = address & ~3;

    arm_write_(a | 0, 0, (uint8_t)(value >>  0));
    arm_write_(a | 1, 1, (uint8_t)(value >>  8));
    arm_write_(a | 2, 2, (uint8_t)(value >> 16));
    arm_write_(a | 3, 3, (uint8_t)(value >> 24));
}

void arm_writeb_n(uint32_t address, uint8_t value) {
    arm_access_bus(address, ARM_BYTE_SZ, NON_SEQ);

    arm_writeb(address, value);
}

void arm_writeh_n(uint32_t address, uint16_t value) {
    arm_access_bus(address, ARM_HWORD_SZ, NON_SEQ);

    arm_writeh(address, value);
}

void arm_write_n(uint32_t address, uint32_t value) {
    arm_access_bus(address, ARM_WORD_SZ, NON_SEQ);

    arm_write(address, value);
}

void arm_writeb_s(uint32_t address, uint8_t value) {
    arm_access_bus(address, ARM_BYTE_SZ, SEQUENTIAL);

    arm_writeb(address, value);
}

void arm_writeh_s(uint32_t address, uint16_t value) {
    arm_access_bus(address, ARM_HWORD_SZ, SEQUENTIAL);

    arm_writeh(address, value);
}

void arm_write_s(uint32_t address, uint32_t value) {
    arm_access_bus(address, ARM_WORD_SZ, SEQUENTIAL);

    arm_write(address, value);
}

/*
 * Utils
 */

static void arm_flag_set(uint32_t flag, bool cond) {
    if (cond)
        arm_r.cpsr |= flag;
    else
        arm_r.cpsr &= ~flag;
}

static void arm_bank_to_regs(int8_t mode) {
    if (mode != ARM_FIQ) {
        arm_r.r[8]  = arm_r.r8_usr;
        arm_r.r[9]  = arm_r.r9_usr;
        arm_r.r[10] = arm_r.r10_usr;
        arm_r.r[11] = arm_r.r11_usr;
        arm_r.r[12] = arm_r.r12_usr;
    }

    switch (mode) {
        case ARM_USR:
        case ARM_SYS:
            arm_r.r[13] = arm_r.r13_usr;
            arm_r.r[14] = arm_r.r14_usr;
        break;

        case ARM_FIQ:
            arm_r.r[8]  = arm_r.r8_fiq;
            arm_r.r[9]  = arm_r.r9_fiq;
            arm_r.r[10] = arm_r.r10_fiq;
            arm_r.r[11] = arm_r.r11_fiq;
            arm_r.r[12] = arm_r.r12_fiq;
            arm_r.r[13] = arm_r.r13_fiq;
            arm_r.r[14] = arm_r.r14_fiq;
        break;

        case ARM_IRQ:
            arm_r.r[13] = arm_r.r13_irq;
            arm_r.r[14] = arm_r.r14_irq;
        break;

        case ARM_SVC:
            arm_r.r[13] = arm_r.r13_svc;
            arm_r.r[14] = arm_r.r14_svc;
        break;

        case ARM_MON:
            arm_r.r[13] = arm_r.r13_mon;
            arm_r.r[14] = arm_r.r14_mon;
        break;

        case ARM_ABT:
            arm_r.r[13] = arm_r.r13_abt;
            arm_r.r[14] = arm_r.r14_abt;
        break;

        case ARM_UND:
            arm_r.r[13] = arm_r.r13_und;
            arm_r.r[14] = arm_r.r14_und;
        break;
    }
}

static void arm_regs_to_bank(int8_t mode) {
    if (mode != ARM_FIQ) {
        arm_r.r8_usr  = arm_r.r[8];
        arm_r.r9_usr  = arm_r.r[9];
        arm_r.r10_usr = arm_r.r[10];
        arm_r.r11_usr = arm_r.r[11];
        arm_r.r12_usr = arm_r.r[12];
    }

    switch (mode) {
        case ARM_USR:
        case ARM_SYS:
            arm_r.r13_usr = arm_r.r[13];
            arm_r.r14_usr = arm_r.r[14];
        break;

        case ARM_FIQ:
            arm_r.r8_fiq  = arm_r.r[8];
            arm_r.r9_fiq  = arm_r.r[9];
            arm_r.r10_fiq = arm_r.r[10];
            arm_r.r11_fiq = arm_r.r[11];
            arm_r.r12_fiq = arm_r.r[12];
            arm_r.r13_fiq = arm_r.r[13];
            arm_r.r14_fiq = arm_r.r[14];
        break;

        case ARM_IRQ:
            arm_r.r13_irq = arm_r.r[13];
            arm_r.r14_irq = arm_r.r[14];
        break;

        case ARM_SVC:
            arm_r.r13_svc = arm_r.r[13];
            arm_r.r14_svc = arm_r.r[14];
        break;

        case ARM_MON:
            arm_r.r13_mon = arm_r.r[13];
            arm_r.r14_mon = arm_r.r[14];
        break;

        case ARM_ABT:
            arm_r.r13_abt = arm_r.r[13];
            arm_r.r14_abt = arm_r.r[14];
        break;

        case ARM_UND:
            arm_r.r13_und = arm_r.r[13];
            arm_r.r14_und = arm_r.r[14];
        break;
    }
}

static void arm_mode_set(int8_t mode) {
    int8_t curr = arm_r.cpsr & 0x1f;

    arm_r.cpsr &= ~0x1f;
    arm_r.cpsr |= mode;

    arm_regs_to_bank(curr);
    arm_bank_to_regs(mode);
}

static bool arm_flag_tst(uint32_t flag) {
    return arm_r.cpsr & flag;
}

static bool arm_cond(int8_t cond) {
    bool res;

    bool n = arm_flag_tst(ARM_N);
    bool z = arm_flag_tst(ARM_Z);
    bool c = arm_flag_tst(ARM_C);
    bool v = arm_flag_tst(ARM_V);

    switch (cond >> 1) {
        case 0: res = z; break;
        case 1: res = c; break;
        case 2: res = n; break;
        case 3: res = v; break;
        case 4: res = c && !z; break;
        case 5: res = n == v; break;
        case 6: res = !z && n == v; break;
        default: res = true; break;
    }

    if (cond & 1) res = !res;

    return res;
}

static void arm_spsr_get(uint32_t *psr) {
    switch (arm_r.cpsr & 0x1f) {
        case ARM_FIQ: *psr = arm_r.spsr_fiq; break;
        case ARM_IRQ: *psr = arm_r.spsr_irq; break;
        case ARM_SVC: *psr = arm_r.spsr_svc; break;
        case ARM_MON: *psr = arm_r.spsr_mon; break;
        case ARM_ABT: *psr = arm_r.spsr_abt; break;
        case ARM_UND: *psr = arm_r.spsr_und; break;
    }
}

static void arm_spsr_set(uint32_t spsr) {
    switch (arm_r.cpsr & 0x1f) {
        case ARM_FIQ: arm_r.spsr_fiq = spsr; break;
        case ARM_IRQ: arm_r.spsr_irq = spsr; break;
        case ARM_SVC: arm_r.spsr_svc = spsr; break;
        case ARM_MON: arm_r.spsr_mon = spsr; break;
        case ARM_ABT: arm_r.spsr_abt = spsr; break;
        case ARM_UND: arm_r.spsr_und = spsr; break;
    }
}

static void arm_spsr_to_cpsr() {
    int8_t curr = arm_r.cpsr & 0x1f;

    arm_spsr_get(&arm_r.cpsr);

    int8_t mode = arm_r.cpsr & 0x1f;

    arm_regs_to_bank(curr);
    arm_bank_to_regs(mode);
}

static void arm_setn(uint32_t res) {
    arm_flag_set(ARM_N, res & (1 << 31));
}

static void arm_setn64(uint64_t res) {
    arm_flag_set(ARM_N, res & (1ULL << 63));
}

static void arm_setz(uint32_t res) {
    arm_flag_set(ARM_Z, res == 0);
}

static void arm_setz64(uint64_t res) {
    arm_flag_set(ARM_Z, res == 0);
}

static void arm_addc(uint64_t res) {
    arm_flag_set(ARM_C, res > 0xffffffff);
}

static void arm_subc(uint64_t res) {
    arm_flag_set(ARM_C, res < 0x100000000ULL);
}

static void arm_addv(uint32_t lhs, uint32_t rhs, uint32_t res) {
    arm_flag_set(ARM_V, ~(lhs ^ rhs) & (lhs ^ res) & 0x80000000);
}

static void arm_subv(uint32_t lhs, uint32_t rhs, uint32_t res) {
    arm_flag_set(ARM_V, (lhs ^ rhs) & (lhs ^ res) & 0x80000000);
}

static uint32_t arm_saturate(int64_t val, int32_t min, int32_t max, bool q) {
    uint32_t res = (uint32_t)val;

    if (val > max || val < min) {
        if (val > max)
            res = (uint32_t)max;
        else
            res = (uint32_t)min;

        if (q) arm_flag_set(ARM_Q, true);
    }

    return res;
}

static uint32_t arm_ssatq(int64_t val) {
    return arm_saturate(val, 0x80000000, 0x7fffffff, true);
}

static bool arm_in_thumb() {
    return arm_flag_tst(ARM_T);
}

static uint16_t arm_fetchh(access_type_e at) {
    switch (arm_r.r[15] >> 24) {
        case 0x0: arm_cycles += 1; return (bios_op = *(uint16_t *)(bios + (arm_r.r[15] & 0x3fff)));
        case 0x2: arm_cycles += 3; return *(uint16_t *)(wram  + (arm_r.r[15] & 0x3ffff));
        case 0x3: arm_cycles += 1; return *(uint16_t *)(iwram + (arm_r.r[15] & 0x7fff));
        case 0x5: arm_cycles += 1; return *(uint16_t *)(pram  + (arm_r.r[15] & 0x3ff));
        case 0x7: arm_cycles += 1; return *(uint16_t *)(oam   + (arm_r.r[15] & 0x3ff));

        case 0x8:
        case 0x9:
            if (at == NON_SEQ)
                arm_cycles += ws_n_t16[0];
            else
                arm_cycles += ws_s_t16[0];

            return *(uint16_t *)(rom + (arm_r.r[15] & 0x1ffffff));

        case 0xa:
        case 0xb:
            if (at == NON_SEQ)
                arm_cycles += ws_n_t16[1];
            else
                arm_cycles += ws_s_t16[1];

            return *(uint16_t *)(rom + (arm_r.r[15] & 0x1ffffff));

        case 0xc:
        case 0xd:
            if (at == NON_SEQ)
                arm_cycles += ws_n_t16[2];
            else
                arm_cycles += ws_s_t16[2];

            return *(uint16_t *)(rom + (arm_r.r[15] & 0x1ffffff));

        default:
            if (at == NON_SEQ)
                return arm_readh_n(arm_r.r[15]);
            else
                return arm_readh_s(arm_r.r[15]);
    }
}

static uint32_t arm_fetch(access_type_e at) {
    switch (arm_r.r[15] >> 24) {
        case 0x0: arm_cycles += 1; return (bios_op = *(uint32_t *)(bios + (arm_r.r[15] & 0x3fff)));
        case 0x2: arm_cycles += 6; return *(uint32_t *)(wram  + (arm_r.r[15] & 0x3ffff));
        case 0x3: arm_cycles += 1; return *(uint32_t *)(iwram + (arm_r.r[15] & 0x7fff));
        case 0x5: arm_cycles += 1; return *(uint32_t *)(pram  + (arm_r.r[15] & 0x3ff));
        case 0x7: arm_cycles += 1; return *(uint32_t *)(oam   + (arm_r.r[15] & 0x3ff));

        case 0x8:
        case 0x9:
            if (at == NON_SEQ)
                arm_cycles += ws_n_arm[0];
            else
                arm_cycles += ws_s_arm[0];

            return *(uint32_t *)(rom + (arm_r.r[15] & 0x1ffffff));

        case 0xa:
        case 0xb:
            if (at == NON_SEQ)
                arm_cycles += ws_n_arm[1];
            else
                arm_cycles += ws_s_arm[1];

            return *(uint32_t *)(rom + (arm_r.r[15] & 0x1ffffff));

        case 0xc:
        case 0xd:
            if (at == NON_SEQ)
                arm_cycles += ws_n_arm[2];
            else
                arm_cycles += ws_s_arm[2];

            return *(uint32_t *)(rom + (arm_r.r[15] & 0x1ffffff));

        default:
            if (at == NON_SEQ)
                return arm_read_n(arm_r.r[15]);
            else
                return arm_read_s(arm_r.r[15]);
    }
}

static uint32_t arm_fetch_n() {
    uint32_t op;

    if (arm_in_thumb()) {
        op = arm_fetchh(NON_SEQ);

        arm_r.r[15] += 2;
    } else {
        op = arm_fetch(NON_SEQ);

        arm_r.r[15] += 4;
    }

    return op;
}

static uint32_t arm_fetch_s() {
    uint32_t op;

    if (arm_in_thumb()) {
        op = arm_fetchh(SEQUENTIAL);

        arm_r.r[15] += 2;
    } else {
        op = arm_fetch(SEQUENTIAL);

        arm_r.r[15] += 4;
    }

    return op;
}

static void arm_load_pipe() {
    arm_pipe[0] = arm_fetch_n();
    arm_pipe[1] = arm_fetch_s();

    pipe_reload = true;
}

static void arm_r15_align() {
    if (arm_in_thumb())
        arm_r.r[15] &= ~1;
    else
        arm_r.r[15] &= ~3;
}

static void arm_interwork() {
    arm_flag_set(ARM_T, arm_r.r[15] & 1);

    arm_r15_align();
    arm_load_pipe();
}

static void arm_cycles_s_to_n() {
    if (arm_r.r[15] & 0x08000000) {
        uint8_t idx = (arm_r.r[15] >> 25) & 3;

        if (arm_in_thumb()) {
            arm_cycles -= ws_s_t16[idx];
            arm_cycles += ws_n_t16[idx];
        } else {
            arm_cycles -= ws_s_arm[idx];
            arm_cycles += ws_n_arm[idx];
        }
    }
}


//Data Processing (Arithmetic)
typedef struct {
    uint64_t lhs;
    uint64_t rhs;
    uint8_t  rd;
    bool     cout;
    bool     s;
} arm_data_t;

#define ARM_ARITH_SUB  0
#define ARM_ARITH_ADD  1

#define ARM_ARITH_NO_C   0
#define ARM_ARITH_CARRY  1

#define ARM_ARITH_NO_REV   0
#define ARM_ARITH_REVERSE  1

static void arm_arith_set(arm_data_t op, uint64_t res, bool add) {
    if (op.rd == 15) {
        if (op.s) {
            arm_spsr_to_cpsr();
            arm_r15_align();
            arm_load_pipe();
            arm_check_irq();
        } else {
            arm_r15_align();
            arm_load_pipe();
        }
    } else if (op.s) {
        arm_setn(res);
        arm_setz(res);

        if (add) {
            arm_addc(res);
            arm_addv(op.lhs, op.rhs, res);
        } else {
            arm_subc(res);
            arm_subv(op.lhs, op.rhs, res);
        }
    }
}

static void arm_arith_add(arm_data_t op, bool adc) {
    uint64_t res  = op.lhs + op.rhs;
    if (adc) res += arm_flag_tst(ARM_C);

    arm_r.r[op.rd] = res;

    arm_arith_set(op, res, ARM_ARITH_ADD);
}

static void arm_arith_subtract(arm_data_t op, bool sbc, bool rev) {
    if (rev) {
        uint64_t tmp = op.lhs;

        op.lhs = op.rhs;
        op.rhs = tmp;
    }

    uint64_t res  = op.lhs - op.rhs;
    if (sbc) res -= !arm_flag_tst(ARM_C);

    arm_r.r[op.rd] = res;

    arm_arith_set(op, res, ARM_ARITH_SUB);
}

static void arm_arith_rsb(arm_data_t op) {
    arm_arith_subtract(op, ARM_ARITH_NO_C, ARM_ARITH_REVERSE);
}

static void arm_arith_rsc(arm_data_t op) {
    arm_arith_subtract(op, ARM_ARITH_CARRY, ARM_ARITH_REVERSE);
}

static void arm_arith_sbc(arm_data_t op) {
    arm_arith_subtract(op, ARM_ARITH_CARRY, ARM_ARITH_NO_REV);
}

static void arm_arith_sub(arm_data_t op) {
    arm_arith_subtract(op, ARM_ARITH_NO_C, ARM_ARITH_NO_REV);
}

static void arm_arith_cmn(arm_data_t op) {
    arm_arith_set(op, op.lhs + op.rhs, ARM_ARITH_ADD);
}

static void arm_arith_cmp(arm_data_t op) {
    arm_arith_set(op, op.lhs - op.rhs, ARM_ARITH_SUB);
}

//Data Processing (Logical)
typedef enum {
    AND,
    BIC,
    EOR,
    MVN,
    ORN,
    ORR,
    SHIFT
} arm_logic_e;

static void arm_logic_set(arm_data_t op, uint32_t res) {
    if (op.rd == 15) {
        if (op.s) {
            arm_spsr_to_cpsr();
            arm_r15_align();
            arm_load_pipe();
            arm_check_irq();
        } else {
            arm_r15_align();
            arm_load_pipe();
        }
    } else if (op.s) {
        arm_setn(res);
        arm_setz(res);

        arm_flag_set(ARM_C, op.cout);
    }
}

static void arm_logic(arm_data_t op, arm_logic_e inst) {
    uint32_t res;

    switch (inst) {
        case AND: res = op.lhs &  op.rhs; break;
        case BIC: res = op.lhs & ~op.rhs; break;
        case EOR: res = op.lhs ^  op.rhs; break;
        case ORN: res = op.lhs | ~op.rhs; break;
        case ORR: res = op.lhs |  op.rhs; break;

        case MVN:   res = ~op.rhs; break;
        case SHIFT: res =  op.rhs; break;
    }

    arm_r.r[op.rd] = res;

    arm_logic_set(op, res);
}

static void arm_asr(arm_data_t op) {
    int64_t val = (int32_t)op.lhs;
    uint8_t sh = op.rhs;

    if (sh > 32) sh = 32;

    op.rhs = val >> sh;
    op.cout = val & (1 << (sh - 1));

    arm_r.r[op.rd] = op.rhs;

    arm_logic_set(op, op.rhs);
}

static void arm_lsl(arm_data_t op) {
    uint64_t val = op.lhs;
    uint8_t sh = op.rhs;

    bool c = arm_flag_tst(ARM_C);

    op.rhs = val;
    op.cout = c;

    if (sh > 32) {
        op.rhs = 0;
        op.cout = 0;
    } else if (sh) {
        op.rhs = val << sh;
        op.cout = val & (1 << (32 - sh));
    }

    arm_r.r[op.rd] = op.rhs;

    arm_logic_set(op, op.rhs);
}

static void arm_lsr(arm_data_t op) {
    uint64_t val = op.lhs;
    uint8_t sh = op.rhs;

    bool c = arm_flag_tst(ARM_C);

    op.rhs = val;
    op.cout = c;

    if (sh > 32) {
        op.rhs = 0;
        op.cout = 0;
    } else if (sh) {
        op.rhs = val >> sh;
        op.cout = val & (1 << (sh - 1));
    }

    arm_r.r[op.rd] = op.rhs;

    arm_logic_set(op, op.rhs);
}

static void arm_ror(arm_data_t op) {
    uint64_t val = op.lhs;
    uint8_t sh = op.rhs;

    bool c = arm_flag_tst(ARM_C);

    op.rhs = val;
    op.cout = c;

    if (sh) {
        sh &= 0x1f;
        if (sh == 0) sh = 32;

        op.rhs = ROR(val, sh);
        op.cout = val & (1 << (sh - 1));
    }

    arm_r.r[op.rd] = op.rhs;

    arm_logic_set(op, op.rhs);
}

static void arm_count_zeros(uint8_t rd) {
    uint8_t rm = arm_op & 0xf;
    uint32_t m = arm_r.r[rm];
    uint32_t i, cnt = 32;

    for (i = 0; i < 32; i++) {
        if (m & (1 << i)) cnt = i ^ 0x1f;
    }

    arm_r.r[rd] = cnt;
}

static void arm_logic_teq(arm_data_t op) {
    arm_logic_set(op, op.lhs ^ op.rhs);
}

static void arm_logic_tst(arm_data_t op) {
    arm_logic_set(op, op.lhs & op.rhs);
}

//Data Processing Operand Decoding
typedef struct {
    uint32_t val;
    bool     cout;
} arm_shifter_t;

#define ARM_FLAG_KEEP  0
#define ARM_FLAG_SET   1

#define ARM_SHIFT_LEFT   0
#define ARM_SHIFT_RIGHT  1

static arm_data_t arm_data_imm_op() {
    uint32_t imm   = (arm_op >>  0) & 0xff;
    uint8_t  shift = (arm_op >>  7) & 0x1e;
    uint8_t  rd    = (arm_op >> 12) & 0xf;
    uint8_t  rn    = (arm_op >> 16) & 0xf;

    imm = ROR(imm, shift);

    arm_data_t op = {
        .rd   = rd,
        .lhs  = arm_r.r[rn],
        .rhs  = imm,
        .cout = imm & (1 << 31),
        .s    = SBIT(arm_op)
    };

    return op;
}

static arm_shifter_t arm_data_regi(uint8_t rm, uint8_t type, uint8_t imm) {
    arm_shifter_t out;

    out.val = arm_r.r[rm];
    out.cout = arm_flag_tst(ARM_C);

    uint32_t m = out.val;
    uint8_t c = out.cout;

    switch (type) {
        case 0: //LSL
            if (imm) {
                out.val = m << imm;
                out.cout = m & (1 << (32 - imm));
            }
            break;

        case 1: //LSR
        case 2: //ASR
            if (imm == 0) imm = 32;

            if (type == 2)
                out.val = (int64_t)((int32_t)m) >> imm;
            else
                out.val = (uint64_t)m >> imm;

            out.cout = m & (1 << (imm - 1));
            break;

        case 3: //ROR
            if (imm) {
                out.val = ROR(m, imm);
                out.cout = m & (1 << (imm - 1));
            } else { //RRX
                out.val = ROR((m & ~1) | c, 1);
                out.cout = m & 1;
            }
            break;
    }

    return out;
}

static arm_data_t arm_data_regi_op() {
    uint8_t rm   = (arm_op >>  0) & 0xf;
    uint8_t type = (arm_op >>  5) & 0x3;
    uint8_t imm  = (arm_op >>  7) & 0x1f;
    uint8_t rd   = (arm_op >> 12) & 0xf;
    uint8_t rn   = (arm_op >> 16) & 0xf;

    arm_shifter_t shift = arm_data_regi(rm, type, imm);

    arm_data_t op = {
        .rd   = rd,
        .lhs  = arm_r.r[rn],
        .rhs  = shift.val,
        .cout = shift.cout,
        .s    = SBIT(arm_op)
    };

    return op;
}

static arm_shifter_t arm_data_regr(uint8_t rm, uint8_t type, uint8_t rs) {
    arm_shifter_t out;

    out.val = arm_r.r[rm];
    out.cout = arm_flag_tst(ARM_C);

    if (rm == 15) out.val += 4;

    uint8_t sh = arm_r.r[rs];

    if (sh >= 32) {
        if (type == 3) sh &= 0x1f;
        if (type == 2 || sh == 0) sh = 32;

        out.val = 0;
        out.cout = 0;
    }

    if (sh && sh <= 32) {
        uint32_t m = arm_r.r[rm];

        switch (type) {
            case 0: //LSL
                out.val = (uint64_t)m << sh;
                out.cout = m & (1 << (32 - sh));
                break;

            case 1: //LSR
            case 2: //ASR
                if (type == 2)
                    out.val = (int64_t)((int32_t)m) >> sh;
                else
                    out.val = (uint64_t)m >> sh;

                out.cout = m & (1 << (sh - 1));
                break;

            case 3: //ROR
                out.val = ROR((uint64_t)m, sh);
                out.cout = m & (1 << (sh - 1));
                break;
        }
    }

    arm_cycles++;

    arm_cycles_s_to_n();

    return out;
}

static arm_data_t arm_data_regr_op() {
    uint8_t rm   = (arm_op >>  0) & 0xf;
    uint8_t type = (arm_op >>  5) & 0x3;
    uint8_t rs   = (arm_op >>  8) & 0xf;
    uint8_t rd   = (arm_op >> 12) & 0xf;
    uint8_t rn   = (arm_op >> 16) & 0xf;

    arm_shifter_t shift = arm_data_regr(rm, type, rs);

    arm_data_t data = {
        .rd   = rd,
        .lhs  = arm_r.r[rn],
        .rhs  = shift.val,
        .cout = shift.cout,
        .s    = SBIT(arm_op)
    };

    return data;
}

static arm_data_t t16_data_imm3_op() {
    uint8_t rd  = (arm_op >> 0) & 0x7;
    uint8_t rn  = (arm_op >> 3) & 0x7;
    uint8_t imm = (arm_op >> 6) & 0x7;

    arm_data_t op  = {
        .rd   = rd,
        .lhs  = arm_r.r[rn],
        .rhs  = imm,
        .cout = false,
        .s    = true
    };

    return op;
}

static arm_data_t t16_data_imm8_op() {
    uint8_t imm = (arm_op >> 0) & 0xff;
    uint8_t rdn = (arm_op >> 8) & 0x7;

    arm_data_t op = {
        .rd   = rdn,
        .lhs  = arm_r.r[rdn],
        .rhs  = imm,
        .cout = false,
        .s    = true
    };

    return op;
}

static arm_data_t t16_data_rdn3_op() {
    uint8_t rdn = (arm_op >> 0) & 7;
    uint8_t rm  = (arm_op >> 3) & 7;

    arm_data_t op = {
        .rd   = rdn,
        .lhs  = arm_r.r[rdn],
        .rhs  = arm_r.r[rm],
        .cout = false,
        .s    = true
    };

    return op;
}

static arm_data_t t16_data_neg_op() {
    uint8_t rd = (arm_op >> 0) & 7;
    uint8_t rm = (arm_op >> 3) & 7;

    arm_data_t op = {
        .rd   = rd,
        .lhs  = 0,
        .rhs  = arm_r.r[rm],
        .cout = false,
        .s    = true
    };

    return op;
}

static arm_data_t t16_data_reg_op() {
    uint8_t rd = (arm_op >> 0) & 7;
    uint8_t rn = (arm_op >> 3) & 7;
    uint8_t rm = (arm_op >> 6) & 7;

    arm_data_t op = {
        .rd   = rd,
        .lhs  = arm_r.r[rn],
        .rhs  = arm_r.r[rm],
        .cout = false,
        .s    = true
    };

    return op;
}

static arm_data_t t16_data_rdn4_op(bool s) {
    uint8_t rm = (arm_op >> 3) & 0xf;

    uint8_t rdn;

    rdn  = (arm_op >> 0) & 0x7;
    rdn |= (arm_op >> 4) & 0x8;

    arm_data_t op = {
        .rd   = rdn,
        .lhs  = arm_r.r[rdn],
        .rhs  = arm_r.r[rm],
        .cout = false,
        .s    = s
    };

    return op;
}

static arm_data_t t16_data_imm7sp_op() {
    uint16_t imm = (arm_op & 0x7f) << 2;

    arm_data_t op = {
        .rd   = 13,
        .lhs  = arm_r.r[13],
        .rhs  = imm,
        .cout = false,
        .s    = true
    };

    return op;
}

static arm_data_t t16_data_imm8sp_op() {
    arm_data_t op = t16_data_imm8_op();

    op.lhs = arm_r.r[13];
    op.rhs <<= 2;
    op.s = false;

    return op;
}

static arm_data_t t16_data_imm8pc_op() {
    arm_data_t op = t16_data_imm8_op();

    op.lhs = arm_r.r[15] & ~3;
    op.rhs <<= 2;
    op.s = false;

    return op;
}

static arm_data_t t16_data_imm5_op(bool rsh) {
    uint8_t rd  = (arm_op >> 0) & 0x7;
    uint8_t rn  = (arm_op >> 3) & 0x7;
    uint8_t imm = (arm_op >> 6) & 0x1f;

    arm_data_t op = {
        .rd   = rd,
        .lhs  = arm_r.r[rn],
        .rhs  = (rsh && imm == 0 ? 32 : imm),
        .cout = false,
        .s    = true
    };

    return op;
}

//Multiplication
typedef struct {
    uint64_t lhs;
    uint64_t rhs;
    uint8_t  ra;
    uint8_t  rd;
    bool     s;
} arm_mpy_t;

#define ARM_MPY_SIGNED  0
#define ARM_MPY_UNSIGN  1

static void arm_mpy_inc_cycles(uint64_t rhs, bool u) {
    uint32_t m1 = rhs & 0xffffff00;
    uint32_t m2 = rhs & 0xffff0000;
    uint32_t m3 = rhs & 0xff000000;

    if      (m1 == 0 || (!u && m1 == 0xffffff00))
        arm_cycles += 1;
    else if (m2 == 0 || (!u && m2 == 0xffff0000))
        arm_cycles += 2;
    else if (m3 == 0 || (!u && m3 == 0xff000000))
        arm_cycles += 3;
    else
        arm_cycles += 4;

    arm_cycles_s_to_n();
}

static void arm_mpy_add(arm_mpy_t op) {
    uint32_t a = arm_r.r[op.ra];

    uint32_t res = a + op.lhs * op.rhs;

    arm_r.r[op.rd] = res;

    if (op.s) {
        arm_setn(res);
        arm_setz(res);
    }

    arm_mpy_inc_cycles(op.rhs, ARM_MPY_SIGNED);

    arm_cycles++;
}

static void arm_mpy(arm_mpy_t op) {
    uint32_t res = op.lhs * op.rhs;

    arm_r.r[op.rd] = res;

    if (op.s) {
        arm_setn(res);
        arm_setz(res);
    }

    arm_mpy_inc_cycles(op.rhs, ARM_MPY_SIGNED);
}

static void arm_mpy_smla__(arm_mpy_t op, bool m, bool n) {
    if (m) op.lhs >>= 16;
    if (n) op.rhs >>= 16;

    int64_t l = (int16_t)op.lhs;
    int64_t r = (int16_t)op.rhs;

    int64_t res = arm_r.r[op.ra] + l * r;

    arm_r.r[op.rd] = res;

    int32_t d = arm_r.r[op.rd];

    if (d != res) arm_flag_set(ARM_Q, true);

    arm_mpy_inc_cycles(op.rhs, ARM_MPY_SIGNED);

    arm_cycles++;
}

static void arm_mpy_smlal(arm_mpy_t op) {
    int64_t l = (int32_t)op.lhs;
    int64_t r = (int32_t)op.rhs;

    int64_t a = arm_r.r[op.ra];
    int64_t d = arm_r.r[op.rd];

    a |= d << 32;

    int64_t res = a + l * r;

    arm_r.r[op.ra] = res;
    arm_r.r[op.rd] = res >> 32;

    if (op.s) {
        arm_setn64(res);
        arm_setz64(res);
    }

    arm_mpy_inc_cycles(op.rhs, ARM_MPY_SIGNED);

    arm_cycles += 2;
}

static void arm_mpy_smlal__(arm_mpy_t op, bool m, bool n) {
    if (m) op.lhs >>= 16;
    if (n) op.rhs >>= 16;

    int64_t l = (int16_t)op.lhs;
    int64_t r = (int16_t)op.rhs;

    int64_t a = arm_r.r[op.ra];
    int64_t d = arm_r.r[op.rd];

    a |= d << 32;

    int64_t res = a + l * r;

    arm_r.r[op.ra] = res;
    arm_r.r[op.rd] = res >> 32;

    arm_mpy_inc_cycles(op.rhs, ARM_MPY_SIGNED);

    arm_cycles += 2;
}

static void arm_mpy_smlaw_(arm_mpy_t op, bool m) {
    if (m) op.lhs >>= 16;

    int64_t l = (int16_t)op.lhs;
    int64_t a = arm_r.r[op.ra];

    int64_t res = (a << 16) + l * op.rhs;

    arm_r.r[op.rd] = res >> 16;

    int32_t d = (int32_t)arm_r.r[op.rd];

    if (d != res) arm_flag_set(ARM_Q, true);

    arm_mpy_inc_cycles(op.rhs, ARM_MPY_SIGNED);

    arm_cycles++;
}

static void arm_mpy_smul(arm_mpy_t op, bool m, bool n) {
    if (m) op.lhs >>= 16;
    if (n) op.rhs >>= 16;

    int64_t l = (int16_t)op.lhs;
    int64_t r = (int16_t)op.rhs;

    int64_t res = l * r;

    arm_r.r[op.rd] = res;

    arm_mpy_inc_cycles(op.rhs, ARM_MPY_SIGNED);
}

static void arm_mpy_smull(arm_mpy_t op) {
    int64_t l = (int32_t)op.lhs;
    int64_t r = (int32_t)op.rhs;

    int64_t res = l * r;

    arm_r.r[op.ra] = res;
    arm_r.r[op.rd] = res >> 32;

    if (op.s) {
        arm_setn64(res);
        arm_setz64(res);
    }

    arm_mpy_inc_cycles(op.rhs, ARM_MPY_SIGNED);

    arm_cycles++;
}

static void arm_mpy_smulw_(arm_mpy_t op, bool m) {
    if (m) op.lhs >>= 16;

    int64_t res =  (int16_t)op.lhs * op.rhs;

    arm_r.r[op.rd] = res >> 16;

    arm_mpy_inc_cycles(op.rhs, ARM_MPY_SIGNED);
}

static void arm_mpy_umlal(arm_mpy_t op) {
    uint64_t l = op.lhs;
    uint64_t r = op.rhs;

    uint64_t a = arm_r.r[op.ra];
    uint64_t d = arm_r.r[op.rd];

    a |= d << 32;

    uint64_t res = a + l * r;

    arm_r.r[op.ra] = res;
    arm_r.r[op.rd] = res >> 32;

    if (op.s) {
        arm_setn64(res);
        arm_setz64(res);
    }

    arm_mpy_inc_cycles(op.rhs, ARM_MPY_UNSIGN);

    arm_cycles += 2;
}

static void arm_mpy_umull(arm_mpy_t op) {
    uint64_t l = op.lhs;
    uint64_t r = op.rhs;

    uint64_t res = l * r;

    arm_r.r[op.ra] = res;
    arm_r.r[op.rd] = res >> 32;

    if (op.s) {
        arm_setn64(res);
        arm_setz64(res);
    }

    arm_mpy_inc_cycles(op.rhs, ARM_MPY_UNSIGN);

    arm_cycles++;
}

static arm_mpy_t arm_mpy_op() {
    uint8_t rn = (arm_op >>  0) & 0xf;
    uint8_t rm = (arm_op >>  8) & 0xf;
    uint8_t ra = (arm_op >> 12) & 0xf;
    uint8_t rd = (arm_op >> 16) & 0xf;
    bool    s  = (arm_op >> 20) & 0x1;

    arm_mpy_t op = {
        .lhs = arm_r.r[rn],
        .rhs = arm_r.r[rm],
        .ra  = ra,
        .rd  = rd,
        .s   = s
    };

    return op;
}

static arm_mpy_t t16_mpy_op() {
    uint8_t rdn = (arm_op >> 0) & 7;
    uint8_t rm  = (arm_op >> 3) & 7;

    arm_mpy_t op = {
        .rd   = rdn,
        .lhs  = arm_r.r[rdn],
        .rhs  = arm_r.r[rm],
        .s    = true
    };

    return op;
}

//Processor State
#define PRIV_MASK   0xf8ff03df
#define USR_MASK    0xf8ff0000
#define STATE_MASK  0x01000020

typedef struct {
    uint8_t  rd;
    uint32_t psr;
    uint32_t mask;
    bool     r;
} arm_psr_t;

static void arm_psr_to_reg(arm_psr_t op) {
    if (op.r) {
        arm_spsr_get(&arm_r.r[op.rd]);
    } else {
        if ((arm_r.cpsr & 0x1f) == ARM_USR)
            arm_r.r[op.rd] = arm_r.cpsr & USR_MASK;
        else
            arm_r.r[op.rd] = arm_r.cpsr & PRIV_MASK;
    }
}

static void arm_reg_to_psr(arm_psr_t op) {
    uint32_t spsr = 0;
    uint32_t mask = 0;

    if (op.mask & 1) mask  = 0x000000ff;
    if (op.mask & 2) mask |= 0x0000ff00;
    if (op.mask & 4) mask |= 0x00ff0000;
    if (op.mask & 8) mask |= 0xff000000;

    uint32_t sec_msk;

    if ((arm_r.cpsr & 0x1f) == ARM_USR)
        sec_msk = USR_MASK;
    else
        sec_msk = PRIV_MASK;

    if (op.r) sec_msk |= STATE_MASK;

    mask &= sec_msk;

    op.psr &= mask;

    if (op.r) {
        arm_spsr_get(&spsr);
        arm_spsr_set((spsr & ~mask) | op.psr);
    } else {
        int8_t curr = arm_r.cpsr & 0x1f;
        int8_t mode = op.psr     & 0x1f;

        arm_r.cpsr &= ~mask;
        arm_r.cpsr |= op.psr;

        arm_regs_to_bank(curr);
        arm_bank_to_regs(mode);

        arm_check_irq();
    }
}

static arm_psr_t arm_mrs_op() {
    arm_psr_t op = {
        .rd = (arm_op >> 12) & 0xf,
        .r  = (arm_op >> 22) & 0x1
    };

    return op;
}

static arm_psr_t arm_msr_imm_op() {
    arm_data_t data = arm_data_imm_op();

    arm_psr_t op = {
        .psr  = data.rhs,
        .mask = (arm_op >> 16) & 0xf,
        .r    = (arm_op >> 22) & 0x1
    };

    return op;
}

static arm_psr_t arm_msr_reg_op() {
    uint8_t rm   = (arm_op >>  0) & 0xf;
    uint8_t mask = (arm_op >> 16) & 0xf;
    bool    r    = (arm_op >> 22) & 0x1;

    arm_psr_t op = {
        .psr  = arm_r.r[rm],
        .mask = mask,
        .r    = r
    };

    return op;
}

//Load/Store
typedef struct {
    uint16_t regs;
    uint8_t  rn;
    uint8_t  rt2;
    uint8_t  rt;
    uint32_t addr;
    int32_t  disp;
} arm_memio_t;

typedef enum {
    BYTE  = 1,
    HWORD = 2,
    WORD  = 4,
    DWORD = 8
} arm_size_e;

static uint32_t arm_memio_reg_get(uint8_t reg) {
    if (reg == 15)
        return arm_r.r[15] + 4;
    else
        return arm_r.r[reg];
}

static void arm_memio_ldm(arm_memio_t op) {
    uint8_t i;

    arm_r.r[op.rn] += op.disp;

    for (i = 0; i < 16; i++) {
        if (op.regs & (1 << i)) {
            arm_r.r[i] = arm_read_s(op.addr);

            op.addr += 4;
        }
    }

    if (op.regs & 0x8000) {
        arm_r15_align();
        arm_load_pipe();
    }

    arm_cycles++;

    arm_cycles_s_to_n();
}

static void arm_memio_ldm_usr(arm_memio_t op) {
    if (arm_op & 0x8000) {
        arm_memio_ldm(op);
        arm_spsr_to_cpsr();
        arm_check_irq();
    } else {
        int8_t mode = arm_r.cpsr & 0x1f;

        arm_mode_set(ARM_USR);
        arm_memio_ldm(op);
        arm_mode_set(mode);
    }
}

static void arm_memio_stm(arm_memio_t op) {
    bool first = true;
    uint8_t i;

    for (i = 0; i < 16; i++) {
        if (op.regs & (1 << i)) {
            arm_write_s(op.addr, arm_memio_reg_get(i));

            if (first) {
                arm_r.r[op.rn] += op.disp;

                first = false;
            }

            op.addr += 4;
        }
    }

    arm_cycles_s_to_n();
}

static void arm_memio_stm_usr(arm_memio_t op) {
    int8_t mode = arm_r.cpsr & 0x1f;

    arm_mode_set(ARM_USR);
    arm_memio_stm(op);
    arm_mode_set(mode);
}

static void arm_memio_ldr(arm_memio_t op) {
    arm_r.r[op.rt] = arm_read_n(op.addr);

    if (op.rt == 15) {
        arm_r15_align();
        arm_load_pipe();
    }

    arm_cycles++;

    arm_cycles_s_to_n();
}

static void arm_memio_ldrb(arm_memio_t op) {
    arm_r.r[op.rt] = arm_readb_n(op.addr);

    if (op.rt == 15) {
        arm_r15_align();
        arm_load_pipe();
    }

    arm_cycles++;

    arm_cycles_s_to_n();
}

static void arm_memio_ldrh(arm_memio_t op) {
    arm_r.r[op.rt] = arm_readh_n(op.addr);

    if (op.rt == 15) {
        arm_r15_align();
        arm_load_pipe();
    }

    arm_cycles++;

    arm_cycles_s_to_n();
}

static void arm_memio_ldrsb(arm_memio_t op) {
    int32_t value = arm_readb_n(op.addr);

    value <<= 24;
    value >>= 24;

    arm_r.r[op.rt] = value;

    if (op.rt == 15) {
        arm_r15_align();
        arm_load_pipe();
    }

    arm_cycles++;

    arm_cycles_s_to_n();
}

static void arm_memio_ldrsh(arm_memio_t op) {
    int32_t value = arm_readh_n(op.addr);

    value <<= (op.addr & 1 ? 24 : 16);
    value >>= (op.addr & 1 ? 24 : 16);

    arm_r.r[op.rt] = value;

    if (op.rt == 15) {
        arm_r15_align();
        arm_load_pipe();
    }

    arm_cycles++;

    arm_cycles_s_to_n();
}

static void arm_memio_ldrd(arm_memio_t op) {
    arm_r.r[op.rt]  = arm_read_n(op.addr + 0);
    arm_r.r[op.rt2] = arm_read_s(op.addr + 4);

    if (op.rt2 == 15) {
        arm_r15_align();
        arm_load_pipe();
    }

    arm_cycles++;

    arm_cycles_s_to_n();
}

static void arm_memio_str(arm_memio_t op) {
    arm_write_n(op.addr, arm_memio_reg_get(op.rt));

    arm_cycles_s_to_n();
}

static void arm_memio_strb(arm_memio_t op) {
    arm_writeb_n(op.addr, arm_memio_reg_get(op.rt));

    arm_cycles_s_to_n();
}

static void arm_memio_strh(arm_memio_t op) {
    arm_writeh_n(op.addr, arm_memio_reg_get(op.rt));

    arm_cycles_s_to_n();
}

static void arm_memio_strd(arm_memio_t op) {
    arm_write_n(op.addr + 0, arm_memio_reg_get(op.rt));
    arm_write_s(op.addr + 4, arm_memio_reg_get(op.rt2));

    arm_cycles_s_to_n();
}

static void arm_memio_load_usr(arm_memio_t op, arm_size_e size) {
    int8_t mode = arm_r.cpsr & 0x1f;

    arm_mode_set(ARM_USR);

    switch (size) {
        case BYTE:  arm_memio_ldrb(op); break;
        case HWORD: arm_memio_ldrh(op); break;
        case WORD:  arm_memio_ldr(op);  break;
        case DWORD: arm_memio_ldrd(op); break;
    }

    arm_mode_set(mode);
}

static void arm_memio_store_usr(arm_memio_t op, arm_size_e size) {
    int8_t mode = arm_r.cpsr & 0x1f;

    arm_mode_set(ARM_USR);

    switch (size) {
        case BYTE:  arm_memio_strb(op); break;
        case HWORD: arm_memio_strh(op); break;
        case WORD:  arm_memio_str(op);  break;
        case DWORD: arm_memio_strd(op); break;
    }

    arm_mode_set(mode);
}

static void arm_memio_ldrbt(arm_memio_t op) {
    arm_memio_load_usr(op, BYTE);
}

static void arm_memio_strbt(arm_memio_t op) {
    arm_memio_store_usr(op, BYTE);
}

static arm_memio_t arm_memio_mult_op() {
    uint16_t regs = (arm_op >>  0) & 0xffff;
    uint8_t  rn   = (arm_op >> 16) & 0xf;
    bool     w    = (arm_op >> 21) & 0x1;
    bool     u    = (arm_op >> 23) & 0x1;
    bool     p    = (arm_op >> 24) & 0x1;

    uint8_t i, cnt = 0;

    for (i = 0; i < 16; i++) {
        if (regs & (1 << i)) cnt += 4;
    }

    arm_memio_t op = {
        .regs = regs,
        .rn   = rn,
        .addr = arm_r.r[rn] & ~3
    };

    if (!u)     op.addr -= cnt;
    if (u == p) op.addr += 4;

    if (w) {
        if (u)
            op.disp =  cnt;
        else
            op.disp = -cnt;
    } else {
        op.disp = 0;
    }

    return op;
}

static arm_memio_t arm_memio_imm_op() {
    uint16_t imm = (arm_op >>  0) & 0xfff;
    uint8_t  rt  = (arm_op >> 12) & 0xf;
    uint8_t  rn  = (arm_op >> 16) & 0xf;
    bool     w   = (arm_op >> 21) & 0x1;
    bool     u   = (arm_op >> 23) & 0x1;
    bool     p   = (arm_op >> 24) & 0x1;

    arm_memio_t op = {
        .rt   = rt,
        .addr = arm_r.r[rn]
    };

    if (rn == 15) op.addr &= ~3;

    int32_t disp;

    if (u)
        disp =  imm;
    else
        disp = -imm;

    if (p)       op.addr     += disp;
    if (!p || w) arm_r.r[rn] += disp;

    return op;
}

static arm_memio_t arm_memio_reg_op() {
    uint8_t rm   = (arm_op >>  0) & 0xf;
    uint8_t type = (arm_op >>  5) & 0x3;
    uint8_t imm  = (arm_op >>  7) & 0x1f;
    uint8_t rt   = (arm_op >> 12) & 0xf;
    uint8_t rn   = (arm_op >> 16) & 0xf;
    bool    w    = (arm_op >> 21) & 0x1;
    bool    u    = (arm_op >> 23) & 0x1;
    bool    p    = (arm_op >> 24) & 0x1;

    arm_memio_t op = {
        .rt   = rt,
        .addr = arm_r.r[rn]
    };

    if (rn == 15) op.addr &= ~3;

    arm_shifter_t shift = arm_data_regi(rm, type, imm);

    int32_t disp;

    if (u)
        disp =  shift.val;
    else
        disp = -shift.val;

    if (p)       op.addr     += disp;
    if (!p || w) arm_r.r[rn] += disp;

    return op;
}

static arm_memio_t arm_memio_immt_op() {
    uint32_t imm = (arm_op >>  0) & 0xfff;
    uint8_t  rt  = (arm_op >> 12) & 0xf;
    uint8_t  rn  = (arm_op >> 16) & 0xf;
    bool     u   = (arm_op >> 23) & 0x1;

    arm_memio_t op = {
        .rt   = rt,
        .addr = arm_r.r[rn]
    };

    if (u)
        arm_r.r[rn] += imm;
    else
        arm_r.r[rn] -= imm;

    return op;
}

static arm_memio_t arm_memio_regt_op() {
    uint8_t rm   = (arm_op >>  0) & 0xf;
    uint8_t type = (arm_op >>  5) & 0x3;
    uint8_t imm  = (arm_op >>  7) & 0x1f;
    uint8_t rt   = (arm_op >> 12) & 0xf;
    uint8_t rn   = (arm_op >> 16) & 0xf;
    bool    u    = (arm_op >> 23) & 0x1;

    arm_memio_t op = {
        .rt   = rt,
        .addr = arm_r.r[rn]
    };

    arm_shifter_t shift = arm_data_regi(rm, type, imm);

    if (u)
        arm_r.r[rn] += shift.val;
    else
        arm_r.r[rn] -= shift.val;

    return op;
}

static arm_memio_t arm_memio_immdh_op() {
    uint8_t  rt  = (arm_op >> 12) & 0xf;
    uint8_t  rn  = (arm_op >> 16) & 0xf;
    bool     w   = (arm_op >> 21) & 0x1;
    bool     u   = (arm_op >> 23) & 0x1;
    bool     p   = (arm_op >> 24) & 0x1;

    arm_memio_t op = {
        .rt   = rt,
        .rt2  = rt | 1,
        .addr = arm_r.r[rn]
    };

    if (rn == 15) op.addr &= ~3;

    uint16_t imm;
    int32_t disp;

    imm  = (arm_op >> 0) & 0xf;
    imm |= (arm_op >> 4) & 0xf0;

    if (u)
        disp =  imm;
    else
        disp = -imm;

    if (p)       op.addr     += disp;
    if (!p || w) arm_r.r[rn] += disp;

    return op;
}

static arm_memio_t arm_memio_regdh_op() {
    uint8_t rm = (arm_op >>  0) & 0xf;
    uint8_t rt = (arm_op >> 12) & 0xf;
    uint8_t rn = (arm_op >> 16) & 0xf;
    bool    w  = (arm_op >> 21) & 0x1;
    bool    u  = (arm_op >> 23) & 0x1;
    bool    p  = (arm_op >> 24) & 0x1;

    arm_memio_t op = {
        .rt   = rt,
        .rt2  = rt | 1,
        .addr = arm_r.r[rn]
    };

    if (rn == 15) op.addr &= ~3;

    int32_t disp;

    if (u)
        disp =  arm_r.r[rm];
    else
        disp = -arm_r.r[rm];

    if (p)       op.addr     += disp;
    if (!p || w) arm_r.r[rn] += disp;

    return op;
}

static arm_memio_t t16_memio_mult_op() {
    uint8_t regs = (arm_op >> 0) & 0xff;
    uint8_t rn   = (arm_op >> 8) & 0x7;

    uint8_t i, cnt = 0;

    for (i = 0; i < 8; i++) {
        if (regs & (1 << i)) cnt += 4;
    }

    arm_memio_t op = {
        .regs = regs,
        .rn   = rn,
        .addr = arm_r.r[rn] & ~3,
        .disp = cnt
    };

    return op;
}

static arm_memio_t t16_memio_imm5_op(int8_t step) {
    uint8_t rt  = (arm_op >> 0) & 0x7;
    uint8_t imm = (arm_op >> 6) & 0x1f;
    uint8_t rn  = (arm_op >> 3) & 0x7;

    uint32_t addr = arm_r.r[rn] + imm * step;

    arm_memio_t op = {
        .rt   = rt,
        .addr = addr
    };

    return op;
}

static arm_memio_t t16_memio_imm8sp_op() {
    uint16_t imm = (arm_op << 2) & 0x3fc;
    uint8_t  rt  = (arm_op >> 8) & 0x7;

    uint32_t addr = arm_r.r[13] + imm;

    arm_memio_t op = {
        .rt   = rt,
        .addr = addr
    };

    return op;
}

static arm_memio_t t16_memio_imm8pc_op() {
    uint16_t imm = (arm_op << 2) & 0x3fc;
    uint8_t  rt  = (arm_op >> 8) & 0x7;

    uint32_t addr = (arm_r.r[15] & ~3) + imm;

    arm_memio_t op = {
        .rt   = rt,
        .addr = addr
    };

    return op;
}

static arm_memio_t t16_memio_reg_op() {
    uint8_t rt = (arm_op >> 0) & 0x7;
    uint8_t rn = (arm_op >> 3) & 0x7;
    uint8_t rm = (arm_op >> 6) & 0x7;

    uint32_t addr = arm_r.r[rn] + arm_r.r[rm];

    arm_memio_t op = {
        .rt   = rt,
        .addr = addr
    };

    return op;
}

//Parallel arithmetic
typedef struct {
    arm_word lhs;
    arm_word rhs;
    uint8_t  rd;
} arm_parith_t;

static void arm_parith_qadd(arm_parith_t op) {
    int64_t lhs = op.lhs.w;
    int64_t rhs = op.rhs.w;

    arm_r.r[op.rd] = arm_ssatq(lhs + rhs);
}

static void arm_parith_qsub(arm_parith_t op) {
    int64_t lhs = op.lhs.w;
    int64_t rhs = op.rhs.w;

    arm_r.r[op.rd] = arm_ssatq(lhs - rhs);
}

static void arm_parith_qdadd(arm_parith_t op) {
    int64_t lhs = op.lhs.w;
    int64_t rhs = op.rhs.w;

    int32_t doubled = arm_ssatq(rhs << 1);

    arm_r.r[op.rd] = arm_ssatq(lhs + doubled);
}

static void arm_parith_qdsub(arm_parith_t op) {
    int64_t lhs = op.lhs.w;
    int64_t rhs = op.rhs.w;

    int32_t doubled = arm_ssatq(rhs << 1);

    arm_r.r[op.rd] = arm_ssatq(lhs - doubled);
}

static arm_parith_t arm_parith_op() {
    uint8_t rm = (arm_op >>  0) & 0xf;
    uint8_t rd = (arm_op >> 12) & 0xf;
    uint8_t rn = (arm_op >> 16) & 0xf;

    arm_parith_t op = {
        .lhs.w = arm_r.r[rm],
        .rhs.w = arm_r.r[rn],
        .rd    = rd
    };

    return op;
}

//Add with Carry
static void arm_adc_imm() {
    arm_arith_add(arm_data_imm_op(), ARM_ARITH_CARRY);
}

static void arm_adc_regi() {
    arm_arith_add(arm_data_regi_op(), ARM_ARITH_CARRY);
}

static void arm_adc_regr() {
    arm_arith_add(arm_data_regr_op(), ARM_ARITH_CARRY);
}

static void t16_adc_rdn3() {
    arm_arith_add(t16_data_rdn3_op(), ARM_ARITH_CARRY);
}

//Add
static void arm_add_imm() {
    arm_arith_add(arm_data_imm_op(), ARM_ARITH_NO_C);
}

static void arm_add_regi() {
    arm_arith_add(arm_data_regi_op(), ARM_ARITH_NO_C);
}

static void arm_add_regr() {
    arm_arith_add(arm_data_regr_op(), ARM_ARITH_NO_C);
}

static void t16_add_imm3() {
    arm_arith_add(t16_data_imm3_op(), ARM_ARITH_NO_C);
}

static void t16_add_imm8() {
    arm_arith_add(t16_data_imm8_op(), ARM_ARITH_NO_C);
}

static void t16_add_reg() {
    arm_arith_add(t16_data_reg_op(), ARM_ARITH_NO_C);
}

static void t16_add_rdn4() {
    arm_arith_add(t16_data_rdn4_op(ARM_FLAG_KEEP), ARM_ARITH_NO_C);
}

static void t16_add_sp7() {
    arm_arith_add(t16_data_imm7sp_op(), ARM_ARITH_NO_C);
}

static void t16_add_sp8() {
    arm_arith_add(t16_data_imm8sp_op(), ARM_ARITH_NO_C);
}

static void t16_adr() {
    arm_arith_add(t16_data_imm8pc_op(), ARM_ARITH_NO_C);
}

//And
static void arm_and_imm() {
    arm_logic(arm_data_imm_op(), AND);
}

static void arm_and_regi() {
    arm_logic(arm_data_regi_op(), AND);
}

static void arm_and_regr() {
    arm_logic(arm_data_regr_op(), AND);
}

static void t16_and_rdn3() {
    arm_logic(t16_data_rdn3_op(), AND);
}

//Bit Shift (ASR, LSL, ...)
static void arm_shift_imm() {
    arm_logic(arm_data_regi_op(), SHIFT);
}

static void arm_shift_reg() {
    arm_logic(arm_data_regr_op(), SHIFT);
}

//Arithmetic Shift Right
static void t16_asr_imm5() {
    arm_asr(t16_data_imm5_op(ARM_SHIFT_RIGHT));
}

static void t16_asr_rdn3() {
    arm_asr(t16_data_rdn3_op());
}

//Branch
static void arm_b() {
    int32_t imm = arm_op;

    imm <<= 8;
    imm >>= 6;

    arm_r.r[15] += imm;

    arm_load_pipe();
}

static void t16_b_imm11() {
    int32_t imm = arm_op;

    imm <<= 21;
    imm >>= 20;

    arm_r.r[15] += imm;

    arm_load_pipe();
}

//Thumb Conditional Branches
static void t16_b_imm8() {
    int32_t imm = arm_op;

    imm <<= 24;
    imm >>= 23;

    int8_t cond = (arm_op >> 8) & 0xf;

    if (arm_cond(cond)) {
        arm_r.r[15] += imm;

        arm_load_pipe();
    }
}

//Bit Clear
static void arm_bic_imm() {
    arm_logic(arm_data_imm_op(), BIC);
}

static void arm_bic_regi() {
    arm_logic(arm_data_regi_op(), BIC);
}

static void arm_bic_regr() {
    arm_logic(arm_data_regr_op(), BIC);
}

static void t16_bic_rdn3() {
    arm_logic(t16_data_rdn3_op(), BIC);
}

//Breakpoint
void arm_bkpt() {
    arm_int(ARM_VEC_PABT, ARM_ABT);
}

void t16_bkpt() {
    arm_int(ARM_VEC_PABT, ARM_ABT);
}

//Branch with Link
static void arm_bl() {
    int32_t imm = arm_op;

    imm <<= 8;
    imm >>= 6;

    arm_r.r[14] =  arm_r.r[15] - ARM_WORD_SZ;
    arm_r.r[15] = (arm_r.r[15] & ~3) + imm;

    arm_load_pipe();
}

//Branch with Link and Exchange
static void arm_blx_imm() {
    int32_t imm = arm_op;

    imm <<= 8;
    imm >>= 6;

    imm |= (arm_op >> 23) & 2;

    arm_flag_set(ARM_T, true);

    arm_r.r[14]  = arm_r.r[15] - ARM_WORD_SZ;
    arm_r.r[15] += imm;

    arm_load_pipe();
}

static void arm_blx_reg() {
    uint8_t rm = arm_op & 0xf;

    arm_r.r[14] = arm_r.r[15] - ARM_WORD_SZ;
    arm_r.r[15] = arm_r.r[rm];

    arm_interwork();
}

static void t16_blx() {
    uint8_t rm = (arm_op >> 3) & 0xf;

    arm_r.r[14] = (arm_r.r[15] - ARM_HWORD_SZ) | 1;
    arm_r.r[15] =  arm_r.r[rm];

    arm_interwork();
}

static void t16_blx_h2() {
    int32_t imm = arm_op;

    imm <<= 21;
    imm >>= 9;

    arm_r.r[14]  = arm_r.r[15];
    arm_r.r[14] += imm;
}

static void t16_blx_lrimm() {
    uint32_t imm = arm_r.r[14];

    imm += (arm_op & 0x7ff) << 1;

    arm_r.r[14] = (arm_r.r[15] - ARM_HWORD_SZ) | 1;
    arm_r.r[15] = imm & ~1;
}

static void t16_blx_h1() {
    t16_blx_lrimm();
    arm_interwork();
}

static void t16_blx_h3() {
    t16_blx_lrimm();
    arm_load_pipe();
}

//Branch and Exchange
static void arm_bx() {
    uint8_t rm = arm_op & 0xf;

    arm_r.r[15] = arm_r.r[rm];

    arm_interwork();
}

static void t16_bx() {
    uint8_t rm = (arm_op >> 3) & 0xf;

    arm_r.r[15] = arm_r.r[rm];

    arm_interwork();
}

//Coprocessor Data Processing
static void arm_cdp() {
    //No coprocessors used
}

static void arm_cdp2() {
    //No coprocessors used
}

//Count Leading Zeros
static void arm_clz() {
    arm_count_zeros((arm_op >> 12) & 0xf);
}

//Compare Negative
static void arm_cmn_imm() {
    arm_arith_cmn(arm_data_imm_op());
}

static void arm_cmn_regi() {
    arm_arith_cmn(arm_data_regi_op());
}

static void arm_cmn_regr() {
    arm_arith_cmn(arm_data_regr_op());
}

static void t16_cmn_rdn3() {
    arm_arith_cmn(t16_data_rdn3_op());
}

//Compare
static void arm_cmp_imm() {
    arm_arith_cmp(arm_data_imm_op());
}

static void arm_cmp_regi() {
    arm_arith_cmp(arm_data_regi_op());
}

static void arm_cmp_regr() {
    arm_arith_cmp(arm_data_regr_op());
}

static void t16_cmp_imm8() {
    arm_arith_cmp(t16_data_imm8_op());
}

static void t16_cmp_rdn3() {
    arm_arith_cmp(t16_data_rdn3_op());
}

static void t16_cmp_rdn4() {
    arm_arith_cmp(t16_data_rdn4_op(true));
}

//Exclusive Or
static void arm_eor_imm() {
    arm_logic(arm_data_imm_op(), EOR);
}

static void arm_eor_regi() {
    arm_logic(arm_data_regi_op(), EOR);
}

static void arm_eor_regr() {
    arm_logic(arm_data_regr_op(), EOR);
}

static void t16_eor_rdn3() {
    arm_logic(t16_data_rdn3_op(), EOR);
}

//Load Coprocessor
static void arm_ldc() {
    //No coprocessors used
}

static void arm_ldc2() {
    //No coprocessors used
}

//Load Multiple
static void arm_ldm() {
    arm_memio_ldm(arm_memio_mult_op());
}

static void arm_ldm_usr() {
    arm_memio_ldm_usr(arm_memio_mult_op());
}

static void t16_ldm() {
    arm_memio_ldm(t16_memio_mult_op());
}

//Load Register
static void arm_ldr_imm() {
    arm_memio_ldr(arm_memio_imm_op());
}

static void arm_ldr_reg() {
    arm_memio_ldr(arm_memio_reg_op());
}

static void t16_ldr_imm5() {
    arm_memio_ldr(t16_memio_imm5_op(ARM_WORD_SZ));
}

static void t16_ldr_sp8() {
    arm_memio_ldr(t16_memio_imm8sp_op());
}

static void t16_ldr_pc8() {
    arm_memio_ldr(t16_memio_imm8pc_op());
}

static void t16_ldr_reg() {
    arm_memio_ldr(t16_memio_reg_op());
}

//Load Register Byte
static void arm_ldrb_imm() {
    arm_memio_ldrb(arm_memio_imm_op());
}

static void arm_ldrb_reg() {
    arm_memio_ldrb(arm_memio_reg_op());
}

static void t16_ldrb_imm5() {
    arm_memio_ldrb(t16_memio_imm5_op(ARM_BYTE_SZ));
}

static void t16_ldrb_reg() {
    arm_memio_ldrb(t16_memio_reg_op());
}

//Load Register Byte Unprivileged
static void arm_ldrbt_imm() {
    arm_memio_ldrbt(arm_memio_immt_op());
}

static void arm_ldrbt_reg() {
    arm_memio_ldrbt(arm_memio_regt_op());
}

//Load Register Dual
static void arm_ldrd_imm() {
    arm_memio_ldrd(arm_memio_immdh_op());
}

static void arm_ldrd_reg() {
    arm_memio_ldrd(arm_memio_regdh_op());
}

//Load Register Halfword
static void arm_ldrh_imm() {
    arm_memio_ldrh(arm_memio_immdh_op());
}

static void arm_ldrh_reg() {
    arm_memio_ldrh(arm_memio_regdh_op());
}

static void t16_ldrh_imm5() {
    arm_memio_ldrh(t16_memio_imm5_op(ARM_HWORD_SZ));
}

static void t16_ldrh_reg() {
    arm_memio_ldrh(t16_memio_reg_op());
}

//Load Register Signed Byte
static void arm_ldrsb_imm() {
    arm_memio_ldrsb(arm_memio_immdh_op());
}

static void arm_ldrsb_reg() {
    arm_memio_ldrsb(arm_memio_regdh_op());
}

static void t16_ldrsb_reg() {
    arm_memio_ldrsb(t16_memio_reg_op());
}

//Load Register Signed Halfword
static void arm_ldrsh_imm() {
    arm_memio_ldrsh(arm_memio_immdh_op());
}

static void arm_ldrsh_reg() {
    arm_memio_ldrsh(arm_memio_regdh_op());
}

static void t16_ldrsh_reg() {
    arm_memio_ldrsh(t16_memio_reg_op());
}

//Logical Shift Left
static void t16_lsl_imm5() {
    arm_lsl(t16_data_imm5_op(ARM_SHIFT_LEFT));
}

static void t16_lsl_rdn3() {
    arm_lsl(t16_data_rdn3_op());
}

//Logical Shift Right
static void t16_lsr_imm5() {
    arm_lsr(t16_data_imm5_op(ARM_SHIFT_RIGHT));
}

static void t16_lsr_rdn3() {
    arm_lsr(t16_data_rdn3_op());
}

//Move to Coprocessor from Register
static void arm_mcr() {
    //No coprocessors used
}

static void arm_mcr2() {
    //No coprocessors used
}

//Move to Coprocessor from Two Registers
static void arm_mcrr() {
    //No coprocessors used
}

static void arm_mcrr2() {
    //No coprocessors used
}

//Multiply Add
static void arm_mla() {
    arm_mpy_add(arm_mpy_op());
}

//Move
static void arm_mov_imm12() {
    arm_data_t op = arm_data_imm_op();

    arm_r.r[op.rd] = op.rhs;

    arm_logic_set(op, op.rhs);
}

static void t16_mov_imm() {
    uint8_t imm = (arm_op >> 0) & 0xff;
    uint8_t rd  = (arm_op >> 8) & 0x7;

    arm_r.r[rd] = imm;

    arm_setn(arm_r.r[rd]);
    arm_setz(arm_r.r[rd]);
}

static void t16_mov_rd4() {
    uint8_t rm = (arm_op >> 3) & 0xf;

    uint8_t rd;

    rd  = (arm_op >> 0) & 7;
    rd |= (arm_op >> 4) & 8;

    arm_r.r[rd] = arm_r.r[rm];

    if (rd == 15) {
        arm_r.r[rd] &= ~1;

        arm_load_pipe();
    }
}

static void t16_mov_rd3() {
    int8_t rd = (arm_op >> 0) & 7;
    int8_t rm = (arm_op >> 3) & 7;

    arm_r.r[rd] = arm_r.r[rm];

    arm_setn(arm_r.r[rd]);
    arm_setz(arm_r.r[rd]);
}

//Move to Register from Coprocessor
static void arm_mrc() {
    //No coprocessors used
}

static void arm_mrc2() {
    //No coprocessors used
}

//Move to Two Registers from Coprocessor
static void arm_mrrc() {
    //No coprocessors used
}

static void arm_mrrc2() {
    //No coprocessors used
}

//Move to Register from Status
static void arm_mrs() {
    arm_psr_to_reg(arm_mrs_op());
}

//Move to Status from Register (also NOP/SEV)
static void arm_msr_imm() {
    arm_reg_to_psr(arm_msr_imm_op());
}

static void arm_msr_reg() {
    arm_reg_to_psr(arm_msr_reg_op());
}

//Multiply
static void arm_mul() {
    arm_mpy(arm_mpy_op());
}

static void t16_mul() {
    arm_mpy(t16_mpy_op());
}

//Move Not
static void arm_mvn_imm() {
    arm_logic(arm_data_imm_op(), MVN);
}

static void arm_mvn_regi() {
    arm_logic(arm_data_regi_op(), MVN);
}

static void arm_mvn_regr() {
    arm_logic(arm_data_regr_op(), MVN);
}

static void t16_mvn_rdn3() {
    arm_logic(t16_data_rdn3_op(), MVN);
}

//Or
static void arm_orr_imm() {
    arm_logic(arm_data_imm_op(), ORR);
}

static void arm_orr_regi() {
    arm_logic(arm_data_regi_op(), ORR);
}

static void arm_orr_regr() {
    arm_logic(arm_data_regr_op(), ORR);
}

static void t16_orr_rdn3() {
    arm_logic(t16_data_rdn3_op(), ORR);
}

//Preload Data
static void arm_pld_imm() {
    //Performance oriented instruction, just ignore
}

static void arm_pld_reg() {
    //Performance oriented instruction, just ignore
}

//Pop
static void t16_pop() {
    uint16_t regs;

    regs  = (arm_op >> 0) & 0x00ff;
    regs |= (arm_op << 7) & 0x8000;

    uint8_t i;

    arm_r.r[13] &= ~3;

    for (i = 0; i < 16; i++) {
        if (regs & (1 << i)) {
            arm_r.r[i] = arm_read_s(arm_r.r[13]);

            arm_r.r[13] += 4;
        }
    }

    if (regs & 0x8000) {
        arm_r.r[15] &= ~1;

        arm_load_pipe();
    }
}

//Push
static void t16_push() {
    uint16_t regs;

    regs  = (arm_op >> 0) & 0x00ff;
    regs |= (arm_op << 6) & 0x4000;

    uint32_t addr = arm_r.r[13] & ~3;

    uint8_t i;

    for (i = 0; i < 16; i++) {
        if (regs & (1 << i)) addr -= 4;
    }

    arm_r.r[13] = addr;

    for (i = 0; i < 16; i++) {
        if (regs & (1 << i)) {
            arm_write_s(addr, arm_memio_reg_get(i));

            addr += 4;
        }
    }
}

//Saturating Add
static void arm_qadd() {
    arm_parith_qadd(arm_parith_op());
}

//Saturating Double and Add
static void arm_qdadd() {
    arm_parith_qdadd(arm_parith_op());
}

//Saturating Double and Subtract
static void arm_qdsub() {
    arm_parith_qdsub(arm_parith_op());
}

//Saturating Subtract
static void arm_qsub() {
    arm_parith_qsub(arm_parith_op());
}

//Rotate Right
static void t16_ror() {
    arm_ror(t16_data_rdn3_op());
}

//Reverse Subtract
static void arm_rsb_imm() {
    arm_arith_rsb(arm_data_imm_op());
}

static void arm_rsb_regi() {
    arm_arith_rsb(arm_data_regi_op());
}

static void arm_rsb_regr() {
    arm_arith_rsb(arm_data_regr_op());
}

static void t16_rsb_rdn3() {
    arm_arith_sub(t16_data_neg_op());
}

//Reverse Subtract with Carry
static void arm_rsc_imm() {
    arm_arith_rsc(arm_data_imm_op());
}

static void arm_rsc_regi() {
    arm_arith_rsc(arm_data_regi_op());
}

static void arm_rsc_regr() {
    arm_arith_rsc(arm_data_regr_op());
}

//Subtract with Carry
static void arm_sbc_imm() {
    arm_arith_sbc(arm_data_imm_op());
}

static void arm_sbc_regi() {
    arm_arith_sbc(arm_data_regi_op());
}

static void arm_sbc_regr() {
    arm_arith_sbc(arm_data_regr_op());
}

static void t16_sbc_rdn3() {
    arm_arith_sbc(t16_data_rdn3_op());
}

//Signed Multiply Accumulate
static void arm_smla__() {
    bool m = (arm_op >> 5) & 1;
    bool n = (arm_op >> 6) & 1;

    arm_mpy_smla__(arm_mpy_op(), m, n);
}

//Signed Multiply Accumulate Long
static void arm_smlal() {
    arm_mpy_smlal(arm_mpy_op());
}

//Signed Multiply Accumulate Long (Halfwords)
static void arm_smlal__() {
    bool m = (arm_op >> 5) & 1;
    bool n = (arm_op >> 6) & 1;

    arm_mpy_smlal__(arm_mpy_op(), m, n);
}

//Signed Multiply Accumulate Word by Halfword
static void arm_smlaw_() {
    arm_mpy_smlaw_(arm_mpy_op(), (arm_op >> 6) & 1);
}

//Signed Multiply
static void arm_smul() {
    bool m = (arm_op >> 5) & 1;
    bool n = (arm_op >> 6) & 1;

    arm_mpy_smul(arm_mpy_op(), m, n);
}

//Signed Multiply Long
static void arm_smull() {
    arm_mpy_smull(arm_mpy_op());
}

//Signed Multiply Word by Halfword
static void arm_smulw_() {
    arm_mpy_smulw_(arm_mpy_op(), (arm_op >> 6) & 1);
}

//Store Coprocessor
static void arm_stc() {
    //No coprocessors used
}

static void arm_stc2() {
    //No coprocessors used
}

//Store Multiple
static void arm_stm() {
    arm_memio_stm(arm_memio_mult_op());
}

static void arm_stm_usr() {
    arm_memio_stm_usr(arm_memio_mult_op());
}

static void t16_stm() {
    arm_memio_stm(t16_memio_mult_op());
}

//Store Register
static void arm_str_imm() {
    arm_memio_str(arm_memio_imm_op());
}

static void arm_str_reg() {
    arm_memio_str(arm_memio_reg_op());
}

static void t16_str_imm5() {
    arm_memio_str(t16_memio_imm5_op(ARM_WORD_SZ));
}

static void t16_str_sp8() {
    arm_memio_str(t16_memio_imm8sp_op());
}

static void t16_str_reg() {
    arm_memio_str(t16_memio_reg_op());
}

//Store Register Byte
static void arm_strb_imm() {
    arm_memio_strb(arm_memio_imm_op());
}

static void arm_strb_reg() {
    arm_memio_strb(arm_memio_reg_op());
}

static void t16_strb_imm5() {
    arm_memio_strb(t16_memio_imm5_op(ARM_BYTE_SZ));
}

static void t16_strb_reg() {
    arm_memio_strb(t16_memio_reg_op());
}

//Store Register Byte Unprivileged
static void arm_strbt_imm() {
    arm_memio_strbt(arm_memio_immt_op());
}

static void arm_strbt_reg() {
    arm_memio_strbt(arm_memio_regt_op());
}

//Store Register Dual
static void arm_strd_imm() {
    arm_memio_strd(arm_memio_immdh_op());
}

static void arm_strd_reg() {
    arm_memio_strd(arm_memio_regdh_op());
}

//Store Register Halfword
static void arm_strh_imm() {
    arm_memio_strh(arm_memio_immdh_op());
}

static void arm_strh_reg() {
    arm_memio_strh(arm_memio_regdh_op());
}

static void t16_strh_imm5() {
    arm_memio_strh(t16_memio_imm5_op(ARM_HWORD_SZ));
}

static void t16_strh_reg() {
    arm_memio_strh(t16_memio_reg_op());
}

//Subtract
static void arm_sub_imm() {
    arm_arith_sub(arm_data_imm_op());
}

static void arm_sub_regi() {
    arm_arith_sub(arm_data_regi_op());
}

static void arm_sub_regr() {
    arm_arith_sub(arm_data_regr_op());
}

static void t16_sub_imm3() {
    arm_arith_sub(t16_data_imm3_op());
}

static void t16_sub_imm8() {
    arm_arith_sub(t16_data_imm8_op());
}

static void t16_sub_reg() {
    arm_arith_sub(t16_data_reg_op());
}

static void t16_sub_sp7() {
    arm_arith_sub(t16_data_imm7sp_op());
}

//Supervisor Call
static void arm_svc() {
    arm_int(ARM_VEC_SVC, ARM_SVC);
}

static void t16_svc() {
    arm_int(ARM_VEC_SVC, ARM_SVC);
}

//Swap
static void arm_swp() {
    uint8_t rt2 = (arm_op >>  0) & 0xf;
    uint8_t rt  = (arm_op >> 12) & 0xf;
    uint8_t rn  = (arm_op >> 16) & 0xf;
    bool    b   = (arm_op >> 22) & 0x1;

    uint32_t val;

    if (b)
        val = arm_readb_n(arm_r.r[rn]);
    else
        val = arm_read_n(arm_r.r[rn]);

    if (b)
        arm_writeb_n(arm_r.r[rn], arm_r.r[rt2]);
    else
        arm_write_n(arm_r.r[rn], arm_r.r[rt2]);

    arm_r.r[rt] = val;

    arm_cycles++;
}

//Test Equivalence
static void arm_teq_imm() {
    arm_logic_teq(arm_data_imm_op());
}

static void arm_teq_regi() {
    arm_logic_teq(arm_data_regi_op());
}

static void arm_teq_regr() {
    arm_logic_teq(arm_data_regr_op());
}

//Test
static void arm_tst_imm() {
    arm_logic_tst(arm_data_imm_op());
}

static void arm_tst_regi() {
    arm_logic_tst(arm_data_regi_op());
}

static void arm_tst_regr() {
    arm_logic_tst(arm_data_regr_op());
}

static void t16_tst_rdn3() {
    arm_logic_tst(t16_data_rdn3_op());
}

//Unsigned Multiply Accumulate Long
static void arm_umlal() {
    arm_mpy_umlal(arm_mpy_op());
}

//Unsigned Multiply Long
static void arm_umull() {
    arm_mpy_umull(arm_mpy_op());
}

//Undefined
static void arm_und() {
    arm_int(ARM_VEC_UND, ARM_UND);
}


static void arm_proc_fill(void (**arr)(), void (*proc)(), int32_t size) {
    int32_t i;

    for (i = 0; i < size; i++) {
        arr[i] = proc;
    }
}

static void arm_proc_set(void (**arr)(), void (*proc)(), uint32_t op, uint32_t mask, int32_t bits) {
    //Bits you need on op needs to be set 1 on mask
    int32_t i, j;
    int32_t zbits = 0;
    int32_t zpos[bits];

    for (i = 0; i < bits; i++) {
        if ((mask & (1 << i)) == 0) zpos[zbits++] = i;
    }

    for (i = 0; i < (1 << zbits); i++) {
        op &= mask;

        for (j = 0; j < zbits; j++) {
            op |= ((i >> j) & 1) << zpos[j];
        }

        arr[op] = proc;
    }
}

void (*arm_proc[2][4096])();
void (*thumb_proc[2048])();

static void arm_proc_init() {
    //Format 27:20,7:4
    arm_proc_fill(arm_proc[0], arm_und, 4096);
    arm_proc_fill(arm_proc[1], arm_und, 4096);

    //Conditional
    arm_proc_set(arm_proc[0], arm_adc_imm,    0b001010100000, 0b111111100000, 12);
    arm_proc_set(arm_proc[0], arm_adc_regi,   0b000010100000, 0b111111100001, 12);
    arm_proc_set(arm_proc[0], arm_adc_regr,   0b000010100001, 0b111111101001, 12);
    arm_proc_set(arm_proc[0], arm_add_imm,    0b001010000000, 0b111111100000, 12);
    arm_proc_set(arm_proc[0], arm_add_regi,   0b000010000000, 0b111111100001, 12);
    arm_proc_set(arm_proc[0], arm_add_regr,   0b000010000001, 0b111111101001, 12);
    arm_proc_set(arm_proc[0], arm_and_imm,    0b001000000000, 0b111111100000, 12);
    arm_proc_set(arm_proc[0], arm_and_regi,   0b000000000000, 0b111111100001, 12);
    arm_proc_set(arm_proc[0], arm_and_regr,   0b000000000001, 0b111111101001, 12);
    arm_proc_set(arm_proc[0], arm_shift_imm,  0b000110100000, 0b111111100001, 12);
    arm_proc_set(arm_proc[0], arm_shift_reg,  0b000110100001, 0b111111101001, 12);
    arm_proc_set(arm_proc[0], arm_b,          0b101000000000, 0b111100000000, 12);
    arm_proc_set(arm_proc[0], arm_bic_imm,    0b001111000000, 0b111111100000, 12);
    arm_proc_set(arm_proc[0], arm_bic_regi,   0b000111000000, 0b111111100001, 12);
    arm_proc_set(arm_proc[0], arm_bic_regr,   0b000111000001, 0b111111101001, 12);
    arm_proc_set(arm_proc[0], arm_bkpt,       0b000100100111, 0b111111111111, 12);
    arm_proc_set(arm_proc[0], arm_bl,         0b101100000000, 0b111100000000, 12);
    arm_proc_set(arm_proc[0], arm_blx_reg,    0b000100100011, 0b111111111111, 12);
    arm_proc_set(arm_proc[0], arm_bx,         0b000100100001, 0b111111111111, 12);
    arm_proc_set(arm_proc[0], arm_cdp,        0b111000000000, 0b111100000001, 12);
    arm_proc_set(arm_proc[0], arm_clz,        0b000101100001, 0b111111111111, 12);
    arm_proc_set(arm_proc[0], arm_cmn_imm,    0b001101110000, 0b111111110000, 12);
    arm_proc_set(arm_proc[0], arm_cmn_regi,   0b000101110000, 0b111111110001, 12);
    arm_proc_set(arm_proc[0], arm_cmn_regr,   0b000101110001, 0b111111111001, 12);
    arm_proc_set(arm_proc[0], arm_cmp_imm,    0b001101010000, 0b111111110000, 12);
    arm_proc_set(arm_proc[0], arm_cmp_regi,   0b000101010000, 0b111111110001, 12);
    arm_proc_set(arm_proc[0], arm_cmp_regr,   0b000101010001, 0b111111111001, 12);
    arm_proc_set(arm_proc[0], arm_eor_imm,    0b001000100000, 0b111111100000, 12);
    arm_proc_set(arm_proc[0], arm_eor_regi,   0b000000100000, 0b111111100001, 12);
    arm_proc_set(arm_proc[0], arm_eor_regr,   0b000000100001, 0b111111101001, 12);
    arm_proc_set(arm_proc[0], arm_ldc,        0b110000010000, 0b111000010000, 12);
    arm_proc_set(arm_proc[0], arm_ldm,        0b100000010000, 0b111001010000, 12);
    arm_proc_set(arm_proc[0], arm_ldm_usr,    0b100001010000, 0b111001010000, 12);
    arm_proc_set(arm_proc[0], arm_ldr_imm,    0b010000010000, 0b111001010000, 12);
    arm_proc_set(arm_proc[0], arm_ldr_reg,    0b011000010000, 0b111001010001, 12);
    arm_proc_set(arm_proc[0], arm_ldrb_imm,   0b010001010000, 0b111001010000, 12);
    arm_proc_set(arm_proc[0], arm_ldrb_reg,   0b011001010000, 0b111001010001, 12);
    arm_proc_set(arm_proc[0], arm_ldrbt_imm,  0b010001110000, 0b111101110000, 12);
    arm_proc_set(arm_proc[0], arm_ldrbt_reg,  0b011001110000, 0b111101110001, 12);
    arm_proc_set(arm_proc[0], arm_ldrd_imm,   0b000001001101, 0b111001011111, 12);
    arm_proc_set(arm_proc[0], arm_ldrd_reg,   0b000000001101, 0b111001011111, 12);
    arm_proc_set(arm_proc[0], arm_ldrh_imm,   0b000001011011, 0b111001011111, 12);
    arm_proc_set(arm_proc[0], arm_ldrh_reg,   0b000000011011, 0b111001011111, 12);
    arm_proc_set(arm_proc[0], arm_ldrsb_imm,  0b000001011101, 0b111001011111, 12);
    arm_proc_set(arm_proc[0], arm_ldrsb_reg,  0b000000011101, 0b111001011111, 12);
    arm_proc_set(arm_proc[0], arm_ldrsh_imm,  0b000001011111, 0b111001011111, 12);
    arm_proc_set(arm_proc[0], arm_ldrsh_reg,  0b000000011111, 0b111001011111, 12);
    arm_proc_set(arm_proc[0], arm_mcr,        0b111000000001, 0b111100010001, 12);
    arm_proc_set(arm_proc[0], arm_mcrr,       0b110001000000, 0b111111110000, 12);
    arm_proc_set(arm_proc[0], arm_mla,        0b000000101001, 0b111111101111, 12);
    arm_proc_set(arm_proc[0], arm_mov_imm12,  0b001110100000, 0b111111100000, 12);
    arm_proc_set(arm_proc[0], arm_mrc,        0b111000010001, 0b111100010001, 12);
    arm_proc_set(arm_proc[0], arm_mrrc,       0b110001010000, 0b111111110000, 12);
    arm_proc_set(arm_proc[0], arm_mrs,        0b000100000000, 0b111110111111, 12);
    arm_proc_set(arm_proc[0], arm_msr_imm,    0b001100100000, 0b111111110000, 12);
    arm_proc_set(arm_proc[0], arm_msr_reg,    0b000100100000, 0b111110111111, 12);
    arm_proc_set(arm_proc[0], arm_mul,        0b000000001001, 0b111111101111, 12);
    arm_proc_set(arm_proc[0], arm_mvn_imm,    0b001111100000, 0b111111100000, 12);
    arm_proc_set(arm_proc[0], arm_mvn_regi,   0b000111100000, 0b111111100001, 12);
    arm_proc_set(arm_proc[0], arm_mvn_regr,   0b000111100001, 0b111111101001, 12);
    arm_proc_set(arm_proc[0], arm_orr_imm,    0b001110000000, 0b111111100000, 12);
    arm_proc_set(arm_proc[0], arm_orr_regi,   0b000110000000, 0b111111100001, 12);
    arm_proc_set(arm_proc[0], arm_orr_regr,   0b000110000001, 0b111111101001, 12);
    arm_proc_set(arm_proc[0], arm_qadd,       0b000100000101, 0b111111111111, 12);
    arm_proc_set(arm_proc[0], arm_qdadd,      0b000101000101, 0b111111111111, 12);
    arm_proc_set(arm_proc[0], arm_qdsub,      0b000101100101, 0b111111111111, 12);
    arm_proc_set(arm_proc[0], arm_qsub,       0b000100100101, 0b111111111111, 12);
    arm_proc_set(arm_proc[0], arm_rsb_imm,    0b001001100000, 0b111111100000, 12);
    arm_proc_set(arm_proc[0], arm_rsb_regi,   0b000001100000, 0b111111100001, 12);
    arm_proc_set(arm_proc[0], arm_rsb_regr,   0b000001100001, 0b111111101001, 12);
    arm_proc_set(arm_proc[0], arm_rsc_imm,    0b001011100000, 0b111111100000, 12);
    arm_proc_set(arm_proc[0], arm_rsc_regi,   0b000011100000, 0b111111100001, 12);
    arm_proc_set(arm_proc[0], arm_rsc_regr,   0b000011100001, 0b111111101001, 12);
    arm_proc_set(arm_proc[0], arm_sbc_imm,    0b001011000000, 0b111111100000, 12);
    arm_proc_set(arm_proc[0], arm_sbc_regi,   0b000011000000, 0b111111100001, 12);
    arm_proc_set(arm_proc[0], arm_sbc_regr,   0b000011000001, 0b111111101001, 12);
    arm_proc_set(arm_proc[0], arm_smla__,     0b000100001000, 0b111111111001, 12);
    arm_proc_set(arm_proc[0], arm_smlal,      0b000011101001, 0b111111101111, 12);
    arm_proc_set(arm_proc[0], arm_smlal__,    0b000101001000, 0b111111111001, 12);
    arm_proc_set(arm_proc[0], arm_smlaw_,     0b000100101000, 0b111111111011, 12);
    arm_proc_set(arm_proc[0], arm_smul,       0b000101101000, 0b111111111001, 12);
    arm_proc_set(arm_proc[0], arm_smull,      0b000011001001, 0b111111101111, 12);
    arm_proc_set(arm_proc[0], arm_smulw_,     0b000100101010, 0b111111111011, 12);
    arm_proc_set(arm_proc[0], arm_stc,        0b110000000000, 0b111000010000, 12);
    arm_proc_set(arm_proc[0], arm_stm,        0b100000000000, 0b111001010000, 12);
    arm_proc_set(arm_proc[0], arm_stm_usr,    0b100001000000, 0b111001010000, 12);
    arm_proc_set(arm_proc[0], arm_str_imm,    0b010000000000, 0b111001010000, 12);
    arm_proc_set(arm_proc[0], arm_str_reg,    0b011000000000, 0b111001010001, 12);
    arm_proc_set(arm_proc[0], arm_strb_imm,   0b010001000000, 0b111001010000, 12);
    arm_proc_set(arm_proc[0], arm_strb_reg,   0b011001000000, 0b111001010001, 12);
    arm_proc_set(arm_proc[0], arm_strbt_imm,  0b010001100000, 0b111101110000, 12);
    arm_proc_set(arm_proc[0], arm_strbt_reg,  0b011001100000, 0b111101110001, 12);
    arm_proc_set(arm_proc[0], arm_strd_imm,   0b000001001111, 0b111001011111, 12);
    arm_proc_set(arm_proc[0], arm_strd_reg,   0b000000001111, 0b111001011111, 12);
    arm_proc_set(arm_proc[0], arm_strh_imm,   0b000001001011, 0b111001011111, 12);
    arm_proc_set(arm_proc[0], arm_strh_reg,   0b000000001011, 0b111001011111, 12);
    arm_proc_set(arm_proc[0], arm_sub_imm,    0b001001000000, 0b111111100000, 12);
    arm_proc_set(arm_proc[0], arm_sub_regi,   0b000001000000, 0b111111100001, 12);
    arm_proc_set(arm_proc[0], arm_sub_regr,   0b000001000001, 0b111111101001, 12);
    arm_proc_set(arm_proc[0], arm_svc,        0b111100000000, 0b111100000000, 12);
    arm_proc_set(arm_proc[0], arm_swp,        0b000100001001, 0b111110111111, 12);
    arm_proc_set(arm_proc[0], arm_teq_imm,    0b001100110000, 0b111111110000, 12);
    arm_proc_set(arm_proc[0], arm_teq_regi,   0b000100110000, 0b111111110001, 12);
    arm_proc_set(arm_proc[0], arm_teq_regr,   0b000100110001, 0b111111111001, 12);
    arm_proc_set(arm_proc[0], arm_tst_imm,    0b001100010000, 0b111111110000, 12);
    arm_proc_set(arm_proc[0], arm_tst_regi,   0b000100010000, 0b111111110001, 12);
    arm_proc_set(arm_proc[0], arm_tst_regr,   0b000100010001, 0b111111111001, 12);
    arm_proc_set(arm_proc[0], arm_umlal,      0b000010101001, 0b111111101111, 12);
    arm_proc_set(arm_proc[0], arm_umull,      0b000010001001, 0b111111101111, 12);

    //Unconditional
    arm_proc_set(arm_proc[1], arm_blx_imm,    0b101000000000, 0b111000000000, 12);
    arm_proc_set(arm_proc[1], arm_cdp2,       0b111000000000, 0b111100000001, 12);
    arm_proc_set(arm_proc[1], arm_ldc2,       0b110000010000, 0b111000010000, 12);
    arm_proc_set(arm_proc[1], arm_mcr2,       0b111000000001, 0b111100010001, 12);
    arm_proc_set(arm_proc[1], arm_mcrr2,      0b110001000000, 0b111111110000, 12);
    arm_proc_set(arm_proc[1], arm_mrc2,       0b111000010001, 0b111100010001, 12);
    arm_proc_set(arm_proc[1], arm_mrrc2,      0b110001010000, 0b111111110000, 12);
    arm_proc_set(arm_proc[1], arm_pld_imm,    0b010101010000, 0b111101110000, 12);
    arm_proc_set(arm_proc[1], arm_pld_reg,    0b011101010000, 0b111101110000, 12);
    arm_proc_set(arm_proc[1], arm_stc2,       0b110000000000, 0b111000010000, 12);
}

static void thumb_proc_init() {
    //Format 15:5
    arm_proc_fill(thumb_proc, arm_und, 2048);

    arm_proc_set(thumb_proc, t16_adc_rdn3,   0b01000001010, 0b11111111110, 11);
    arm_proc_set(thumb_proc, t16_add_imm3,   0b00011100000, 0b11111110000, 11);
    arm_proc_set(thumb_proc, t16_add_imm8,   0b00110000000, 0b11111000000, 11);
    arm_proc_set(thumb_proc, t16_add_reg,    0b00011000000, 0b11111110000, 11);
    arm_proc_set(thumb_proc, t16_add_rdn4,   0b01000100000, 0b11111111000, 11);
    arm_proc_set(thumb_proc, t16_add_sp7,    0b10110000000, 0b11111000000, 11);
    arm_proc_set(thumb_proc, t16_add_sp8,    0b10101000000, 0b11111000000, 11);
    arm_proc_set(thumb_proc, t16_adr,        0b10100000000, 0b11111000000, 11);
    arm_proc_set(thumb_proc, t16_and_rdn3,   0b01000000000, 0b11111111110, 11);
    arm_proc_set(thumb_proc, t16_asr_imm5,   0b00010000000, 0b11111000000, 11);
    arm_proc_set(thumb_proc, t16_asr_rdn3,   0b01000001000, 0b11111111110, 11);
    arm_proc_set(thumb_proc, t16_b_imm8,     0b11010000000, 0b11110000000, 11);
    arm_proc_set(thumb_proc, t16_b_imm11,    0b11100000000, 0b11111000000, 11);
    arm_proc_set(thumb_proc, t16_bic_rdn3,   0b01000011100, 0b11111111110, 11);
    arm_proc_set(thumb_proc, t16_bkpt,       0b10111110000, 0b11111111000, 11);
    arm_proc_set(thumb_proc, t16_blx,        0b01000111100, 0b11111111100, 11);
    arm_proc_set(thumb_proc, t16_blx_h1,     0b11101000000, 0b11111000000, 11);
    arm_proc_set(thumb_proc, t16_blx_h2,     0b11110000000, 0b11111000000, 11);
    arm_proc_set(thumb_proc, t16_blx_h3,     0b11111000000, 0b11111000000, 11);
    arm_proc_set(thumb_proc, t16_bx,         0b01000111000, 0b11111111100, 11);
    arm_proc_set(thumb_proc, t16_cmn_rdn3,   0b01000010110, 0b11111111110, 11);
    arm_proc_set(thumb_proc, t16_cmp_imm8,   0b00101000000, 0b11111000000, 11);
    arm_proc_set(thumb_proc, t16_cmp_rdn3,   0b01000010100, 0b11111111110, 11);
    arm_proc_set(thumb_proc, t16_cmp_rdn4,   0b01000101000, 0b11111111000, 11);
    arm_proc_set(thumb_proc, t16_eor_rdn3,   0b01000000010, 0b11111111110, 11);
    arm_proc_set(thumb_proc, t16_ldm,        0b11001000000, 0b11111000000, 11);
    arm_proc_set(thumb_proc, t16_ldr_imm5,   0b01101000000, 0b11111000000, 11);
    arm_proc_set(thumb_proc, t16_ldr_sp8,    0b10011000000, 0b11111000000, 11);
    arm_proc_set(thumb_proc, t16_ldr_pc8,    0b01001000000, 0b11111000000, 11);
    arm_proc_set(thumb_proc, t16_ldr_reg,    0b01011000000, 0b11111110000, 11);
    arm_proc_set(thumb_proc, t16_ldrb_imm5,  0b01111000000, 0b11111000000, 11);
    arm_proc_set(thumb_proc, t16_ldrb_reg,   0b01011100000, 0b11111110000, 11);
    arm_proc_set(thumb_proc, t16_ldrh_imm5,  0b10001000000, 0b11111000000, 11);
    arm_proc_set(thumb_proc, t16_ldrh_reg,   0b01011010000, 0b11111110000, 11);
    arm_proc_set(thumb_proc, t16_ldrsb_reg,  0b01010110000, 0b11111110000, 11);
    arm_proc_set(thumb_proc, t16_ldrsh_reg,  0b01011110000, 0b11111110000, 11);
    arm_proc_set(thumb_proc, t16_lsl_imm5,   0b00000000000, 0b11111000000, 11);
    arm_proc_set(thumb_proc, t16_lsl_rdn3,   0b01000000100, 0b11111111110, 11);
    arm_proc_set(thumb_proc, t16_lsr_imm5,   0b00001000000, 0b11111000000, 11);
    arm_proc_set(thumb_proc, t16_lsr_rdn3,   0b01000000110, 0b11111111110, 11);
    arm_proc_set(thumb_proc, t16_mov_imm,    0b00100000000, 0b11111000000, 11);
    arm_proc_set(thumb_proc, t16_mov_rd4,    0b01000110000, 0b11111111000, 11);
    arm_proc_set(thumb_proc, t16_mov_rd3,    0b00000000000, 0b11111111110, 11);
    arm_proc_set(thumb_proc, t16_mul,        0b01000011010, 0b11111111110, 11);
    arm_proc_set(thumb_proc, t16_mvn_rdn3,   0b01000011110, 0b11111111110, 11);
    arm_proc_set(thumb_proc, t16_orr_rdn3,   0b01000011000, 0b11111111110, 11);
    arm_proc_set(thumb_proc, t16_pop,        0b10111100000, 0b11111110000, 11);
    arm_proc_set(thumb_proc, t16_push,       0b10110100000, 0b11111110000, 11);
    arm_proc_set(thumb_proc, t16_ror,        0b01000001110, 0b11111111110, 11);
    arm_proc_set(thumb_proc, t16_rsb_rdn3,   0b01000010010, 0b11111111110, 11);
    arm_proc_set(thumb_proc, t16_sbc_rdn3,   0b01000001100, 0b11111111110, 11);
    arm_proc_set(thumb_proc, t16_stm,        0b11000000000, 0b11111000000, 11);
    arm_proc_set(thumb_proc, t16_str_imm5,   0b01100000000, 0b11111000000, 11);
    arm_proc_set(thumb_proc, t16_str_sp8,    0b10010000000, 0b11111000000, 11);
    arm_proc_set(thumb_proc, t16_str_reg,    0b01010000000, 0b11111110000, 11);
    arm_proc_set(thumb_proc, t16_strb_imm5,  0b01110000000, 0b11111000000, 11);
    arm_proc_set(thumb_proc, t16_strb_reg,   0b01010100000, 0b11111110000, 11);
    arm_proc_set(thumb_proc, t16_strh_imm5,  0b10000000000, 0b11111000000, 11);
    arm_proc_set(thumb_proc, t16_strh_reg,   0b01010010000, 0b11111110000, 11);
    arm_proc_set(thumb_proc, t16_sub_imm3,   0b00011110000, 0b11111110000, 11);
    arm_proc_set(thumb_proc, t16_sub_imm8,   0b00111000000, 0b11111000000, 11);
    arm_proc_set(thumb_proc, t16_sub_reg,    0b00011010000, 0b11111110000, 11);
    arm_proc_set(thumb_proc, t16_sub_sp7,    0b10110000100, 0b11111111100, 11);
    arm_proc_set(thumb_proc, t16_svc,        0b11011111100, 0b11111111000, 11);
    arm_proc_set(thumb_proc, t16_tst_rdn3,   0b01000010000, 0b11111111110, 11);
}

void arm_init() {
    bios   = malloc(0x4000);
    wram   = malloc(0x40000);
    iwram  = malloc(0x8000);
    pram   = malloc(0x400);
    vram   = malloc(0x18000);
    oam    = malloc(0x400);
    rom    = malloc(0x2000000);
    eeprom = malloc(0x2000);
    sram   = malloc(0x10000);
    flash  = malloc(0x20000);

    arm_proc_init();
    thumb_proc_init();

    key_input.w = 0x3ff;
    wait_cnt.w  = 0;
    arm_cycles  = 0;

    update_ws();
}

void arm_uninit() {
    free(bios);
    free(wram);
    free(iwram);
    free(pram);
    free(vram);
    free(oam);
    free(rom);
    free(eeprom);
    free(sram);
    free(flash);
}

#define ARM_COND_UNCOND  0b1111

static void t16_inc_r15() {
    if (pipe_reload)
        pipe_reload = false;
    else
        arm_r.r[15] += 2;
}

static void t16_step() {
    arm_pipe[1] = arm_fetchh(SEQUENTIAL);

    thumb_proc[arm_op >> 5]();

    t16_inc_r15();
}

static void arm_inc_r15() {
    if (pipe_reload)
        pipe_reload = false;
    else
        arm_r.r[15] += 4;
}

static void arm_step() {
    arm_pipe[1] = arm_fetch(SEQUENTIAL);

    uint32_t proc;

    proc  = (arm_op >> 16) & 0xff0;
    proc |= (arm_op >>  4) & 0x00f;

    int8_t cond = arm_op >> 28;

    if (cond == ARM_COND_UNCOND)
        arm_proc[1][proc]();
    else if (arm_cond(cond))
        arm_proc[0][proc]();

    arm_inc_r15();
}

void arm_exec(uint32_t target_cycles) {
    if (int_halt) {
        timers_clock(target_cycles);

        return;
    }

    while (arm_cycles < target_cycles) {
        uint32_t cycles = arm_cycles;

        arm_op      = arm_pipe[0];
        arm_pipe[0] = arm_pipe[1];

        if (arm_in_thumb())
            t16_step();
        else
            arm_step();

        if (int_halt) arm_cycles = target_cycles;

        if (tmr_enb) timers_clock(arm_cycles - cycles);
    }

    arm_cycles -= target_cycles;
}

void arm_int(uint32_t address, int8_t mode) {
    uint32_t cpsr = arm_r.cpsr;

    arm_mode_set(mode);
    arm_spsr_set(cpsr);

    //Set FIQ Disable flag based on exception
    if (address == ARM_VEC_FIQ ||
        address == ARM_VEC_RESET)
        arm_flag_set(ARM_F, true);

    /*
     * Adjust PC based on exception
     *
     * Exc  ARM  Thumb
     * DA   $+8  $+8
     * FIQ  $+4  $+4
     * IRQ  $+4  $+4
     * PA   $+4  $+4
     * UND  $+4  $+2
     * SVC  $+4  $+2
     */
    if (address == ARM_VEC_UND ||
        address == ARM_VEC_SVC) {
        if (arm_in_thumb())
            arm_r.r[15] -= 2;
        else
            arm_r.r[15] -= 4;
    }

    if (address == ARM_VEC_FIQ ||
        address == ARM_VEC_IRQ ||
        address == ARM_VEC_PABT) {
        if (arm_in_thumb())
            arm_r.r[15] -= 0;
        else
            arm_r.r[15] -= 4;
    }

    arm_flag_set(ARM_T, false);
    arm_flag_set(ARM_I, true);

    arm_r.r[14] = arm_r.r[15];
    arm_r.r[15] = address;

    arm_load_pipe();
}

void arm_check_irq() {
    if (!arm_flag_tst(ARM_I) &&
        (int_enb_m.w & 1) &&
        (int_enb.w & int_ack.w))
        arm_int(ARM_VEC_IRQ, ARM_IRQ);
}

void arm_reset() {
    arm_int(ARM_VEC_RESET, ARM_SVC);
}


// video.h / video.c

void run_frame();

#define LINES_VISIBLE  160
#define LINES_TOTAL    228

#define CYC_LINE_TOTAL  1232
#define CYC_LINE_HBLK0  1006
#define CYC_LINE_HBLK1  (CYC_LINE_TOTAL - CYC_LINE_HBLK0)

uint8_t screen[240*160*4];

static const uint8_t x_tiles_lut[16] = { 1, 2, 4, 8, 2, 4, 4, 8, 1, 1, 2, 4, 0, 0, 0, 0 };
static const uint8_t y_tiles_lut[16] = { 1, 2, 4, 8, 1, 1, 2, 4, 2, 4, 4, 8, 0, 0, 0, 0 };

void draw_pixel_pal(uint32_t addr, uint32_t pal)
{
	*(uint32_t *)(screen + addr) = palette[pal];
}

void draw_pixel_col(uint32_t addr, uint32_t col)
{
	*(uint32_t *)(screen + addr) = col;
}

void add_pixel_pal(uint32_t addr, uint32_t pal)
{
	uint32_t mix = 0x000000FF |
		(((screen[addr+1] + ((palette[pal] & 0x0000FF00) >> 8)) >> 1) << 8) |
		(((screen[addr+2] + ((palette[pal] & 0x00FF0000) >> 16)) >> 1) << 16) |
		(((screen[addr+3] + ((palette[pal] & 0xFF000000) >> 24)) >> 1) << 24);

	*(uint32_t *)(screen + addr) = mix;
}

void add_pixel_col(uint32_t addr, uint32_t col)
{
	uint32_t mix = 0x000000FF |
		(((screen[addr+1] + ((col & 0x0000FF00) >> 8)) >> 1) << 8) |
		(((screen[addr+2] + ((col & 0x00FF0000) >> 16)) >> 1) << 16) |
		(((screen[addr+3] + ((col & 0xFF000000) >> 24)) >> 1) << 24);

	*(uint32_t *)(screen + addr) = mix;
}

#define WIN0_ENB     (1 << 13)
#define WIN1_ENB     (1 << 14)
#define WINOBJ_ENB   (1 << 15)
#define WINANY_ENB  (WIN0_ENB | WIN1_ENB | WINOBJ_ENB)

static int win_show_y(uint8_t layer, uint8_t y) {
    uint8_t mask = 1 << layer;

    bool win0_in = win_in.b.b0 & mask;
    bool win1_in = win_in.b.b1 & mask;

    bool winobj_in = win_out.b.b1 & mask;

    bool win_out_ = win_out.b.b0 & mask;

    bool inside_win0   = false;
    bool inside_win1   = false;
    bool inside_winobj = false;

    if (disp_cnt.w & WIN0_ENB) {
        inside_win0 = y >= win0_v.b.b1 &&
                      y <  win0_v.b.b0;
    }

    if (disp_cnt.w & WIN1_ENB) {
        inside_win1 = y >= win1_v.b.b1 &&
                      y <  win1_v.b.b0;
    }

    if (disp_cnt.w & WINOBJ_ENB) {
        //This need to be checked per X pixel as it isn't a range,
        //so in this case we should aways return true so the X check is made
        inside_winobj = true;
    }

    if (win0_in && inside_win0) return 2;
    if (win1_in && inside_win1) return 2;

    if (winobj_in && inside_winobj) return 2;

    if (win_out_ && !(inside_win0 || inside_win1 || inside_winobj)) return 1;

    return win_out_ ? 2 : 0;
}

static int win_show_x(uint8_t layer, uint8_t x, int prev) {
    if (prev != 2) return prev;

    uint8_t mask = 1 << layer;

    bool win0_in = win_in.b.b0 & mask;
    bool win1_in = win_in.b.b1 & mask;

    bool winobj_in = win_out.b.b1 & mask;

    bool win_out_ = win_out.b.b0 & mask;

    bool inside_win0   = false;
    bool inside_win1   = false;
    bool inside_winobj = false;

    if (disp_cnt.w & WIN0_ENB) {
        inside_win0 = x >= win0_h.b.b1 &&
                      x <  win0_h.b.b0;
    }

    if (disp_cnt.w & WIN1_ENB) {
        inside_win1 = x >= win1_h.b.b1 &&
                      x <  win1_h.b.b0;
    }

    //if (disp_cnt.w & WINOBJ_ENB) {
    //    inside_winobj = layerobj[x];
    //}

    if (win0_in && inside_win0) return 1;
    if (win1_in && inside_win1) return 1;

    //if (winobj_in && inside_winobj) return 1;

    if (win_out_ && !(inside_win0 || inside_win1 || inside_winobj)) return 1;

    return 0;
}

static void render_obj(uint8_t prio) {
    if (!(disp_cnt.w & OBJ_ENB)) return;

    uint8_t obj_index;
    uint32_t offset = 0x3f8;

	uint8_t eff = (bld_cnt.w >> 6) & 3;

    uint32_t surf_addr = v_count.w * 240 * 4;

    for (obj_index = 0; obj_index < 128; obj_index++) {
        uint16_t attr0 = oam[offset + 0] | (oam[offset + 1] << 8);
        uint16_t attr1 = oam[offset + 2] | (oam[offset + 3] << 8);
        uint16_t attr2 = oam[offset + 4] | (oam[offset + 5] << 8);

        offset -= 8;

        int16_t  obj_y    = (attr0 >>  0) & 0xff;
        bool     affine   = (attr0 >>  8) & 0x1;
        bool     dbl_size = (attr0 >>  9) & 0x1;
        bool     hidden   = (attr0 >>  9) & 0x1;
        uint8_t  obj_shp  = (attr0 >> 14) & 0x3;
        uint8_t  affine_p = (attr1 >>  9) & 0x1f;
        uint8_t  obj_size = (attr1 >> 14) & 0x3;
        uint8_t  chr_prio = (attr2 >> 10) & 0x3;

        if (chr_prio != prio || (!affine && hidden)) continue;

        int16_t pa, pb, pc, pd;

        pa = pd = 0x100; //1.0
        pb = pc = 0x000; //0.0

        if (affine) {
            uint32_t p_base = affine_p * 32;

            pa = oam[p_base + 0x06] | (oam[p_base + 0x07] << 8);
            pb = oam[p_base + 0x0e] | (oam[p_base + 0x0f] << 8);
            pc = oam[p_base + 0x16] | (oam[p_base + 0x17] << 8);
            pd = oam[p_base + 0x1e] | (oam[p_base + 0x1f] << 8);
        }

        uint8_t lut_idx = obj_size | (obj_shp << 2);

        uint8_t x_tiles = x_tiles_lut[lut_idx];
        uint8_t y_tiles = y_tiles_lut[lut_idx];

        int32_t rcx = x_tiles * 4;
        int32_t rcy = y_tiles * 4;

        if (affine && dbl_size) {
            rcx *= 2;
            rcy *= 2;
        }

        if (obj_y + rcy * 2 > 0xff) obj_y -= 0x100;

        if (obj_y <= (int32_t)v_count.w && (obj_y + rcy * 2 > v_count.w)) {
            uint8_t  obj_mode = (attr0 >> 10) & 0x3;
            bool     mosaic   = (attr0 >> 12) & 0x1;
            bool     is_256   = (attr0 >> 13) & 0x1;
            int16_t  obj_x    = (attr1 >>  0) & 0x1ff;
            bool     flip_x   = (attr1 >> 12) & 0x1;
            bool     flip_y   = (attr1 >> 13) & 0x1;
            uint16_t chr_numb = (attr2 >>  0) & 0x3ff;
            uint8_t  chr_pal  = (attr2 >> 12) & 0xf;

            uint32_t chr_base = 0x10000 | chr_numb * 32;

            obj_x <<= 7;
            obj_x >>= 7;

            int32_t x, y = v_count.w - obj_y;

            if (!affine && flip_y) y ^= (y_tiles * 8) - 1;

            uint8_t tsz = is_256 ? 64 : 32; //Tile block size (in bytes, = (8 * 8 * bpp) / 8)
            uint8_t lsz = is_256 ?  8 :  4; //Pixel line row size (in bytes)

            int32_t ox = pa * -rcx + pb * (y - rcy) + (x_tiles << 10);
            int32_t oy = pc * -rcx + pd * (y - rcy) + (y_tiles << 10);

            if (!affine && flip_x) {
                ox = (x_tiles * 8 - 1) << 8;
                pa = -0x100;
            }

            uint32_t tys = (disp_cnt.w & MAP_1D_FLAG) ? x_tiles * tsz : 1024; //Tile row stride

            uint32_t address = surf_addr + obj_x * 4;

            for (x = 0; x < rcx * 2;
                x++,
                ox += pa,
                oy += pc,
                address += 4) {

                if (obj_x + x < 0) continue;
                if (obj_x + x >= 240) break;

                uint32_t vram_addr;
                uint32_t pal_idx;

                uint16_t tile_x = ox >> 11;
                uint16_t tile_y = oy >> 11;

                if (ox < 0 || tile_x >= x_tiles) continue;
                if (oy < 0 || tile_y >= y_tiles) continue;

                uint16_t chr_x = (ox >> 8) & 7;
                uint16_t chr_y = (oy >> 8) & 7;

                uint32_t chr_addr =
                    chr_base       +
                    tile_y   * tys +
                    chr_y    * lsz;

                if (is_256) {
                    vram_addr = chr_addr + tile_x * 64 + chr_x;
                    pal_idx   = vram[vram_addr];
                } else {
                    vram_addr = chr_addr + tile_x * 32 + (chr_x >> 1);
                    pal_idx   = (vram[vram_addr] >> (chr_x & 1) * 4) & 0xf;
                }

                uint32_t pal_addr = 0x100 | pal_idx | (!is_256 ? chr_pal * 16 : 0);

                if (pal_idx)
				{
					// *(uint32_t *)(screen + address) = palette[pal_addr];
					//if (eff == 1) add_pixel_pal(address, pal_addr);
					//else draw_pixel_pal(address, pal_addr);
					draw_pixel_pal(address, pal_addr); // looks better without blending on objects?
				}
            }
        }
    }
}

static const uint8_t bg_enb[3] = { 0xf, 0x7, 0xc };

static void render_bg() {
    uint8_t mode = disp_cnt.w & 7;

	uint8_t eff = (bld_cnt.w >> 6) & 3;

    uint32_t surf_addr = v_count.w * 240 * 4;

    switch (mode) {
        case 0:
        case 1:
        case 2: {
            uint8_t enb = (disp_cnt.w >> 8) & bg_enb[mode];

            int8_t prio, bg_idx;

            for (prio = 3; prio >= 0; prio--) {
                for (bg_idx = 3; bg_idx >= 0; bg_idx--) {

					// check windowing here

					if ((bg[bg_idx].ctrl.w & 3) != prio) continue;

                    if (!(enb & (1 << bg_idx))) continue;

					bool win = disp_cnt.w & WINANY_ENB;
					int win_show = 1;

					if (win) {
						win_show = win_show_y(bg_idx, v_count.w);
						if (win_show == 0) continue;
					}

                    uint32_t chr_base  = ((bg[bg_idx].ctrl.w >>  2) & 0x3)  << 14;
                    bool     is_256    =  (bg[bg_idx].ctrl.w >>  7) & 0x1;
                    uint16_t scrn_base = ((bg[bg_idx].ctrl.w >>  8) & 0x1f) << 11;
                    bool     aff_wrap  =  (bg[bg_idx].ctrl.w >> 13) & 0x1;
                    uint16_t scrn_size =  (bg[bg_idx].ctrl.w >> 14);

                    bool affine = mode == 2 || (mode == 1 && bg_idx == 2);

                    uint32_t address = surf_addr;

                    if (affine) {
                        int16_t pa = bg_pa[bg_idx].w;
                        int16_t pb = bg_pb[bg_idx].w;
                        int16_t pc = bg_pc[bg_idx].w;
                        int16_t pd = bg_pd[bg_idx].w;

                        int32_t ox = ((int32_t)bg_refxi[bg_idx].w << 4) >> 4;
                        int32_t oy = ((int32_t)bg_refyi[bg_idx].w << 4) >> 4;

                        bg_refxi[bg_idx].w += pb;
                        bg_refyi[bg_idx].w += pd;

                        uint8_t tms = 16 << scrn_size;
                        uint8_t tmsk = tms - 1;

                        uint8_t x;

                        for (x = 0; x < 240;
                            x++,
                            ox += pa,
                            oy += pc,
                            address += 4) {

							if (win) {
								if (win_show_x(bg_idx, x, win_show) == 0) continue;
							}

                            int16_t tmx = ox >> 11;
                            int16_t tmy = oy >> 11;

                            if (aff_wrap) {
                                tmx &= tmsk;
                                tmy &= tmsk;
                            } else {
                                if (tmx < 0 || tmx >= tms) continue;
                                if (tmy < 0 || tmy >= tms) continue;
                            }

                            uint16_t chr_x = (ox >> 8) & 7;
                            uint16_t chr_y = (oy >> 8) & 7;

                            uint32_t map_addr = scrn_base + tmy * tms + tmx;

                            uint32_t vram_addr = chr_base + vram[map_addr] * 64 + chr_y * 8 + chr_x;

                            uint16_t pal_idx = vram[vram_addr];

                            if (pal_idx)
							{
								// *(uint32_t *)(screen + address) = palette[pal_idx];
								if (eff == 1) add_pixel_pal(address, pal_idx);
								else draw_pixel_pal(address, pal_idx);
							}
                        }
                    } else {
                        uint16_t oy     = v_count.w + bg[bg_idx].yofs.w;
                        uint16_t tmy    = oy >> 3;
                        uint16_t scrn_y = (tmy >> 5) & 1;

                        uint8_t x;

                        for (x = 0; x < 240; x++) {

							if (win) {
								if (win_show_x(bg_idx, x, win_show) == 0) continue;
							}

                            uint16_t ox     = x + bg[bg_idx].xofs.w;
                            uint16_t tmx    = ox >> 3;
                            uint16_t scrn_x = (tmx >> 5) & 1;

                            uint16_t chr_x = ox & 7;
                            uint16_t chr_y = oy & 7;

                            uint16_t pal_idx;
                            uint16_t pal_base = 0;

                            uint32_t map_addr = scrn_base + (tmy & 0x1f) * 32 * 2 + (tmx & 0x1f) * 2;

                            switch (scrn_size) {
                                case 1: map_addr += scrn_x * 2048; break;
                                case 2: map_addr += scrn_y * 2048; break;
                                case 3: map_addr += scrn_x * 2048 + scrn_y * 4096; break;
                            }

                            uint16_t tile = vram[map_addr + 0] | (vram[map_addr + 1] << 8);

                            uint16_t chr_numb = (tile >>  0) & 0x3ff;
                            bool     flip_x   = (tile >> 10) & 0x1;
                            bool     flip_y   = (tile >> 11) & 0x1;
                            uint8_t  chr_pal  = (tile >> 12) & 0xf;

                            if (!is_256) pal_base = chr_pal * 16;

                            if (flip_x) chr_x ^= 7;
                            if (flip_y) chr_y ^= 7;

                            uint32_t vram_addr;

                            if (is_256) {
                                vram_addr = chr_base + chr_numb * 64 + chr_y * 8 + chr_x;
                                pal_idx   = vram[vram_addr];
                            } else {
                                vram_addr = chr_base + chr_numb * 32 + chr_y * 4 + (chr_x >> 1);
                                pal_idx   = (vram[vram_addr] >> (chr_x & 1) * 4) & 0xf;
                            }

                            uint32_t pal_addr = pal_idx | pal_base;

                            if (pal_idx)
							{
								// *(uint32_t *)(screen + address) = palette[pal_addr];
								if (eff == 1) add_pixel_pal(address, pal_addr);
								else draw_pixel_pal(address, pal_addr);
							}

                            address += 4;
                        }
                    }
                }

                render_obj(prio);
            }
        }
        break;

        case 3: {
            uint8_t x;
            uint32_t frm_addr = v_count.w * 480;

            for (x = 0; x < 240; x++) {
                uint16_t pixel = vram[frm_addr + 0] | (vram[frm_addr + 1] << 8);

                uint8_t r = ((pixel >>  0) & 0x1f) << 3;
                uint8_t g = ((pixel >>  5) & 0x1f) << 3;
                uint8_t b = ((pixel >> 10) & 0x1f) << 3;

                uint32_t rgba = 0xff;

                rgba |= (r | (r >> 5)) <<  8;
                rgba |= (g | (g >> 5)) << 16;
                rgba |= (b | (b >> 5)) << 24;

                // *(uint32_t *)(screen + surf_addr) = rgba;
				if (eff == 1) add_pixel_col(surf_addr, rgba);
				else draw_pixel_col(surf_addr, rgba);

                surf_addr += 4;

                frm_addr += 2;
            }
        }
        break;

        case 4: {
            uint8_t x, frame = (disp_cnt.w >> 4) & 1;
            uint32_t frm_addr = 0xa000 * frame + v_count.w * 240;

            for (x = 0; x < 240; x++) {
                uint8_t pal_idx = vram[frm_addr++];

                // *(uint32_t *)(screen + surf_addr) = palette[pal_idx];
				if (eff == 1) add_pixel_pal(surf_addr, pal_idx);
				else draw_pixel_pal(surf_addr, pal_idx);

                surf_addr += 4;
            }
        }
        break;
    }
}

static void render_line() {	
    uint32_t addr;

    uint32_t addr_s = v_count.w * 240 * 4;
    uint32_t addr_e = addr_s + 240 * 4;

    for (addr = addr_s; addr < addr_e; addr += 0x10) {
        // *(uint32_t *)(screen + (addr | 0x0)) = palette[0];
        // *(uint32_t *)(screen + (addr | 0x4)) = palette[0];
        // *(uint32_t *)(screen + (addr | 0x8)) = palette[0];
        // *(uint32_t *)(screen + (addr | 0xc)) = palette[0];
		draw_pixel_pal((addr|0x0), 0);
		draw_pixel_pal((addr|0x4), 0);
		draw_pixel_pal((addr|0x8), 0);
		draw_pixel_pal((addr|0xC), 0);
    }

    if ((disp_cnt.w & 7) > 2) {
        render_bg(0);
        render_obj(0);
        render_obj(1);
        render_obj(2);
        render_obj(3);
    } else {
        render_bg();
    }
}

static void vblank_start() {
    if (disp_stat.w & VBLK_IRQ) trigger_irq(VBLK_FLAG);

    disp_stat.w |= VBLK_FLAG;
}

static void hblank_start() {
    if (disp_stat.w & HBLK_IRQ) trigger_irq(HBLK_FLAG);

    disp_stat.w |= HBLK_FLAG;
}

static void vcount_match() {
    if (disp_stat.w & VCNT_IRQ) trigger_irq(VCNT_FLAG);

    disp_stat.w |= VCNT_FLAG;
}

void run_frame() {
    disp_stat.w &= ~VBLK_FLAG;

    for (v_count.w = 0; v_count.w < LINES_TOTAL; v_count.w++) {
        disp_stat.w &= ~(HBLK_FLAG | VCNT_FLAG);

        //V-Count match and V-Blank start
        if (v_count.w == disp_stat.b.b1) vcount_match();

        if (v_count.w == LINES_VISIBLE) {
            bg_refxi[2].w = bg_refxe[2].w;
            bg_refyi[2].w = bg_refye[2].w;

            bg_refxi[3].w = bg_refxe[3].w;
            bg_refyi[3].w = bg_refye[3].w;

            vblank_start();
            dma_transfer(VBLANK);
        }

        arm_exec(CYC_LINE_HBLK0);

        //H-Blank start
        if (v_count.w < LINES_VISIBLE) {
			if (fps_graphics > 0)
			{
            	render_line();
			}
            dma_transfer(HBLANK);
        }

        hblank_start();

        arm_exec(CYC_LINE_HBLK1);

		sound_clock(CYC_LINE_TOTAL);
    }

	//sound_buffer_wrap();
}

// main.c

const int64_t max_rom_sz = 32 * 1024 * 1024;

static uint32_t to_pow2(uint32_t val) {
    val--;

    val |= (val >>  1);
    val |= (val >>  2);
    val |= (val >>  4);
    val |= (val >>  8);
    val |= (val >> 16);

    return val + 1;
}

int main(int argc, char* argv[]) {
    printf("gdkGBA - Gameboy Advance emulator made by gdkchan\n");
    printf("This is FREE software released into the PUBLIC DOMAIN\n\n");

    arm_init();

    if (argc < 3) {
		printf("Arguments: <ROM file> <button file>\n");

        return 0;
    }

    FILE *image;

    image = fopen("gdkGBA/gba_bios.bin", "rb");

    if (image == NULL) {
        printf("Error: GBA BIOS not found!\n");
        printf("Place it on this directory with the name \"gba_bios.bin\".\n");

        return 0;
    }

    fread(bios, 16384, 1, image);

    fclose(image);

    image = fopen(argv[1], "rb");

    if (image == NULL) {
        printf("Error: ROM file couldn't be opened.\n");
        printf("Make sure that the file exists and the name is correct.\n");

        return 0;
    }

    fseek(image, 0, SEEK_END);

    cart_rom_size = ftell(image);
    cart_rom_mask = to_pow2(cart_rom_size) - 1;

    if (cart_rom_size > max_rom_sz) cart_rom_size = max_rom_sz;

    fseek(image, 0, SEEK_SET);
    fread(rom, cart_rom_size, 1, image);

    fclose(image);

	
	char tty_name[16];

	system("tty > temp.val");
	system("echo \"                \" >> temp.val");

	int tty_file = open("temp.val", O_RDWR);
	read(tty_file, &tty_name, 16);
	close(tty_file);

	system("rm temp.val");

	for (int i=0; i<16; i++)
	{
		if (tty_name[i] <= ' ') tty_name[i] = 0;
	}

	tty_file = open(tty_name, O_RDWR);
	ioctl(tty_file, KDSETMODE, KD_GRAPHICS); // turn off tty

	int screen_file = 0;
	int screen_handheld = 0;
	unsigned short screen_large_buffer[640*480];
	unsigned short screen_small_buffer[320*240];

	for (unsigned long i=0; i<640*480; i++)
	{
		screen_large_buffer[i] = 0;
	}

	for (unsigned long i=0; i<320*240; i++)
	{
		screen_small_buffer[i] = 0;
	}

	char size_buffer[3];
	int size_file = open("/sys/class/graphics/fb0/virtual_size", O_RDONLY);
	read(size_file, &size_buffer, 3);
	close(size_file);

	if (size_buffer[0] == '3' && size_buffer[1] == '2' && size_buffer[2] == '0')
	{
		screen_handheld = 1;
	}
	else
	{
		screen_handheld = 0;
	}

	int buttons_file = 0;
	char buttons_buffer[13] = { '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0' };

	int sound_enable = 1;
	int sound_file = 0;
	int sound_fragment = 0x0004000A; // 4 blocks, each is 2^A = 2^10 = 1024
	int sound_stereo = 0;
	int sound_format = AFMT_S16_BE; // AFMT_U8;
	int sound_speed = (unsigned int)(61162/2); // calculations: 61162 = 1024 * (4194304 / 70224) + 1

	sound_file = open("/dev/dsp", O_WRONLY);

	ioctl(sound_file, SNDCTL_DSP_SETFRAGMENT, &sound_fragment); // needed to stop the drift
	ioctl(sound_file, SNDCTL_DSP_STEREO, &sound_stereo);
	ioctl(sound_file, SNDCTL_DSP_SETFMT, &sound_format);
	ioctl(sound_file, SNDCTL_DSP_SPEED, &sound_speed);

	unsigned short sound_array[512*4];

    arm_reset();

    bool run = true;

	unsigned long previous_clock = 0;

    while (run) {
        run_frame();

		key_input.w |= BTN_U;
		key_input.w |= BTN_D;
		key_input.w |= BTN_L;
		key_input.w |= BTN_R;
		key_input.w |= BTN_A;
		key_input.w |= BTN_B;
		key_input.w |= BTN_LT;
		key_input.w |= BTN_RT;
		key_input.w |= BTN_SEL;
		key_input.w |= BTN_STA;

		buttons_file = open(argv[2], O_RDONLY);
		read(buttons_file, &buttons_buffer, 13);
		close(buttons_file);

		// get key inputs
		if (buttons_buffer[0] != '0') run = false; // exit
		if (buttons_buffer[1] != '0') key_input.w &= ~BTN_U;
		if (buttons_buffer[2] != '0') key_input.w &= ~BTN_D;
		if (buttons_buffer[3] != '0') key_input.w &= ~BTN_L;
		if (buttons_buffer[4] != '0') key_input.w &= ~BTN_R;
		if (buttons_buffer[5] != '0') key_input.w &= ~BTN_SEL;
		if (buttons_buffer[6] != '0') key_input.w &= ~BTN_STA;
		if (buttons_buffer[7] != '0') key_input.w &= ~BTN_A;
		if (buttons_buffer[8] != '0') key_input.w &= ~BTN_B;
		if (buttons_buffer[11] != '0') key_input.w &= ~BTN_LT;
		if (buttons_buffer[12] != '0') key_input.w &= ~BTN_RT;

		if (fps_graphics > 0)
		{
			if (screen_handheld == 0)
			{
				unsigned short color = 0x0000;

				for (int y=0; y<160; y++)
				{
					for (int x=0; x<240; x++)
					{
						color = (unsigned short)(0x0000 |
							((screen[y * 240 * 4 + x * 4 + 3] & 0xF8) >> 3) |
							((screen[y * 240 * 4 + x * 4 + 2] & 0xFC) << 3) |
							((screen[y * 240 * 4 + x * 4 + 1] & 0xF8) << 8));

						screen_large_buffer[(y+40) * 640 * 2 + (x+40) * 2] = (unsigned short)color;
						screen_large_buffer[(y+40) * 640 * 2 + (x+40) * 2 + 1] = (unsigned short)color;
						screen_large_buffer[(y+40) * 640 * 2 + 640 + (x+40) * 2] = (unsigned short)color;
						screen_large_buffer[(y+40) * 640 * 2 + 640 + (x+40) * 2 + 1] = (unsigned short)color;
					}
				}

				screen_file = open("/dev/fb0", O_RDWR);
				write(screen_file, &screen_large_buffer, 640*480*2);
				close(screen_file);
			}
			else
			{
				for (int y=0; y<160; y++)
				{
					for (int x=0; x<240; x++)
					{
						screen_small_buffer[(y+40) * 320 + (x+40)] = (unsigned short)(0x0000 |
							((screen[y * 240 * 4 + x * 4 + 3] & 0xF8) >> 3) |
							((screen[y * 240 * 4 + x * 4 + 2] & 0xFC) << 3) |
							((screen[y * 240 * 4 + x * 4 + 1] & 0xF8) << 8));
					}
				}

				screen_file = open("/dev/fb0", O_RDWR);
				write(screen_file, &screen_small_buffer, 320*240*2);
				close(screen_file);
			}
		}
		
		if (fps_audio > 0)
		{
			unsigned short sound_temp = 0;

			// collect audio into array
			for (int i = 0; i < 512*fps_audio; i++)
			{
				sound_temp = (unsigned short)((snd_buffer[snd_cur_play & BUFF_SAMPLES_MSK] >> 2)); // left channel

				snd_cur_play++;
				if (snd_cur_play >= BUFF_SAMPLES) snd_cur_play -= BUFF_SAMPLES;

				sound_temp += (unsigned short)((snd_buffer[snd_cur_play & BUFF_SAMPLES_MSK] >> 2)); // right channel

				snd_cur_play++;
				if (snd_cur_play >= BUFF_SAMPLES) snd_cur_play -= BUFF_SAMPLES;

				sound_array[i] = (unsigned char)((unsigned short)(sound_temp) + 0x8000); // signed
			}

			//Avoid desync between the Play cursor and the Write cursor
			//snd_cur_play += ((int32_t)(snd_cur_write - snd_cur_play) >> 8) & ~1;

			// write audio to /dev/fb0
			write(sound_file, &sound_array, 512*2*fps_audio); // AUDIO_SAMPLES

			// reset
			snd_cur_play = 0;
			snd_cur_write = 0;
		}

		if (fps_wait > 0)
		{
			while (clock() < previous_clock + 16743 * fps_wait) { }
			previous_clock = clock();
		}

		fps_counter++;

		if (fps_rate == 60)
		{
			fps_counter = 0;

			fps_graphics = 1;
			fps_audio = 1;
			fps_wait = 1;
		}
		else if (fps_rate == 45)
		{
			if (fps_counter >= 3) fps_counter = 0;
			
			if (fps_counter == 0)
			{
				fps_graphics = 0;
				fps_audio = 3;
				fps_wait = 3;
			}
			else
			{
				fps_graphics = 1;
				fps_audio = 0;
				fps_wait = 0;
			}
		}
		else if (fps_rate == 30)
		{
			if (fps_counter >= 2) fps_counter = 0;
			
			if (fps_counter == 0)
			{
				fps_graphics = 0;
				fps_audio = 2;
				fps_wait = 2;
			}
			else
			{
				fps_graphics = 1;
				fps_audio = 0;
				fps_wait = 0;
			}
		}
		else if (fps_rate == 20)
		{
			if (fps_counter >= 3) fps_counter = 0;
			
			if (fps_counter == 0)	
			{
				fps_graphics = 1;
				fps_audio = 3;
				fps_wait = 3;
			}
			else
			{
				fps_graphics = 0;
				fps_audio = 0;
				fps_wait = 0;
			}
		}
    }

    arm_uninit();

	ioctl(tty_file, KDSETMODE, KD_TEXT); // turn on tty
	close(tty_file);

    return 0;
}
