#include "fmplayer_backend.h"
#include "fmplayer_drumrom.h"
#include "fmdriver/fmdriver.h"
#include "fmdriver/ppz8.h"

#include "ymfm_opn.h"

#include <new>

namespace {

using namespace ymfm;

struct ymfm_backend;

// The chip is driven at its native rate for the real OPNA master clock with the
// lowest fidelity (output = clock / 48).
enum {
  CHIP_CLOCK = 7987200,
  OUTPUT_RATE = 166400,
  CLOCKS_PER_SAMPLE = CHIP_CLOCK / OUTPUT_RATE,
  ADPCM_RAM_MASK = FMPLAYER_ADPCM_RAM_SIZE - 1,
  // ymfm leaves each engine at its own internal full scale and the relative
  // balance to the caller. Fed the same registers, libopna's SSG is 0.187x as
  // loud as ymfm's (measured YMF288 volume table plus its analog output chain);
  // apply that here so the two backends agree. FM and drum/ADPCM already match.
  // SSG_GAIN_FP16 = 12288,
  // TODO: I've always thought 98fmplayer's SSG was kind of quiet, so I've adjusted this based on at least one source that says 0.25 is a good ratio:
  // https://blog.hiroaki.jp/blog/2021/07/11/s98player-using-ymfm/
  SSG_GAIN_FP16 = 16384,
};

// The single instance lives in the caller-provided storage buffer; the driver
// trampoline callbacks reach it via fmdriver_work.opna.
struct ymfm_backend : ymfm_interface {
  ymfm_backend() : chip(*this) { }

  void init(struct ppz8 *ppz8, struct fmdriver_work *work, void *adpcm_ram,
            const uint8_t *drum_rom);
  void mix(int16_t *buf, unsigned samples);

  // ymfm_interface
  void ymfm_set_timer(uint32_t tnum, int32_t duration_in_clocks) override;
  uint8_t ymfm_external_read(access_class type, uint32_t address) override;
  void ymfm_external_write(access_class type, uint32_t address, uint8_t data) override;
  void ymfm_update_irq(bool asserted) override;

  void fire_timer(uint32_t tnum);

  void writereg(unsigned reg, unsigned val);
  unsigned readreg(unsigned reg);
  uint8_t status();

  ym2608 chip;
  struct fmdriver_work *work = nullptr;
  struct ppz8 *ppz8 = nullptr;
  void *adpcm_ram = nullptr;
  const uint8_t *drum_rom = nullptr;

  int64_t clocks_now = 0;
  // Scheduled absolute chip-clock time each timer fires at, or -1 if inactive.
  int64_t timer_clocks[2] = {-1, -1};

  // Output cursor/end valid only while mix() runs, so a key-on-register write can
  // advance the engine one sample to observe its own transition (see writereg).
  int16_t *out_cur = nullptr;
  int16_t *out_end = nullptr;

  void emit_one();

