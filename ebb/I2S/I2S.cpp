#include "I2S.h"

#define I2S_DF_WORDWIDTH                16
#define I2S_DF_SAMPLERATE               48000
#define I2S_DF_MASTERSLAVE              I2S_SLAVE
#define I2S_DF_STEREOMONO               I2S_STEREO
#define I2S_DF_MUTED                    I2S_UNMUTED
#define I2S_DF_INTERRUPT_FIFO_LEVEL     4

#define I2S_MAX_DENOMINATOR             256
#define I2S_MAX_NUMERATOR               256
#define I2S_MAX_BITRATE_DIV             64

//#define I2S_PCLK_RATE                   24000000
#define I2S_PCLK_RATE                   120000000

FunctionPointer I2S::I2STXISR;
FunctionPointer I2S::I2SRXISR;

bool I2S::txisr;
bool I2S::rxisr;

I2S::I2S(bool rxtx, PinName sd, PinName ws, PinName clk)
{
    NVIC_DisableIRQ (I2S_IRQn);

    _sd = sd;
    _ws = ws;
    _clk = clk;
    _rxtx = rxtx;

    ws_d = true;
    clk_d = true;
    mclk_d = false;

    fourwire = false;

    reg_write_err = 0;

    pin_setup();

    if (pin_setup_err != 0) {
        perror("I2S Pins incorrectly defined.");
    }

    defaulter();
}

I2S::I2S(bool rxtx, PinName sd)
{
    NVIC_DisableIRQ (I2S_IRQn);

    _sd = sd;
    _rxtx = rxtx;

    ws_d = false;
    clk_d = false;
    mclk_d = false;

    fourwire = false;

    reg_write_err = 0;

    pin_setup();

    if (pin_setup_err != 0) {
        perror("I2S Pins incorrectly defined.");
    }

    defaulter();
}

I2S::I2S(bool rxtx, PinName sd, bool fourwiremode)
{
    NVIC_DisableIRQ (I2S_IRQn);

    _sd = sd;
    _rxtx = rxtx;

    ws_d = false;
    clk_d = false;
    mclk_d = false;

    reg_write_err = 0;

    fourwire = fourwiremode;

    pin_setup();

    if (pin_setup_err != 0) {
        perror("I2S Pins incorrectly defined.");
    }

    defaulter();
}

I2S::I2S(bool rxtx, PinName sd, PinName ws, bool fourwiremode)
{
    NVIC_DisableIRQ (I2S_IRQn);

    _sd = sd;
    _ws = ws;
    _rxtx = rxtx;

    ws_d = true;
    clk_d = false;
    mclk_d = false;

    reg_write_err = 0;

    fourwire = fourwiremode;

    pin_setup();

    if (pin_setup_err != 0) {
        perror("I2S Pins incorrectly defined.");
    }

    defaulter();
}

I2S::I2S(bool rxtx, PinName sd, PinName ws)
{
    NVIC_DisableIRQ (I2S_IRQn);

    _sd = sd;
    _ws = ws;
    _rxtx = rxtx;

    ws_d = true;
    clk_d = false;
    mclk_d = false;

    reg_write_err = 0;

    fourwire = false;

    pin_setup();

    if (pin_setup_err != 0) {
        perror("I2S Pins incorrectly defined.");
    }

    defaulter();
}

I2S::~I2S()
{
    NVIC_DisableIRQ (I2S_IRQn);
    deallocating = true;
    pin_setup();
    write_registers();
}

void I2S::defaulter()
{
    LPC_SC->PCONP |= (1 << 27);

    stop();
    master = false;
    deallocating = false;

    frequency(I2S_DF_SAMPLERATE);
    wordsize(I2S_DF_WORDWIDTH);
    masterslave (I2S_DF_MASTERSLAVE);
    stereomono (I2S_DF_STEREOMONO);
    set_interrupt_fifo_level(I2S_DF_INTERRUPT_FIFO_LEVEL);
    mute (I2S_DF_MUTED);

    NVIC_SetVector(I2S_IRQn, (uint32_t) & _i2sisr);
    NVIC_EnableIRQ (I2S_IRQn);
}

void I2S::write(char buf[], int len)
{
    if (_rxtx == I2S_TRANSMIT) {
        if (len > max_fifo_points())
            len = max_fifo_points();
        if (len <= 0)
            return;
        int temp = 0;
        for (int i = 0; i < len; i += 4) {
            temp = 0;
            for (int j = 0; j < 4; j++) {
                temp |= int(buf[i + j]) << (j * 8);
            }
            LPC_I2S->TXFIFO = temp;
        }
    }

}

