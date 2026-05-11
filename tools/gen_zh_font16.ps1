param(
  [string]$Root = "."
)
Add-Type -AssemblyName System.Drawing

$files = Get-ChildItem -Path $Root -Recurse -File -Include *.c,*.h,*.md | Where-Object { $_.FullName -notmatch '\\.pio\\' }
$chars = New-Object System.Collections.Generic.HashSet[int]
foreach ($f in $files) {
  $txt = Get-Content $f.FullName -Raw -ErrorAction SilentlyContinue
  if ($null -eq $txt) { continue }
  foreach ($ch in $txt.ToCharArray()) {
    $cp = [int][char]$ch
    if (($cp -ge 0x4E00 -and $cp -le 0x9FFF) -or ($cp -ge 0x3000 -and $cp -le 0x303F)) {
      [void]$chars.Add($cp)
    }
  }
}

# ensure common punctuation
foreach ($cp in @(0xFF0C,0x3002,0xFF1A,0xFF08,0xFF09,0x3001,0xFF05,0xFF01,0xFF1F)) { [void]$chars.Add($cp) }

$codes = $chars | Sort-Object

$fontName = "SimSun"
$fontSize = 14.0
$bmp = New-Object System.Drawing.Bitmap 16,16
$gfx = [System.Drawing.Graphics]::FromImage($bmp)
$gfx.Clear([System.Drawing.Color]::Black)
$gfx.TextRenderingHint = [System.Drawing.Text.TextRenderingHint]::SingleBitPerPixelGridFit
$font = New-Object System.Drawing.Font($fontName, $fontSize, [System.Drawing.FontStyle]::Regular, [System.Drawing.GraphicsUnit]::Pixel)
$brush = New-Object System.Drawing.SolidBrush([System.Drawing.Color]::White)
$sf = New-Object System.Drawing.StringFormat
$sf.Alignment = [System.Drawing.StringAlignment]::Center
$sf.LineAlignment = [System.Drawing.StringAlignment]::Center

$hPath = "include/font_zh16.h"
$cPath = "src/font_zh16.c"

$h = @()
$h += "#ifndef FONT_ZH16_H"
$h += "#define FONT_ZH16_H"
$h += "#include <stdint.h>"
$h += "typedef struct { uint16_t code; uint8_t rows[32]; } FontZh16Glyph;"
$h += "extern const FontZh16Glyph g_font_zh16[];"
$h += "extern const uint16_t g_font_zh16_count;"
$h += "const FontZh16Glyph *FontZh16_Find(uint16_t code);"
$h += "#endif"

$c = @()
$c += '#include "font_zh16.h"'
$c += "const FontZh16Glyph g_font_zh16[] = {"

foreach ($cp in $codes) {
  $ch = [char]$cp
  $gfx.Clear([System.Drawing.Color]::Black)
  $rect = New-Object System.Drawing.RectangleF(0,0,16,16)
  $gfx.DrawString([string]$ch, $font, $brush, $rect, $sf)

  $rows = New-Object System.Collections.Generic.List[string]
  for ($y=0; $y -lt 16; $y++) {
    $b0 = 0
    $b1 = 0
    for ($x=0; $x -lt 8; $x++) {
      $p = $bmp.GetPixel($x,$y)
      if ($p.R -gt 20) { $b0 = $b0 -bor (1 -shl (7-$x)) }
    }
    for ($x=8; $x -lt 16; $x++) {
      $p = $bmp.GetPixel($x,$y)
      if ($p.R -gt 20) { $b1 = $b1 -bor (1 -shl (15-$x)) }
    }
    $rows.Add(("0x{0:X2},0x{1:X2}" -f $b0,$b1)) | Out-Null
  }
  $cpHex = ('0x{0:X4}' -f $cp)
  $rowText = ($rows -join ',')
  $c += ("  {" + $cpHex + ",{" + $rowText + "}},")
}

$c += "};"
$c += ("const uint16_t g_font_zh16_count = {0};" -f $codes.Count)
$c += "const FontZh16Glyph *FontZh16_Find(uint16_t code)"
$c += "{"
$c += "    uint16_t i;"
$c += "    for (i = 0U; i < g_font_zh16_count; ++i)"
$c += "    {"
$c += "        if (g_font_zh16[i].code == code) return &g_font_zh16[i];"
$c += "    }"
$c += "    return (const FontZh16Glyph *)0;"
$c += "}"

Set-Content -Path $hPath -Value ($h -join "`r`n") -Encoding UTF8
Set-Content -Path $cPath -Value ($c -join "`r`n") -Encoding UTF8

$gfx.Dispose(); $bmp.Dispose(); $font.Dispose(); $brush.Dispose(); $sf.Dispose()
Write-Output ("Generated {0} glyphs" -f $codes.Count)
