#pragma once

#include <stdint.h>
#include <stdbool.h>

//------------------------------------------------------------------------------

void server_init(void);
void server_clear(void);
bool server_update(bool connected, bool byte_ie, uint8_t byte_in, uint8_t* byte_out);

void server_handle_questionmark(void);
void server_handle_bang(void);
void server_handle_ctrlc(void);

void server_handle_c(void);
void server_handle_D(void);
void server_handle_g(void);
void server_handle_G(void);
void server_handle_H(void);
void server_handle_k(void);
void server_handle_m(void);
void server_handle_M(void);
void server_handle_p(void);
void server_handle_P(void);
void server_handle_q(void);
void server_handle_Q(void);
void server_handle_R(void);
void server_handle_s(void);
void server_handle_v(void);
void server_handle_z0(void);
void server_handle_Z0(void);
void server_handle_z1(void);
void server_handle_Z1(void);

void server_handle_packet(void);
void server_on_hit_breakpoint(void);

void server_flash_erase(uint32_t addr, uint32_t size);
void server_put_cache(uint32_t addr, uint8_t data);
void server_flush_cache(void);

//------------------------------------------------------------------------------
