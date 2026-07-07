@echo off
chcp 65001 >nul
echo ==========================================
echo 🔥 消防监测系统 - 一键上传并连接
echo ==========================================
echo.

REM 检查是否安装了 scp
where scp >nul 2>nul
if %errorlevel% neq 0 (
    echo ❌ 未找到 scp 命令
    echo    请先安装 Git for Windows
    echo    下载地址: https://git-scm.com/download/win
    echo.
    pause
    exit /b
)

echo 📦 正在上传代码到服务器...
scp -r "C:\Users\CJY\Documents\kimi\workspace\rojo-backend" root@139.196.153.51:/root/

if %errorlevel% neq 0 (
    echo.
    echo ❌ 上传失败，请检查：
    echo    1. 服务器密码是否正确
    echo    2. 网络是否连接
    echo.
    pause
    exit /b
)

echo ✅ 代码上传成功！
echo.
echo 🔌 正在连接服务器执行安装...
ssh root@139.196.153.51 "cd /root/rojo-backend && chmod +x install.sh && sudo ./install.sh"

pause