void I2S::write(int buf[], int len)
{
    if (_rxtx == I2S_TRANSMIT && wordwidth > 0) {
        if (len > max_fifo_points()) {
            len = max_fifo_points();
            printf("Trying to write too much data!\n\r");
        }
        if (len <= 0)
            return;
        uint32_t temp = 0;
        int increment = 32 / wordwidth;
        unsigned char recast[] =
        { 0, 0, 0, 0 };
        for (int i = 0; i < len; i += increment) {
            temp = 0;

            switch (wordwidth) {

                case 8:

                    recast[0] = (int8_t) buf[i + 0];
                    recast[1] = (int8_t) buf[i + 1];
                    recast[2] = (int8_t) buf[i + 2];
                    recast[3] = (int8_t) buf[i + 3];
                    break;
                case 16:
                    recast[0] = (((int16_t) buf[i + 0]) >> 0) & 0xFF;
                    recast[1] = (((int16_t) buf[i + 0]) >> 8) & 0xFF;
                    recast[2] = (((int16_t) buf[i + 1]) >> 0) & 0xFF;
                    recast[3] = (((int16_t) buf[i + 1]) >> 8) & 0xFF;
                    break;
                case 32:
                    recast[0] = (((int32_t) buf[i + 0]) >> 0) & 0xFF;
                    recast[1] = (((int32_t) buf[i + 0]) >> 8) & 0xFF;
                    recast[2] = (((int32_t) buf[i + 0]) >> 16) & 0xFF;
                    recast[3] = (((int32_t) buf[i + 0]) >> 24) & 0xFF;
                    break;

            }
            for (int j = 0; j < 4; j++) {

                temp |= recast[j] << (j * 8);
            }

            //if(((temp >> 16) & 0xFFFF) == 0xFFFF) printf("Hmmm %x %x %x\n\r",temp, increment,i); //|| temp &0xFFFF == 0xFFFF
            //if((buf[i]-buf[i+1])>5000 || (buf[i]-buf[i+1])<-5000) printf("J:%i,%i\n\r",buf[i],buf[i+1]);
            //printf("%x\n",temp);
            LPC_I2S->TXFIFO = temp;
        }
    }
}

void I2S::write(int bufr[], int bufl[], int len)
{
    //#TODO: Write this!
}

int I2S::read()
{
    return LPC_I2S->RXFIFO;
}

void I2S::read(char buf[], int len)
{
    bool len_valid = true;
    if (len <= 0)
        return;
    if (len >= fifo_points())
        len = fifo_points();
    int temp[8];
    int counter = 0;
    int increment = 4;            //32/wordwidth;
    int fifo_levl = fifo_level();
    while (counter < fifo_levl && len_valid) {
        temp[counter] = LPC_I2S->RXFIFO;
        for (int j = 0; j < increment; j++) {
            if ((counter * 4) + j > len) {
                len_valid = false;
                break;
            }
            buf[counter + j] = temp[counter] >> (j * 8);

        }
        counter++;
    }
}

void I2S::read(int buf[], int len)
{
    bool len_valid = true;
    if (len <= 0)
        return;
    if (len >= fifo_points())
        len = fifo_points();
    int temp[8];
    int counter = 0;
    int increment = 32 / wordwidth;
    int fifo_levl = fifo_level();
    while (counter < fifo_levl && len_valid) {
        temp[counter] = LPC_I2S->RXFIFO;
        for (int j = 0; j < increment; j++) {
            if ((counter * increment) + j > len) {
                len_valid = false;
                break;
            }
            buf[counter + j] = temp[counter] >> (j * wordwidth);

        }
        counter++;
    }
}

void I2S::read(int bufr[], int bufl[], int len)
{
    //#TODO: Write this
}

int I2S::max_fifo_points()
{
    switch (wordwidth) {
        case 8:
            return (4 * 8);
        case 16:
            return (2 * 8);
        case 32:
            return 8;
        default:
            return 0;
    }
}

int I2S::fifo_points()
{
    switch (wordwidth) {
        case 8:
            return (4 * fifo_level());
        case 16:
            return (2 * fifo_level());
        case 32:
            return fifo_level();
        default:
            return 0;
    }
}

void I2S::power(bool pwr)
{
    if (pwr) {
        stopped = false;
    } else {
        stopped = true;
    }
    write_registers();
}

void I2S::masterslave(bool mastermode)
{
    if (mastermode == I2S_MASTER) {
        master = true;
    } else {
        master = false;
    }
    write_registers();
}

void I2S::wordsize(int words)
{
    wordwidth = words;
    write_registers();
}

void I2S::mclk_freq(int freq)
{
    mclk_frequency = freq;
    write_registers();
}

void I2S::frequency(int desired_freq)
{
    freq = desired_freq;
    write_registers();
}

