import pandas as pd
import matplotlib.pyplot as plt
from mpl_toolkits.mplot3d import Axes3D
import os
import numpy as np

# 获取当前脚本所在目录
current_dir = os.path.dirname(__file__)

# 构建数据文件的相对路径
file_path = os.path.join(current_dir, 'Output.csv')

# 读取数据文件
data = pd.read_csv(file_path, header=None, names=['X', 'Y', 'Z'])

# 提取X、Y、Z列数据
x = data['X']
y = data['Y']
z = data['Z']

# 设置层的厚度
layer_thickness = 0.1

# 获取Z坐标的最小值和最大值
z_min = z.min()
z_max = z.max()

# 计算层数
num_layers = int(np.ceil((z_max - z_min) / layer_thickness))

def plot_layer(layer_index):
    # 创建3D图形
    fig = plt.figure()
    ax = fig.add_subplot(111, projection='3d')

    # 计算当前层的Z坐标范围
    z_lower = z_min + layer_index * layer_thickness
    z_upper = z_lower + layer_thickness
    mask = (z >= z_lower) & (z < z_upper)

    # 绘制当前层的散点图
    ax.scatter(x[mask], y[mask], z[mask], c='r', marker='o')

    # 设置坐标轴标签
    ax.set_xlabel('X Label')
    ax.set_ylabel('Y Label')
    ax.set_zlabel('Z Label')

    # 设置坐标轴范围
    ax.set_xlim([x.min(), x.max()])
    ax.set_ylim([y.min(), y.max()])
    ax.set_zlim([z.min(), z.max()])

    # 显示图形
    plt.show()

def plot_all():
    # 创建3D图形
    fig = plt.figure()
    ax = fig.add_subplot(111, projection='3d')

    # 绘制所有数据的散点图
    ax.scatter(x, y, z, c='r', marker='o')

    # 设置坐标轴标签
    ax.set_xlabel('X Label')
    ax.set_ylabel('Y Label')
    ax.set_zlabel('Z Label')

    # 设置坐标轴范围
    ax.set_xlim([x.min(), x.max()])
    ax.set_ylim([y.min(), y.max()])
    ax.set_zlim([z.min(), z.max()])

    # 显示图形
    plt.show()

# 选择显示模式
mode = input("请输入显示模式（all 或 layer）：")

if mode == 'all':
    plot_all()
elif mode == 'layer':
    # 选择要显示的层索引（从0到num_layers-1）
    layer_index = int(input(f"请输入要显示的层索引（0到{num_layers-1}）："))
    plot_layer(layer_index)
else:
    print("无效的显示模式")