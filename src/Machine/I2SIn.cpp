// I2SIn.cpp
// This I2SIn code uses the ESP32 IS21 device #1
// FluidNC's I2SOut uses IS20 device #0
//
// Copyright (c) 2018 - Simon Jouet
// Copyright (c) 2020 - Michiyasu Odaki
// Copyright (c) 2020 -	Mitch Bradley
// Copyright (c) 2021 - Patrick Horton
//
// Use of this source code is governed by a GPLv3 license that can be found in the LICENSE file.
// See https://gitlab.unizar.es/761347/esp-idf/blob/11c1da5276421569e8e2cfd5828d543dc7cbfec3/components/soc/esp32/include/soc/i2s_struct.h
// for the data structure that maps to the register addresses for I2S1 starting at 0x3FF6D000

#include "I2SIn.h"
#include "I2SIBus.h"

#include "../Logging.h"

#include <stdint.h>
#include <esp_attr.h>  // IRAM_ATTR
#include <freertos/FreeRTOS.h>
#include <driver/periph_ctrl.h>
#include <rom/lldesc.h>
#include <soc/i2s_struct.h>
#include <freertos/queue.h>
#include <stdatomic.h>


#define MONITOR_I2S_IN


#ifdef MONITOR_I2S_IN
    static uint32_t count_int =           0;
    static uint32_t count_chgs =          0;
    // static uint32_t count_rx_take_data =  0;
    // static uint32_t count_rx_wfull =      0;
    // static uint32_t count_rx_rempty =     0;
    // static uint32_t count_rx_hung =       0;
    static uint32_t count_in_done =       0;
    static uint32_t count_in_suc_eof =    0;
    // static uint32_t count_in_dscr_err =   0;
    // static uint32_t count_in_err_eof =    0;
    // static uint32_t count_in_dscr_empty = 0;
    static void monitorI2SInTask(void* parameter);
#endif


//----------------------------------------------------

#define I2S_IN_NUM_BITS  32     // 32 required (16 not tested) for FluidNC

static const int I2S_IN_DMABUF_COUNT  = 4;     // units, number of DMA buffers to store data
static const int I2S_IN_DMABUF_LEN    = 8;     // bytes; each buffer holds one 1 pair of L/R 32 bit samples (we only use the R)
static const int I2S_IN_SAMPLE_SIZE   = sizeof(uint32_t);     // 4 bytes, 32 bits per sample
static const int DMA_SAMPLE_COUNT     = I2S_IN_DMABUF_LEN / I2S_IN_SAMPLE_SIZE;  // there are two samples per buffer (we only use one)

typedef struct {
    uint32_t**   buffers;
    uint32_t*    current;
    uint32_t     rw_pos;
    lldesc_t**   desc;
    // xQueueHandle queue;
} i2s_in_dma_t;


static pinnum_t i2s_in_ws_pin    = 0;
static pinnum_t i2s_in_bck_pin   = 0;
static pinnum_t i2s_in_data_pin  = 0;
static int      i2s_in_num_chips = 0;

static i2s_in_dma_t i_dma;
static intr_handle_t i2s_in_isr_handle;
static int i2s_in_initialized = 0;
static uint32_t i2s_in_value = 0;

static portMUX_TYPE i2s_in_spinlock = portMUX_INITIALIZER_UNLOCKED;

#define I2S_IN_ENTER_CRITICAL()                                                                                                           \
    do {                                                                                                                                   \
        if (xPortInIsrContext()) {                                                                                                         \
            portENTER_CRITICAL_ISR(&i2s_in_spinlock);                                                                                     \
        } else {                                                                                                                           \
            portENTER_CRITICAL(&i2s_in_spinlock);                                                                                         \
        }                                                                                                                                  \
    } while (0)
#define I2S_IN_EXIT_CRITICAL()                                                                                                            \
    do {                                                                                                                                   \
        if (xPortInIsrContext()) {                                                                                                         \
            portEXIT_CRITICAL_ISR(&i2s_in_spinlock);                                                                                      \
        } else {                                                                                                                           \
            portEXIT_CRITICAL(&i2s_in_spinlock);                                                                                          \
        }                                                                                                                                  \
    } while (0)

