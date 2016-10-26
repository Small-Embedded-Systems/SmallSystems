/*
 *  Copyright 2013 Embedded Artists AB
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 */

#ifndef XBEE_H
#define XBEE_H

#define RX_BUF_SIZE (512)
#define XBEE_BUF_SZ (200)

#define XBEE_ADDRLO_BROADCAST (0x0000FFFF)
#define XBEE_ADDRHI_BROADCAST (0x00000000)

/**
 * Interface to Digi International's XBee module. The XBee S1 module has
 * been used during testing of this interface.
 */
class XBee {
public:

    /** Error codes returned from public methods */
    enum XBeeError {
        Ok = 0,
        ReadError = -1,
        CmdError = -2,
        BufTooSmallError = -3,
        TimeOutError = -4,
        NotInitializedError = -5,
        ArgumentError = -6

    };

    /** Callback function/method types. See registerCallback() */
    enum CallbackType {
        /** Device is up and ready */
        CbDeviceUp = 0,
        /** Device is down (disconnected) */
        CbDeviceDown,
        /** A node has been found */
        CbNodeFound,
        /** Transmit status */
        CbTxStat,
        /** Data is available */
        CbDataAvailable,
        CbNum // must be last

    };

    /** Xbee types */
    enum XBeeType {
        EndDevice = 0,
        Coordinator
    };

    /** Transmit status */
    enum XBeeTxStatus {
        TxStatusOk = 0,
        TxStatusNoAck,
        TxStatusCCA,
        TxStatusPurged
    };

    /**
     * Create an interface to an XBee module.
     *
     * @param tx UART TX line
     * @param tx UART rx line
     * @param reset reset pin
     * @param sleep sleep request pin
     */
    XBee(PinName tx, PinName rx, PinName reset, PinName sleep);

    /**
     * Initialize the XBee module and configure it to be of a
     * specific type.
     *
     * Note: This implementation will always configure the XBee module to
     * work in API mode.
     *
     * @param type the type of this XBee node
     * @param panId the PAN ID to use for the XBee network. This  argument
     * must contain 4 characters representing hexadecimal values
     * (e.g. "A1E0" means the hexadecimal value 0xA1E0);
     */
    XBeeError init(XBeeType type, const char* panId);

    /**
     * Register a callback function
     *
     * @param fptr Callback function to register
     * @param type The type of event that will trigger a call to the function.
     */
    void registerCallback(void (*fptr)(void), CallbackType type) {
        if (fptr) {
            _callbacks[type].attach(fptr);
        }
    }

    /**
     * Register a callback method
     *
     * @param tptr pointer to the object to call the member function on
     * @param mptr pointer to the member function to be called
     * @param type The type of event that will trigger a call to the method.
     */
    template<typename T>
    void registerCallback(T* tptr, void (T::*mptr)(void), CallbackType type) {
        if((mptr != NULL) && (tptr != NULL)) {
            _callbacks[type].attach(tptr, mptr);
        }
    }

    /**
     * Call this method repeatedly to process incoming data.
     */
    void process();

    /**
     * Get address of remote node. This method will only return valid data
     * when called in the context of the CbDataAvailable and CbNodeFound
     * callbacks
     *
     * @param addrHi Top 32 bits of address will be written to this argument
     * @param addrLo Bottom 32 bits of address will be written to this argument
     */
    XBeeError getRemoteAddress(uint32_t* addrHi, uint32_t* addrLo);

    /**
     * Get signal strength indicator value (RSSI). This method will only
     * return valid data when called in the context of the
     * CbDataAvailable and CbNodeFound callbacks
     *
     * @param rssi RSSI value will be written to this argument
     */
    XBeeError getRssi(uint8_t* rssi );

    /**
     * Get the transmit status. This method will only return valid data when
     * called in the context of the CbTxStat callback.
     *
     * @param frameId the frame ID will be written to this argument
     * @param status the status will be written to this argument
     */
    XBeeError getTxStatus(uint8_t* frameId, XBeeTxStatus* status);

    /**
     * Get received data. This method will only return valid data when called
     * in the context of the CbDataAvailable callback
     *
     * @param data will point to a buffer with received data
     * @param len will contain the length of the received data
     */
    XBeeError getData(char** data, uint8_t* len);

    /**
     * Send data to a node with specified address. It is also possible to
     * broadcast a message using broadcast address (XBEE_ADDRHI_BROADCAST,
     * XBEE_ADDRLO_BROADCAST).
     *
     * @param addrHi Top 32 bits of address
     * @param addrLo Bottom 32 bits of address
     * @param data buffer containing data to send
     * @param len number of bytes to send
     * @param frameId the ID of the frame will be written to this argument.
     * The ID can then be used to match this request with the status returned
     * in the CbTxStat callback.
     */
    XBeeError send(uint32_t addrHi, uint32_t addrLo, char* data,
        uint8_t len, uint8_t* frameId);

    /**
     * Send a Node Discover request. All modules on the operating channel and
     * PAN ID should respond. The responses will be reported in the CbNodeFound
     * callback.
     */
    XBeeError discoverNodes();

    /**
     * Request the module to enter sleep mode.
     */
    XBeeError enterSleep();

    /**
     * Request the module to exit sleep mode.
     */
    XBeeError exitSleep();


protected:


private:

    enum RfState {
        RfStateFrame = 0,
        RfStateLength,
        RfStateData
    };


    bool _initialized;
    Serial _serial;
    XBeeType _type;
    DigitalOut _reset;
    DigitalOut _sleep;

    uint8_t rxqIn;
    uint8_t rxqOut;
    uint8_t rxq[RX_BUF_SIZE];

    uint32_t _rfFrameTimeout;
    Timer _rfFrameTimer;

    RfState _rfState;
    uint32_t _rfPos;
    uint32_t _rfFrameLen;
    char _rfBuf[XBEE_BUF_SZ];
    uint8_t _rfFrameId;

    uint32_t _addrHi;
    uint32_t _addrLo;
    uint8_t _rssi;
    uint8_t _frameId;
    XBeeTxStatus _txStatus;
    char* _recvData;
    uint8_t _recvLen;


    FunctionPointer _callbacks[CbNum];


    void uartRxIrq();
    void uartRxQPut(uint8_t data);
    uint8_t uartRxQGet();
    bool uartRxQIsEmpty();
    uint32_t uartReceive(char *buf, uint32_t buflen);
    int32_t uartReadLine(char* buf, uint32_t bufLen, uint32_t timeout);

    void resetModule();
    XBeeError commandMode();
    XBeeError atGet(const char* atCmd, char* resp, uint32_t respLen);
    XBeeError atSet(const char* atCmd);

    void processByte(char data);
    void processFrame(char* buf, uint32_t len);

    void handleAtResponse(uint8_t frameId, char* atBuf, uint8_t status,
        char* valueBuf, uint32_t valueLen);

    void handleDiscovery(uint8_t status, char* buf, uint32_t len);
    void handleTxStatus(uint8_t frameId, uint8_t status);
    void processData(uint32_t addrHi, uint32_t addrLo, uint8_t rssi,
        uint8_t opt, char* buf, uint32_t len);
    void handleModemStatus(uint8_t status);

    char checksum(char* buf, uint32_t len);
    uint32_t bufTo32bitInt(const char* buf);
    void int32bitToBuf(uint32_t v, char* buf);

    XBeeError apiTx64(uint32_t addrHi, uint32_t addrLo, char* data,
        uint32_t len, uint8_t* frameId);
    XBeeError apiAtCmd(const char* atCmd, uint32_t param, bool useParameter);

    uint8_t getFrameId();


};

#endif
