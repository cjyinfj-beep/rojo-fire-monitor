# ESP-IDF 一键环境验证与修复脚本
# 用途：直接激活 ESP-IDF 环境并验证所有配置
# 运行方式：右键 PowerShell -> 以管理员身份运行，然后执行：
#   & "C:\Users\CJY\Documents\kimi\workspace\esp-idf-verify.ps1"

param(
    [switch]$SetSystemEnv    # 加上 -SetSystemEnv 参数可将环境变量写入系统
)

$Host.UI.RawUI.BackgroundColor = "Black"
$ErrorActionPreference = "Continue"

function Write-Header($text) {
    Write-Host "`n========================================" -ForegroundColor Cyan
    Write-Host $text -ForegroundColor Cyan
    Write-Host "========================================" -ForegroundColor Cyan
}

function Write-Check($label, $exists) {
    if ($exists) {
        Write-Host "  [✅] $label" -ForegroundColor Green
    } else {
        Write-Host "  [❌] $label" -ForegroundColor Red
    }
}

function Write-Info($text) {
    Write-Host "  ℹ $text" -ForegroundColor Yellow
}

# ============ 配置路径 ============
$IDF_PATH       = "E:\ESP-IDF\5.5.4\v5.5.4\esp-idf"
$IDF_TOOLS_PATH = "E:\ESP-IDF\554"
$GIT_PATH       = "E:\GIT\Git\cmd\git.exe"
$PYTHON_PATH    = "$IDF_TOOLS_PATH\python_env\idf5.5_py3.11_env\Scripts\python.exe"
$CMAKE_DIR      = "$IDF_TOOLS_PATH\tools\cmake\3.30.2\bin"
$NINJA_DIR      = "$IDF_TOOLS_PATH\tools\ninja\1.12.1"

Write-Header "ESP-IDF 环境验证脚本"

# ============ 1. 验证关键文件存在性 ============
Write-Header "1. 验证关键文件"

$checks = @(
    @{ Path = $IDF_PATH;              Label = "ESP-IDF 源码目录" },
    @{ Path = "$IDF_PATH\install.bat"; Label = "install.bat" },
    @{ Path = "$IDF_PATH\export.bat"; Label = "export.bat" },
    @{ Path = $GIT_PATH;               Label = "Git 可执行文件" },
    @{ Path = $PYTHON_PATH;            Label = "Python 虚拟环境" },
    @{ Path = "$CMAKE_DIR\cmake.exe";  Label = "CMake" },
    @{ Path = "$NINJA_DIR\ninja.exe";   Label = "Ninja" }
)

$allGood = $true
foreach ($check in $checks) {
    $exists = Test-Path -Path $check.Path
    Write-Check $check.Label $exists
    if (-not $exists) { $allGood = $false }
}

if (-not $allGood) {
    Write-Header "错误：部分文件缺失"
    Write-Info "请确认上述缺失项后重新运行脚本。"
    Read-Host "按 Enter 退出"
    exit 1
}

# ============ 2. 设置环境变量（当前会话） ============
Write-Header "2. 激活 ESP-IDF 环境"

$env:IDF_PATH        = $IDF_PATH
$env:IDF_TOOLS_PATH  = $IDF_TOOLS_PATH
$env:ESP_IDF_VERSION = "5.5.4"

# 将工具目录加入 PATH（当前会话）
$toolPaths = @(
    $PYTHON_PATH | Split-Path          # Python Scripts 目录
    $CMAKE_DIR
    $NINJA_DIR
    $GIT_PATH | Split-Path             # Git cmd 目录
)

$env:PATH = ($toolPaths -join ";") + ";" + $env:PATH

Write-Info "IDF_PATH = $env:IDF_PATH"
Write-Info "Python   = $PYTHON_PATH"
Write-Info "CMake    = $CMAKE_DIR\cmake.exe"
Write-Info "Ninja    = $NINJA_DIR\ninja.exe"

# ============ 3. 验证各组件版本 ============
Write-Header "3. 验证各组件版本"

# Git 版本
try {
    $gitVer = & $GIT_PATH --version 2>$null
    Write-Info "Git: $gitVer"
} catch { Write-Info "Git: 无法获取版本" }

# Python 版本
try {
    $pyVer = & $PYTHON_PATH --version 2>$null
    Write-Info "Python: $pyVer"
} catch { Write-Info "Python: 无法获取版本" }

# CMake 版本
try {
    $cmakeVer = & "$CMAKE_DIR\cmake.exe" --version 2>$null | Select-Object -First 1
    Write-Info "CMake: $cmakeVer"
} catch { Write-Info "CMake: 无法获取版本" }

# Ninja 版本
try {
    $ninjaVer = & "$NINJA_DIR\ninja.exe" --version 2>$null
    Write-Info "Ninja: $ninjaVer"
} catch { Write-Info "Ninja: 无法获取版本" }

# ============ 4. 核心验证：运行 idf.py ============
Write-Header "4. 核心验证：idf.py --version"

try {
    $idfPyPath = "$IDF_PATH\tools\idf.py"
    $output = & $PYTHON_PATH $idfPyPath --version 2>&1
    if ($output -match "ESP-IDF") {
        Write-Check "idf.py 运行成功: $output" $true
    } else {
        Write-Check "idf.py 输出异常: $output" $false
    }
} catch {
    Write-Check "idf.py 运行失败: $_" $false
}

# ============ 5. 可选：将环境变量写入系统 ============
if ($SetSystemEnv) {
    Write-Header "5. 写入系统环境变量"
    try {
        [Environment]::SetEnvironmentVariable("IDF_PATH", $IDF_PATH, "User")
        [Environment]::SetEnvironmentVariable("IDF_TOOLS_PATH", $IDF_TOOLS_PATH, "User")
        Write-Check "系统环境变量 IDF_PATH 已写入" $true
        Write-Check "系统环境变量 IDF_TOOLS_PATH 已写入" $true
        Write-Info "需要重新登录 Windows 或重启应用才能生效"
    } catch {
        Write-Check "写入系统环境变量失败" $false
        Write-Info "请以管理员身份运行 PowerShell 后重试"
    }
} else {
    Write-Header "5. 系统环境变量（可选）"
    Write-Info "当前环境变量仅在当前 PowerShell 会话中有效"
    Write-Info "如需永久生效，请重新运行脚本并加上参数："
    Write-Info "  & '<脚本路径>' -SetSystemEnv"
}

# ============ 6. 总结 ============
Write-Header "验证完成"
Write-Info "如果 idf.py 成功输出版本号，说明 ESP-IDF 环境已可用！"
Write-Info ""
Write-Info "VS Code 中仍需手动重新加载窗口 (Ctrl+Shift+P -> Reload Window)"
Write-Info "以便 ESP-IDF 扩展识别到已更新的配置。"

Read-Host "`n按 Enter 退出"
