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

#include "mbed.h"
#include "mbed_debug.h"
#include "XBee.h"

#define XBEE_END_CMD ("\r")
#define CR (0x0D)
#define RESP_OK ("OK")

#define XBEE_START_DEL (0x7E)

#define XBEE_API_ID_TX_64    (0x00)
#define XBEE_API_ID_AT_CMD   (0x08)
#define XBEE_API_ID_AT_RESP  (0x88)
#define XBEE_API_ID_TX_STAT  (0x89)
#define XBEE_API_ID_RX_64    (0x80)
#define XBEE_API_ID_RX_16    (0x81)
#define XBEE_API_ID_MOD_STAT (0x8A)

#define XBEE_MOD_STAT_HW_RESET    (0)
#define XBEE_MOD_STAT_WD_RESET    (1)
#define XBEE_MOD_STAT_ASSOC       (2)
#define XBEE_MOD_STAT_DIASSOC     (3)
#define XBEE_MOD_STAT_SYNC_LOST   (4)
#define XBEE_MOD_STAT_COORD_REALG (5)
#define XBEE_MOD_STAT_COORD_START (6)

#define XBEE_TX_STAT_SUCCESS  (0)
#define XBEE_TX_STAT_NOACK    (1)
#define XBEE_TX_STAT_CCA_FAIL (2)
#define XBEE_TX_STAT_PURGED   (3)

#define XBEE_RECV_FRAME_TO (2000)

XBee::XBee(PinName tx, PinName rx, PinName reset, PinName sleep) :
    _serial(tx, rx), _reset(reset), _sleep(sleep) {
    _serial.baud(9600);
    _serial.format(8, Serial::None, 1);

    _serial.attach(this, &XBee::uartRxIrq, Serial::RxIrq);

    rxqIn = 0;
    rxqOut = 0;
    _rfFrameTimeout = 0;
    _rfFrameTimer.reset();

    _rfState = RfStateFrame;
    _rfPos = 0;
    _rfFrameLen = 0;
    _rfFrameId = 0;

    _initialized = false;
    _type = Coordinator;

    _addrHi = 0;
    _addrLo = 0;
    _rssi = 0;
    _frameId = 0;
    _txStatus = TxStatusOk;
    _recvData = NULL;
    _recvLen = 0;

    // we enter sleep when sleep pin is high
    _sleep = 0;
}

XBee::XBeeError XBee::init(XBeeType type, const char* panId) {
    XBeeError err = Ok;
    char idBuf[10];

    resetModule();

   _type = type;

    do {

        if (panId == NULL || strlen(panId) != 4) {
            err = ArgumentError;
            break;
        }

        err = commandMode();
        if (err != Ok) {
            debug("XBee: +++ failed\n");
            break;
        }

        // set the sleep mode to Hibernate
        err = atSet("ATSM1");
        if (err != Ok) {
            debug("Xbee: ATSM1 failed\r\n");
            break;
        }

        // change PAN ID to EAEA
        sprintf(idBuf, "ATID%s", panId);
        err = atSet(idBuf);
        if (err != Ok) {
            debug("Xbee: ATID failed\r\n");
            break;
        }

        // use 64-bit addressing
        err = atSet("ATMYFFFE");
        if (err != Ok) {
            debug("Xbee: ATMYFFFE failed\r\n");
            break;
        }

        // Coordinator
        if (type == Coordinator) {

            // - will not perform Active Scan to locate PAN ID
            // - will not perform Energy Scan to determine free channel
            // - allow End devices to associate with this coordinator
            err = atSet("ATA24");
            if (err != Ok) {
                debug("Xbee: ATA24 failed\r\n");
                break;
            }

            // set this node as coordinator
            err = atSet("ATCE1");
            if (err != Ok) {
                debug("Xbee: ATCE1 failed\r\n");
                break;
            }
        }
        else {
            // - only associates with Coordinator on matching PAN ID
            // - only associates with Coordinator on matching channel
            // - device attempts association until success
            err = atSet("ATA14");
            if (err != Ok) {
                debug("Xbee: ATA14 failed\r\n");
                break;
            }
        }


        // change to API mode
        err = atSet("ATAP1");
        if (err != Ok) {
            debug("Xbee: ATAP1 failed\r\n");
            break;
        }

    } while(0);


    return err;
}

void XBee::process() {
    uint32_t len = 0;
    char data = 0;

    if (_rfFrameTimeout > 0 && (int)_rfFrameTimeout < _rfFrameTimer.read_ms()) {
        _rfState = RfStateFrame;
        debug("Xbee: Frame timer expired\r\n");
        _rfFrameTimeout = 0;
        _rfFrameTimer.stop();
    }

    if (!uartRxQIsEmpty()) {

        len = uartReceive(&data, 1);
        if (len > 0) {
            processByte(data);
        }

    }
}