#define I2S_IN_ENTER_CRITICAL_ISR() portENTER_CRITICAL_ISR(&i2s_in_spinlock)
#define I2S_IN_EXIT_CRITICAL_ISR() portEXIT_CRITICAL_ISR(&i2s_in_spinlock)
    // macros not used


// forwards

static void i2s_in_start();
static void IRAM_ATTR i2s_in_intr_handler(void* arg);




//--------------------------------------------------
// utilities
//--------------------------------------------------

static void gpio_matrix(pinnum_t gpio, uint32_t signal_idx, int mode, bool out_inv=false, bool oen_inv=false)
    // set pinMode and tie pins to I2S/DMA peripheral
{
    PIN_FUNC_SELECT(GPIO_PIN_MUX_REG[gpio], PIN_FUNC_GPIO);
    gpio_set_direction((gpio_num_t)gpio, (gpio_mode_t)mode);
    if (GPIO_MODE_DEF_INPUT==(gpio_mode_t)mode)
    {
        gpio_matrix_in(gpio, signal_idx, false);  // false = invert
    }
    else
    {
        // output pins can be iverted, though we use the params
        gpio_matrix_out(gpio, signal_idx, out_inv, oen_inv);
        // I'm still not sure oen_inv einv is .. there is no documentation on gpio_matrix_out()
    }
}


static inline void i2s_in_reset_fifo_without_lock()
{
    I2S1.conf.rx_fifo_reset = 1;
    I2S1.conf.rx_fifo_reset = 0;
    I2S1.conf.tx_fifo_reset = 1;
    I2S1.conf.tx_fifo_reset = 0;
}


static int i2s_clear_dma_buffer(lldesc_t* dma_desc)
{
    uint32_t* buf = (uint32_t*)dma_desc->buf;
    for (int i = 0; i < DMA_SAMPLE_COUNT; i++)
    {
        buf[i] = 0;
    }
    return 0;
}


static int i2s_clear_i_dma_buffers()
{
    for (int buf_idx = 0; buf_idx < I2S_IN_DMABUF_COUNT; buf_idx++)
    {
        i_dma.desc[buf_idx]->owner        = 1;  // 1 == "the allowed operator is the DMA controller"
        i_dma.desc[buf_idx]->eof          = 0;  // does nothing on rx
        i_dma.desc[buf_idx]->sosf         = 0;
        i_dma.desc[buf_idx]->length       = 0;                      // "number of valid bytes in the buffer"
        i_dma.desc[buf_idx]->size         = I2S_IN_DMABUF_LEN;      // "the size of the buffer"
        i_dma.desc[buf_idx]->buf          = (uint8_t*)i_dma.buffers[buf_idx];
        i_dma.desc[buf_idx]->offset       = 0;
        i_dma.desc[buf_idx]->qe.stqe_next = (lldesc_t*)((buf_idx < (I2S_IN_DMABUF_COUNT - 1)) ? (i_dma.desc[buf_idx + 1]) : i_dma.desc[0]);
        i2s_clear_dma_buffer(i_dma.desc[buf_idx]);
    }
    return 0;
}


//--------------------------------------------------
// public i2s_in_init()
//--------------------------------------------------

