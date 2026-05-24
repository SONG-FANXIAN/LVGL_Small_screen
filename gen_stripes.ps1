$headerPath = "src/mmexport_rgb565_240.h"
$width = 240
$height = 240
$headerText = "#ifndef MMEXPORT_RGB565_240_H`n#define MMEXPORT_RGB565_240_H`n#include <stdint.h>`n#define MMEXPORT_RGB565_240_WIDTH $width`n#define MMEXPORT_RGB565_240_HEIGHT $height`nstatic const uint8_t mmexport_rgb565_240_map[] = {`n"
Set-Content -Path $headerPath -Value $headerText
for ($y = 0; $y -lt $height; $y++) {
    $line = New-Object System.Text.StringBuilder
    for ($x = 0; $x -lt $width; $x++) {
        if ($x -lt 80) { $rgb = 0xF800 }
        elseif ($x -lt 160) { $rgb = 0x07E0 }
        else { $rgb = 0x001F }
        $low = $rgb -band 0xFF
        $high = ($rgb -shr 8) -band 0xFF
        [void]$line.Append("0x$($low.ToString('X2')),0x$($high.ToString('X2'))")
        if ($x -lt ($width - 1) -or $y -lt ($height - 1)) { [void]$line.Append(",") }
    }
    [void]$line.Append("`n")
    Add-Content -Path $headerPath -Value $line.ToString()
    if ($y % 20 -eq 0) { Write-Host "Progress: $y / $height" }
}
Add-Content -Path $headerPath -Value "};`n#endif"
Write-Host "File created successfully: $headerPath"