XBee::XBeeError XBee::getRemoteAddress(uint32_t* addrHi, uint32_t* addrLo) {
    if (!_initialized) {
        return NotInitializedError;
    }

    if (addrHi == NULL || addrLo == NULL) {
        return ArgumentError;
    }

    *addrHi = _addrHi;
    *addrLo = _addrLo;

    return Ok;
}

XBee::XBeeError XBee::getRssi(uint8_t* rssi ) {
    if (!_initialized) {
        return NotInitializedError;
    }

    if (rssi == NULL) {
        return ArgumentError;
    }

    *rssi = _rssi;

    return Ok;
}

XBee::XBeeError XBee::getTxStatus(uint8_t* frameId, XBeeTxStatus* status) {
    if (!_initialized) {
        return NotInitializedError;
    }

    if (frameId == NULL || status == NULL) {
        return ArgumentError;
    }

    *frameId = _frameId;
    *status = _txStatus;

    return Ok;
}

XBee::XBeeError XBee::getData(char** data, uint8_t* len) {
    if (!_initialized) {
        return NotInitializedError;
    }

    if (data == NULL || len == NULL) {
        return ArgumentError;
    }

    *data = _recvData;
    *len = _recvLen;

    return Ok;
}

XBee::XBeeError XBee::send(uint32_t addrHi, uint32_t addrLo, char* data,
    uint8_t len, uint8_t* frameId)
{
  if (!_initialized) {
    return NotInitializedError;
  }

  return apiTx64(addrHi, addrLo, data, len, frameId);
}

XBee::XBeeError XBee::discoverNodes() {
    if (!_initialized) {
      return NotInitializedError;
    }

    return apiAtCmd("ND", 0, false);
}


XBee::XBeeError XBee::enterSleep() {
    _sleep = 1;

    return Ok;
}

XBee::XBeeError XBee::exitSleep() {
    _sleep = 0;

    return Ok;
}


void XBee::uartRxIrq() {
    while(_serial.readable()) {
        uartRxQPut(_serial.getc());
    }
}

void XBee::uartRxQPut(uint8_t data)
{
  // full
  if (rxqOut == (rxqIn + 1) % RX_BUF_SIZE) {
    return;
  }

  rxq[rxqIn] = data;
  rxqIn = (rxqIn + 1) % RX_BUF_SIZE;
}

uint8_t XBee::uartRxQGet()
{
  uint8_t d = 0;
  // empty
  if (rxqIn == rxqOut) {
    return 0;
  }

  d = rxq[rxqOut];
  rxqOut = (rxqOut + 1) % RX_BUF_SIZE;

  return d;
}

bool XBee::uartRxQIsEmpty()
{
  return (rxqIn == rxqOut);
}

uint32_t XBee::uartReceive(char *buf, uint32_t buflen)
{

  uint32_t pos = 0;

  while(buflen > 0 && !uartRxQIsEmpty()) {
    buf[pos] = uartRxQGet();
    pos++;
    buflen--;
  }

  return pos;
}

int32_t XBee::uartReadLine(char* buf, uint32_t bufLen, uint32_t timeout)
{
  uint32_t pos = 0;
  uint32_t len = 0;
  Timer tim;

  tim.reset();
  tim.start();

  while(pos < bufLen && tim.read_ms() < (int)timeout) {

    len = uartReceive(&buf[pos], 1);
    if (len > 0 && buf[pos] == CR) {
      buf[pos] = '\0';
      break;
    }

    pos += len;
  }

  if (pos >= bufLen) {
    return BufTooSmallError;
  }

  if (tim.read_ms() > (int)timeout) {
    return TimeOutError;
  }

  return pos;
}

void XBee::resetModule() {
    // reset pulse must be at least 200 ns. Using 1 ms.
    _reset = 0;
    wait_ms(1);
    _reset = 1;

    // wait to make sure the module has started
    wait_ms(500);
    rxqIn = 0;
    rxqOut = 0;
}

XBee::XBeeError XBee::commandMode() {
    XBeeError err = Ok;
    int32_t lineLen = 0;
    char ebuf[10];

    _serial.printf("+++");

    lineLen = uartReadLine(ebuf, 10, 1200);

    do {
      if (lineLen < 0) {
        // error while reading
        err = ReadError;
        break;
      }

      if (strcmp(RESP_OK, (char*)ebuf) != 0) {
        // didn't receive OK
        err = CmdError;
        break;
      }
    } while(0);

    return err;
}