void i2s_in_init(pinnum_t ws, pinnum_t bck, pinnum_t data, int num_chips)
{
    Assert(!i2s_in_initialized,"i2s_in_init called more than once");

    i2s_in_ws_pin = ws;
    i2s_in_bck_pin = bck;
    i2s_in_data_pin = data;
    i2s_in_num_chips = num_chips;

    // To make sure hardware is enabled before any hardware register operations.

    periph_module_reset(PERIPH_I2S1_MODULE);
    periph_module_enable(PERIPH_I2S1_MODULE);

    // Route the i2s pins to the appropriate GPIO
    // The IDX's tie the pins to the I2S peripheral,
    // I2S1I_DATA_IN15_IDX is magic and MUST be used for RX

    gpio_matrix(i2s_in_ws_pin, I2S1I_WS_OUT_IDX, GPIO_MODE_DEF_OUTPUT);
    gpio_matrix(i2s_in_bck_pin, I2S1I_BCK_OUT_IDX, GPIO_MODE_DEF_OUTPUT);
    gpio_matrix(i2s_in_data_pin, I2S1I_DATA_IN15_IDX, GPIO_MODE_DEF_INPUT);

    // Allocate the array of pointers to the buffers

    i_dma.buffers = (uint32_t**)malloc(sizeof(uint32_t*) * I2S_IN_DMABUF_COUNT);
    Assert(i_dma.buffers != nullptr);

    // Allocate each buffer that can be used by the DMA controller

    for (int buf_idx = 0; buf_idx < I2S_IN_DMABUF_COUNT; buf_idx++)
    {
        i_dma.buffers[buf_idx] = (uint32_t*)heap_caps_calloc(1, I2S_IN_DMABUF_LEN, MALLOC_CAP_DMA);
        Assert(i_dma.buffers[buf_idx] != nullptr);
    }

    // Allocate the array of DMA descriptors

    i_dma.desc = (lldesc_t**)malloc(sizeof(lldesc_t*) * I2S_IN_DMABUF_COUNT);
    Assert(i_dma.desc != nullptr);

    // Allocate each DMA descriptor that will be used by the DMA controller

    for (int buf_idx = 0; buf_idx < I2S_IN_DMABUF_COUNT; buf_idx++)
    {
        i_dma.desc[buf_idx] = (lldesc_t*)heap_caps_malloc(sizeof(lldesc_t), MALLOC_CAP_DMA);
        Assert(i_dma.desc[buf_idx] != nullptr);
    }

    // Initialize dma

    i2s_clear_i_dma_buffers();
    i_dma.rw_pos  = 0;
    i_dma.current = NULL;
    // i_dma.queue   = xQueueCreate(I2S_IN_DMABUF_COUNT, sizeof(uint32_t*));

    // Set the first DMA descriptor

    I2S1.in_link.addr = (uint32_t)i_dma.desc[0];

    // stop i2s

    I2S1.in_link.stop = 1;
    I2S1.conf.rx_start = 0;
    I2S1.int_clr.val = I2S1.int_st.val;  //clear pending interrupt

    //reset i2s
    // This register is at 0x3ff6d008

    I2S1.conf.tx_reset = 1;
    I2S1.conf.tx_reset = 0;
    I2S1.conf.rx_reset = 1;
    I2S1.conf.rx_reset = 0;

    //reset dma

    I2S1.lc_conf.in_rst  = 1;  // Set this bit to reset in DMA FSM. (R/W)
    I2S1.lc_conf.in_rst  = 0;
    I2S1.lc_conf.out_rst = 1;  // Set this bit to reset out DMA FSM. (R/W)
    I2S1.lc_conf.out_rst = 0;

    i2s_in_reset_fifo_without_lock();

    // Basic configuration

    I2S1.lc_conf.ahbm_fifo_rst      = 0;
    I2S1.lc_conf.ahbm_rst           = 0;
    I2S1.lc_conf.out_loop_test      = 0;
    I2S1.lc_conf.in_loop_test       = 0;
    I2S1.lc_conf.out_auto_wrback    = 0;
    I2S1.lc_conf.out_no_restart_clr = 0;
    I2S1.lc_conf.out_eof_mode       = 0;
    I2S1.lc_conf.outdscr_burst_en   = 0;
    I2S1.lc_conf.indscr_burst_en    = 0;
    I2S1.lc_conf.out_data_burst_en  = 0;
    I2S1.lc_conf.check_owner        = 0;
    I2S1.lc_conf.mem_trans_en       = 0;

    I2S1.conf2.lcd_en               = 0;
    I2S1.conf2.camera_en            = 0;
    I2S1.pdm_conf.pcm2pdm_conv_en   = 0;
    I2S1.pdm_conf.pdm2pcm_conv_en   = 0;

    I2S1.fifo_conf.dscr_en = 0;

    // there is a "channel mode" and a "fifo mode"
    //
    // the "fifo mode" is
    //     0 == "16-bit dual channel data"
    //     1 == "16-bit single channel data"
    //     2 == "32-bit dual channel data"
    //     3 == "32-bit single channel data"
    //
    // the "channel mode" is only explained in a confusing table, in reference to "fifo_mode"
    // with both rows and columns labelled as "modes", we assume the doc's "I2S_RX_CHAN_MOD" header
    // applies to the column.  The "MSB_RIGHT" bit also comes into play and reverses the words
    // "left" and "right" in the below table (and in our usage) and doubling the size of the table
    // in the document.
    //
    //     FIFO MODE      0            1              2            3
    //  CHANNEL MODE
    //        0           left+right   -              left+right   -
    //        1           left+right   left+left      left+right   left+left
    //        2           left+right   right+right    left+right   right+right
    //        3           left+right   -              left+right   -
    //
    // where the "dashes" presumably indicate illegal combinations.
    // 2,2 seems the most logical and works the best for RX

    I2S1.conf_chan.rx_chan_mod = 2;
    I2S1.conf_single_data      = 0;
    I2S1.conf.rx_mono          = 0;

#if I2S_IN_NUM_BITS == 16
    // Not used (or tested) for FluidNC
    I2S1.fifo_conf.tx_fifo_mod        = 0;
    I2S1.fifo_conf.rx_fifo_mod        = 0;
    I2S1.sample_rate_conf.tx_bits_mod = 16;  // default is 16-bits
    I2S1.sample_rate_conf.rx_bits_mod = 16;  // default is 16-bits
#else
    // fifo_mode 2 delivers twice as many frame interrupts as fifo_mode 3
    // fifo_mode 1 delivers half as many frame interrupts as fifo mode 3
    I2S1.fifo_conf.tx_fifo_mod = 2;
    I2S1.fifo_conf.rx_fifo_mod = 2;
    // Data width is 32-bit. Forgetting this setting will result in a 16-bit transfer.
    I2S1.sample_rate_conf.tx_bits_mod = 32;
    I2S1.sample_rate_conf.rx_bits_mod = 32;
#endif

    I2S1.fifo_conf.dscr_en = 1;  //connect DMA to fifo
    I2S1.conf.tx_start     = 0;
    I2S1.conf.rx_start     = 0;

    I2S1.conf.rx_msb_right   = 1;  // Set this bit to place right-channel data at the MSB in the receive FIFO.
    I2S1.conf.rx_right_first = 0;  // Setting this bit allows the right-channel data to be received first.

    I2S1.conf.tx_slave_mod              = 0;  // Master
    I2S1.fifo_conf.tx_fifo_mod_force_en = 1;  // The bit should always be set to 1.
    I2S1.fifo_conf.rx_fifo_mod_force_en = 1;  // The bit should always be set to 1.
    I2S1.pdm_conf.rx_pdm_en             = 0;  // Set this bit to enable receiver’s PDM mode.
    I2S1.pdm_conf.tx_pdm_en             = 0;  // Set this bit to enable transmitter’s PDM mode.

    // I2S_COMM_FORMAT_I2S_LSB

    I2S1.conf.tx_short_sync = 0;  // Set this bit to enable transmitter in PCM standard mode.
    I2S1.conf.rx_short_sync = 0;  // Set this bit to enable receiver in PCM standard mode.
    I2S1.conf.tx_msb_shift  = 0;  // Do not use the Philips standard to avoid bit-shifting
    I2S1.conf.rx_msb_shift  = 0;  // Do not use the Philips standard to avoid bit-shifting

    //-----------------------------------------------------
    // Important, empirically derived, settings:
    //-----------------------------------------------------

    I2S1.rx_eof_num = DMA_SAMPLE_COUNT;
        // This statement says that a frame is 2 32-bit samples in length.
        // IMPORTANT  - this actually determines the frequency of ISR  "frame success" interrupts.
        // From the documentation: "I2S_RXEOF_NUM_REG is used for configuring the data size of a
        // single transfer operation, in multiples of one word" where a word is 32 bits.
        // Except it's NOT just for single transfers but for the general frame layout on RX.
        // Without this no matter what else you do you will get a (low) constant number of
        // frame success interrupts regardless of what you put in the i_dma.desc values.

    // With the 74HC165's that I have, there was a problem that they were transmitting
    // their data immediately upon WS going high at the same time BCK goes low.
    // I "fixed" that with a frequency dependent capacitor tween WS and ground.
    // After much experimentation I determined that you can influence the ESP32's interpretation
    // of the frame so that it "works", more or less with either of the 2 settings below.

    // It seems to spuriously change values.
    // More testing is needed (on a soldered circuit board)
    // 472 ceramic capacitor for 4Mhz BCK
    // 104 ceramic capacitor for 500Khz BCK

    // I2S1.timing.tx_bck_in_delay    = 0;    // : 2;
    // I2S1.timing.tx_ws_in_delay     = 0;    // : 2;
    // I2S1.timing.rx_bck_in_delay    = 0;    // : 2;
    // I2S1.timing.rx_ws_in_delay     = 0;    // : 2;
    // I2S1.timing.rx_sd_in_delay     = 2;    // : 2;   2 SEEMED TO WORK  3 intermittent
    // I2S1.timing.tx_bck_out_delay   = 0;    // : 2;
    // I2S1.timing.tx_ws_out_delay    = 0;    // : 2;
    // I2S1.timing.tx_sd_out_delay    = 0;    // : 2;
    I2S1.timing.rx_ws_out_delay    = 2;    // : 2;   2 SEEMED TO WORK  3 intermittent
    // I2S1.timing.rx_bck_out_delay   = 0;    // : 2;
    // I2S1.timing.tx_dsync_sw        = 0;    // : 1;
    // I2S1.timing.rx_dsync_sw        = 0;    // : 1;
    // I2S1.timing.data_enable_delay  = 0;    // : 2;
    // I2S1.timing.tx_bck_in_inv      = 0;    // : 1;

    //-------------------
    // set the clock
    //-------------------
    // set clock (fi2s) 160MHz / N
    // frequency could be parameterized

    I2S1.clkm_conf.clka_en = 0;  // Use 160 MHz PLL_D2_CLK as reference
#if I2S_IN_NUM_BITS == 16
    // not used or tested for FluidNC
    I2S1.clkm_conf.clkm_div_num = 10;  // minimum value of 2, reset value of 4, max 256 (I²S clock divider’s integral value)
#else
    I2S1.clkm_conf.clkm_div_num = 80;   // prh was 5;  and giving 14.7Mhz // minimum value of 2, reset value of 4, max 256 (I²S clock divider’s integral value)
        // was/is 5 in I2SO giving 16Mhz -- 10=8Mhz -- 20==4Mhz
#endif
    // b/a = 0
    I2S1.clkm_conf.clkm_div_b = 0;  // 0 at reset
    I2S1.clkm_conf.clkm_div_a = 0;  // 0 at reset, what about divide by 0? (not an issue)
    // Bit clock configuration bit in transmitter mode.
    // fbck = fi2s / tx_bck_div_num = (160 MHz / 5) / 2 = 16 MHz
    I2S1.sample_rate_conf.tx_bck_div_num = 2;  // minimum value of 2 defaults to 6
    I2S1.sample_rate_conf.rx_bck_div_num = 2;

    // Enable RX interrupts (DMA Interrupts)
    // All possible interrupts are listed here
    // and all RX ones were were experimented with.
    // The only one that is needed is in_suc_eof

    // I2S1.int_ena.rx_take_data =  0;
    // I2S1.int_ena.rx_wfull =      0;
    // I2S1.int_ena.rx_rempty =     0;         // setting this one will crash the machine
    // I2S1.int_ena.rx_hung =       0;
    // I2S1.int_ena.in_done =       0;
    I2S1.int_ena.in_suc_eof =    1;
    // I2S1.int_ena.in_dscr_err =   0;
    // I2S1.int_ena.in_err_eof =    0;
    // I2S1.int_ena.in_dscr_empty = 0;
    // I2S1.int_ena.tx_put_data =   0;
    // I2S1.int_ena.tx_wfull =      0;
    // I2S1.int_ena.tx_rempty =     0;
    // I2S1.int_ena.tx_hung =       0;
    // I2S1.int_ena.out_done =      0;
    // I2S1.int_ena.out_eof =       0;
    // I2S1.int_ena.out_dscr_err =  0;
    // I2S1.int_ena.out_total_eof = 0;

    // Allocate and Enable the I2S interrupt

    esp_intr_alloc(ETS_I2S1_INTR_SOURCE, 0, i2s_in_intr_handler, nullptr, &i2s_in_isr_handle);
    esp_intr_enable(i2s_in_isr_handle);

    // Remember GPIO pin numbers

    i2s_in_initialized = 1;

    // Start the I2S peripheral

    i2s_in_start();

    // Create the task that will monitor the buffers

    #ifdef MONITOR_I2S_IN
        xTaskCreatePinnedToCore(
            monitorI2SInTask,
            "monitorI2SInTask",
            4096,
            NULL,
            1,
            nullptr,
            CONFIG_ARDUINO_RUNNING_CORE  // must run the task on same core
        );
    #endif
}



