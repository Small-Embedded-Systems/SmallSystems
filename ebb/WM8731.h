/**
*
* @section LICENSE
*
* Copyright (c) 2012 mbed
*
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files (the "Software"), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
* copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in
* all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
* THE SOFTWARE.
*
* @section DESCRIPTION
*    A Driver set for the I2C half of the WM8731. Based on the WM8731
*    code http://mbed.org/users/p07gbar/code/WM8731/
*
*/ 

#ifndef WM8731_H
#define WM8731_H

#include "mbed.h"

#define WM8731_CS_HIGH              true
#define WM8731_CS_LOW               false

#define WM8731_ON                   true
#define WM8731_OFF                  false

#define WM8731_MUTE                 true
#define WM8731_UNMUTE               false

#define WM8731_MASTER               true
#define WM8731_SLAVE                false

#define WM8731_LINE                 0
#define WM8731_MIC                  1
#define WM8731_NO_IN               -1

#define WM8731_DE_EMPH_DISABLED     0
#define WM8731_DE_EMPH_32KHZ        1
#define WM8731_DE_EMPH_44KHZ        2
#define WM8731_DE_EMPH_48KHZ        3

/** A class to control the I2C part of the WM8731
 * 
 */
class WM8731 {
public:

    /** Create an instance of the WM8731 class
     *
     * @param i2c_sda The SDA pin of the I2C
     * @param i2c_scl The SCL pin of the I2C
     */
    WM8731(PinName i2c_sda, PinName i2c_scl);
    
    /** Create an instance of the WM8731 class
     *
     * @param i2c_sda The SDA pin of the I2C
     * @param i2c_scl The SCL pin of the I2C
     * @param cs_level The level of the CS pin on the WM8731
     */
    WM8731(PinName i2c_sda, PinName i2c_scl, bool cs_level);
    
    /** Control the power of the device
     *
     * @param on_off The power state
     */
    void power(bool on_off);
    
    /** Control the input source of the device
     *
     * @param input Select the source of the input of the device: WM8731_LINE, WM8731_MIC, WM8731_NO_IN
     */
    void input_select(int input);
    
    /** Set the headphone volume
     *
     * @param h_volume The desired headphone volume: 0->1
     */
    void headphone_volume(float h_volume);
    
    /** Set the line in pre-amp volume
     *
     * @param li_volume The desired line in volume: 0->1
     */
    void linein_volume(float li_volume);
    
    /** Turn on/off the microphone pre-amp boost
     * 
     * @param mic_boost Boost on or off
     */
    void microphone_boost(bool mic_boost);
    
    /** Mute the input
     *
     * @param mute Mute on/off
     */
    void input_mute(bool mute);
    
    /** Mute the output
     *
     * @param mute Mute on/off
     */
    void output_mute(bool mute);
    
    /** Turn on/off the input stage
     *
     * @param on_off Input stage on(true)/off(false)
     */
    void input_power(bool on_off);
    
    /** Turn on/off the output stage
     *
     * @param on_off Output stage on(true)/off(false)
     */
    void output_power(bool on_off);
    
    /** Select the word size
     *
     * @param words 16/20/24/32 bits
     */
    void wordsize(int words);
    
    /** Select interface mode: Master or Slave
     *
     * @param master Interface mode: master(true)/slave
     */
    void master(bool master);
    
    /** Select the sample rate
     *
     * @param freq Frequency: 96/48/32/8 kHz
     */
    void frequency(int freq);
    
    /** Enable the input highpass filter
     *
     * @param enabled Input highpass filter enabled
     */
    void input_highpass(bool enabled);
    
    /** Enable the output soft mute
     *
     * @param enabled Output soft mute enabled
     */
    void output_softmute(bool enabled);
    
    /** Turn on and off the I2S
     * 
     * @param on_off Switch the I2S interface on(true)/off(false)
     */
    void interface_switch(bool on_off);
    
