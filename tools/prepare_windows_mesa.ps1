$ErrorActionPreference = 'Stop'
$archive = Join-Path $env:RUNNER_TEMP 'mesa-26.2.5.7z'
$folder = Join-Path $env:RUNNER_TEMP 'mesa-26.2.5'
Invoke-WebRequest 'https://github.com/pal1000/mesa-dist-win/releases/download/26.2.5/mesa3d-26.2.5-release-msvc.7z' -OutFile $archive
$actual = (Get-FileHash $archive -Algorithm SHA256).Hash.ToLowerInvariant()
if ($actual -ne '997123ef4ce8896204ab912d5b54f19372c487ff9835bcefafa2fca23b8953c2') { throw 'Mesa checksum mismatch' }
& 7z x $archive "-o$folder" -y | Out-Null
if ($LASTEXITCODE -ne 0) { throw 'Mesa extraction failed' }
$driver = Get-ChildItem $folder -Recurse -Filter opengl32.dll | Where-Object { $_.FullName -match '[\\/]x64[\\/]' } | Select-Object -First 1
if (-not $driver) { throw 'Mesa x64 OpenGL driver not found' }
"LUNORYX_MESA_DIR=$($driver.DirectoryName)" | Out-File -FilePath $env:GITHUB_ENV -Encoding utf8 -Append