int I2S::fifo_level()
{
    int level = 0;
    if (_rxtx == I2S_TRANSMIT) {
        level = LPC_I2S->STATE;
        level >>= 16;
        level &= 0xF;
    } else {
        level = LPC_I2S->STATE;
        level >>= 8;
        level &= 0xF;
    }
    return level;
}

void I2S::stereomono(bool stereomode)
{
    if (stereomode == I2S_STEREO) {
        stereo = true;
    } else {
        stereo = false;
    }
}

void I2S::mute()
{
    muted = true;
    write_registers();
}

void I2S::mute(bool mute_en)
{
    muted = mute_en;
    write_registers();
}

void I2S::stop()
{
    stopped = true;
    write_registers();
}

void I2S::set_interrupt_fifo_level(int level)
{
    interrupt_fifo_level = level;
    write_registers();
}

void I2S::start()
{
    stopped = false;
    muted = false;
    write_registers();
}

bool I2S::setup_ok()
{
    if ((reg_write_err + pin_setup_err) > 0)
        return false;
    else
        return true;
}

void I2S::pin_setup()
{
    pin_setup_err = 0;

    if (_rxtx == I2S_TRANSMIT) {
        //printf("\n\rSetting up pins....\n\r");
        if (_sd != p11)
            pin_setup_err++;
        if (_ws != p12 && ws_d == true)
            pin_setup_err++;
        if (_clk != p13 && clk_d == true)
            pin_setup_err++;
        //printf("Hmm....%i\n\r", pin_setup_err);
    } else {
        if (_sd != p14)
            pin_setup_err++;
        if (_ws != p33 && ws_d == true)
            pin_setup_err++;
        if (_clk != p34 && clk_d == true)
            pin_setup_err++;
    }

    if (pin_setup_err == 0) {
        if (_rxtx == I2S_TRANSMIT) {
            int val1 = 1;
            if (deallocating) {
                val1 = 0;
            }
            LPC_IOCON->P0_9 |= ((1<<7) | (1<<0));
//            LPC_PINCON->PINSEL0 |= (val1 << 18); //set p5 as transmit serial data line
            if (ws_d == true)
                LPC_IOCON->P0_8 |= ((1<<7) | (1<<0));
//                LPC_PINCON->PINSEL0 |= (val1 << 14); //set p7 as transmit clock line
            if (clk_d == true)
                LPC_IOCON->P0_7 |= ((1<<7) | (1<<0));
//                LPC_PINCON->PINSEL0 |= (val1 << 16); //set p6 as word select line

        } else {
            int val1 = 1;
            int val2 = 2;
            if (deallocating) {
                val1 = 0;
                val2 = 0;
            }

//            if (_sd == p8)
            LPC_IOCON->P0_6 |= (1<<0);
//                LPC_PINCON->PINSEL0 |= (val1 << 12);
//            else
//                LPC_PINCON->PINSEL1 |= (val2 << 18);

            if (ws_d == true) {
                LPC_IOCON->P0_5 |= (1<<0);
//                if (_ws == p29)
//                    LPC_PINCON->PINSEL0 |= (val1 << 10);
//                else
//                    LPC_PINCON->PINSEL1 |= (val2 << 16);
            }

            if (clk_d == true) {
                LPC_IOCON->P0_4 |= (1<<0);
//                if (_clk == p15)
//                    LPC_PINCON->PINSEL0 |= (val1 << 8);
//                else
//                    LPC_PINCON->PINSEL1 |= (val2 << 14);
            }
        }
    }
}