//-------------------------------------------
// i2s_in_start() and stop()
//-------------------------------------------

static void i2s_in_start()
{
    I2S_IN_ENTER_CRITICAL();

    // reest TX/RX module

    I2S1.conf.tx_reset = 1;
    I2S1.conf.tx_reset = 0;
    I2S1.conf.rx_reset = 1;
    I2S1.conf.rx_reset = 0;

    // reset DMA

    I2S1.lc_conf.in_rst  = 1;
    I2S1.lc_conf.in_rst  = 0;
    I2S1.lc_conf.out_rst = 1;
    I2S1.lc_conf.out_rst = 0;

    I2S1.in_link.addr = (uint32_t)i_dma.desc[0];

    // reset FIFO

    i2s_in_reset_fifo_without_lock();

    // start DMA link

    I2S1.conf_chan.rx_chan_mod = 1;  // 0-two channel;1-right;2-left;3-righ;4-left
    I2S1.conf_single_data      = 0;

    // Connect DMA to FIFO

    I2S1.fifo_conf.dscr_en = 1;  // Set this bit to enable I2S DMA mode. (R/W)

    I2S1.int_clr.val    = 0xFFFFFFFF;
    I2S1.in_link.start = 1;

    I2S1.conf.rx_start = 1;

    I2S_IN_EXIT_CRITICAL();
}