XBee::XBeeError XBee::atGet(const char* atCmd, char* resp, uint32_t respLen)
{

    int32_t lineLen = 0;
    XBeeError err = Ok;

    _serial.printf("%s%s", atCmd, XBEE_END_CMD);

    do {

      // a response is expected
      if (resp != NULL && respLen > 0) {
        lineLen = uartReadLine(resp, respLen, 1000);

        if (lineLen < 0) {
          // error while reading
          err = ReadError;
          break;
        }

      }

    } while(0);

    return err;
}

XBee::XBeeError XBee::atSet(const char* atCmd)
{
  char b[10];
  XBeeError err = Ok;

  err = atGet(atCmd, b, 10);
  if (err == Ok) {

    if (strcmp(RESP_OK, (char*)b) != 0) {
      // didn't receive OK
      err = CmdError;
    }
  }

  return err;
}

void XBee::processByte(char data)
{
    switch(_rfState) {
    case RfStateFrame:
        if (data == XBEE_START_DEL) {
            _rfPos = 0;
            _rfFrameLen = 0;
            _rfState = RfStateLength;

            // start timer to make sure an entire frame is received
            // within a specific time
            _rfFrameTimeout = XBEE_RECV_FRAME_TO;
            _rfFrameTimer.reset();
            _rfFrameTimer.start();
        }

        break;
    case RfStateLength:
        _rfFrameLen |= (data << (8*(1-_rfPos)));
        _rfPos++;
        if (_rfPos == 2) {
            _rfPos = 0;
            _rfState = RfStateData;

            if (_rfFrameLen > XBEE_BUF_SZ) {
                debug("Xbee: Frame len %d > max buffer len %d\r\n",
                        (int)_rfFrameLen, (int)XBEE_BUF_SZ);
                _rfFrameLen = XBEE_BUF_SZ;
            }

        }
        break;
    case RfStateData:
        _rfBuf[_rfPos++] = data;
        // read up until checksum (1 byte)
        if (_rfPos == _rfFrameLen+1) {
            _rfState = RfStateFrame;

            // cancel timer
            _rfFrameTimeout = 0;
            _rfFrameTimer.stop();

            processFrame(_rfBuf, _rfPos);
        }
        break;

    }
}

void XBee::processFrame(char* buf, uint32_t len)
{

    uint32_t addrLo = 0;
    uint32_t addrHi = 0;
    char* b = NULL;
    uint32_t bLen = 0;

    if (len < 2) {
        debug("Xbee: Invalid frame length (%d)\r\n", (int)len);
        return;
    }

    // verify checksum
    if (checksum(buf, len) != 0) {
        debug("Xbee: Invalid checksum\r\n");
        return;
    }

    switch(buf[0]) {
    case XBEE_API_ID_AT_RESP:
        if (len < 5) {
            debug("Xbee: AT resp data too small: %d\r\n ", (int)len);
            return;
        }

        // there is a value
        if (len > 6) {
            b = &buf[5];
            bLen = len-5-1;
        }

        handleAtResponse(buf[1], &buf[2], buf[4], b, bLen);
        break;
    case XBEE_API_ID_TX_STAT:
        handleTxStatus(buf[1], buf[2]);
        break;
    case XBEE_API_ID_RX_64:
        if (len < 12) {
            debug("Xbee: RX data too small: %d\r\n ", (int)len);
            return;
        }
        addrHi = bufTo32bitInt(&buf[1]);
        addrLo = bufTo32bitInt(&buf[5]);

        processData(addrHi, addrLo, buf[9], buf[10], &buf[11], len-11-1);
        break;
    case XBEE_API_ID_RX_16:
        debug("Xbee: RX 16 bit (unhandled)\r\n");
        break;
    case XBEE_API_ID_MOD_STAT:
        handleModemStatus(buf[1]);
        break;
    default:
        debug("Xbee: Unhandled API ID: %x\r\n ", buf[0]);

        break;
    }

}

void XBee::handleAtResponse(uint8_t frameId, char* atBuf, uint8_t status,
    char* valueBuf, uint32_t valueLen)
{

  if (strncmp("ND", (char*)atBuf, 2) == 0) {
    handleDiscovery(status, valueBuf, valueLen);
  }

}

void XBee::handleDiscovery(uint8_t status, char* buf, uint32_t len)
{

  if (status == 0 && len >= 11) {
    _addrHi = bufTo32bitInt(&buf[2]);
    _addrLo = bufTo32bitInt(&buf[6]);
    _rssi = buf[10];

    _callbacks[CbNodeFound].call();

  }
}

