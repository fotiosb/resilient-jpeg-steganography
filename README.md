# Resilient JPEG Steganography — PHP Extension

A native C extension for PHP that embeds invisible, machine-readable identity strings inside JPEG images as a Digital Rights Management (DRM) mechanism. The encoded payload survives common image manipulations including JPEG recompression, moderate resizing, brightness/contrast adjustments, and cropping attacks.

---

## Table of Contents

1. [Purpose and Motivation](#purpose-and-motivation)
2. [Theory of Operation](#theory-of-operation)
   - [Why Steganography Over Watermarking](#why-steganography-over-watermarking)
   - [Chroma-Shift Encoding](#chroma-shift-encoding)
   - [Bar-Code Spatial Layout](#bar-code-spatial-layout)
   - [The Anchor Bar](#the-anchor-bar)
   - [Anti-Crop Protection](#anti-crop-protection)
   - [Watermarking Subsystem](#watermarking-subsystem)
3. [Architecture](#architecture)
   - [Component Overview](#component-overview)
   - [Source File Map](#source-file-map)
   - [Data Flow — Encoding](#data-flow--encoding)
   - [Data Flow — Decoding](#data-flow--decoding)
4. [Encoder Options Reference](#encoder-options-reference)
5. [Decoder Options Reference](#decoder-options-reference)
6. [Strengths and Limitations](#strengths-and-limitations)
7. [Building](#building)
   - [Prerequisites](#prerequisites)
   - [Build Steps](#build-steps)
8. [Installation](#installation)
9. [PHP Usage](#php-usage)
   - [Encoding an Image](#encoding-an-image)
   - [Decoding an Image](#decoding-an-image)
10. [Testing Protocol](#testing-protocol)
11. [Maintenance](#maintenance)
12. [Developer Notes](#developer-notes)
    - [Key Source Files](#key-source-files)
    - [Bugs Fixed in This Release](#bugs-fixed-in-this-release)
    - [Known Limitations and Future Work](#known-limitations-and-future-work)
13. [License](#license)

---

## Purpose and Motivation

Online distribution of copyrighted images creates a persistent identification problem: once an image leaves your server, standard metadata (EXIF, IPTC) can be trivially stripped, and visual watermarks can be removed with commodity photo-editing tools. This module addresses the problem by encoding a short identity token — up to 8 ASCII characters — **inside the image pixel data itself**, in a way that is invisible to the human eye yet recoverable by the decoder even after the image has been resaved, resized, or cropped.

Typical use cases:

- Embedding a per-customer licence code before delivering a stock photograph.
- Tracking which distribution copy of an image was leaked.
- Proving authorship of an image against removal of metadata.

---

## Theory of Operation

### Why Steganography Over Watermarking

Visible watermarks alter luminance (the Y channel in YCbCr) in a way the human visual system can clearly detect. Sophisticated attackers use clone-brush tools to overwrite the watermark region with surrounding texture. Invisible watermarks embedded in the frequency domain (DCT coefficients) are fragile: a single re-encode at a different quality setting can destroy them.

This module uses **chroma steganography**: it shifts the Cb and/or Cr colour-difference channels of selected pixels by a small integer (typically 1–2 in production; 50 in the diagnostic visualisation mode shown in the documentation). The human visual system is far less sensitive to chrominance changes than to luminance changes, so shifts of ±1–2 on an 0–255 scale are perceptually invisible. The luminance (Y) channel is left untouched.

### Chroma-Shift Encoding

Each bit of the payload is encoded as a chroma shift direction on a vertical bar of pixels that spans the full height of the image:

| Bit value | Chroma shift direction |
|-----------|----------------------|
| `1`       | Shift **up** (`Cb += shift`, `Cr += shift`) |
| `0`       | Shift **down** (`Cb -= shift`, `Cr -= shift`) |

The default shift magnitude is `2` on both the Cb and Cr channels, configurable independently via `Cb_Shift` and `Cr_Shift`. The magnitude must be chosen to balance perceptibility against robustness after recompression. Larger values survive more aggressive manipulation but become visible sooner.

### Bar-Code Spatial Layout

The encoded bars are arranged as a regular grid across the image width (or height for portrait images):

- **29 bars total** — 1 anchor bar + 28 data bars.
- Each data bar encodes **2 bits**: one from the top half of the image, one from the bottom half.
- 28 bars × 2 bits = **56 bits = 8 × 7-bit ASCII characters**.
- Bars are spaced equally across the usable image area; spacing is computed from the image dimensions at encode time.
- For portrait images (height > width), bars are arranged horizontally to maintain the same pixel density.

Each bar occupies several adjacent pixel columns (the exact width is determined by the step factor), giving the decoder a neighbourhood of pixels to vote on, which improves robustness after recompression noise is introduced.

### The Anchor Bar

The first bar in the sequence is a special **anchor bar** with two distinguishing properties:

1. Its chroma shift magnitude is one unit higher than the data bars, making it statistically distinguishable.
2. It uses an alternating up/down chroma pattern (rather than uniform direction), which compensates for the larger magnitude and allows the decoder to identify it by its alternating signature.

The anchor bar's column position is the reference point from which the decoder calculates the grid step and locates all data bars. When anti-crop protection is disabled, bar positions are computed from image dimensions alone; when enabled, they are computed from the anchor bar's detected position.

### Anti-Crop Protection

If an attacker crops the image, all bar-position calculations based on image dimensions are thrown off. Anti-crop protection solves this by:

1. Leaving an uncoded **margin** on each side of the image (configurable as a percentage of the image width/height via `Steg_Anti_Crop_Percent`). The margin is large enough that moderate cropping will not reach the first data bar.
2. Encoding the anchor bar at a position within this margin, so it survives moderate crops and gives the decoder a fixed reference point.
3. At decode time, the decoder **searches** for the anchor bar's column rather than assuming a position, and uses the distance to the second anchor reference to reconstruct the step.

There is an explicit **no-crop hint** option (`Steg_Decode_No_Crop_Hint`) for images that are trusted not to have been cropped: it skips anchor searching and uses image dimensions directly, which is faster and more reliable for the common case.

### Watermarking Subsystem

In addition to the steganographic payload, the module can overlay a **visible text watermark** repeated in a tiled pattern across the image. Watermarking uses the GD library (with FreeType) to render the watermark string at a configurable font size into the Y (luminance) channel of matching pixels. Both subsystems are independently configurable and can be used simultaneously.

---

## Architecture

### Component Overview

```
PHP script
    │
    │  calls steg_easy_apply2() / steg_easy_decode2()
    ▼
SWIG-generated PHP extension (steganography2.so)
    │
    ▼
interface.c          — high-level encode/decode logic; spike array management
    │
    ├── jpeg.c       — JPEG compress/decompress wrappers; chroma-spike detection
    │                  and decoding; statistical message reconstruction
    │
    ├── jsteg.c      — bit-buffer injection/extraction helpers
    │
    └── jpeg-v4/     — heavily modified IJG JPEG v4 codec
            └── jccolor.c  — RGB↔YCbCr conversion + steganographic encoding
```

### Source File Map

| File | Role |
|------|------|
| `src/interface.c` | Entry points `steg_easy_apply2()` and `steg_easy_decode2()`. Allocates the `spike` detection array, drives the encode/decode pipeline, and returns the decoded message string to PHP. |
| `src/interface.h` | Public C declarations for the two entry points. |
| `src/jpeg.c` | Wrappers around the modified IJG codec for compress/decompress. Contains all spike-analysis logic: `find_chroma_spike_cols3`, `find_chroma_spike_cols4`, `find_chroma_spike_rows3`, anchor detection (`getAnchorStep`, `populateSpikeBlockArray`), and the message reconstruction functions (`decodeStegMessage3/4`, `processMessageStrings`). |
| `src/jpeg.h` | Declarations for all `jpeg.c` functions plus key structs (`image_data`, `spikeBlock`, `strStruct`, etc.). |
| `src/jsteg.c` | Low-level bit-buffer helpers: `jsteg_inject_buffer` (load bits into the bitsource), `jsteg_extract_buffer` (read bits from the bitsink), `jsteg_compress`, `jsteg_decompress`. |
| `src/jsteg.h` | Declarations for `jsteg.c`. |
| `src/steg.i` | SWIG interface file that exposes `steg_easy_apply2` and `steg_easy_decode2` to PHP. |
| `src/steganography2.php` | SWIG-generated PHP proxy class. Import this in your PHP scripts. |
| `src/php_steganography2.h` | SWIG-generated C header required during compilation. |
| `src/VeraMono.ttf` | Bitstream Vera Monospace font used for text watermarking via GD/FreeType. |
| `src/tests.c` | Standalone C test harness (compile separately; not part of the extension). |
| `src/jpeg-v4/jccolor.c` | **Core steganographic encoding engine.** Modified IJG colour-conversion routine that intercepts the RGB→YCbCr conversion pass to inject chroma shifts and render text watermarks. |
| `src/jpeg-v4/*.c/h` | IJG JPEG v4 codec (partially modified). See `src/jpeg-v4/README` for the original library documentation. |

### Data Flow — Encoding

```
PHP: steg_easy_apply2(message, options, infile, outfile)
 │
 ├─ steg_init()          — copy all options into global config vars
 ├─ steg_decompress()    — decode infile into raw RGB bitmap (image_data)
 ├─ jsteg_max_size()     — compute maximum bit capacity of image
 ├─ jsteg_compress()     — re-encode bitmap back to JPEG:
 │      └─ jccolor.c     — during RGB→YCbCr conversion, compute bar positions,
 │                         shift Cb/Cr of bar pixels, render text watermark
 └─ steg_cleanup()       — free resources
```

### Data Flow — Decoding

```
PHP: steg_easy_decode2(options, infile, outfile)
 │
 ├─ steg_decode_init()   — copy all decode options into global config vars
 ├─ steg_decompress()    — decode infile into raw RGB bitmap
 │      └─ jccolor.c     — during YCbCr→RGB conversion, record chroma spikes
 │                         into the global spike[][] array
 ├─ jsteg_compress()     — re-encode to outfile (diagnostic output image)
 ├─ [anchor detection]   — getAnchorStep() or dimension-based calculation
 ├─ [bit extraction]     — find_chroma_spikes() / stepThrough()
 ├─ [message decode]     — decodeStegMessage3/4() — convert bit strings to chars
 ├─ processMessageStrings() — statistical voting to select most likely string
 └─ return Steg_Message  — decoded ASCII string returned to PHP
```

---

## Encoder Options Reference

Passed as arguments to `steg_easy_apply2()`:

| Parameter | Type | Description |
|-----------|------|-------------|
| `message` | `string` | Visible watermark text. Any length (very long strings not recommended). |
| `watermark_font_size` | `short` | Font size in points for the text watermark. |
| `Steg_Encode_Old_Overlay_On` | `short` | `1` = use legacy watermark mode; `0` = use current mode. |
| `Y_BUMP` | `short` | Y-channel luminance adjustment for watermarked pixels (positive = lighter, negative = darker). |
| `Y_BUMP_REACTIVE` | `short` | `1` = Y-bump adapts to current pixel brightness (lighter for dark pixels, darker for bright pixels). |
| `Watermark_X_Step` | `short` | Horizontal pixel stride between repeated watermark instances. |
| `Watermark_Y_Step` | `short` | Vertical pixel stride between repeated watermark instances. |
| `Steg_Encode_Anti_Crop_On` | `short` | `1` = embed anchor bar and reserve edge margins. `0` = spread data across full image (more resilient to resize, fragile to any crop). |
| `Steg_Anti_Crop_Percent` | `short` | Percentage of image dimension to leave uncoded on each edge (e.g. `10` = leave 10% margin each side). |
| `Steg_Encode_Col_Chroma_Shift_On` | `short` | `1` = enable steganographic chroma-shift encoding. |
| `Steg_Encode_Col_Chroma_Shift` | `short` | Diagnostic visualisation shift magnitude (normally unused in production). |
| `Steg_Encode_Col_Chroma_Shift_Cb_On` | `short` | `1` = apply shift to Cb channel. |
| `Steg_Encode_Col_Chroma_Shift_Cr_On` | `short` | `1` = apply shift to Cr channel. |
| `Steg_Encode_Col_Chroma_Shift_Down` | `short` | `1` = force all shifts downward (diagnostic). |
| `Steg_Encode_Col_Step_factor` | `short` | Controls bar-grid density (default `60`; higher = denser bars). |
| `Cb_Shift` | `short` | Chroma shift magnitude on the Cb channel (recommended: `1` or `2`). |
| `Cr_Shift` | `short` | Chroma shift magnitude on the Cr channel (recommended: `1` or `2`). |
| `Steg_Encode_Message` | `string` | The steganographic payload (up to **8 ASCII characters**). |
| `infile` | `string` | Absolute path to the source JPEG file. |
| `outfile` | `string` | Absolute path for the encoded output JPEG file. |

---

## Decoder Options Reference

Passed as arguments to `steg_easy_decode2()`:

| Parameter | Type | Description |
|-----------|------|-------------|
| `message` | `string` | Watermark string (passed for reference; not used in decoding). |
| `Steg_Decode_Specific_Diff` | `short` | Enable bounds-based chroma-spike detection (`1` = on). |
| `Cb_Shift` | `short` | Must match the value used during encoding. |
| `Cr_Shift` | `short` | Must match the value used during encoding. |
| `Steg_Decode_Bound` | `short` | Tolerance for chroma spike detection (helps with noisy/scaled images; try `3`). |
| `Steg_Decode_Anti_Crop_On` | `short` | `1` = image was encoded with anti-crop protection. |
| `Steg_Decode_No_Crop_Hint` | `short` | `1` = assume the image has not been cropped (faster, more reliable if true). |
| `Steg_Anti_Crop_Percent` | `short` | Must exactly match the value used during encoding. |
| `Steg_Decode_Ignore_Y` | `short` | `1` = skip Y-channel comparisons (helps with brightness-attacked images). |
| `Steg_Decode_Y_Bound` | `short` | Y-comparison tolerance (try `0`, `5`, or `10` for difficult images). |
| `infile` | `string` | Absolute path to the encoded JPEG to decode. |
| `outfile` | `string` | Absolute path for the diagnostic output image (shows detected spikes). |

**Return value:** The decoded steganographic message string, or an HTML-formatted string containing candidate strings ranked by statistical confidence. Returns `-1` (as string) on error.

---

## Strengths and Limitations

### Strengths

- **Perceptual invisibility** — chroma shifts of ±1–2 are below the threshold of human colour discrimination for typical photographic content.
- **JPEG recompression resilience** — the bar code uses wide pixel columns so that JPEG block artefacts on re-encode affect only some pixels in each bar; majority voting recovers the correct bit.
- **Resize resilience** — bars are scaled proportionally with the image; the step is recalculated from image dimensions or the anchor at decode time.
- **Brightness-attack resilience** — the Ignore Y and Y-bound decoder options allow decoding despite moderate brightness or contrast adjustments.
- **Crop resilience (with anti-crop)** — the anchor bar and margin scheme allow decoding after moderate crops, provided the crop does not reach the encoded region.
- **Portrait/landscape aware** — the system automatically switches between column-bars and row-bars for portrait images.
- **Statistical redundancy** — the advanced (anti-crop, anchor-detected) decoder performs hundreds of slightly offset decodings and picks the most-repeated result, dramatically improving robustness.

### Limitations

- **8-character payload limit** — 56 bits at 7 bits/char = exactly 8 ASCII characters. Extending this would require more bars, meaning either a denser grid (less resilient) or a larger minimum image size.
- **JPEG-specific** — the encoding pipeline is tightly coupled to the IJG JPEG v4 codec. PNG, WebP, or other formats are not supported.
- **Requires original format** — the image must remain a JPEG. Converting to PNG and back will destroy the chroma structure (PNG is lossless and does not use YCbCr subsampling).
- **Aggressive downscaling** — scaling an image below approximately 30% of original dimensions compresses bars below the reliable detection threshold.
- **Aggressive cropping** — even with anti-crop enabled, cropping that removes more than the margin percentage on the relevant edge will destroy the anchor and may prevent decoding.
- **Fixed 2048×2048 spike array** — the spike detection array is statically sized for images up to 2048×2048 pixels. Larger images will cause out-of-bounds writes (see Future Work).

---

## Building

### Prerequisites

The following must be present on the build machine:

| Dependency | Minimum version | Purpose |
|------------|-----------------|---------|
| GCC or Clang | any C99-capable | Compiler |
| PHP development headers | 5.x or 7.x | `php-config`, `php.h` |
| SWIG | 2.0+ | Generates the PHP extension glue code |
| libgd | 2.0+ | Image creation and FreeType text rendering |
| FreeType2 | 2.4+ | TrueType font rendering for watermarks |
| zlib | 1.2+ | JPEG stream compression (transitive via libgd) |
| GNU Autotools | any recent | `autoconf`, `automake`, `libtool` |
| `pkg-config` | any | Locates freetype2 and zlib |

On a Debian/Ubuntu system:

```bash
sudo apt-get install build-essential autoconf automake libtool pkg-config \
    swig php-dev libgd-dev libfreetype6-dev zlib1g-dev
```

On a RHEL/CentOS/Fedora system:

```bash
sudo dnf install gcc autoconf automake libtool pkg-config \
    swig php-devel gd-devel freetype-devel zlib-devel
```

### Build Steps

```bash
# 1. Regenerate the build system (only needed if configure.ac or Makefile.am changed)
autoreconf -fi

# 2. Configure
./configure

# 3. Build
make

# 4. The extension module will be at:
#    src/.libs/steganography2.so   (Linux)
```

If you receive errors about a missing `FONTDIR` definition, ensure `jccolor.c` is compiled with the path to `VeraMono.ttf`. The default assumes the font is installed in the same directory; override via:

```bash
./configure CFLAGS="-DFONTDIR='\"/path/to/font/directory/\"'"
```

---

## Installation

```bash
# 1. Copy the extension into PHP's extension directory
sudo make install

# 2. Or manually:
sudo cp src/.libs/steganography2.so $(php-config --extension-dir)/

# 3. Enable in php.ini:
echo "extension=steganography2" | sudo tee -a $(php-config --php-ini)

# 4. Copy the PHP proxy class where your scripts can reach it:
cp src/steganography2.php /your/app/lib/

# 5. Copy the font where the extension can find it, and update FONTDIR if needed.
#    Default font path assumed at compile time (see jccolor.c TTF_LOCATION macro).

# 6. Create and make writable the directory where the decoder dumps diagnostic images:
sudo mkdir -p /var/www/html/steg/admin/steg
sudo chown www-data:www-data /var/www/html/steg/admin/steg
```

Verify the extension is loaded:

```bash
php -m | grep steganography2
```

---

## PHP Usage

### Encoding an Image

```php
<?php
require_once '/path/to/steganography2.php';

$result = steganography2::steg_easy_apply2(
    /* Visible watermark text */         'Copyright 2024',
    /* Watermark font size */            12,
    /* Old watermark mode */             0,
    /* Y luminance bump */               5,
    /* Reactive Y bump */                1,
    /* Watermark X step (px) */          200,
    /* Watermark Y step (px) */          100,
    /* Anti-crop protection */           1,
    /* Anti-crop margin % */             10,
    /* Chroma-shift encoding ON */       1,
    /* Diagnostic shift value */         2,
    /* Encode Cb channel */              1,
    /* Encode Cr channel */              1,
    /* Force shift down (diag) */        0,
    /* Step factor */                    60,
    /* Cb shift magnitude */             2,
    /* Cr shift magnitude */             2,
    /* Steganographic payload (≤8 ch) */ 'CUST1234',
    /* Input JPEG path */                '/images/original.jpg',
    /* Output JPEG path */               '/images/encoded.jpg'
);

if ($result === -1) {
    error_log('Steganographic encoding failed');
}
```

### Decoding an Image

```php
<?php
require_once '/path/to/steganography2.php';

$decoded = steganography2::steg_easy_decode2(
    /* Reference watermark text */       'Copyright 2024',
    /* Bounds-based spike detect */      1,
    /* Cb shift (must match encode) */   2,
    /* Cr shift (must match encode) */   2,
    /* Spike detection bound */          3,
    /* Anti-crop mode (match encode) */  1,
    /* No-crop hint */                   1,   // set 1 if image not cropped
    /* Anti-crop % (match encode) */     10,
    /* Ignore Y channel */               0,
    /* Y bound */                        5,
    /* Encoded JPEG path */              '/images/encoded.jpg',
    /* Diagnostic output path */         '/tmp/steg_debug.jpg'
);

// $decoded contains the most-likely decoded string,
// followed by HTML-formatted alternative candidates.
echo htmlspecialchars($decoded);
```

**Tip:** If decoding fails, try `Steg_Decode_Ignore_Y = 1` first, then vary `Steg_Decode_Y_Bound` (0, 5, 10). For noisy or heavily resized images enable `Steg_Decode_Specific_Diff = 1` and adjust `Steg_Decode_Bound`.

---

## Testing Protocol

A structured test matrix should be run after any change to encoding parameters or after deployment to a new server:

1. **Select a representative set** of 10–20 test images (varying content, dimensions, aspect ratios).
2. **Encode** each image with a known payload at `Cb_Shift = Cr_Shift = 2` and `Steg_Encode_Anti_Crop_On = 1`.
3. **Apply the following manipulations** to each encoded image:
   - Resave at JPEG quality 70%, baseline optimised.
   - Adjust brightness +15%, resave at quality 70%.
   - Scale down by 10%, resave at quality 70%.
   - Crop 5% off one edge, resave at quality 70%.
   - Scale down 10% AND crop 5%, resave at quality 70%.
4. **Decode** each manipulation with `Steg_Decode_No_Crop_Hint = 1` for uncropped variants, `0` for cropped.
5. **Record** which manipulations produce incorrect or empty results.
6. **For failures**, retry with `Steg_Decode_Y_Bound = 0` and then `= 10`, and try `Steg_Decode_Ignore_Y = 1`.

---

## Maintenance

The decoder produces a diagnostic JPEG at the path specified by `outfile`. These accumulate over time. Purge them periodically:

```bash
# Manual
rm -f /var/www/html/steg/admin/steg/*

# Via cron (daily at 03:00)
0 3 * * * find /var/www/html/steg/admin/steg -type f -mtime +1 -delete
```

---

## Developer Notes

### Key Source Files

The two files where the steganographic logic lives are:

- **`src/jpeg-v4/jccolor.c`** — The modified IJG colour-conversion routine. The `rgb_ycc_convert` function is where chroma shifts are applied during JPEG compression. Look for the `#ifdef STEG_SUPPORTED` blocks and the Fotios comment sections.
- **`src/jpeg.c`** — All post-decompression spike analysis: detection, anchor bar location, bit extraction, and statistical message reconstruction.

All source files contain inline comments explaining design decisions.

### Bugs Fixed in This Release

The following issues were identified and corrected compared to the original development snapshot:

| # | File | Issue | Fix |
|---|------|-------|-----|
| 1 | `jpeg.c` — `steg_compress2()` | Hardcoded absolute path to a developer workstation filesystem was left in production code, causing silent failure on any other machine. | Replaced with `/tmp/steg_decodedump.jpg`. |
| 2 | `jsteg.c` — `jsteg_decompress()` | `malloc(max + 1)` allocated N+1 bytes, but `nbuf[max + 1]` wrote the null terminator **two bytes past the end** of the buffer — a heap buffer overflow. | Changed to `malloc(max + 2)` and `nbuf[max] = '\0'`. |
| 3 | `interface.c` — `steg_easy_decode2()` | The `spike` and `rowAnchorSpike` pointer arrays were freed **before** the data blocks (`temp1`, `temp2`) they point into, creating use-after-free and dangling-pointer hazards. | Reversed the free order: data blocks freed first, then pointer arrays. |
| 4 | `jpeg.c` — `get_xy()` | Buffer size was 1024 bytes, but JPEG files with large EXIF, ICC Profile, or XMP metadata blocks can have their `SOF0` marker well beyond byte 1024, causing the function to return -1 (image unreadable). Additionally, a `fread` returning fewer bytes than requested (e.g. small files or read errors) was treated as a fatal error before checking the magic bytes. The internal `while` loop had no array-bounds guard. | Increased buffer to 64 KB, `fclose` before returning on all paths, replaced size check with `bytes_read < 4`, added `(size_t)i + N < bytes_read` guards throughout. |
| 5 | `jsteg.c` — `jsteg_compress()` | Variable `newsz` was uninitialized and used when `max == 0` (the `if (max != 0)` branch was skipped), causing undefined behaviour. | Initialized `newsz = (long)bufsz` at declaration. |
| 6 | `jpeg.c` — `processMessageStrings()` | `sscanf(s, "%[^#]%n", pss[i].str, &charCount)` had no field-width limit; `pss[i].str` is 16 bytes, so a token longer than 15 characters would overflow the buffer. | Added explicit width specifier: `"%15[^#]%n"`. |

### Known Limitations and Future Work

- **Fixed 2048×2048 `spike` array** — The current allocation in `interface.c` is `2048 × 2048 × sizeof(int)` (16 MB). Images larger than 2048 pixels in either dimension will cause out-of-bounds writes in `jccolor.c` and the spike analysis routines. A future fix would pass image dimensions into the allocation and add bounds guards in all spike array accessors.

- **Global state prevents thread safety** — `jpeg.c` uses many file-scope global variables (`watermark`, `Steg_Encode_Message`, `my_cinfo_struct`, etc.). The module cannot be used safely from multiple PHP threads simultaneously (PHP-FPM in multi-threaded mode). This is acceptable for a single-threaded FastCGI deployment but should be refactored to use per-call context structs for thread safety.

- **Payload capacity** — 56 bits (8 × 7-bit ASCII) is sufficient for short codes (order IDs, customer numbers) but not for UUIDs or hashes. Extending the payload would require either increasing the number of bars (reducing per-bar resilience), using multiple bits per bar (more complex encoding/decoding), or encoding into both chroma channels independently (already partially supported via the Cb/Cr split options).

- **IJG JPEG v4 base library** — The embedded codec is version 4 of the Independent JPEG Group library, which is very old. Modern distributions ship libjpeg-turbo (IJG v9-compatible) which is significantly faster and supports more JPEG variants. Migrating to libjpeg-turbo's public API would remove the need to vendor a modified codec, simplify the build, and improve compatibility.

---

## License

### Module (this project)

The steganography extension code authored by Fotios Basagiannis is distributed under MIT licence. The base JPEG handling code (`jpeg.c`, `jsteg.c`) was originally authored by Micheal Smith (though heavily modified by Fotios Basagiannis) and is also distributed under the MIT License; see individual file headers for the full text.

### Embedded JPEG library (`src/jpeg-v4/`)

The Independent JPEG Group's JPEG v4 library is distributed under the IJG licence. See `src/jpeg-v4/README` for terms.

### Font (`src/VeraMono.ttf`)

Bitstream Vera Fonts are distributed under the Bitstream Vera Fonts Public License. See https://www.gnome.org/fonts/ for terms.