//------------------------------------------
// I2SIn DMA Interrupt handler
//------------------------------------------

static void IRAM_ATTR i2s_in_intr_handler(void* arg)
{
    #ifdef MONITOR_I2S_IN
        count_int++;
        // if (I2S1.int_st.rx_take_data  ) count_rx_take_data  ++;
        // if (I2S1.int_st.rx_wfull      ) count_rx_wfull      ++;
        // if (I2S1.int_st.rx_rempty     ) count_rx_rempty     ++;
        // if (I2S1.int_st.rx_hung       ) count_rx_hung       ++;
        if (I2S1.int_st.in_done       ) count_in_done       ++;
        if (I2S1.int_st.in_suc_eof    ) count_in_suc_eof    ++;
        // if (I2S1.int_st.in_dscr_err   ) count_in_dscr_err   ++;
        // if (I2S1.int_st.in_err_eof    ) count_in_err_eof    ++;
        // if (I2S1.int_st.in_dscr_empty ) count_in_dscr_empty ++;
    #endif

    if (I2S1.int_st.in_suc_eof)
    {
        // Get the descriptor of the last item in the linked list, then
        // get the value from it's buffer, shift it as needed, and if
        // changed, hand it off to the I2SIBus for handling

        lldesc_t* finish_desc = (lldesc_t*)I2S1.in_eof_des_addr;    // in_link_dscr;
        uint32_t *buf_ptr = (uint32_t*)finish_desc->buf;
        uint32_t value = *buf_ptr;    // 1 sample per buffer
        value >>= (4 - i2s_in_num_chips) *8;

        if (i2s_in_value != value)
        {
            #ifdef MONITOR_I2S_IN
                count_chgs++;
            #endif

            i2s_in_value = value;
            // log_debug(String(i2s_in_value,HEX));
            Machine::I2SIBus::handleValueChange(i2s_in_value);
        }
   }

    // clear interrupt

    I2S1.int_clr.val = I2S1.int_st.val;  //clear pending interrupt
}




//--------------------------------------------------------
// I2SI monitoring task
//--------------------------------------------------------

#ifdef MONITOR_I2S_IN
    static void monitorI2SInTask(void* parameter)
    {
        while (1)
        {
            vTaskDelay(1000);        // every second
            log_debug(
                "isr=" << count_int <<
                " chg=" << count_chgs  <<
                // " td=" << count_rx_take_data  <<
                // " wf=" << count_rx_wfull      <<
                // " re=" << count_rx_rempty     <<
                // " rh=" << count_rx_hung       <<
                " id=" << count_in_done       <<
                " is=" << count_in_suc_eof    <<
                // " ie=" << count_in_dscr_err   <<
                // " ef=" << count_in_err_eof    <<
                // " de=" << count_in_dscr_empty <<
                " val=" << String(i2s_in_value,HEX) );

            static UBaseType_t uxHighWaterMark = 0;
    #ifdef DEBUG_TASK_STACK
            reportTaskStackSize(uxHighWaterMark);
    #endif
        }
    }
#endif