  ym2608::output_data out;
};

static_assert(sizeof(ymfm_backend) <= FMPLAYER_BACKEND_STORAGE_SIZE, "backend storage too small");

void ymfm_backend::init(struct ppz8 *ppz8_, struct fmdriver_work *work_,
                        void *adpcm_ram_, const uint8_t *drum_rom_) {
  ppz8 = ppz8_;
  work = work_;
  adpcm_ram = adpcm_ram_;
  drum_rom = drum_rom_;

  chip.set_fidelity(OPN_FIDELITY_MIN);
  chip.reset();

  ppz8_init(ppz8, OUTPUT_RATE, 0xa000);
  ppz8_set_interpolation(ppz8, PPZ8_INTERP_SINC);

  memset(this->work, 0, sizeof(*this->work));
  this->work->opna_writereg = [](struct fmdriver_work *w, unsigned addr, unsigned data) {
    ((ymfm_backend *)w->opna)->writereg(addr, data);
  };
  this->work->opna_readreg = [](struct fmdriver_work *w, unsigned addr) -> unsigned {
    return ((ymfm_backend *)w->opna)->readreg(addr);
  };
  this->work->opna_status = [](struct fmdriver_work *w, bool a1) -> uint8_t {
    (void)a1;
    return ((ymfm_backend *)w->opna)->status();
  };
  this->work->opna = this;
  this->work->ppz8 = ppz8;
  this->work->ppz8_functbl = &ppz8_functbl;
}

void ymfm_backend::writereg(unsigned reg, unsigned val) {
  val &= 0xff;
  if (reg < 0x100) {
    chip.write_address((uint8_t)reg);
    chip.write_data((uint8_t)val);
  } else {
    chip.write_address_hi((uint8_t)reg);
    chip.write_data_hi((uint8_t)val);
  }
  // We need to wait before performing any other commands.
  // The original PMD driver on actual PC-98 hardware handles this,
  // but not this version: the libopna implementation doesn't care
  // about the delay, but ymfm at least requires something.
  emit_one();
}

unsigned ymfm_backend::readreg(unsigned reg) {
  if (reg > 0xf) return 0xff;
  chip.write_address((uint8_t)reg);
  return chip.read_data();
}

uint8_t ymfm_backend::status() {
  // Report only the timer bits (via the chip status), matching the libopna
  // backend; the driver applies its own masking.
  return chip.read_status();
}

void ymfm_backend::fire_timer(uint32_t tnum) {
  m_engine->engine_timer_expired(tnum);
}

static int clamp16(int x) {
  if (x > 32767) return 32767;
  if (x < -32768) return -32768;
  return x;
}

void ymfm_backend::emit_one() {
  if (out_cur >= out_end)
    return;
  chip.generate(&out, 1);
  int32_t ssg = (out.data[2] * SSG_GAIN_FP16) >> 16;
  out_cur[0] = clamp16(out.data[0] + ssg);
  out_cur[1] = clamp16(out.data[1] + ssg);
  out_cur += 2;
}

void ymfm_backend::mix(int16_t *buf, unsigned samples) {
  int16_t *start = buf;
  out_cur = start;
  out_end = start + samples * 2;
  while (out_cur < out_end) {
    // Generate one output sample at the chip's native rate, then fire any timer
    // scheduled to have expired by this point (its callback may issue key-on writes
    // that advance the engine themselves, so the loop is cursor-driven).
    emit_one();
    clocks_now += CLOCKS_PER_SAMPLE;
    for (;;) {
      int best = -1;
      for (int t = 0; t < 2; t++)
        if (timer_clocks[t] >= 0 && timer_clocks[t] <= clocks_now &&
            (best < 0 || timer_clocks[t] < timer_clocks[best]))
          best = t;
      if (best < 0) break;
      fire_timer(best);
    }
  }
  unsigned n = (unsigned)(out_end - start) / 2;
  out_cur = out_end = nullptr;
  // ppz8 runs at the backend output rate and mixes into the same buffer.
  ppz8_mix(ppz8, start, n);
}

void ymfm_backend::ymfm_set_timer(uint32_t tnum, int32_t duration_in_clocks) {
  if (duration_in_clocks < 0) {
    if (tnum < 2) timer_clocks[tnum] = -1;
    return;
  }
  if (tnum < 2) timer_clocks[tnum] = clocks_now + duration_in_clocks;
}

uint8_t ymfm_backend::ymfm_external_read(access_class type, uint32_t address) {
  if (type == ACCESS_ADPCM_A && drum_rom)
    return drum_rom[address & (FMPLAYER_DRUM_ROM_SIZE - 1)];
  if (type == ACCESS_ADPCM_B && adpcm_ram)
    return ((uint8_t *)adpcm_ram)[address & ADPCM_RAM_MASK];
  return 0;
}

void ymfm_backend::ymfm_external_write(access_class type, uint32_t address, uint8_t data) {
  if (type == ACCESS_ADPCM_B && adpcm_ram)
    ((uint8_t *)adpcm_ram)[address & ADPCM_RAM_MASK] = data;
}

void ymfm_backend::ymfm_update_irq(bool asserted) {
  if (asserted && work->driver_opna_interrupt)
    work->driver_opna_interrupt(work);
}

} // namespace

extern "C" {

static void ymfm_init(void *storage, struct ppz8 *ppz8,
                      struct fmdriver_work *work, void *adpcm_ram) {
  ymfm_backend *impl = new (storage) ymfm_backend();
  impl->init(ppz8, work, adpcm_ram, fmplayer_drum_rom_get());
}

static void ymfm_mix(void *storage, int16_t *buf, unsigned samples) {
  ((ymfm_backend *)storage)->mix(buf, samples);
}

} // extern "C"

extern "C" const struct fmplayer_backend fmplayer_backend_ymfm = {
  "ymfm", OUTPUT_RATE, 0xa000, sizeof(ymfm_backend), ymfm_init, ymfm_mix,
};
