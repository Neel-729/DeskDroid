#pragma once

namespace ClockFeature {
void begin();
void nextQuote();
void update(unsigned long now, bool quotesEnabled, bool format24);
void syncToRtc();

const char* timeRow();
const char* quoteRow();
}
