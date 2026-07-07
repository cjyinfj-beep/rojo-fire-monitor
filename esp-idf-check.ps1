# ESP-IDF 环境检查与修复脚本
# 用途：检查诊断报告中提到的关键路径是否存在，并给出修复建议

param(
    [switch]$FixGitPath
)

$ErrorActionPreference = "Stop"
$Host.UI.RawUI.BackgroundColor = "Black"

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

Write-Header "ESP-IDF 环境检查脚本"

# 1. 检查 ESP-IDF 源码路径
Write-Header "1. ESP-IDF 源码路径检查"
$idfCandidates = @(
    "E:\ESP-IDF\esp-idf",
    "E:\ESP-IDF\v5.4\esp-idf",
    "C:\Users\$env:USERNAME\esp\esp-idf",
    "C:\Users\$env:USERNAME\esp-idf",
    "C:\esp\esp-idf"
)

$foundIdf = $false
foreach ($path in $idfCandidates) {
    if (Test-Path -Path "$path\install.bat") {
        Write-Check "找到 ESP-IDF 源码: $path" $true
        $foundIdf = $true
        break
    }
}
if (-not $foundIdf) {
    Write-Check "ESP-IDF 源码 (install.bat)" $false
    Write-Info "建议: 执行 'git clone -b v5.4 --recursive https://github.com/espressif/esp-idf.git' 到 E:\ESP-IDF\esp-idf"
}

# 2. 检查 ESP-IDF Tools 路径
Write-Header "2. ESP-IDF Tools 路径检查"
$toolsPath = "E:\ESP-IDF\554"
if (Test-Path -Path $toolsPath) {
    Write-Check "Tools 根目录: $toolsPath" $true
    if (Test-Path -Path "$toolsPath\tools") {
        Write-Check "Tools 子目录存在" $true
    } else {
        Write-Check "Tools 子目录存在" $false
        Write-Info "建议: 运行 ESP-IDF 安装器，或执行 install.bat 下载工具"
    }
} else {
    Write-Check "Tools 根目录: $toolsPath" $false
    Write-Info "建议: 在 VS Code 设置中确认 idf.toolsPath 的值"
}

# 3. 检查 Git
Write-Header "3. Git 路径检查"
$gitPaths = @(
    "E:\GIT\Git\cmd\git.exe",
    "C:\Program Files\Git\cmd\git.exe",
    "C:\Program Files (x86)\Git\cmd\git.exe",
    "${env:ProgramFiles}\Git\cmd\git.exe"
)

$foundGit = $false
foreach ($gitPath in $gitPaths) {
    if (Test-Path -Path $gitPath) {
        Write-Check "Git 可执行文件: $gitPath" $true
        $foundGit = $true

        # 尝试获取版本
        try {
            $gitVersion = & $gitPath --version 2>$null
            Write-Info "Git 版本: $gitVersion"
        } catch {
            Write-Info "无法获取 Git 版本，但文件存在"
        }
        break
    }
}

if (-not $foundGit) {
    Write-Check "Git 可执行文件" $false
    Write-Info "建议: 从 https://git-scm.com/download/win 安装 Git，建议路径不要有空格"
} else {
    # 提示 VS Code 设置
    Write-Info "请在 VS Code 设置 (Ctrl+,) 中搜索 'idf.gitPath'，将其指向上述正确路径"
    Write-Info "或者手动编辑 settings.json，添加: `"idf.gitPath`": `"E:\\GIT\\Git\\cmd\\git.exe`""
}

# 4. 检查 Python 虚拟环境
Write-Header "4. Python 虚拟环境检查"
$pythonCandidates = @(
    "C:\Users\$env:USERNAME\.espressif\python_env\idf5.4_py3.11_env\Scripts\python.exe",
    "C:\Users\$env:USERNAME\.espressif\python_env\idf5.4_py3.12_env\Scripts\python.exe",
    "C:\Users\$env:USERNAME\.espressif\python_env\idf5.3_py3.11_env\Scripts\python.exe",
    "E:\ESP-IDF\554\python_env\Scripts\python.exe"
)

$foundPython = $false
foreach ($pyPath in $pythonCandidates) {
    if (Test-Path -Path $pyPath) {
        Write-Check "Python 虚拟环境: $pyPath" $true
        $foundPython = $true
        try {
            $pyVersion = & $pyPath --version 2>$null
            Write-Info "Python 版本: $pyVersion"
        } catch {}
        break
    }
}

