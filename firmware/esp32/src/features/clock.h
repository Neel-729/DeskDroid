#pragma once

namespace ClockFeature {
void begin(unsigned long now);
void nextQuote(unsigned long now);
void update(unsigned long now, bool quotesEnabled, bool format24);
void syncToRtc(unsigned long now);

const char* timeRow();
const char* quoteRow();
}