void XBee::handleTxStatus(uint8_t frameId, uint8_t status)
{
    _frameId = frameId;
    switch(status)  {
    case XBEE_TX_STAT_SUCCESS:
      _txStatus = TxStatusOk;
      break;
    case XBEE_TX_STAT_NOACK:
      _txStatus = TxStatusNoAck;
      break;
    case XBEE_TX_STAT_CCA_FAIL:
      _txStatus = TxStatusCCA;
      break;
    case XBEE_TX_STAT_PURGED:
      _txStatus = TxStatusPurged;
      break;
    }

    _callbacks[CbTxStat].call();

}

void XBee::processData(uint32_t addrHi, uint32_t addrLo, uint8_t rssi,
    uint8_t opt, char* buf, uint32_t len)
{
    _addrHi = addrHi;
    _addrLo = addrLo;
    _rssi = rssi;
    _recvData = buf;
    _recvLen = len;

    _callbacks[CbDataAvailable].call();
}

void XBee::handleModemStatus(uint8_t status) {

    if (_type == Coordinator && status == XBEE_MOD_STAT_COORD_START) {
        _initialized = true;
        _callbacks[CbDeviceUp].call();
    }
    else if (_type == EndDevice && status == XBEE_MOD_STAT_ASSOC) {
        _initialized = 1;
        _callbacks[CbDeviceUp].call();
    }
    else if (_type == EndDevice && status == XBEE_MOD_STAT_DIASSOC) {
        _initialized = false;
        _callbacks[CbDeviceDown].call();
    }

}

char XBee::checksum(char* buf, uint32_t len)
{
    int i = 0;
    char cs = 0;

    for (i = 0; i < (int)len; i++) {
        cs += buf[i];
    }

    return (0xFF - cs);
}

uint32_t XBee::bufTo32bitInt(const char* buf)
{
    uint32_t v = 0;

    v |= (buf[0] << 24);
    v |= (buf[1] << 16);
    v |= (buf[2] << 8);
    v |= (buf[3]);

    return v;
}

void XBee::int32bitToBuf(uint32_t v, char* buf)
{

  buf[0] = ((v >> 24) & 0xff);
  buf[1] = ((v >> 16) & 0xff);
  buf[2] = ((v >> 8) & 0xff);
  buf[3] = ((v >> 0) & 0xff);

}


XBee::XBeeError XBee::apiTx64(uint32_t addrHi, uint32_t addrLo, char* data,
    uint32_t len, uint8_t* frameId)
{
    char buf[100];


    // limiting to 85 bytes data. Remaining 15 bytes belong to the
    // frame
    if (len > 85) {
        return ArgumentError;
    }

    buf[0] = XBEE_START_DEL;

    // length
    buf[1] = 0;
    buf[2] = 11+len;

    // AP ID
    buf[3] = XBEE_API_ID_TX_64;

    // frame ID
    buf[4] = getFrameId();


    // address
    int32bitToBuf(addrHi, &buf[5]);
    int32bitToBuf(addrLo, &buf[9]);

    // options
    buf[13] = 0;

    // data
    memcpy(&buf[14], data, len);

    // checksum
    buf[14+len] = checksum(&buf[3], buf[2]);

    if (frameId != NULL) {
        *frameId = buf[4];
    }


    for (int i = 0; i < (15+len); i++) {
        _serial.putc(buf[i]);
    }


    return Ok;
}

XBee::XBeeError XBee::apiAtCmd(const char* atCmd, uint32_t param, bool useParameter)
{
    char buf[12];
    int pos = 0;

    buf[0] = XBEE_START_DEL;

    // length
    buf[1] = 0;
    buf[2] = 4;
    if (useParameter) {
      buf[2] += 4;
    }

    // AP ID
    buf[3] = XBEE_API_ID_AT_CMD;

    // frame ID
    buf[4] = getFrameId();

    // AT cmd
    buf[5] = atCmd[0];
    buf[6] = atCmd[1];
    pos = 7;

    // AT parameter
    if (useParameter) {
      buf[pos++] = ((param >> 24) & 0xff);
      buf[pos++] = ((param >> 16) & 0xff);
      buf[pos++] = ((param >> 8) & 0xff);
      buf[pos++] = ((param >> 0) & 0xff);
    }

    // checksum
    buf[pos] = checksum(&buf[3], pos-3);
    pos++;

    for (int i = 0; i < pos; i++) {
        _serial.putc(buf[i]);
    }

    return Ok;
}

uint8_t XBee::getFrameId(void)
{
    _rfFrameId++;
    if (_rfFrameId == 0) {
        _rfFrameId = 1;
    }

    return _rfFrameId;
}
