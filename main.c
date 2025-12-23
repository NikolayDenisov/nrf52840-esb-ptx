#include "app_util.h"
#include "boards.h"
#include "nrf.h"
#include "nrf_delay.h"
#include "nrf_error.h"
#include "nrf_esb.h"
#include "nrf_esb_error_codes.h"
#include "nrf_gpio.h"
#include "sdk_common.h"
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "nrf_log.h"
#include "nrf_log_ctrl.h"
#include "nrf_log_default_backends.h"

static nrf_esb_payload_t tx_payload =
    NRF_ESB_CREATE_PAYLOAD(0, 0x01, 0x00, 0x00, 0x00, 0x11, 0x00, 0x00, 0x00);

static nrf_esb_payload_t rx_payload;

#define NRF_LOG_ENABLED 1

void nrf_esb_event_handler(nrf_esb_evt_t const *p_event) {
  switch (p_event->evt_id) {
  case NRF_ESB_EVENT_TX_SUCCESS:
    NRF_LOG_DEBUG("TX SUCCESS EVENT");
    break;
  case NRF_ESB_EVENT_TX_FAILED:
    NRF_LOG_DEBUG("TX FAILED EVENT");
    (void)nrf_esb_flush_tx();
    (void)nrf_esb_start_tx();
    break;
  case NRF_ESB_EVENT_RX_RECEIVED:
    NRF_LOG_DEBUG("RX RECEIVED EVENT");
    while (nrf_esb_read_rx_payload(&rx_payload) == NRF_SUCCESS) {
      if (rx_payload.length > 0) {
        NRF_LOG_DEBUG("RX RECEIVED PAYLOAD");
      }
    }
    break;
  }
}

void clocks_start(void) {
  NRF_CLOCK->EVENTS_HFCLKSTARTED = 0;
  NRF_CLOCK->TASKS_HFCLKSTART = 1;

  while (NRF_CLOCK->EVENTS_HFCLKSTARTED == 0)
    ;
}

void gpio_init(void) {
  nrf_gpio_range_cfg_output(8, 15);
  bsp_board_init(BSP_INIT_LEDS);
}

uint32_t esb_init(void) {
  uint32_t err_code;
  uint8_t base_addr_0[4] = {0xE7, 0xE7, 0xE7, 0xE7};
  uint8_t base_addr_1[4] = {0xC2, 0xC2, 0xC2, 0xC2};
  uint8_t addr_prefix[8] = {0xE7, 0xC2, 0xC3, 0xC4, 0xC5, 0xC6, 0xC7, 0xC8};

  nrf_esb_config_t nrf_esb_config = NRF_ESB_DEFAULT_CONFIG;
  nrf_esb_config.protocol = NRF_ESB_PROTOCOL_ESB_DPL;
  nrf_esb_config.retransmit_delay = 600;
  nrf_esb_config.bitrate = NRF_ESB_BITRATE_2MBPS;
  nrf_esb_config.event_handler = nrf_esb_event_handler;
  nrf_esb_config.mode = NRF_ESB_MODE_PTX;
  nrf_esb_config.selective_auto_ack = false;

  err_code = nrf_esb_init(&nrf_esb_config);

  VERIFY_SUCCESS(err_code);

  err_code = nrf_esb_set_base_address_0(base_addr_0);
  VERIFY_SUCCESS(err_code);

  err_code = nrf_esb_set_base_address_1(base_addr_1);
  VERIFY_SUCCESS(err_code);

  err_code = nrf_esb_set_prefixes(addr_prefix, NRF_ESB_PIPE_COUNT);
  VERIFY_SUCCESS(err_code);

  return err_code;
}

int main(void) {
  ret_code_t err_code;

  gpio_init();

  err_code = NRF_LOG_INIT(NULL);
  APP_ERROR_CHECK(err_code);

  NRF_LOG_DEFAULT_BACKENDS_INIT();

  clocks_start();

  err_code = esb_init();
  APP_ERROR_CHECK(err_code);

  NRF_LOG_DEBUG("Enhanced ShockBurst Transmitter Example started.");

  while (true) {
    NRF_LOG_DEBUG("Transmitting packet %02x", tx_payload.data[1]);

    tx_payload.noack = false;
    if (nrf_esb_write_payload(&tx_payload) == NRF_SUCCESS) {
      // Toggle one of the LEDs.
      nrf_gpio_pin_write(
          LED_1, !(tx_payload.data[1] % 8 > 0 && tx_payload.data[1] % 8 <= 4));
      nrf_gpio_pin_write(
          LED_2, !(tx_payload.data[1] % 8 > 1 && tx_payload.data[1] % 8 <= 5));
      nrf_gpio_pin_write(
          LED_3, !(tx_payload.data[1] % 8 > 2 && tx_payload.data[1] % 8 <= 6));
      // nrf_gpio_pin_write(LED_4, !(tx_payload.data[1]%8>3));
      tx_payload.data[1]++;
    } else {
      NRF_LOG_WARNING("Sending packet failed");
    }

    nrf_delay_ms(5000);
  }
}