    /** Reset the device and settings
     *
     */
    void reset();
    
    /** Set the microphone sidetone volume
     *
     * @param sidetone_volume The volume of the sidetone: 0->1
     */
    void sidetone(float sidetone_vol);
    
    /** Set the analog bypass
     * 
     * @param bypass_en Enable the bypass: enabled(true)
     */
    void bypass(bool bypass_en);
    
    /** Set the deemphasis frequency
     *
     * @param code The deemphasis code: WM8731_DE_EMPH_DISABLED, WM8731_DE_EMPH_32KHZ, WM8731_DE_EMPH_44KHZ, WM8731_DE_EMPH_48KHZ
     */
    void deemphasis(char code);
    
    /** Enable the input highpass filter
     *
     * @param enable Enable the input highpass filter enabled(true)
     */
     
    void adc_highpass(bool enable);
    
    /** Start the device sending/recieving etc
    */
    void start();
    
    /** Stop the device sending/recieving etc
    */
    void stop();
    
private:
    
    enum reg_address {
        line_in_vol_left        = 0x00,
        line_in_vol_right       = 0x01,
        headphone_vol_left      = 0x02,
        headphone_vol_right     = 0x03,
        path_analog             = 0x04,
        path_digital            = 0x05,
        power_control           = 0x06,
        interface_format        = 0x07,
        sample_rate             = 0x08,
        interface_activation    = 0x09,
        reset_reg               = 0x0F,
        all                     = 0xFF
    };
    
    enum tlv320_defaults{
        df_bypass_         = 0,
        df_ADC_source      = WM8731_LINE,
        df_mic_mute        = WM8731_UNMUTE,
        df_li_mute_left    = 0,
        df_li_mute_right   = 0,
        df_mic_boost_      = 0,
        df_out_mute        = WM8731_UNMUTE,
        
        df_de_emph_code         = 0x00,
        df_ADC_highpass_enable  = 0,
        
        df_device_all_pwr     = 1,
        df_device_clk_pwr     = 1,
        df_device_osc_pwr     = 1,
        df_device_out_pwr     = 1,
        df_device_dac_pwr     = 1,
        df_device_adc_pwr     = 1,
        df_device_mic_pwr     = 0,
        df_device_lni_pwr     = 1,
        
        df_device_master      = 0,
        df_device_lrswap      = 0,
        df_device_lrws        = 0,
        df_device_bitlength   = 32,

        df_ADC_rate           = 32000,
        df_DAC_rate           = 32000,

        df_device_interface_active = 0
    };
    
    
    I2C i2c;
    uint8_t address;
    void command(reg_address add, uint16_t byte);
    void form_cmd(reg_address add);
    void defaulter();
    
    char gen_samplerate();
    
    //I2S i2s_tx(I2S_TRANSMIT, p5, p6 , p7);
    //I2S i2s_rx(I2S_RECIEVE , p8, p29, p30);
    
    float hp_vol_left, hp_vol_right;
    float li_vol_left, li_vol_right;
    float sdt_vol;
    bool li_mute_left, li_mute_right;
    bool bypass_;
    bool ADC_source;
    bool ADC_source_old;
    bool mic_mute;
    bool mic_boost_;
    bool out_mute;
    char de_emph_code;
    bool ADC_highpass_enable;
    
    bool device_all_pwr;
    bool device_clk_pwr;
    bool device_osc_pwr;
    bool device_out_pwr;
    bool device_dac_pwr;
    bool device_adc_pwr;
    bool device_mic_pwr;
    bool device_lni_pwr;
    
    bool device_master;
    bool device_lrswap;
    bool device_lrws;
    char device_bitlength;
    static const char device_data_form = 0x02;
    
    int ADC_rate;
    int DAC_rate;
    static const bool device_usb_mode = false;
    static const bool device_clk_in_div = false;
    static const bool device_clk_out_div = false;
    bool device_interface_active;
};

#endif
