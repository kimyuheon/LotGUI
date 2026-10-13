param(
    [switch]$FindThree,
    [string]$Ffmpeg = ""
)

$ErrorActionPreference = "Stop"
$root = (Resolve-Path (Join-Path $PSScriptRoot "../..")).Path
$exe = Join-Path $root "build/Release/list_control_demo.exe"
$output = Join-Path $root "docs/videos/list-control-demo.mp4"
$log = Join-Path $root "build/list-control-video-ffmpeg.log"

if (-not (Test-Path -LiteralPath $exe)) {
    throw "Build list_control_demo (Release) before recording."
}
if (-not $Ffmpeg) {
    $bundled = Join-Path $root "build/video_tools/imageio_ffmpeg/binaries"
    $found = Get-ChildItem -LiteralPath $bundled -Filter "ffmpeg-*.exe" `
        -ErrorAction SilentlyContinue | Select-Object -First 1
    if ($found) {
        $Ffmpeg = $found.FullName
    } else {
        $Ffmpeg = (Get-Command ffmpeg -ErrorAction Stop).Source
    }
}

New-Item -ItemType Directory -Path (Split-Path $output) -Force | Out-Null
if ($FindThree) {
    $frames = Join-Path $root "build/list-control-find-frames"
    New-Item -ItemType Directory -Path $frames -Force | Out-Null
    & $exe --capture-find $frames
    if ($LASTEXITCODE -ne 0) {
        throw "list_control_demo frame capture failed."
    }
    $output = Join-Path $root "docs/videos/list-control-find-3.mp4"
    & $Ffmpeg -y -hide_banner -loglevel error -framerate 8 `
        -i (Join-Path $frames "frame-%04d.ppm") -frames:v 64 `
        -c:v libx264 -preset veryfast -crf 18 -pix_fmt yuv420p `
        -movflags +faststart $output
    if ($LASTEXITCODE -ne 0) {
        throw "ffmpeg failed to encode the Find video."
    }
    Write-Output $output
    return
}
$app = Start-Process -FilePath $exe -ArgumentList "--video-demo" `
    -WorkingDirectory (Split-Path $exe) -PassThru
try {
    Start-Sleep -Seconds 2
    $encoder = Start-Process -FilePath $Ffmpeg -WindowStyle Hidden -PassThru `
        -Wait -RedirectStandardError $log -ArgumentList @(
            "-y", "-hide_banner", "-loglevel", "error",
            "-f", "gdigrab", "-framerate", "15",
            "-i", '"title=LotUI ListControl"', "-t", "20",
            "-vf", "pad=ceil(iw/2)*2:ceil(ih/2)*2",
            "-c:v", "libx264", "-preset", "veryfast",
            "-crf", "22", "-pix_fmt", "yuv420p",
            "-movflags", "+faststart", $output
        )
    if ($encoder.ExitCode -ne 0) {
        throw "ffmpeg exited with code $($encoder.ExitCode): " +
            (Get-Content -LiteralPath $log -Raw)
    }
} finally {
    if (-not $app.HasExited) {
        if (-not $app.WaitForExit(5000)) {
            $app.CloseMainWindow() | Out-Null
            if (-not $app.WaitForExit(2000)) {
                $app.Kill()
            }
        }
    }
}

Write-Output $output
