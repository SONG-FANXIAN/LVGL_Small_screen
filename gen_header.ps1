$ErrorActionPreference = 'Stop';
$imagePath = 'mmexport1681037462961.jpg';
$headerPath = 'src/mmexport_rgb565_240.h';
if (-not (Test-Path $imagePath)) { throw 'Image not found' }
Add-Type -AssemblyName System.Drawing;
$img = [System.Drawing.Image]::FromFile((Resolve-Path $imagePath).Path);
$targetWidth = 240; $targetHeight = 240;
$resized = New-Object System.Drawing.Bitmap($targetWidth, $targetHeight);
$g = [System.Drawing.Graphics]::FromImage($resized);
$g.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic;
$g.DrawImage($img, 0, 0, $targetWidth, $targetHeight);
$totalSize = $targetWidth * $targetHeight * 2;
$bytes = New-Object byte[] $totalSize;
$index = 0;
for ($y = 0; $y -lt $targetHeight; $y++) {
    for ($x = 0; $x -lt $targetWidth; $x++) {
        $c = $resized.GetPixel($x, $y);
        $r = [int][Math]::Floor(($c.R * 31) / 255);
        $g_v = [int][Math]::Floor(($c.G * 63) / 255);
        $b = [int][Math]::Floor(($c.B * 31) / 255);
        $rgb = ($r -shl 11) -bor ($g_v -shl 5) -bor $b;
        $bytes[$index++] = $rgb -band 0xFF;
        $bytes[$index++] = ($rgb -shr 8) -band 0xFF;
    }
}
$sb = New-Object System.Text.StringBuilder;
for ($i = 0; $i -lt $bytes.Length; $i++) {
    [void]$sb.Append('0x' + $bytes[$i].ToString('X2'));
    if ($i -lt ($bytes.Length - 1)) { [void]$sb.Append(','); if (($i + 1) % 16 -eq 0) { [void]$sb.AppendLine() } else { [void]$sb.Append(' ') } }
}
$headerContent = "#ifndef MMEXPORT_RGB565_240_H`n#define MMEXPORT_RGB565_240_H`n#include <stdint.h>`n#define MMEXPORT_RGB565_240_WIDTH 240`n#define MMEXPORT_RGB565_240_HEIGHT 240`n#define MMEXPORT_RGB565_240_SIZE $totalSize`nstatic const uint8_t mmexport_rgb565_240_map[] = {`n" + $sb.ToString() + "`n};`n#endif";
if (-not (Test-Path 'src')) { New-Item -ItemType Directory 'src' }
[System.IO.File]::WriteAllText($headerPath, $headerContent);
$g.Dispose(); $resized.Dispose(); $img.Dispose();
Write-Output "File created: $headerPath"; Write-Output "Total bytes: $totalSize";
