import os
import sys

# 清除 MSYSTEM 环境变量
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

print("=== ESP-IDF 环境验证 ===")
print(f"IDF_PATH: {os.environ['IDF_PATH']}")
print(f"IDF_TOOLS_PATH: {os.environ['IDF_TOOLS_PATH']}")
print(f"MSYSTEM in env: {'MSYSTEM' in os.environ}")

# 直接调用 idf.py 的 main 函数
sys.path.insert(0, r'E:\ESP-IDF\5.5.4\v5.5.4\esp-idf\tools')

# 设置 idf.py 的 argv
sys.argv = ['idf.py', '--version']

# 执行 idf.py
exec(open(r'E:\ESP-IDF\5.5.4\v5.5.4\esp-idf\tools\idf.py').read())

# 手动调用 main（因为 exec 不会触发 __name__ == '__main__'）
try:
    main()
except SystemExit as e:
    print(f"Exit code: {e.code}")
