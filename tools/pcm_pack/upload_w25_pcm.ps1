param(
    [Parameter(Mandatory=$true)][string]$Port,
    [Parameter(Mandatory=$true)][string]$PcmPath,
    [Parameter(Mandatory=$false)][UInt32]$Address = 0x00001000,
    [Parameter(Mandatory=$false)][int]$Chunk = 2048,
    [Parameter(Mandatory=$false)][int]$Baud = 460800
)

if (-not (Test-Path -LiteralPath $PcmPath)) {
    throw "PCM file not found: $PcmPath"
}
if ($Chunk -le 0 -or $Chunk -gt 2048) {
    throw "Chunk must be 1..2048"
}

$data = [System.IO.File]::ReadAllBytes($PcmPath)
$len = [UInt32]$data.Length
Write-Host "PCM bytes: $len"

$sp = New-Object System.IO.Ports.SerialPort $Port,$Baud,'None',8,'One'
$sp.ReadTimeout = 10000
$sp.WriteTimeout = 10000
$sp.DtrEnable = $true
$sp.RtsEnable = $false
$sp.Open()

function Wait-LineToken {
    param(
        [System.IO.Ports.SerialPort]$PortObj,
        [string]$Token,
        [int]$TimeoutSec = 20
    )
    $deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSec)
    $line = ""
    while ([DateTime]::UtcNow -lt $deadline) {
        try {
            $line = $PortObj.ReadLine().Trim()
        } catch {
            continue
        }
        if ($line -eq $Token) { return $line }
    }
    throw "Expect $Token, got [$line]"
}

try {
    Start-Sleep -Milliseconds 150
    $sp.DiscardInBuffer()
    $sp.DiscardOutBuffer()

    $cmd = [System.Text.Encoding]::ASCII.GetBytes("PCMUPLOAD`n")
    $sp.Write($cmd, 0, $cmd.Length)

    $null = Wait-LineToken -PortObj $sp -Token "READY" -TimeoutSec 15
    Write-Host "Device READY"

    $hdr = New-Object byte[] 8
    [BitConverter]::GetBytes([UInt32]$Address).CopyTo($hdr, 0)
    [BitConverter]::GetBytes([UInt32]$len).CopyTo($hdr, 4)
    $sp.Write($hdr, 0, 8)

    $null = Wait-LineToken -PortObj $sp -Token "HDR" -TimeoutSec 20
    Write-Host "Header OK"

    $null = Wait-LineToken -PortObj $sp -Token "ERS" -TimeoutSec 300
    Write-Host "Erase OK"

    $offset = 0
    while ($offset -lt $len) {
        $take = [Math]::Min($Chunk, $len - $offset)
        $len2 = New-Object byte[] 2
        $len2[0] = [byte]($take -band 0xFF)
        $len2[1] = [byte](($take -shr 8) -band 0xFF)
        $sp.Write($len2, 0, 2)
        $sp.Write($data, $offset, $take)

        $ack = $sp.ReadByte()
        if ($ack -ne 0x06) {
            throw ("ACK failed at offset {0}, got 0x{1:X2}" -f $offset, $ack)
        }
        $offset += $take
        if (($offset % (64KB)) -eq 0) {
            Write-Host ("Uploaded {0}/{1}" -f $offset, $len)
        }
    }

    $null = Wait-LineToken -PortObj $sp -Token "DONE" -TimeoutSec 30
    Write-Host "Upload DONE"
}
finally {
    if ($sp.IsOpen) { $sp.Close() }
}
