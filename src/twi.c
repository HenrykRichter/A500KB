#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/twi.h>
#include <string.h>

#include "twi.h"

static volatile uint8_t busy;
static struct {
  uint8_t buffer[TWI_BUFFER_LENGTH];
  uint8_t length;
  uint8_t index;
  void (*callback)(uint8_t, uint8_t *);
} transmission;

void twi_init() {
  TWBR = ((F_CPU / TWI_FREQ) - 16) / 2;
  TWSR = 0; // prescaler = 1

  busy = 0;

  sei();

  TWCR = _BV(TWEN);
}

uint8_t *twi_wait() {
  while (busy);
  return &transmission.buffer[1];
}

uint8_t twi_isbusy()
{
 return busy;
}

void twi_start(void) {
  TWCR = _BV(TWINT) | _BV(TWEN) | _BV(TWIE) | _BV(TWSTA);
}

void twi_stop(void) {
  TWCR = _BV(TWINT) | _BV(TWEN) | _BV(TWSTO);
}

void twi_ack() {
  TWCR = _BV(TWINT) | _BV(TWEN) | _BV(TWIE) | _BV(TWEA);
}

void twi_nack() {
  TWCR = _BV(TWINT) | _BV(TWEN) | _BV(TWIE);
}

void twi_send(uint8_t data) {
  TWDR = data;
}

void twi_recv() {
  transmission.buffer[transmission.index++] = TWDR;
}

void twi_reply() {
  if (transmission.index < (transmission.length - 1)) {
    twi_ack();
  } else {
    twi_nack();
  }
}

void twi_done() {
  uint8_t address = transmission.buffer[0] >> 1;
  uint8_t *data = &transmission.buffer[1];

  busy = 0;

  if (transmission.callback != NULL) {
    transmission.callback(address, data);
  }
}

void twi_write(uint8_t address, uint8_t* data, uint8_t length, void (*callback)(uint8_t, uint8_t *)) {
  twi_wait();

  busy = 1;

  transmission.buffer[0] = (address << 1) | TW_WRITE;
  transmission.length = length + 1;
  transmission.index = 0;
  transmission.callback = callback;
  memcpy(&transmission.buffer[1], data, length);

  twi_start();
}

void twi_read(uint8_t address, uint8_t length, void (*callback)(uint8_t, uint8_t *)) {
  twi_wait();

  busy = 1;

  transmission.buffer[0] = (address << 1) | TW_READ;
  transmission.length = length + 1;
  transmission.index = 0;
  transmission.callback = callback;

  twi_start();
}

ISR(TWI_vect) {
  switch (TW_STATUS) {
  case TW_START:
  case TW_REP_START:
  case TW_MT_SLA_ACK:
  case TW_MT_DATA_ACK:
    if (transmission.index < transmission.length) {
      twi_send(transmission.buffer[transmission.index++]);
      twi_nack();
    } else {
      twi_stop();
      twi_done();
    }
    break;

  case TW_MR_DATA_ACK:
    twi_recv();
    twi_reply();
    break;

  case TW_MR_SLA_ACK:
    twi_reply();
    break;

  case TW_MR_DATA_NACK:
    twi_recv();
    twi_stop();
    twi_done();
    break;

  case TW_MT_SLA_NACK:
  case TW_MR_SLA_NACK:
  case TW_MT_DATA_NACK:
  default:
    twi_stop();
    twi_done();
    break;
  }
}
