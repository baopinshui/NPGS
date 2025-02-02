import subprocess

# 获取所有过时的包
outdated_packages = subprocess.check_output(['pip', 'list', '--outdated']).decode('utf-8').split('\n')[2:-1]

# 提取包名
packages_to_update = [pkg.split()[0] for pkg in outdated_packages]

# 更新所有过时的包
for package in packages_to_update:
    subprocess.call(['pip', 'install', '--upgrade', package])