if (-not $foundPython) {
    Write-Check "Python 虚拟环境" $false
    Write-Info "建议: 运行 ESP-IDF 安装器，或手动执行 esp-idf\install.bat 来创建虚拟环境"
}

# 5. 检查 CMake 和 Ninja
Write-Header "5. CMake / Ninja 检查"
$cmakeInPath = $null
$ninjaInPath = $null

try { $cmakeInPath = Get-Command cmake -ErrorAction SilentlyContinue } catch {}
try { $ninjaInPath = Get-Command ninja -ErrorAction SilentlyContinue } catch {}

$toolsCMake = "$toolsPath\tools\cmake\"
$toolsNinja = "$toolsPath\tools\ninja\"

if ($cmakeInPath) {
    Write-Check "CMake 在系统 PATH 中" $true
    Write-Info "路径: $($cmakeInPath.Source)"
} elseif (Test-Path -Path $toolsCMake) {
    Write-Check "CMake 在 Tools 目录中" $true
    Write-Info "请确保 VS Code 已重新加载，以便扩展识别此路径"
} else {
    Write-Check "CMake" $false
    Write-Info "建议: 运行 install.bat 下载，或从 https://cmake.org/download/ 安装"
}

if ($ninjaInPath) {
    Write-Check "Ninja 在系统 PATH 中" $true
    Write-Info "路径: $($ninjaInPath.Source)"
} elseif (Test-Path -Path $toolsNinja) {
    Write-Check "Ninja 在 Tools 目录中" $true
    Write-Info "请确保 VS Code 已重新加载，以便扩展识别此路径"
} else {
    Write-Check "Ninja" $false
    Write-Info "建议: 运行 install.bat 下载，Ninja 通常随 ESP-IDF 工具一起安装"
}

# 6. 检查环境变量空格问题
Write-Header "6. 环境变量空格检查"
$envPath = $env:PATH
if ($envPath -match "\s") {
    Write-Check "系统 PATH 包含空格" $true
    Write-Info "警告: ESP-IDF 工具链对路径空格敏感。建议将工具安装在无空格路径下。"
    Write-Info "当前路径中的空格可能导致某些工具脚本执行失败。"
} else {
    Write-Check "系统 PATH 无空格" $true
}

# 7. 自动修复 Git 路径（可选）
if ($FixGitPath -and $foundGit) {
    Write-Header "7. 尝试自动修复 VS Code 设置"
    $vscodeSettings = "$env:APPDATA\Code\User\settings.json"
    $gitAbsolute = "E:\GIT\Git\cmd\git.exe"

    if (Test-Path -Path $vscodeSettings) {
        try {
            $settings = Get-Content -Path $vscodeSettings -Raw | ConvertFrom-Json -AsHashtable
            if ($null -eq $settings) { $settings = @{} }
            $settings["idf.gitPath"] = $gitAbsolute
            $settings | ConvertTo-Json -Depth 10 | Set-Content -Path $vscodeSettings
            Write-Check "已更新 VS Code 设置: idf.gitPath = $gitAbsolute" $true
            Write-Info "请按 Ctrl+Shift+P -> 'Developer: Reload Window' 重新加载 VS Code"
        } catch {
            Write-Check "自动修复 settings.json 失败" $false
            Write-Info "请手动修改: $vscodeSettings"
            Write-Info "添加或修改: `"idf.gitPath`": `"$gitAbsolute`""
        }
    } else {
        Write-Check "VS Code 用户设置文件不存在: $vscodeSettings" $false
    }
} elseif ($FixGitPath) {
    Write-Info "跳过自动修复，因为未找到 Git 可执行文件。"
}

# 总结
Write-Header "检查完成总结"
Write-Info "下一步操作："
Write-Info "1. 根据上面的检查结果，确认缺失的项目"
Write-Info "2. 在 VS Code 中运行 'ESP-IDF: Configure ESP-IDF Extension' 配置 IDF_PATH"
Write-Info "3. 修复 Git 路径（可运行本脚本带 -FixGitPath 参数自动修复）"
Write-Info "4. 重新加载 VS Code 窗口 (Ctrl+Shift+P -> Reload Window)"
Write-Info "5. 在 VS Code 终端运行 'idf.py --version' 验证"

Write-Host "`n按 Enter 键退出..." -ForegroundColor Gray
Read-Host
