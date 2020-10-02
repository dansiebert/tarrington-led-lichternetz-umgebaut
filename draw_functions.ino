// =======================================================================

void clearMatrix() {
  uint8_t *ptr = displaybuf;
    for (uint16_t i = 0; i < NUM_ROWS * 8; i++) {
      *ptr = 0x00;
      ptr++;
    }
}

// =======================================================================

void drawPoint(uint16_t x, uint16_t y, uint8_t pixel) {
  if ( x < 0 || x > 63 || y < 0 || y > 19 ) {
    return;
  }
  uint8_t *byte = displaybuf + x / 8 + y * 8;
  uint8_t  bit = x % 8;
  if (pixel) {
    *byte |= 0x80 >> bit;
  } else {
    *byte &= ~(0x80 >> bit);
  }
}
// =======================================================================

void drawRect(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint8_t pixel) {
  for (uint16_t x = x1; x < x2; x++) {
    for (uint16_t y = y1; y < y2; y++) {
      drawPoint(x, y, pixel);
    }
  }
}
// =======================================================================

void drawImage(uint16_t xoffset, uint16_t yoffset, uint16_t width, uint16_t height, const uint8_t *image) {
  for (uint16_t y = 0; y < height; y++) {
    for (uint16_t x = 0; x < width; x++) {
      const uint8_t *byte = image + (x + y * width) / 8;
      uint8_t  bit = 7 - x % 8;
      uint8_t  pixel = (*byte >> bit) & 1;
      drawPoint(x + xoffset, y + yoffset, pixel);
    }
  }
}
// =======================================================================

// (x, y) top-left position, x should be multiple of 8
void drawDigital_8(uint16_t x, uint16_t y, uint8_t n) {
  if ((n >= 10) || (0 != (x % 8))) {
    return;
  }
  uint8_t *pDst = displaybuf + y * 8 + x / 8;
  const uint8_t *pSrc = digitals + n * 16;
  for (uint8_t i = 0; i < 16; i++) {
    *pDst = *pSrc;
    pDst += 8;
    pSrc++;
  }
}

// =======================================================================

void drawDigital_16(uint16_t x, uint16_t y, uint8_t n) {
  uint8_t *pDst = displaybuf + y * 8 + x / 8;
  const uint8_t *pSrc = BigFont + n * 32;
  for (uint8_t i = 0; i < 16; i++) {
    *pDst = *pSrc;
    pDst++;
    pSrc++;
    *pDst = *pSrc;
    pDst += 7;
    pSrc++;
  }
}

// =======================================================================
