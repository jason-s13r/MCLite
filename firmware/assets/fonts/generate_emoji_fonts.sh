#!/bin/zsh
set -e
cd "$(dirname "$0")"

FONT_FILE="OpenMoji-black-glyf.ttf"
LV_FONT_CONV="npx lv_font_conv"

# Generate 12px emoji font for T-Deck
$LV_FONT_CONV \
  --font "$FONT_FILE" \
  -r 0x1F600-0x1F64F \
  -r 0x1F44D-0x1F44F \
  -r 0x1F4AF,0x1F525,0x1F31F,0x1F4A1,0x1F389 \
  -r 0x26A0 \
  -r 0x1F4AC,0x1F4CC,0x1F5FA,0x1F50B,0x1F4E1 \
  -r 0x2600,0x1F327,0x1F328,0x1F319 \
  -r 0x1F436,0x1F431 \
  -r 0x2764,0x2705,0x274C,0x27A1,0x1F517 \
  --size 12 \
  --bpp 1 \
  --format lvgl \
  --lv-font-name lv_font_emoji_12 \
  --lv-fallback lv_font_montserrat_12 \
  -o emoji_font_12.c

# Generate 16px emoji font for T-Watch
node "$LV_FONT_CONV" \
  --font "$FONT_FILE" \
  -r 0x1F600-0x1F64F \
  -r 0x1F44D-0x1F44F \
  -r 0x1F4AF,0x1F525,0x1F31F,0x1F4A1,0x1F389 \
  -r 0x26A0 \
  -r 0x1F4AC,0x1F4CC,0x1F5FA,0x1F50B,0x1F4E1 \
  -r 0x2600,0x1F327,0x1F328,0x1F319 \
  -r 0x1F436,0x1F431 \
  -r 0x2764,0x2705,0x274C,0x27A1,0x1F517 \
  --size 16 \
  --bpp 1 \
  --format lvgl \
  --lv-font-name lv_font_emoji_16 \
  --lv-fallback lv_font_montserrat_16 \
  -o emoji_font_16.c

echo "Done!"