void I2S::write_registers()
{
    reg_write_err = 0;
    //Clock Multiplier Calculations
    float pre_mult = 0;
    int pre_num = 0;
    int pre_den = 0;
    int bitrate_div = 0;
    if (master == true) { // In the hope of not doing all this heavy lifting every configuration
        //printf("Doing some clock magic..\n\r");
        int bitrate = freq * 64;
        float target_div = I2S_PCLK_RATE / float(bitrate * 2);// Work out what divider is needed in the end, including the halving of rates the smoother does
        if (mclk_frequency == 0) {
            float rnd = fmod(target_div,1);// To make the X/Y fraction closest to 1, we set the last divider to the nearest integer to the rate divider
            bitrate_div = int(target_div - rnd);
            while (bitrate_div > I2S_MAX_BITRATE_DIV) { // But this might be out of range, so we right shift it into focus
                bitrate_div >>= 1;
            }
            if (bitrate_div == 0) { // Could be zero, which would disable the the clock...
                bitrate_div = 1;
            }
            pre_mult = float(bitrate_div) / target_div;    // Work out what we have left to correct
            pre_num = 0;
            pre_den = 0;
            fraction_estimator(pre_mult, &pre_num, &pre_den);// Get the function to work out the closest fraction, there might be some point in adding some possible multipliers of these values to add to the smoothing, the reference manual (UM10360 page 480) suggests this

        } else {
            pre_mult = float(mclk_frequency * 2) / (I2S_PCLK_RATE);
            pre_num = 0;
            pre_den = 0;
            fraction_estimator(pre_mult, &pre_num, &pre_den);// Get the function to work out the closest fraction, there might be some point in adding some possible multipliers of these values to add to the smoothing, the reference manual (UM10360 page 480) suggests this
            bitrate_div = int(
                              I2S_PCLK_RATE * float(pre_num) / float(pre_den)
                              / float(bitrate));
        }

        old_freq = freq;
        old_pre_num = pre_num;
        old_pre_den = pre_den;
        old_bitrate_div = bitrate_div;
    } else {
        pre_num = 0; //old_pre_num;
        pre_den = 0; //old_pre_den;
        bitrate_div = 1; //old_bitrate_div;
    }

    //Clock Multiplier, MCLK setup
    if (_rxtx == I2S_TRANSMIT) {
        int regvals = ((pre_num << 8) & 0xFF00) | (pre_den & 0xFF);
        LPC_I2S->TXRATE = regvals;                // Setting the X/Y fraction
        LPC_I2S->TXBITRATE = (bitrate_div - 1) & 0x3F;// Setting up the bitrate divider, the periferal adds one to this

        LPC_I2S->TXMODE = fourwire << 2;

        if (mclk_d == true) {
            LPC_I2S->TXMODE |= (1 << 3);
        }
    } else {
        int regvals = ((pre_num << 8) & 0xFF00) | (pre_den & 0xFF);
        LPC_I2S->RXRATE = regvals;                // Setting the X/Y fraction
        LPC_I2S->RXBITRATE = (bitrate_div - 1) & 0x3F;// Setting up the bitrate divider, the periferal adds one to this

        LPC_I2S->RXMODE = fourwire << 2;

        if (mclk_d == true) {
            LPC_I2S->RXMODE |= (1 << 3);
        }
    }

    switch (wordwidth) {
        case 8:
            wordwidth_code = 0;
            break;
        case 16:
            wordwidth_code = 1;
            break;
        case 32:
            wordwidth_code = 3;
            break;
        default:
            reg_write_err++;
            break;
    }

    int I2SDA_reg = (wordwidth_code & 0x3);
    I2SDA_reg |= ((!stereo << 2) & 0x4);
    I2SDA_reg |= ((stopped << 3) & 0x8);
    I2SDA_reg |= ((!master << 5) & 0x20);
    I2SDA_reg |= (0x1F << 6);
    I2SDA_reg |= ((muted << 15) & 0x8000);

    if (_rxtx == I2S_TRANSMIT) {
        LPC_I2S->DAO = I2SDA_reg;
    } else {
        LPC_I2S->DAI = I2SDA_reg;
    }

    if (_rxtx == I2S_TRANSMIT) {
        if (txisr) {
            LPC_I2S->IRQ = (LPC_I2S->IRQ & 0xFF0FFFFF)
                              | ((interrupt_fifo_level & 0xF) << 16);
            LPC_I2S->IRQ |= 0x2;
        } else {
            LPC_I2S->IRQ &= 0xFFFFFFFD;
        }
    } else {
        if (rxisr) {
            LPC_I2S->IRQ = (LPC_I2S->IRQ & 0xFFFFF0FF)
                              | ((interrupt_fifo_level & 0xF) << 8);
            LPC_I2S->IRQ |= 0x1;
        }

        else {
            LPC_I2S->IRQ &= 0xFFFFFFFE;
        }
    }
}

void I2S::_i2sisr(void)
{
    I2STXISR.call();
    I2SRXISR.call();
}

// A function to find the nearest fraction to that put to it, with numerator and denomnator less than 256
// This is used when trying to get the clocks correct

void I2S::fraction_estimator(float in, int * num, int * den)
{
    int test_num = 0;
    int test_den = 0;
    float least_error = 1;
    int least_err_den = 0;
    float genval;
    float generr;

    for (test_den = 1; test_den < I2S_MAX_DENOMINATOR; test_den++) {
        test_num = int(float(test_den) * in);
        if (test_num < I2S_MAX_NUMERATOR && test_num > 0) {
            genval = float(test_num) / float(test_den);
            generr = mod(genval - in);
            if (generr < least_error) {
                least_error = generr;
                least_err_den = test_den;
            }
            if (generr == 0) {
                break;
            }
        }
    }

    test_num = int(float(least_err_den) * in);
    *num = test_num;
    *den = least_err_den;

}

float I2S::mod(float in)
{
    if (in < 0)
        in *= -1;

    return in;
}