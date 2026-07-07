import os
import subprocess
import sys

# 清除 MSYSTEM 环境变量（如果存在）
if 'MSYSTEM' in os.environ:
    del os.environ['MSYSTEM']

# 设置 ESP-IDF 环境变量
os.environ['IDF_PATH'] = r'E:\ESP-IDF\5.5.4\v5.5.4\esp-idf'
os.environ['IDF_TOOLS_PATH'] = r'E:\ESP-IDF\554'

# 更新 PATH
tool_paths = [
    r'E:\ESP-IDF\554\python_env\idf5.5_py3.11_env\Scripts',
    r'E:\ESP-IDF\554\tools\cmake\3.30.2\bin',
    r'E:\ESP-IDF\554\tools\ninja\1.12.1',
    r'E:\GIT\Git\cmd',
]

os.environ['PATH'] = ';'.join(tool_paths) + ';' + os.environ.get('PATH', '')

# 运行 idf.py --version
python_exe = r'E:\ESP-IDF\554\python_env\idf5.5_py3.11_env\Scripts\python.exe'
idf_py = r'E:\ESP-IDF\5.5.4\v5.5.4\esp-idf\tools\idf.py'

result = subprocess.run(
    [python_exe, idf_py, '--version'],
    capture_output=True,
    text=True
)

print(f'Return code: {result.returncode}')
print(f'Stdout: {result.stdout.strip()}')
print(f'Stderr: {result.stderr.strip()}')
